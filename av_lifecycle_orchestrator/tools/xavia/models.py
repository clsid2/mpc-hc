"""
Pydantic models for XTEN-AV XAVIA API entities.
"""

from __future__ import annotations

from typing import Literal, Optional

from pydantic import BaseModel


class XAVIARoomParams(BaseModel):
    """Parameters describing a room for BOM generation."""

    room_type: str
    capacity: int
    length_ft: float
    width_ft: float
    ceiling_height_ft: Optional[float] = None
    use_case: Optional[str] = None
    budget_tier: Optional[Literal["economy", "standard", "premium"]] = None


class XAVIAProduct(BaseModel):
    """A product from the XAVIA catalogue."""

    id: str
    name: str
    manufacturer: str
    model: str
    category: str
    msrp: float
    description: str
    compatible_with: list[str] = []


class XAVIABOMResponse(BaseModel):
    """Response returned by the XAVIA BOM generation endpoint."""

    room_type: str
    recommended_products: list[XAVIAProduct]
    total_msrp: float
    compatibility_verified: bool
    notes: list[str] = []
