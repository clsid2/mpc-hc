"""Deal registration models for manufacturer deal-reg workflows."""

from datetime import date, datetime
from typing import Literal, Optional

from pydantic import BaseModel, Field, computed_field


class DealRegistration(BaseModel):
    """Data required to submit a deal registration to a manufacturer."""

    manufacturer: str
    dealer_name: str
    dealer_id: Optional[str] = None
    end_user_company: str
    end_user_contact_name: str
    end_user_contact_email: str
    end_user_contact_phone: Optional[str] = None
    project_name: str
    project_description: str
    estimated_close_date: date
    estimated_value: float
    product_lines: list[str] = Field(default_factory=list)
    quantities: dict[str, int] = Field(default_factory=dict)
    requested_discount_pct: Optional[float] = None
    supporting_documents: list[str] = Field(
        default_factory=list,
        description="Paths to supporting PDF documents.",
    )
    additional_fields: dict = Field(default_factory=dict)


class DealResult(BaseModel):
    """Outcome of a single deal registration submission."""

    manufacturer: str
    status: Literal["submitted", "approved", "pending", "rejected", "error"]
    registration_id: Optional[str] = None
    submitted_at: datetime = Field(default_factory=datetime.utcnow)
    approval_window_days: Optional[int] = None
    approval_deadline: Optional[date] = None
    response_message: Optional[str] = None
    discount_approved_pct: Optional[float] = None


class DealRegistrationBatch(BaseModel):
    """Batch of deal registration results for a project."""

    project_name: str
    registrations: list[DealResult] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_submitted(self) -> int:
        """Count of registrations with status 'submitted'."""
        return sum(1 for r in self.registrations if r.status == "submitted")

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_approved(self) -> int:
        """Count of registrations with status 'approved'."""
        return sum(1 for r in self.registrations if r.status == "approved")

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_pending(self) -> int:
        """Count of registrations with status 'pending'."""
        return sum(1 for r in self.registrations if r.status == "pending")
