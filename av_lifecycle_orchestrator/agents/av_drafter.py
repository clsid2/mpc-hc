"""Architectural AV Drafter agent.

Handles the ``design`` workflow phase. For each room in ``spatial_data``,
constructs room parameters and calls the XAVIA API (or a mock) to generate
a Bill of Materials (BOM). Falls back to LLM-based BOM generation if XAVIA
is unavailable. Merges per-room BOMs into a unified project-level
``bom_json``.
"""

import json
import logging
from datetime import datetime, timezone
from typing import Any

from agents.base import BaseAgent, get_llm_from_config

logger = logging.getLogger(__name__)

# ── LLM fallback prompt ────────────────────────────────────────────────────

_BOM_GENERATION_PROMPT = """\
You are an expert AV system designer. Given room parameters, generate a
detailed Bill of Materials (BOM) for the AV equipment needed.

Return a JSON array of items. Each item must have:
{
  "manufacturer": "string",
  "model_number": "string",
  "description": "string",
  "category": "Displays|Microphones|Speakers|Cameras|DSPs|Control Systems|Cables|Mounts|Switchers|Other",
  "quantity": int,
  "unit_cost": float,
  "unit_price": float,
  "room_assignment": "string (room name)"
}

Use realistic manufacturers (Crestron, Biamp, Shure, Samsung, QSC, etc.)
and realistic pricing. Return ONLY the JSON array, no markdown or extra text.
"""


# ── XAVIA integration ──────────────────────────────────────────────────────

async def _generate_bom_via_xavia(room: dict) -> list[dict]:
    """Call the XAVIA API to generate a BOM for a single room.

    Returns a list of BOM item dicts, or raises on failure.
    """
    from tools.xavia.client import XAVIAClient
    from config import get_settings

    settings = get_settings()
    client = XAVIAClient(
        api_key=settings.xavia_api_key,
        base_url=settings.xavia_base_url,
        mock_mode=settings.mock_mode,
    )

    try:
        room_params = {
            "room_type": room.get("room_type", "conference"),
            "capacity": room.get("capacity", 10),
            "length_ft": room.get("length_ft", 20.0),
            "width_ft": room.get("width_ft", 15.0),
        }

        result = await client.generate_bom(room_params)
        products = result.get("recommended_products", [])
        room_name = room.get("name", "Unknown Room")

        items: list[dict] = []
        for prod in products:
            items.append({
                "manufacturer": prod.get("manufacturer", "Unknown"),
                "model_number": prod.get("model", ""),
                "description": prod.get("description", prod.get("name", "")),
                "category": prod.get("category", "Other"),
                "quantity": 1,
                "unit_cost": prod.get("msrp", 0) * 0.6,  # Estimate dealer cost
                "unit_price": prod.get("msrp", 0),
                "room_assignment": room_name,
            })

        return items
    finally:
        await client.close()


async def _generate_bom_via_llm(room: dict) -> list[dict]:
    """Use the LLM as a fallback to generate a BOM for a single room."""
    llm = get_llm_from_config()
    agent = BaseAgent(llm=llm)

    room_description = (
        f"Room: {room.get('name', 'Unknown')}\n"
        f"Type: {room.get('room_type', 'conference')}\n"
        f"Capacity: {room.get('capacity', 10)}\n"
        f"Dimensions: {room.get('length_ft', 20)}ft x {room.get('width_ft', 15)}ft "
        f"({room.get('area_sqft', 300)} sq ft)\n"
        f"Ceiling Height: {room.get('ceiling_height_ft', 9)}ft\n"
        f"AV Requirements: {', '.join(room.get('av_requirements', ['standard AV']))}\n"
        f"Existing Infrastructure: {', '.join(room.get('existing_infrastructure', ['none known']))}"
    )

    response_text = await agent.invoke_llm(
        user_message=room_description,
        system_message=_BOM_GENERATION_PROMPT,
    )

    # Parse the JSON response
    response_text = response_text.strip()
    if response_text.startswith("```"):
        response_text = response_text.split("```")[1]
        if response_text.startswith("json"):
            response_text = response_text[4:]
        response_text = response_text.strip()

    items = json.loads(response_text)

    # Ensure room_assignment is set
    room_name = room.get("name", "Unknown Room")
    for item in items:
        if not item.get("room_assignment"):
            item["room_assignment"] = room_name

    return items


# ── Main node function ─────────────────────────────────────────────────────

async def av_drafter_node(state: dict) -> dict:
    """Architectural AV Drafter node.

    Generates a project-level BOM by creating per-room BOMs via XAVIA
    (with LLM fallback) and merging them.
    """
    logger.info("AV Drafter starting BOM generation")

    try:
        spatial_data = state.get("spatial_data")
        project_context = state.get("project_context")

        if not spatial_data or not spatial_data.get("rooms"):
            return {"error": "No spatial_data with rooms available for BOM generation."}

        rooms = spatial_data["rooms"]
        project_name = "Untitled Project"
        if project_context:
            project_name = project_context.get("project_name", project_name)

        all_items: list[dict] = []
        generation_source = "xavia"

        for room in rooms:
            room_name = room.get("name", "Unknown Room")
            logger.info("Generating BOM for room: %s", room_name)

            try:
                # Try XAVIA first
                room_items = await _generate_bom_via_xavia(room)
                logger.info(
                    "XAVIA generated %d item(s) for room '%s'",
                    len(room_items), room_name,
                )
            except Exception as xavia_exc:
                logger.warning(
                    "XAVIA BOM generation failed for room '%s': %s. "
                    "Falling back to LLM.",
                    room_name, xavia_exc,
                )
                try:
                    room_items = await _generate_bom_via_llm(room)
                    generation_source = "llm_fallback"
                    logger.info(
                        "LLM generated %d item(s) for room '%s'",
                        len(room_items), room_name,
                    )
                except Exception as llm_exc:
                    logger.error(
                        "LLM BOM generation also failed for room '%s': %s",
                        room_name, llm_exc,
                    )
                    room_items = []

            all_items.extend(room_items)

        # Compute totals
        equipment_subtotal = sum(
            item.get("unit_price", 0) * item.get("quantity", 1)
            for item in all_items
        )

        bom_json: dict[str, Any] = {
            "project_name": project_name,
            "items": all_items,
            "equipment_subtotal": round(equipment_subtotal, 2),
            "total_items": len(all_items),
            "generated_by": generation_source,
            "generated_at": datetime.now(timezone.utc).isoformat(),
        }

        logger.info(
            "BOM generation complete: %d item(s), equipment subtotal=$%.2f",
            len(all_items), equipment_subtotal,
        )

        return {"bom_json": bom_json}

    except Exception as exc:
        logger.exception("AV Drafter failed")
        return {"error": f"AV Drafter error: {exc}"}
