"""
Async client for the Jetbuilt REST API.
"""

from __future__ import annotations

import logging
from collections.abc import AsyncGenerator
from typing import Any

import httpx

from av_lifecycle_orchestrator.tools.common.http_client import (
    RetryConfig,
    create_http_client,
    request_with_retry,
)
from av_lifecycle_orchestrator.tools.common.rate_limiter import TokenBucketRateLimiter

logger = logging.getLogger(__name__)

_PAGE_SIZE = 25  # Jetbuilt returns 25 items per page


class JetbuiltClient:
    """Thin wrapper around the Jetbuilt v1 REST API.

    Parameters
    ----------
    token:
        API token used for authentication.
    base_url:
        Base URL for the Jetbuilt API (e.g. ``https://app.jetbuilt.com/api``).
    mock_mode:
        When ``True``, return canned fixture data instead of making real
        HTTP requests.
    """

    def __init__(
        self,
        token: str,
        base_url: str = "https://app.jetbuilt.com/api",
        mock_mode: bool = False,
    ) -> None:
        self.token = token
        self.base_url = base_url
        self.mock_mode = mock_mode
        self.rate_limiter = TokenBucketRateLimiter(max_tokens=50, refill_interval=10.0)
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
                    "Authorization": f"Token token={self.token}",
                    "Accept": "application/vnd.jetbuilt.v1",
                    "Content-Type": "application/json",
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
            return self._mock_response(endpoint)

        await self.rate_limiter.acquire()
        client = self._get_client()
        response = await request_with_retry(
            client, method, endpoint, self._retry_config, **kwargs
        )
        return response.json()

    # ------------------------------------------------------------------
    # Products
    # ------------------------------------------------------------------

    async def get_products(
        self,
        search: str | None = None,
        page: int = 1,
    ) -> list[dict]:
        """Retrieve a page of products, optionally filtered by *search*."""
        params: dict[str, Any] = {"page": page}
        if search:
            params["search"] = search
        return await self._request("GET", "/products", params=params)

    async def get_product(self, product_id: int | str) -> dict:
        """Retrieve a single product by its ID."""
        return await self._request("GET", f"/products/{product_id}")

    # ------------------------------------------------------------------
    # Projects
    # ------------------------------------------------------------------

    async def get_projects(self, page: int = 1) -> list[dict]:
        """Retrieve a page of projects."""
        return await self._request("GET", "/projects", params={"page": page})

    async def get_project(self, project_id: int | str) -> dict:
        """Retrieve a single project by its ID."""
        return await self._request("GET", f"/projects/{project_id}")

    async def create_project(self, data: dict) -> dict:
        """Create a new project."""
        return await self._request("POST", "/projects", json=data)

    async def add_product_to_project(
        self,
        project_id: int | str,
        product_data: dict,
    ) -> dict:
        """Add a product line-item to an existing project."""
        return await self._request(
            "POST",
            f"/projects/{project_id}/items",
            json=product_data,
        )

    # ------------------------------------------------------------------
    # Pagination helper
    # ------------------------------------------------------------------

    async def paginate_all(
        self,
        endpoint: str,
        params: dict[str, Any] | None = None,
    ) -> AsyncGenerator[dict, None]:
        """Iterate through *all* pages of a paginated endpoint.

        Yields individual items one-by-one.  Stops when a page returns
        fewer than ``_PAGE_SIZE`` items (indicating the last page).
        """
        page = 1
        base_params = dict(params or {})
        while True:
            base_params["page"] = page
            items = await self._request("GET", endpoint, params=base_params)
            if not isinstance(items, list):
                # Some endpoints wrap the list in an object.
                items = items.get("data", items.get("items", []))
            for item in items:
                yield item
            if len(items) < _PAGE_SIZE:
                break
            page += 1

    # ------------------------------------------------------------------
    # Mock data
    # ------------------------------------------------------------------

    @staticmethod
    def _mock_response(endpoint: str) -> Any:
        """Return realistic fixture data for the given *endpoint*."""
        if "/products/" in endpoint and not endpoint.endswith("/products"):
            return {
                "id": 10001,
                "name": "PTZ Camera 30x",
                "manufacturer": "PTZOptics",
                "model_number": "PT30X-SDI-GY-G3",
                "description": "30x optical zoom PTZ camera with SDI and HDMI output",
                "cost": 1599.00,
                "price": 2199.00,
                "shipping_cost": 25.00,
                "shipping_price": 50.00,
                "tax_equipment": 0.0,
                "category": "Cameras",
                "upc": "123456789012",
                "weight": 5.2,
            }
        if endpoint.endswith("/products") or "/products?" in endpoint:
            return [
                {
                    "id": 10001,
                    "name": "PTZ Camera 30x",
                    "manufacturer": "PTZOptics",
                    "model_number": "PT30X-SDI-GY-G3",
                    "description": "30x optical zoom PTZ camera",
                    "cost": 1599.00,
                    "price": 2199.00,
                    "shipping_cost": 25.00,
                    "shipping_price": 50.00,
                    "tax_equipment": 0.0,
                    "category": "Cameras",
                    "upc": None,
                    "weight": 5.2,
                },
                {
                    "id": 10002,
                    "name": "4K Presentation Switcher",
                    "manufacturer": "Crestron",
                    "model_number": "HD-PS622",
                    "description": "6x2 4K presentation switcher",
                    "cost": 3200.00,
                    "price": 4500.00,
                    "shipping_cost": 30.00,
                    "shipping_price": 60.00,
                    "tax_equipment": 0.0,
                    "category": "Switchers",
                    "upc": None,
                    "weight": 8.0,
                },
            ]
        if "/projects/" in endpoint and "/items" in endpoint:
            return {
                "id": 50001,
                "product_id": 10001,
                "product_name": "PTZ Camera 30x",
                "quantity": 2,
                "cost": 1599.00,
                "price": 2199.00,
                "phase": "Phase 1",
            }
        if "/projects/" in endpoint:
            return {
                "id": 2001,
                "name": "Corporate Boardroom AV Upgrade",
                "client_name": "Acme Corp",
                "status": "active",
                "created_at": "2025-01-15T10:30:00Z",
                "updated_at": "2025-03-20T14:45:00Z",
                "total_cost": 45000.00,
                "total_price": 67500.00,
            }
        if endpoint.endswith("/projects"):
            return [
                {
                    "id": 2001,
                    "name": "Corporate Boardroom AV Upgrade",
                    "client_name": "Acme Corp",
                    "status": "active",
                    "created_at": "2025-01-15T10:30:00Z",
                    "updated_at": "2025-03-20T14:45:00Z",
                    "total_cost": 45000.00,
                    "total_price": 67500.00,
                },
            ]
        return {}

    # ------------------------------------------------------------------
    # Cleanup
    # ------------------------------------------------------------------

    async def close(self) -> None:
        """Close the underlying HTTP client."""
        if self._client and not self._client.is_closed:
            await self._client.aclose()
