"""
Pydantic models for Jetbuilt API entities.
"""

from __future__ import annotations

from typing import Optional

from pydantic import BaseModel


class JetbuiltProduct(BaseModel):
    """A product from the Jetbuilt catalogue."""

    id: int
    name: str
    manufacturer: str
    model_number: str
    description: str
    cost: float
    price: float
    shipping_cost: float
    shipping_price: float
    tax_equipment: float
    category: str
    upc: Optional[str] = None
    weight: Optional[float] = None


class JetbuiltProject(BaseModel):
    """A Jetbuilt project (quote / proposal)."""

    id: int
    name: str
    client_name: str
    status: str
    created_at: str
    updated_at: str
    total_cost: float
    total_price: float


class JetbuiltProjectItem(BaseModel):
    """A line-item within a Jetbuilt project."""

    id: int
    product_id: int
    product_name: str
    quantity: int
    cost: float
    price: float
    phase: Optional[str] = None
