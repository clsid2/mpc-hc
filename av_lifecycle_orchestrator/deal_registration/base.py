from abc import ABC, abstractmethod
from typing import Optional
import yaml
import logging
from pathlib import Path

logger = logging.getLogger(__name__)


class BaseDealRegistrationHandler(ABC):
    """Abstract base class for manufacturer-specific deal registration handlers."""

    manufacturer_name: str = ""
    _template_config: Optional[dict] = None

    def __init__(self):
        self._load_template()

    def _load_template(self) -> None:
        """Load the YAML template configuration for this manufacturer."""
        template_path = Path(__file__).parent / "templates" / f"{self.manufacturer_name}.yaml"
        if template_path.exists():
            with open(template_path) as f:
                self._template_config = yaml.safe_load(f)
        else:
            logger.warning(f"No template found at {template_path}")
            self._template_config = {}

    @property
    def registration_method(self) -> str:
        return self._template_config.get("registration_method", "email")

    @property
    def required_fields(self) -> list[str]:
        return self._template_config.get("required_fields", [])

    @property
    def processing_time_hours(self) -> Optional[int]:
        return self._template_config.get("processing_time_hours")

    @property
    def approval_window_days(self) -> Optional[int]:
        return self._template_config.get("approval_window_days")

    @property
    def constraints(self) -> dict:
        return self._template_config.get("constraints", {})

    @abstractmethod
    async def validate_eligibility(self, deal: dict) -> bool:
        """Check dealer authorization, quantity thresholds, etc.
        Raises ValueError if not eligible with explanation."""

    @abstractmethod
    async def prepare_payload(self, deal: dict) -> dict:
        """Build the manufacturer-specific submission payload (API body or email content)."""

    @abstractmethod
    async def submit(self, deal: dict) -> dict:
        """Submit the deal registration. Returns a result dict with status, registration_id, etc."""

    def get_template_context(self, deal: dict) -> dict:
        """Return Jinja2 context for email template rendering."""
        return {
            "manufacturer": self.manufacturer_name,
            "dealer_name": deal.get("dealer_name", ""),
            "dealer_id": deal.get("dealer_id", ""),
            "end_user_company": deal.get("end_user_company", ""),
            "end_user_contact_name": deal.get("end_user_contact_name", ""),
            "end_user_contact_email": deal.get("end_user_contact_email", ""),
            "project_name": deal.get("project_name", ""),
            "project_description": deal.get("project_description", ""),
            "estimated_close_date": deal.get("estimated_close_date", ""),
            "estimated_value": deal.get("estimated_value", 0),
            "product_lines": deal.get("product_lines", []),
            "quantities": deal.get("quantities", {}),
        }

    def validate_required_fields(self, deal: dict) -> list[str]:
        """Check that all required fields from the YAML template are present."""
        missing = []
        for field in self.required_fields:
            if field not in deal or not deal[field]:
                missing.append(field)
        return missing
