"""
Shared async HTTP client factory with retry logic.
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field

import httpx
from tenacity import (
    retry,
    retry_if_exception,
    stop_after_attempt,
    wait_exponential,
    wait_fixed,
    RetryCallState,
)

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_TIMEOUT = httpx.Timeout(connect=10.0, read=30.0, write=30.0, pool=10.0)


@dataclass
class RetryConfig:
    """Configuration for request retry behaviour."""

    max_retries: int = 3
    base_delay: float = 1.0
    max_delay: float = 30.0
    retry_on_status: list[int] = field(
        default_factory=lambda: [429, 500, 502, 503, 504]
    )


# ---------------------------------------------------------------------------
# Client factory
# ---------------------------------------------------------------------------


def create_http_client(
    *,
    timeout: httpx.Timeout | None = None,
    headers: dict[str, str] | None = None,
    base_url: str = "",
) -> httpx.AsyncClient:
    """Return a pre-configured ``httpx.AsyncClient``.

    Parameters
    ----------
    timeout:
        Custom timeout configuration.  Falls back to ``DEFAULT_TIMEOUT``.
    headers:
        Default headers attached to every request.
    base_url:
        Optional base URL prepended to relative request paths.
    """
    return httpx.AsyncClient(
        timeout=timeout or DEFAULT_TIMEOUT,
        headers=headers or {},
        base_url=base_url,
        follow_redirects=True,
    )


# ---------------------------------------------------------------------------
# Retry-aware request helper
# ---------------------------------------------------------------------------


class _RetryableStatusError(Exception):
    """Raised internally when the response status code is retryable."""

    def __init__(self, response: httpx.Response) -> None:
        self.response = response
        super().__init__(
            f"Retryable HTTP {response.status_code} from {response.url}"
        )


def _should_retry(exc: BaseException) -> bool:
    """Tenacity predicate -- retry only on ``_RetryableStatusError``."""
    return isinstance(exc, _RetryableStatusError)


def _wait_for_retry_after(retry_state: RetryCallState):
    """Custom wait strategy that honours ``Retry-After`` on 429 responses."""
    exc = retry_state.outcome.exception()  # type: ignore[union-attr]
    if isinstance(exc, _RetryableStatusError) and exc.response.status_code == 429:
        retry_after = exc.response.headers.get("Retry-After")
        if retry_after is not None:
            try:
                wait_seconds = float(retry_after)
                logger.debug("Respecting Retry-After header: %.1fs", wait_seconds)
                return wait_seconds
            except (ValueError, TypeError):
                pass
    # Fall back to exponential backoff.
    return wait_exponential(
        multiplier=retry_state.retry_object.wait.keywords.get("multiplier", 1),  # type: ignore[attr-defined]
        min=retry_state.retry_object.wait.keywords.get("min", 1),  # type: ignore[attr-defined]
        max=retry_state.retry_object.wait.keywords.get("max", 30),  # type: ignore[attr-defined]
    )(retry_state)


async def request_with_retry(
    client: httpx.AsyncClient,
    method: str,
    url: str,
    retry_config: RetryConfig | None = None,
    **kwargs,
) -> httpx.Response:
    """Execute an HTTP request with automatic retries.

    Parameters
    ----------
    client:
        An ``httpx.AsyncClient`` instance (usually from ``create_http_client``).
    method:
        HTTP method (``GET``, ``POST``, etc.).
    url:
        Request URL (may be relative if *client* has a ``base_url``).
    retry_config:
        Retry parameters.  Uses sensible defaults when ``None``.
    **kwargs:
        Forwarded verbatim to ``client.request()``.
    """
    cfg = retry_config or RetryConfig()

    @retry(
        retry=retry_if_exception(_should_retry),
        stop=stop_after_attempt(cfg.max_retries + 1),
        wait=wait_exponential(
            multiplier=cfg.base_delay,
            min=cfg.base_delay,
            max=cfg.max_delay,
        ),
        before_sleep=lambda rs: logger.warning(
            "Retrying %s %s (attempt %d) after %s",
            method,
            url,
            rs.attempt_number,
            rs.outcome.exception() if rs.outcome else "unknown",
        ),
        reraise=True,
    )
    async def _do_request() -> httpx.Response:
        response = await client.request(method, url, **kwargs)
        if response.status_code in cfg.retry_on_status:
            raise _RetryableStatusError(response)
        response.raise_for_status()
        return response

    # Monkey-patch wait to honour Retry-After
    _do_request.retry.wait = _wait_for_retry_after  # type: ignore[attr-defined]

    try:
        return await _do_request()
    except _RetryableStatusError as exc:
        # All retries exhausted -- return the last response so callers can
        # inspect the status code rather than getting an opaque exception.
        return exc.response
