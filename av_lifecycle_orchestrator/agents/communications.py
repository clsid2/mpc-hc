"""Communications & Fulfillment AI agent.

Handles the ``deal_registration`` workflow phase. Identifies unique
manufacturers in the BOM, looks up the appropriate deal registration
handler for each, validates eligibility, prepares payloads, and submits
deal registrations. Also sends notification emails with BOM and BoD
attachments via the Gmail API.
"""

import json
import logging
from datetime import datetime, timezone
from typing import Any, Optional

from agents.base import BaseAgent

logger = logging.getLogger(__name__)


def _build_deal_dict(manufacturer: str, bom_json: dict,
                     project_context: dict, financials: dict) -> dict:
    """Build a deal registration dictionary from state data for a
    specific manufacturer."""
    # Filter BOM items for this manufacturer
    manufacturer_items = [
        item for item in bom_json.get("items", [])
        if item.get("manufacturer", "").lower() == manufacturer.lower()
    ]

    # Calculate manufacturer-specific totals
    manufacturer_total = sum(
        item.get("unit_price", item.get("dealer_price", 0)) * item.get("quantity", 1)
        for item in manufacturer_items
    )

    # Extract product lines (unique categories)
    product_lines = list(set(
        item.get("category", "Other") for item in manufacturer_items
    ))

    # Build quantities by model
    quantities: dict[str, int] = {}
    for item in manufacturer_items:
        model = item.get("model_number", item.get("model", "unknown"))
        quantities[model] = quantities.get(model, 0) + item.get("quantity", 1)

    return {
        "manufacturer": manufacturer,
        "dealer_name": project_context.get("client_name", ""),
        "dealer_id": project_context.get("dealer_id", ""),
        "end_user_company": project_context.get("end_user_company",
                                                 project_context.get("client_name", "")),
        "end_user_contact_name": project_context.get("end_user_contact_name", ""),
        "end_user_contact_email": project_context.get("end_user_contact_email", ""),
        "project_name": project_context.get("project_name", ""),
        "project_description": project_context.get("project_description", ""),
        "estimated_close_date": project_context.get("timeline", ""),
        "estimated_value": manufacturer_total,
        "product_lines": product_lines,
        "quantities": quantities,
        "items": manufacturer_items,
        "total_project_value": financials.get("base_estimate", 0),
    }


async def _submit_deal_registration(manufacturer: str,
                                    deal: dict) -> dict:
    """Submit a deal registration for a manufacturer using the registry handler.

    Returns a result dict with status and details.
    """
    try:
        from deal_registration.registry import get_handler
        handler = get_handler(manufacturer)
    except KeyError:
        logger.warning(
            "No deal registration handler registered for '%s'; skipping.",
            manufacturer,
        )
        return {
            "manufacturer": manufacturer,
            "status": "skipped",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "registration_id": None,
            "message": f"No handler registered for manufacturer '{manufacturer}'.",
        }

    try:
        # Validate required fields
        missing_fields = handler.validate_required_fields(deal)
        if missing_fields:
            logger.warning(
                "Deal registration for '%s' missing required fields: %s",
                manufacturer, missing_fields,
            )

        # Validate eligibility
        try:
            eligible = await handler.validate_eligibility(deal)
        except ValueError as ve:
            logger.warning(
                "Deal registration eligibility check failed for '%s': %s",
                manufacturer, ve,
            )
            return {
                "manufacturer": manufacturer,
                "status": "ineligible",
                "submitted_at": datetime.now(timezone.utc).isoformat(),
                "registration_id": None,
                "message": f"Eligibility check failed: {ve}",
            }

        # Prepare payload
        payload = await handler.prepare_payload(deal)

        # Submit
        result = await handler.submit(deal)

        return {
            "manufacturer": manufacturer,
            "status": result.get("status", "submitted"),
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "registration_id": result.get("registration_id"),
            "message": result.get("message", "Deal registration submitted successfully."),
        }

    except Exception as exc:
        logger.exception(
            "Deal registration submission failed for '%s'", manufacturer
        )
        return {
            "manufacturer": manufacturer,
            "status": "error",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "registration_id": None,
            "message": f"Submission error: {exc}",
        }


