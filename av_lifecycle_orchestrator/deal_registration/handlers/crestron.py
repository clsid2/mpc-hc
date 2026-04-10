import logging
import uuid
from datetime import datetime, timezone

from ..base import BaseDealRegistrationHandler
from ..registry import register_handler

logger = logging.getLogger(__name__)


@register_handler("crestron")
class CrestronDealHandler(BaseDealRegistrationHandler):
    """Deal registration handler for Crestron -- email-based with CRM merge tags."""

    manufacturer_name = "crestron"

    async def validate_eligibility(self, deal: dict) -> bool:
        """Check dealer authorization standing. Deal must have a valid dealer_id."""
        missing = self.validate_required_fields(deal)
        if missing:
            raise ValueError(
                f"Crestron deal registration missing required fields: {missing}"
            )

        if not deal.get("dealer_id"):
            raise ValueError(
                "Crestron deal registration requires an authorized dealer ID. "
                "Ensure your Crestron dealer authorization is current."
            )

        dealer_status = deal.get("dealer_authorization_status", "active")
        if dealer_status != "active":
            raise ValueError(
                f"Dealer authorization status is '{dealer_status}'. "
                "Only dealers with 'active' authorization can register deals with Crestron."
            )

        return True

    async def prepare_payload(self, deal: dict) -> dict:
        """Build email payload using CRM merge tags for Crestron Fusion parsing."""
        context = self.get_template_context(deal)

        merge_tag_body = (
            f"[%CompanyName%] {context['end_user_company']}\n"
            f"[%ContractName%] {context['project_name']}\n"
            f"[%ConnectInfo%] {context['end_user_contact_name']} | {context['end_user_contact_email']}\n"
            f"[%DealerID%] {context['dealer_id']}\n"
            f"[%DealerName%] {context['dealer_name']}\n"
            f"[%ProjectDescription%] {context['project_description']}\n"
            f"[%EstimatedValue%] {context['estimated_value']}\n"
            f"[%EstimatedCloseDate%] {context['estimated_close_date']}\n"
            f"[%ProductLines%] {', '.join(context['product_lines'])}\n"
            f"[%Quantities%] {context['quantities']}\n"
        )

        return {
            "method": "email",
            "to": self._template_config.get("email_to", "dealreg@crestron.com"),
            "subject": f"Deal Registration: {context['project_name']} - {context['dealer_name']}",
            "body": merge_tag_body,
            "merge_tags": {
                "CompanyName": context["end_user_company"],
                "ContractName": context["project_name"],
                "ConnectInfo": f"{context['end_user_contact_name']} | {context['end_user_contact_email']}",
                "DealerID": context["dealer_id"],
                "DealerName": context["dealer_name"],
                "ProjectDescription": context["project_description"],
                "EstimatedValue": context["estimated_value"],
                "EstimatedCloseDate": context["estimated_close_date"],
                "ProductLines": context["product_lines"],
                "Quantities": context["quantities"],
            },
        }

    async def submit(self, deal: dict) -> dict:
        """Submit email-based deal registration with merge-tagged body."""
        await self.validate_eligibility(deal)
        payload = await self.prepare_payload(deal)

        registration_id = f"CRES-{uuid.uuid4().hex[:8].upper()}"

        return {
            "status": "submitted",
            "registration_id": registration_id,
            "manufacturer": self.manufacturer_name,
            "method": "email",
            "submitted_at": datetime.now(timezone.utc).isoformat(),
            "email_to": payload["to"],
            "email_subject": payload["subject"],
            "payload": payload,
            "notes": "Email-based submission with CRM merge tags for Crestron Fusion parsing.",
        }
