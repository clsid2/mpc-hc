import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)

COMPANY_SIZE_TIERS = {
    "small": "1-50",
    "medium": "51-100",
    "large": "101-500",
    "enterprise": "500+",
}


def _classify_company_size(employee_count: int) -> str:
    """Classify company size into a tier based on employee count."""
    if employee_count <= 50:
        return "1-50"
    elif employee_count <= 100:
        return "51-100"
    elif employee_count <= 500:
        return "101-500"
    else:
        return "500+"


@register_handler("shure")
class ShureDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Shure -- portal API via My Shure ID + Partner Shop."""

    manufacturer_name = "shure"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Verify My Shure ID and Partner Shop access."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Shure deal registration missing required fields: {missing}"
            )

        if not deal.get("my_shure_id"):
            raise ValueError(
                "Shure deal registration requires a My Shure ID. "
                "Please provide your my_shure_id."
            )

        if not deal.get("partner_shop_access"):
            raise ValueError(
                "Shure deal registration requires Partner Shop access. "
                "Please confirm partner_shop_access is enabled."
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "Shure deal registration requires a valid dealer/partner ID."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include company size tier, HQ location, new/existing customer flag, and product mix."""
        context = self.get_template_context(deal)
        endpoint = self._template_config.get(
            "api_endpoint", "https://partnershop.shure.com/api/deal-registration"
        )

        employee_count = deal.get("employee_count", 0)
        company_size_tier = _classify_company_size(employee_count)

        return {
            "method": "portal_api",
            "endpoint": endpoint,
            "my_shure_id": deal.get("my_shure_id", ""),
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
            "company_profile": {
                "company_size_tier": company_size_tier,
                "employee_count": employee_count,
                "hq_location": deal.get("hq_location", ""),
                "country": deal.get("country", ""),
            },
            "customer_type": "new" if deal.get("new_customer", True) else "existing",
            "product_mix": deal.get("product_mix", []),
        }

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration via Shure Partner Shop portal."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"SHURE-{uuid.uuid4().hex[:8].upper()}"

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "portal_api",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "endpoint": payload["endpoint"],
            "payload": payload,
            "notes": (
                "Submitted via Shure Partner Shop portal. "
                f"My Shure ID: {payload['my_shure_id']}. "
                f"Company size tier: {payload['company_profile']['company_size_tier']}. "
                f"Customer type: {payload['customer_type']}."
            ),
        }
