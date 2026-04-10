"""Estimation & Procurement AI agent.

Handles the ``estimation`` workflow phase. Queries the Jetbuilt API for
dealer pricing on each BOM item, computes labor estimates based on
equipment count and room complexity, and builds a comprehensive financial
model with ROM bounds.
"""

import logging
from datetime import datetime, timezone
from typing import Any

from agents.base import BaseAgent

logger = logging.getLogger(__name__)

# ── Labor rate schedule ─────────────────────────────────────────────────────

LABOR_RATES: dict[str, float] = {
    "lead_installer": 85.0,
    "system_engineer": 125.0,
    "dsp_programmer": 150.0,
}

# ── Jetbuilt pricing integration ───────────────────────────────────────────


async def _query_jetbuilt_pricing(item: dict) -> dict:
    """Query Jetbuilt API for dealer pricing on a single BOM item.

    Returns a dict with ``cost``, ``price``, and ``shipping_cost`` keys,
    or the original item pricing if the query fails.
    """
    from config import get_settings
    from tools.jetbuilt.client import JetbuiltClient

    settings = get_settings()
    client = JetbuiltClient(
        token=settings.jetbuilt_api_token,
        base_url=settings.jetbuilt_base_url,
        mock_mode=settings.mock_mode,
    )

    try:
        search_query = f"{item.get('manufacturer', '')} {item.get('model_number', '')}".strip()
        if not search_query:
            return {}

        products = await client.get_products(search=search_query)

        if not products:
            return {}

        # Find the best match (first result)
        match = products[0] if isinstance(products, list) else {}

        return {
            "cost": match.get("cost", 0),
            "price": match.get("price", 0),
            "shipping_cost": match.get("shipping_cost", 0),
            "jetbuilt_product_id": match.get("id"),
            "pricing_source": "jetbuilt",
        }
    except Exception as exc:
        logger.warning(
            "Jetbuilt pricing query failed for '%s': %s",
            search_query, exc,
        )
        return {}
    finally:
        await client.close()


def _compute_labor_estimate(total_items: int, num_rooms: int) -> dict:
    """Compute labor hours and costs based on equipment count and room count.

    Heuristic:
    - Base hours = total_items * 0.5
    - Lead installer = base * 0.5
    - System engineer = base * 0.3
    - DSP programmer = base * 0.2
    - Room complexity adds 2 hours per room for each role.
    """
    base_hours = total_items * 0.5

    lead_hours = round(base_hours * 0.5 + (num_rooms * 2), 2)
    engineer_hours = round(base_hours * 0.3 + (num_rooms * 2), 2)
    dsp_hours = round(base_hours * 0.2 + (num_rooms * 2), 2)

    lead_cost = round(lead_hours * LABOR_RATES["lead_installer"], 2)
    engineer_cost = round(engineer_hours * LABOR_RATES["system_engineer"], 2)
    dsp_cost = round(dsp_hours * LABOR_RATES["dsp_programmer"], 2)

    labor_subtotal = round(lead_cost + engineer_cost + dsp_cost, 2)

    return {
        "base_hours": base_hours,
        "roles": {
            "lead_installer": {
                "hours": lead_hours,
                "rate": LABOR_RATES["lead_installer"],
                "cost": lead_cost,
            },
            "system_engineer": {
                "hours": engineer_hours,
                "rate": LABOR_RATES["system_engineer"],
                "cost": engineer_cost,
            },
            "dsp_programmer": {
                "hours": dsp_hours,
                "rate": LABOR_RATES["dsp_programmer"],
                "cost": dsp_cost,
            },
        },
        "labor_subtotal": labor_subtotal,
    }


# ── Main node function ─────────────────────────────────────────────────────

