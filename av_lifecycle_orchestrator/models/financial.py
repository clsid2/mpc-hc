"""Financial models: labor estimates, ROM, and overall financial model."""

from pydantic import BaseModel, Field, computed_field


class LaborLineItem(BaseModel):
    """A single labor line item."""

    role: str
    hourly_rate: float
    estimated_hours: float

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total(self) -> float:
        """Total = hourly_rate * estimated_hours."""
        return self.hourly_rate * self.estimated_hours


class LaborEstimate(BaseModel):
    """Collection of labor line items with a computed total."""

    line_items: list[LaborLineItem] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_labor_cost(self) -> float:
        """Sum of all labor line-item totals."""
        return sum(item.total for item in self.line_items)


class ROMEstimate(BaseModel):
    """Rough Order of Magnitude estimate."""

    base_estimate: float
    equipment_subtotal: float
    labor_subtotal: float
    labor_breakdown: LaborEstimate
    contingency_pct: float = 0.10
    notes: list[str] = Field(default_factory=list)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def lower_bound(self) -> float:
        """Conservative lower bound = base_estimate * 0.75."""
        return self.base_estimate * 0.75

    @computed_field  # type: ignore[prop-decorator]
    @property
    def upper_bound(self) -> float:
        """Conservative upper bound = base_estimate * 1.75."""
        return self.base_estimate * 1.75

    @computed_field  # type: ignore[prop-decorator]
    @property
    def contingency_amount(self) -> float:
        """Contingency = base_estimate * contingency_pct."""
        return self.base_estimate * self.contingency_pct


class FinancialModel(BaseModel):
    """Full financial model aggregating ROM, BOM, tax, and shipping."""

    rom: ROMEstimate
    bom_total: float
    tax_rate: float = 0.0
    shipping_total: float = 0.0

    @computed_field  # type: ignore[prop-decorator]
    @property
    def tax_amount(self) -> float:
        """Tax = bom_total * tax_rate."""
        return self.bom_total * self.tax_rate

    @computed_field  # type: ignore[prop-decorator]
    @property
    def grand_total(self) -> float:
        """Grand total = bom_total + tax_amount + shipping_total + labor + contingency."""
        return (
            self.bom_total
            + self.tax_amount
            + self.shipping_total
            + self.rom.labor_subtotal
            + self.rom.contingency_amount
        )
