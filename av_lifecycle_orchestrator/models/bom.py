"""Bill of Materials models."""

import uuid
from datetime import datetime
from typing import Literal, Optional

from pydantic import BaseModel, Field, computed_field


class BOMLineItem(BaseModel):
    """A single line item on a bill of materials."""

    item_id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    manufacturer: str
    model_number: str
    description: str
    category: Literal[
        "display",
        "audio",
        "video",
        "control",
        "infrastructure",
        "mounting",
        "cable",
        "dsp",
        "network",
        "other",
    ]
    quantity: int
    unit_cost: float = 0.0
    unit_price: float = 0.0
    shipping_cost: float = 0.0
    taa_compliant: Optional[bool] = None
    jitc_certified: Optional[bool] = None
    country_of_origin: Optional[str] = None
    room_assignment: Optional[str] = None

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_cost(self) -> float:
        """Total cost = quantity * unit_cost."""
        return self.quantity * self.unit_cost

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_price(self) -> float:
        """Total price = quantity * unit_price."""
        return self.quantity * self.unit_price


class BillOfMaterials(BaseModel):
    """Complete bill of materials for a project."""

    project_name: str
    items: list[BOMLineItem] = Field(default_factory=list)
    generated_by: str = Field(
        ...,
        description="'xavia' or 'manual'",
    )
    generated_at: datetime = Field(default_factory=datetime.utcnow)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def equipment_subtotal(self) -> float:
        """Sum of total_cost across all line items."""
        return sum(item.total_cost for item in self.items)

    @computed_field  # type: ignore[prop-decorator]
    @property
    def total_items(self) -> int:
        """Total number of line items."""
        return len(self.items)

    # -- convenience methods --------------------------------------------------

    def add_item(self, item: BOMLineItem) -> None:
        """Append a line item to the BOM."""
        self.items.append(item)

    def remove_item(self, item_id: str) -> bool:
        """Remove a line item by its item_id. Returns True if found and removed."""
        for idx, item in enumerate(self.items):
            if item.item_id == item_id:
                self.items.pop(idx)
                return True
        return False

    def get_by_manufacturer(self, manufacturer: str) -> list[BOMLineItem]:
        """Return all items matching the given manufacturer (case-insensitive)."""
        manufacturer_lower = manufacturer.lower()
        return [
            item
            for item in self.items
            if item.manufacturer.lower() == manufacturer_lower
        ]

    def get_by_category(self, category: str) -> list[BOMLineItem]:
        """Return all items matching the given category."""
        return [item for item in self.items if item.category == category]
