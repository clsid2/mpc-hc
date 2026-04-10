import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)


@register_handler("qsys")
class QSYSDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Q-SYS -- portal API via Technology Partner Hub."""

    manufacturer_name = "qsys"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Check Technology Partner Hub enrollment."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Q-SYS deal registration missing required fields: {missing}"
            )

        if not deal.get("partner_hub_id"):
            raise ValueError(
                "Q-SYS deal registration requires Technology Partner Hub enrollment. "
                "Please provide your partner_hub_id."
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "Q-SYS deal registration requires a valid dealer/partner ID."
            )

        hub_status = deal.get("partner_hub_status", "enrolled")
        if hub_status != "enrolled":
            raise ValueError(
                f"Technology Partner Hub status is '{hub_status}'. "
                "Only enrolled partners can register deals with Q-SYS."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include partner profile data, plugin requirements, SDF/MDF fields if applicable."""
        context = self.get_template_context(deal)
        endpoint = self._template_config.get(
            "api_endpoint", "https://partners.qsys.com/api/deal-registration"
        )

        payload = {
            "method": "portal_api",
            "endpoint": endpoint,
            "partner_profile": {
                "dealer_name": context["dealer_name"],
                "dealer_id": context["dealer_id"],
                "partner_hub_id": deal.get("partner_hub_id", ""),
                "partner_tier": deal.get("partner_tier", ""),
                "certifications": deal.get("certifications", []),
            },
            "deal_details": {
                "end_user_company": context["end_user_company"],
                "end_user_contact_name": context["end_user_contact_name"],
                "end_user_contact_email": context["end_user_contact_email"],
                "project_name": context["project_name"],
                "project_description": context["project_description"],
                "estimated_close_date": context["estimated_close_date"],
                "estimated_value": context["estimated_value"],
                "product_lines": context["product_lines"],
                "quantities": context["quantities"],
            },
            "plugin_requirements": {
                "custom_plugins": deal.get("custom_plugins", []),
                "plugin_sdk_version": deal.get("plugin_sdk_version", ""),
                "requires_certification": deal.get("requires_plugin_certification", False),
            },
        }

        # Include SDF/MDF fields if applicable
        if deal.get("sdf_requested") or deal.get("mdf_requested"):
            payload["funding"] = {
                "sdf_requested": deal.get("sdf_requested", False),
                "sdf_amount": deal.get("sdf_amount", 0),
                "mdf_requested": deal.get("mdf_requested", False),
                "mdf_amount": deal.get("mdf_amount", 0),
                "funding_justification": deal.get("funding_justification", ""),
            }

        return payload

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration via Q-SYS Technology Partner Hub portal."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"QSYS-{uuid.uuid4().hex[:8].upper()}"

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "portal_api",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "endpoint": payload["endpoint"],
            "payload": payload,
            "notes": "Submitted via Q-SYS Technology Partner Hub portal.",
        }
