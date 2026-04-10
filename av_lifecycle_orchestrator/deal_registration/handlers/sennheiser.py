import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)


@register_handler("sennheiser")
class SennheiserDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Sennheiser -- email-based, 50+ business users threshold."""

    manufacturer_name = "sennheiser"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Check 50+ business users threshold and verify Sennheiser Account Number."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Sennheiser deal registration missing required fields: {missing}"
            )

        if not deal.get("sennheiser_account_number"):
            raise ValueError(
                "Sennheiser deal registration requires a valid Sennheiser Account Number. "
                "Please provide your sennheiser_account_number."
            )

        min_users = self.constraints.get("min_business_users", 50)
        business_users = deal.get("business_users", 0)
        if business_users < min_users:
            raise ValueError(
                f"Sennheiser deal registration requires a minimum of {min_users} "
                f"business users. This deal specifies {business_users} users."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include account number, prospect details, and product quantities."""
        context = self.get_template_context(deal)

        return {
            "method": "email",
            "to": self._template_config.get("email_to", "dealreg@sennheiser.com"),
            "subject": (
                f"Deal Registration: {context['end_user_company']} - "
                f"{deal.get('business_users', 0)} users"
            ),
            "sennheiser_account_number": deal.get("sennheiser_account_number", ""),
            "prospect_details": {
                "end_user_company": context["end_user_company"],
                "end_user_contact_name": context["end_user_contact_name"],
                "end_user_contact_email": context["end_user_contact_email"],
                "business_users": deal.get("business_users", 0),
                "industry": deal.get("industry", ""),
            },
            "dealer_profile": {
                "dealer_name": context["dealer_name"],
                "dealer_id": context["dealer_id"],
            },
            "deal_details": {
                "project_name": context["project_name"],
                "project_description": context["project_description"],
                "estimated_close_date": context["estimated_close_date"],
                "estimated_value": context["estimated_value"],
                "product_lines": context["product_lines"],
                "quantities": context["quantities"],
            },
            "max_discount_percent": self.constraints.get("max_discount_percent", 15),
        }

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration with 48-business-hour approval window, max 15% discount."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"SENN-{uuid.uuid4().hex[:8].upper()}"
        approval_hours = self.processing_time_hours or 48
        max_discount = self.constraints.get("max_discount_percent", 15)

        return {
            "status": "pending",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "email",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "email_to": payload["to"],
            "approval_window_hours": approval_hours,
            "max_discount_percent": max_discount,
            "payload": payload,
            "notes": (
                f"Sennheiser deal registration submitted. "
                f"Approval within {approval_hours} business hours. "
                f"Maximum discount: {max_discount}%. "
                f"Minimum {self.constraints.get('min_business_users', 50)} business users required."
            ),
        }