async def estimator_node(state: dict) -> dict:
    """Estimation & Procurement AI node.

    1. Queries Jetbuilt for dealer pricing on each BOM item.
    2. Computes labor estimates based on equipment count and room complexity.
    3. Builds a financial model with equipment subtotal, labor subtotal,
       base estimate, ROM range, and contingency.
    """
    logger.info("Estimator starting pricing and labor analysis")

    try:
        bom_json = state.get("bom_json")

        if not bom_json or not bom_json.get("items"):
            return {"error": "No bom_json with items available for estimation."}

        items = bom_json["items"]
        updated_items: list[dict] = []

        # ── Query Jetbuilt pricing for each item ────────────────────────
        for item in items:
            pricing = await _query_jetbuilt_pricing(item)

            updated_item = dict(item)
            if pricing:
                # Update with Jetbuilt dealer pricing if available
                updated_item["dealer_cost"] = pricing.get("cost", item.get("unit_cost", 0))
                updated_item["dealer_price"] = pricing.get("price", item.get("unit_price", 0))
                updated_item["shipping_cost"] = pricing.get("shipping_cost", 0)
                updated_item["jetbuilt_product_id"] = pricing.get("jetbuilt_product_id")
                updated_item["pricing_source"] = "jetbuilt"
                logger.info(
                    "Jetbuilt pricing found for %s %s: cost=$%.2f, price=$%.2f",
                    item.get("manufacturer", "?"),
                    item.get("model_number", "?"),
                    pricing.get("cost", 0),
                    pricing.get("price", 0),
                )
            else:
                # Keep original pricing from BOM generation
                updated_item["dealer_cost"] = item.get("unit_cost", 0)
                updated_item["dealer_price"] = item.get("unit_price", 0)
                updated_item["shipping_cost"] = 0
                updated_item["pricing_source"] = "estimate"
                logger.info(
                    "No Jetbuilt pricing for %s %s; using estimated pricing.",
                    item.get("manufacturer", "?"),
                    item.get("model_number", "?"),
                )

            updated_items.append(updated_item)

        # ── Compute equipment subtotal ──────────────────────────────────
        equipment_subtotal = sum(
            item.get("dealer_cost", 0) * item.get("quantity", 1)
            for item in updated_items
        )
        shipping_total = sum(
            item.get("shipping_cost", 0) * item.get("quantity", 1)
            for item in updated_items
        )

        # ── Compute labor estimate ──────────────────────────────────────
        total_items = len(updated_items)
        # Infer number of rooms from unique room assignments
        room_assignments = set(
            item.get("room_assignment", "default") for item in updated_items
        )
        num_rooms = max(len(room_assignments), 1)

        labor = _compute_labor_estimate(total_items, num_rooms)

        # ── Build financial model ───────────────────────────────────────
        equipment_subtotal = round(equipment_subtotal, 2)
        labor_subtotal = labor["labor_subtotal"]
        base_estimate = round(equipment_subtotal + labor_subtotal + shipping_total, 2)
        contingency = round(base_estimate * 0.10, 2)

        financials: dict[str, Any] = {
            "equipment_subtotal": equipment_subtotal,
            "shipping_total": round(shipping_total, 2),
            "labor": labor,
            "labor_subtotal": labor_subtotal,
            "base_estimate": base_estimate,
            "contingency": contingency,
            "contingency_rate": 0.10,
            "total_with_contingency": round(base_estimate + contingency, 2),
            "num_rooms": num_rooms,
            "total_items": total_items,
            "computed_at": datetime.now(timezone.utc).isoformat(),
        }

        rom_estimate = {
            "base_estimate": base_estimate,
            "lower_bound": round(base_estimate * 0.75, 2),
            "upper_bound": round(base_estimate * 1.75, 2),
            "methodology": "jetbuilt_pricing_with_labor",
            "computed_at": datetime.now(timezone.utc).isoformat(),
        }

        # ── Update BOM JSON with pricing ────────────────────────────────
        updated_bom: dict[str, Any] = dict(bom_json)
        updated_bom["items"] = updated_items
        updated_bom["equipment_subtotal"] = equipment_subtotal
        updated_bom["pricing_updated_at"] = datetime.now(timezone.utc).isoformat()

        logger.info(
            "Estimation complete: equipment=$%.2f, labor=$%.2f, "
            "base=$%.2f, ROM=$%.2f-$%.2f",
            equipment_subtotal, labor_subtotal, base_estimate,
            rom_estimate["lower_bound"], rom_estimate["upper_bound"],
        )

        return {
            "bom_json": updated_bom,
            "financials": financials,
            "rom_estimate": rom_estimate,
        }

    except Exception as exc:
        logger.exception("Estimator failed")
        return {"error": f"Estimator error: {exc}"}
