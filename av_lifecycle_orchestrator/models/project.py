"""Project context and client requirements models."""

from datetime import datetime
from typing import Optional

from pydantic import BaseModel, Field


class ClientRequirements(BaseModel):
    """Captures the client's high-level project requirements."""

    client_name: str
    project_name: str
    project_type: str = Field(
        ...,
        description="e.g. 'conference_room', 'all_hands', 'huddle'",
    )
    room_count: int
    special_requirements: list[str] = Field(default_factory=list)
    budget_range_low: Optional[float] = None
    budget_range_high: Optional[float] = None
    timeline_weeks: Optional[int] = None
    is_federal: bool = False
    is_military: bool = False


class RoomRequirement(BaseModel):
    """Requirements for a single room within the project."""

    room_name: str
    room_type: str
    capacity: int
    area_sqft: Optional[float] = None
    length_ft: Optional[float] = None
    width_ft: Optional[float] = None
    ceiling_height_ft: Optional[float] = None
    existing_av_infrastructure: list[str] = Field(default_factory=list)


class ProjectContext(BaseModel):
    """Top-level container for all project information."""

    client: ClientRequirements
    rooms: list[RoomRequirement] = Field(default_factory=list)
    opr_text: Optional[str] = Field(
        None,
        description="Owner's Project Requirements text",
    )
    notes: Optional[str] = None
    created_at: datetime = Field(default_factory=datetime.utcnow)
    updated_at: datetime = Field(default_factory=datetime.utcnow)
