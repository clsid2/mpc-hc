"""
LangChain ``@tool`` wrappers for XTEN-AV XAVIA API operations.
"""

from __future__ import annotations

import json
import logging

from langchain_core.tools import tool

from av_lifecycle_orchestrator.config import get_settings
from av_lifecycle_orchestrator.tools.xavia.client import XAVIAClient

logger = logging.getLogger(__name__)


def _get_client() -> XAVIAClient:
    """Create an ``XAVIAClient`` from application settings."""
    settings = get_settings()
    return XAVIAClient(
        api_key=settings.xavia_api_key,
        base_url=settings.xavia_base_url,
        mock_mode=settings.mock_mode,
    )


@tool
async def generate_av_bom(
    room_type: str,
    capacity: int,
    length_ft: float,
    width_ft: float,
) -> str:
    """Generate an AV bill-of-materials for a room.

    Args:
        room_type: Type of room (e.g. "conference", "huddle", "auditorium").
        capacity: Number of people the room should accommodate.
        length_ft: Room length in feet.
        width_ft: Room width in feet.

    Returns:
        A formatted string with recommended products and total MSRP.
    """
    client = _get_client()
    try:
        result = await client.generate_bom(
            {
                "room_type": room_type,
                "capacity": capacity,
                "length_ft": length_ft,
                "width_ft": width_ft,
            }
        )

        lines: list[str] = [
            f"BOM for {result.get('room_type', room_type)} room "
            f"(capacity {capacity}):",
        ]
        for prod in result.get("recommended_products", []):
            lines.append(
                f"  - {prod.get('name')} ({prod.get('manufacturer')} "
                f"{prod.get('model')}) | MSRP: ${prod.get('msrp', 0):.2f}"
            )
        lines.append(f"Total MSRP: ${result.get('total_msrp', 0):.2f}")
        lines.append(
            f"Compatibility verified: {result.get('compatibility_verified', False)}"
        )
        for note in result.get("notes", []):
            lines.append(f"  Note: {note}")
        return "\n".join(lines)
    finally:
        await client.close()


@tool
async def check_av_compatibility(items_json: str) -> str:
    """Check whether a set of AV equipment items are compatible.

    Args:
        items_json: A JSON string representing a list of items. Each item
            should have at least ``id`` and ``name`` keys.

    Returns:
        A formatted compatibility report.
    """
    client = _get_client()
    try:
        items = json.loads(items_json)
        result = await client.check_compatibility(items)

        compatible = result.get("compatible", False)
        lines: list[str] = [
            f"Compatibility check: {'PASS' if compatible else 'FAIL'}",
        ]
        for issue in result.get("issues", []):
            lines.append(f"  Issue: {issue}")
        for rec in result.get("recommendations", []):
            lines.append(f"  Recommendation: {rec}")
        return "\n".join(lines)
    finally:
        await client.close()


@tool
async def search_av_products(query: str) -> str:
    """Search the XTEN-AV XAVIA product catalogue.

    Args:
        query: Free-text search term (e.g. manufacturer, model, keyword).

    Returns:
        A formatted string listing matching products.
    """
    client = _get_client()
    try:
        products = await client.search_products(query)

        if not products:
            return f"No XAVIA products found for query '{query}'."

        lines: list[str] = [f"Found {len(products)} product(s):"]
        for p in products:
            lines.append(
                f"  - [{p.get('id')}] {p.get('name')} "
                f"({p.get('manufacturer')} {p.get('model')}) "
                f"| MSRP: ${p.get('msrp', 0):.2f} | Category: {p.get('category')}"
            )
        return "\n".join(lines)
    finally:
        await client.close()
