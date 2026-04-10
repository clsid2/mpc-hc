"""Compliance result models: AVIXA, TAA, STIG, and full report."""

from datetime import datetime
from typing import Literal, Optional

from pydantic import BaseModel, Field, computed_field


class ComplianceCheckResult(BaseModel):
    """Result of a single compliance check."""

    check_name: str
    passed: bool
    severity: Literal["critical", "warning", "info"]
    message: str
    affected_items: list[str] = Field(default_factory=list)


class AVIXAComplianceResult(BaseModel):
    """AVIXA standard compliance results."""

    standard: str = "J-STD-710"
    checks: list[ComplianceCheckResult] = Field(default_factory=list)
    missing_attributes: list[dict] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def passed(self) -> bool:
        """True if every check passed."""
        return all(check.passed for check in self.checks)


class TAAComplianceResult(BaseModel):
    """Trade Agreements Act compliance results."""

    checks: list[ComplianceCheckResult] = Field(default_factory=list)
    non_compliant_items: list[str] = Field(default_factory=list)
    non_compliant_countries: list[str] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def passed(self) -> bool:
        """True if every check passed."""
        return all(check.passed for check in self.checks)


class STIGComplianceResult(BaseModel):
    """STIG (Security Technical Implementation Guide) compliance results."""

    stig_version: str
    checks: list[ComplianceCheckResult] = Field(default_factory=list)
    uncertified_items: list[str] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def passed(self) -> bool:
        """True if every check passed."""
        return all(check.passed for check in self.checks)


class FullComplianceReport(BaseModel):
    """Aggregated compliance report spanning AVIXA, TAA, and STIG."""

    avixa: Optional[AVIXAComplianceResult] = None
    taa: Optional[TAAComplianceResult] = None
    stig: Optional[STIGComplianceResult] = None
    generated_at: datetime = Field(default_factory=datetime.utcnow)
    requires_federal_compliance: bool = False

    @computed_field  # type: ignore[prop-decorator]
    @property
    def overall_passed(self) -> bool:
        """True when all present sub-reports pass."""
        results: list[bool] = []
        if self.avixa is not None:
            results.append(self.avixa.passed)
        if self.taa is not None:
            results.append(self.taa.passed)
        if self.stig is not None:
            results.append(self.stig.passed)
        return all(results) if results else True