async def _send_notification_email(project_context: dict, bom_json: dict,
                                   bod_narrative: Optional[str]) -> None:
    """Send a notification email with BOM and BoD attachments via Gmail API.

    This is best-effort -- failures are logged but do not block the workflow.
    """
    try:
        from tools.gmail.client import GmailClient
        from config import get_settings

        settings = get_settings()
        client = GmailClient(
            credentials_json=settings.gmail_credentials_json,
            token_json=settings.gmail_token_json,
            sender_email=settings.gmail_sender_email,
            sender_name=settings.gmail_sender_name,
            mock_mode=settings.mock_mode,
        )

        recipient = project_context.get(
            "end_user_contact_email",
            project_context.get("notification_email", ""),
        )
        if not recipient:
            logger.info("No recipient email configured; skipping notification.")
            return

        project_name = project_context.get("project_name", "AV Project")
        subject = f"Deal Registration Update - {project_name}"

        # Build HTML body
        item_count = bom_json.get("total_items", 0)
        equipment_total = bom_json.get("equipment_subtotal", 0)
        manufacturers = list(set(
            item.get("manufacturer", "Unknown")
            for item in bom_json.get("items", [])
        ))

        body = f"""
        <h2>Deal Registration Notification</h2>
        <p>Deal registrations have been processed for project <strong>{project_name}</strong>.</p>
        <h3>Summary</h3>
        <ul>
            <li><strong>Equipment Items:</strong> {item_count}</li>
            <li><strong>Equipment Subtotal:</strong> ${equipment_total:,.2f}</li>
            <li><strong>Manufacturers:</strong> {', '.join(sorted(manufacturers))}</li>
        </ul>
        <p>Please see the attached BOM and Basis of Design documents for details.</p>
        """

        # Build attachments
        attachments: list[dict] = []

        # BOM as JSON attachment
        bom_bytes = json.dumps(bom_json, indent=2, default=str).encode("utf-8")
        attachments.append({
            "filename": f"{project_name.replace(' ', '_')}_BOM.json",
            "content": bom_bytes,
            "content_type": "application/json",
        })

        # BoD as text attachment (would normally be PDF)
        if bod_narrative:
            bod_bytes = bod_narrative.encode("utf-8")
            attachments.append({
                "filename": f"{project_name.replace(' ', '_')}_BoD.txt",
                "content": bod_bytes,
                "content_type": "text/plain",
            })

        await client.send_email(
            to=recipient,
            subject=subject,
            html_body=body,
            attachments=attachments if attachments else None,
        )
        logger.info("Notification email sent to %s", recipient)

    except Exception as exc:
        logger.warning("Failed to send notification email (non-blocking): %s", exc)


# ── Main node function ─────────────────────────────────────────────────────

async def communications_node(state: dict) -> dict:
    """Communications & Fulfillment AI node.

    1. Identifies unique manufacturers in the BOM.
    2. For each manufacturer, looks up a deal registration handler.
    3. Validates eligibility, prepares payload, and submits.
    4. Sends a notification email with BOM and BoD attachments.
    5. Collects all results into ``deal_registrations``.
    """
    logger.info("Communications agent starting deal registration")

    try:
        bom_json = state.get("bom_json")
        project_context = state.get("project_context") or {}
        financials = state.get("financials") or {}
        bod_narrative = state.get("bod_narrative")

        if not bom_json or not bom_json.get("items"):
            return {"error": "No bom_json with items available for deal registration."}

        items = bom_json["items"]

        # ── Identify unique manufacturers ───────────────────────────────
        manufacturers = list(set(
            item.get("manufacturer", "").strip()
            for item in items
            if item.get("manufacturer", "").strip()
        ))
        manufacturers.sort()

        logger.info(
            "Found %d unique manufacturer(s): %s",
            len(manufacturers), ", ".join(manufacturers),
        )

        # ── Process deal registrations ──────────────────────────────────
        results: list[dict] = []

        for manufacturer in manufacturers:
            logger.info("Processing deal registration for '%s'", manufacturer)

            deal = _build_deal_dict(manufacturer, bom_json, project_context, financials)
            result = await _submit_deal_registration(manufacturer, deal)
            results.append(result)

            logger.info(
                "Deal registration for '%s': status=%s, id=%s",
                manufacturer,
                result.get("status", "?"),
                result.get("registration_id", "N/A"),
            )

        # ── Send notification email (best-effort) ──────────────────────
        await _send_notification_email(project_context, bom_json, bod_narrative)

        logger.info(
            "Communications complete: %d deal registration(s) processed",
            len(results),
        )

        return {"deal_registrations": results}

    except Exception as exc:
        logger.exception("Communications agent failed")
        return {"error": f"Communications error: {exc}"}
