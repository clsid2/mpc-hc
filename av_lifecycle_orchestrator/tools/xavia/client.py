"""
Async client for the XTEN-AV XAVIA API.
"""

from __future__ import annotations

import logging
from typing import Any, Optional

import httpx

from av_lifecycle_orchestrator.tools.common.http_client import (
    RetryConfig,
    create_http_client,
    request_with_retry,
)
from av_lifecycle_orchestrator.tools.common.rate_limiter import TokenBucketRateLimiter

logger = logging.getLogger(__name__)


class XAVIAClient:
    """Async wrapper around the XTEN-AV XAVIA API.

    Parameters
    ----------
    api_key:
        API key used for authentication via the ``X-API-Key`` header.
    base_url:
        Base URL for the XAVIA API.
    mock_mode:
        When ``True``, return canned fixture data instead of calling the
        real API.
    """

    def __init__(
        self,
        api_key: str,
        base_url: str = "https://api.xten-av.com/v1",
        mock_mode: bool = False,
    ) -> None:
        self.api_key = api_key
        self.base_url = base_url
        self.mock_mode = mock_mode
        self.rate_limiter = TokenBucketRateLimiter(max_tokens=30, refill_interval=10.0)
        self._retry_config = RetryConfig()
        self._client: httpx.AsyncClient | None = None

    # ------------------------------------------------------------------
    # HTTP helpers
    # ------------------------------------------------------------------

    def _get_client(self) -> httpx.AsyncClient:
        if self._client is None or self._client.is_closed:
            self._client = create_http_client(
                base_url=self.base_url,
                headers={
                    "X-API-Key": self.api_key,
                    "Content-Type": "application/json",
                    "Accept": "application/json",
                },
            )
        return self._client

    async def _request(
        self,
        method: str,
        endpoint: str,
        **kwargs: Any,
    ) -> Any:
        if self.mock_mode:
            return self._mock_response(endpoint, kwargs)

        await self.rate_limiter.acquire()
        client = self._get_client()
        response = await request_with_retry(
            client, method, endpoint, self._retry_config, **kwargs
        )
        return response.json()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    async def generate_bom(self, room_params: dict) -> dict:
        """Generate a bill-of-materials for a room.

        Parameters
        ----------
        room_params:
            Dictionary with keys such as ``room_type``, ``capacity``,
            ``length_ft``, ``width_ft``, etc.
        """
        return await self._request("POST", "/bom/generate", json=room_params)

    async def check_compatibility(self, items: list[dict]) -> dict:
        """Check compatibility among a set of AV equipment items."""
        return await self._request(
            "POST", "/compatibility/check", json={"items": items}
        )

    async def search_products(
        self,
        query: str,
        category: Optional[str] = None,
    ) -> list[dict]:
        """Search the XAVIA product catalogue."""
        params: dict[str, str] = {"q": query}
        if category:
            params["category"] = category
        return await self._request("GET", "/products", params=params)

    async def get_room_recommendation(
        self,
        room_type: str,
        capacity: int,
    ) -> dict:
        """Get an AV equipment recommendation for a room type and size."""
        return await self._request(
            "GET",
            "/rooms/recommend",
            params={"room_type": room_type, "capacity": capacity},
        )

    # ------------------------------------------------------------------
    # Mock data
    # ------------------------------------------------------------------

    @staticmethod
    def _mock_response(endpoint: str, kwargs: dict[str, Any]) -> Any:
        """Return realistic AV equipment fixture data."""

        _display = {
            "id": "xav-prod-001",
            "name": "86\" 4K Interactive Display",
            "manufacturer": "Samsung",
            "model": "QM86R-B",
            "category": "Displays",
            "msrp": 5499.00,
            "description": "86-inch 4K UHD professional interactive display",
            "compatible_with": ["xav-prod-003", "xav-prod-004"],
        }
        _dsp = {
            "id": "xav-prod-002",
            "name": "Audio DSP Processor",
            "manufacturer": "Biamp",
            "model": "TesiraFORTE AI",
            "category": "DSPs",
            "msrp": 3200.00,
            "description": "Networked audio DSP with AEC",
            "compatible_with": ["xav-prod-005"],
        }
        _mount = {
            "id": "xav-prod-003",
            "name": "Heavy Duty Flat Wall Mount",
            "manufacturer": "Chief",
            "model": "PNRUB",
            "category": "Mounts",
            "msrp": 349.00,
            "description": "Universal flat wall mount for 42-86 inch displays",
            "compatible_with": ["xav-prod-001"],
        }
        _cable_hdmi = {
            "id": "xav-prod-004",
            "name": "Active Optical HDMI Cable 50ft",
            "manufacturer": "Crestron",
            "model": "CBL-HD-50RGBH",
            "category": "Cables",
            "msrp": 189.00,
            "description": "50ft active optical HDMI 2.0 cable",
            "compatible_with": ["xav-prod-001"],
        }
        _mic = {
            "id": "xav-prod-005",
            "name": "Ceiling Microphone Array",
            "manufacturer": "Shure",
            "model": "MXA920",
            "category": "Microphones",
            "msrp": 2899.00,
            "description": "Ceiling-mounted beamforming microphone array",
            "compatible_with": ["xav-prod-002"],
        }

        if "/bom/generate" in endpoint:
            return {
                "room_type": "conference",
                "recommended_products": [_display, _dsp, _mount, _cable_hdmi, _mic],
                "total_msrp": 12136.00,
                "compatibility_verified": True,
                "notes": [
                    "Recommended 1x display for rooms up to 20 people.",
                    "Ceiling mic covers up to 900 sq ft.",
                    "Mount is compatible with selected display.",
                ],
            }

        if "/compatibility/check" in endpoint:
            return {
                "compatible": True,
                "issues": [],
                "recommendations": [
                    "All selected items are compatible.",
                    "Consider adding a cable management kit.",
                ],
            }

        if "/products" in endpoint:
            return [_display, _dsp, _mount, _cable_hdmi, _mic]

        if "/rooms/recommend" in endpoint:
            return {
                "room_type": "conference",
                "capacity": 12,
                "recommended_products": [_display, _dsp, _mic],
                "total_msrp": 11598.00,
                "notes": [
                    "Standard conference room setup for 12 people.",
                ],
            }

        return {}

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

    async def close(self) -> None:
        """Close the underlying HTTP client."""
        if self._client and not self._client.is_closed:
            await self._client.aclose()
