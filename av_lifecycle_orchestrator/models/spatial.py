"""Spatial data models for floor-plan ingestion and quantity take-off."""

from typing import Literal, Optional

from pydantic import BaseModel, Field, computed_field


class DetectedSymbol(BaseModel):
    """A single symbol detected from a floor-plan source."""

    symbol_type: str
    x: float
    y: float
    width: float
    height: float
    confidence: float = Field(..., ge=0.0, le=1.0)
    layer: Optional[str] = None
    attributes: dict = Field(default_factory=dict)


class RoomDimensions(BaseModel):
    """Physical dimensions of a room."""

    length_ft: float
    width_ft: float
    area_sqft: float
    ceiling_height_ft: Optional[float] = None
    perimeter_ft: float


class FloorPlanData(BaseModel):
    """Parsed data extracted from a floor-plan file."""

    source_file: str
    source_type: Literal["dxf", "pdf", "png", "jpg"]
    rooms: list[dict] = Field(
        default_factory=list,
        description="Each entry has 'room_name' and 'dimensions' (RoomDimensions).",
    )
    detected_symbols: list[DetectedSymbol] = Field(default_factory=list)
    data_drops: int = 0
    power_receptacles: int = 0
    existing_av_devices: int = 0


class QuantityTakeoff(BaseModel):
    """Aggregated quantity take-off derived from a floor plan."""

    floor_plan: FloorPlanData
    total_rooms: int
    total_symbols: int
    extraction_method: str = Field(
        ...,
        description="Extraction method used: 'dxf', 'opencv', or 'vision'.",
    )
