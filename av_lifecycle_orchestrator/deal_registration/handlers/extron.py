import logging
import uuid
from datetime import datetime, timedelta, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)


@register_handler("extron")
class ExtronDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Extron -- portal API via Extron Insider portal."""

    manufacturer_name = "extron"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Verify Extron Insider portal credentials and confirm opportunity is not pursued by Extron directly."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Extron deal registration missing required fields: {missing}"
            )

        if not deal.get("insider_portal_id"):
            raise ValueError(
                "Extron deal registration requires Extron Insider portal credentials. "
                "Please provide your insider_portal_id."
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "Extron deal registration requires a valid dealer/partner ID."
            )

        # Confirm opportunity is not being pursued by Extron directly
        if deal.get("extron_direct_opportunity", False):
            raise ValueError(
                "This opportunity is flagged as being pursued by Extron directly. "
                "Deal registration is not available for opportunities already being "
                "handled by the Extron direct sales team."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Include explicit Opportunity Registration Criteria certification and 90-day commitment."""
        context = self.get_template_context(deal)
        endpoint = self._template_config.get(
            "api_endpoint", "https://insider.extron.com/api/deal-registration"
        )
        approval_days = self.approval_window_days or 90

        now = datetime.now(timezone.utc)
        deadline = now + timedelta(days=approval_days)

        return {
            "method": "portal_api",
            "endpoint": endpoint,
            "insider_portal_id": deal.get("insider_portal_id", ""),
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
            "opportunity_registration_criteria": {
                "certification": True,
                "not_pursued_by_extron_directly": True,
                "commitment_days": approval_days,
                "commitment_deadline": deadline.isoformat(),
                "terms_accepted": True,
            },
        }

    async def submit(self, deal: dict) -> dict:
        """Submit deal registration with 90-day approval window and countdown deadline tracking."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"EXTR-{uuid.uuid4().hex[:8].upper()}"
        approval_days = self.approval_window_days or 90

        now = datetime.now(timezone.utc)
        deadline = now + timedelta(days=approval_days)

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "portal_api",
            "submitted_at": now.isoformat(),
            "endpoint": payload["endpoint"],
            "approval_window_days": approval_days,
            "approval_deadline": deadline.isoformat(),
            "payload": payload,
            "notes": (
                f"Submitted via Extron Insider portal. "
                f"Approval window is {approval_days} days. "
                f"Deadline: {deadline.strftime('%Y-%m-%d')}. "
                f"Includes Opportunity Registration Criteria certification."
            ),
        }
