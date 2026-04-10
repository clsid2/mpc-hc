"""
LangChain ``@tool`` wrappers for Jetbuilt API operations.
"""

from __future__ import annotations

import asyncio
import json
import logging
from typing import Optional

from langchain_core.tools import tool

from av_lifecycle_orchestrator.config import get_settings
from av_lifecycle_orchestrator.tools.jetbuilt.client import JetbuiltClient

logger = logging.getLogger(__name__)


def _get_client() -> JetbuiltClient:
    """Create a ``JetbuiltClient`` from application settings."""
    settings = get_settings()
    return JetbuiltClient(
        token=settings.jetbuilt_api_token,
        base_url=settings.jetbuilt_base_url,
        mock_mode=settings.mock_mode,
    )


@tool
async def search_jetbuilt_products(
    query: str,
    category: Optional[str] = None,
) -> str:
    """Search the Jetbuilt product catalogue.

    Args:
        query: Free-text search term (e.g. manufacturer, model, keyword).
        category: Optional category filter (e.g. "Cameras", "Switchers").

    Returns:
        A formatted string listing matching products with pricing.
    """
    client = _get_client()
    try:
        products = await client.get_products(search=query)

        if category:
            products = [
                p
                for p in products
                if p.get("category", "").lower() == category.lower()
            ]

        if not products:
            return f"No Jetbuilt products found for query '{query}'."

        lines: list[str] = [f"Found {len(products)} product(s):"]
        for p in products:
            lines.append(
                f"  - [{p.get('id')}] {p.get('name')} "
                f"({p.get('manufacturer')} {p.get('model_number')}) "
                f"| Cost: ${p.get('cost', 0):.2f} | Price: ${p.get('price', 0):.2f}"
            )
        return "\n".join(lines)
    finally:
        await client.close()


@tool
async def get_jetbuilt_product_pricing(product_id: str) -> str:
    """Retrieve detailed pricing for a specific Jetbuilt product.

    Args:
        product_id: The Jetbuilt product ID.

    Returns:
        A formatted string with full pricing breakdown.
    """
    client = _get_client()
    try:
        product = await client.get_product(product_id)

        return (
            f"Product: {product.get('name')}\n"
            f"Manufacturer: {product.get('manufacturer')}\n"
            f"Model: {product.get('model_number')}\n"
            f"Category: {product.get('category')}\n"
            f"Cost: ${product.get('cost', 0):.2f}\n"
            f"Price: ${product.get('price', 0):.2f}\n"
            f"Shipping Cost: ${product.get('shipping_cost', 0):.2f}\n"
            f"Shipping Price: ${product.get('shipping_price', 0):.2f}\n"
            f"Tax (Equipment): ${product.get('tax_equipment', 0):.2f}"
        )
    finally:
        await client.close()


@tool
async def create_jetbuilt_project(name: str, client_name: str) -> str:
    """Create a new project in Jetbuilt.

    Args:
        name: Project name / title.
        client_name: Client or customer name associated with the project.

    Returns:
        A confirmation string with the new project's ID and details.
    """
    client = _get_client()
    try:
        project = await client.create_project(
            {"name": name, "client_name": client_name}
        )
        return (
            f"Project created successfully!\n"
            f"ID: {project.get('id')}\n"
            f"Name: {project.get('name')}\n"
            f"Client: {project.get('client_name')}\n"
            f"Status: {project.get('status')}"
        )
    finally:
        await client.close()
