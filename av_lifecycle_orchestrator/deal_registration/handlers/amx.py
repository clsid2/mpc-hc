import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)


@register_handler("amx")
class AMXDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for AMX -- portal API via AMX Partner Portal."""

    manufacturer_name = "amx"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Check AMX Partner Portal access."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"AMX deal registration missing required fields: {missing}"
            )

        if not deal.get("partner_portal_id"):
            raise ValueError(
                "AMX deal registration requires Partner Portal access. "
                "Please provide your partner_portal_id."
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "AMX deal registration requires a valid dealer/partner ID."
            )

        if not deal.get("company_code"):
            raise ValueError(
                "AMX deal registration requires a Company Code. "
                "Please provide your AMX-assigned company_code."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include Company Code, Department, terms acceptance, and preferred contact method."""
        context = self.get_template_context(deal)
        endpoint = self._template_config.get(
            "api_endpoint", "https://partner.amx.com/api/deal-registration"
        )

        return {
            "method": "portal_api",
            "endpoint": endpoint,
            "partner_portal_id": deal.get("partner_portal_id", ""),
            "company_code": deal.get("company_code", ""),
            "department": deal.get("department", "Sales"),
            "dealer_profile": {
                "dealer_name": context["dealer_name"],
                "dealer_id": context["dealer_id"],
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
            "terms_accepted": deal.get("terms_accepted", True),
            "preferred_contact_method": deal.get("preferred_contact_method", "email"),
        }

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration via AMX Partner Portal."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"AMX-{uuid.uuid4().hex[:8].upper()}"

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "portal_api",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "endpoint": payload["endpoint"],
            "payload": payload,
            "notes": (
                "Submitted via AMX Partner Portal. "
                f"Company Code: {payload['company_code']}. "
                f"Department: {payload['department']}. "
                f"Preferred contact method: {payload['preferred_contact_method']}."
            ),
        }
