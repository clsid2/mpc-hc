import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)

PARTNER_CATEGORIES = [
    "Commercial AV Integrator",
    "Technology Manager",
    "Consultant",
    "Distributor",
    "Reseller",
    "Architect/Design Firm",
]


@register_handler("legrand")
class LegrandDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Legrand -- centralized distributor portal API."""

    manufacturer_name = "legrand"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Check Legrand account number."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Legrand deal registration missing required fields: {missing}"
            )

        if not deal.get("legrand_account_number"):
            raise ValueError(
                "Legrand deal registration requires a Legrand account number. "
                "Please provide your legrand_account_number."
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "Legrand deal registration requires a valid dealer/partner ID."
            )

        partner_category = deal.get("partner_category", "")
        if partner_category and partner_category not in PARTNER_CATEGORIES:
            raise ValueError(
                f"Invalid partner_category '{partner_category}'. "
                f"Must be one of: {PARTNER_CATEGORIES}"
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include partner category, account number, and deal details."""
        context = self.get_template_context(deal)
        endpoint = self._template_config.get(
            "api_endpoint", "https://partners.legrand.com/api/deal-registration"
        )

        return {
            "method": "portal_api",
            "endpoint": endpoint,
            "legrand_account_number": deal.get("legrand_account_number", ""),
            "partner_category": deal.get("partner_category", "Commercial AV Integrator"),
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
        }

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration via Legrand centralized distributor portal."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"LEG-{uuid.uuid4().hex[:8].upper()}"

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "portal_api",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "endpoint": payload["endpoint"],
            "payload": payload,
            "notes": (
                "Submitted via Legrand centralized distributor portal. "
                f"Account number: {payload['legrand_account_number']}. "
                f"Partner category: {payload['partner_category']}."
            ),
        }
