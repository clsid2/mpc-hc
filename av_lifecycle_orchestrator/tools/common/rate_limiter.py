"""
Async token-bucket rate limiter for controlling API request throughput.
"""

from __future__ import annotations

import asyncio
import time


class TokenBucketRateLimiter:
    """A simple token-bucket rate limiter suitable for ``asyncio`` workloads.

    Parameters
    ----------
    max_tokens:
        Maximum number of tokens (requests) the bucket can hold.
    refill_interval:
        Seconds between full refills of the bucket back to *max_tokens*.
    """

    def __init__(self, max_tokens: int, refill_interval: float) -> None:
        self.max_tokens = max_tokens
        self.refill_interval = refill_interval
        self._tokens = float(max_tokens)
        self._last_refill = time.monotonic()
        self._lock = asyncio.Lock()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    async def acquire(self) -> None:
        """Wait until a token is available, then consume one.

        If the bucket is empty the coroutine will sleep until enough time
        has elapsed for at least one token to be refilled.
        """
        while True:
            async with self._lock:
                self._refill()
                if self._tokens >= 1.0:
                    self._tokens -= 1.0
                    return
                # Calculate how long until at least one token is available.
                tokens_needed = 1.0 - self._tokens
                refill_rate = self.max_tokens / self.refill_interval
                wait_time = tokens_needed / refill_rate

            # Sleep *outside* the lock so other coroutines can proceed.
            await asyncio.sleep(wait_time)

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _refill(self) -> None:
        """Add tokens proportional to the time elapsed since the last refill."""
        now = time.monotonic()
        elapsed = now - self._last_refill
        if elapsed <= 0:
            return
        refill_rate = self.max_tokens / self.refill_interval
        new_tokens = elapsed * refill_rate
        self._tokens = min(self.max_tokens, self._tokens + new_tokens)
        self._last_refill = now
