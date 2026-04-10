"""
LangGraph ProjectState TypedDict with annotated reducers.
"""

from __future__ import annotations

import operator
from typing import Annotated, Optional, TypedDict

from langgraph.graph import add_messages


# ── Reducers ─────────────────────────────────────────────────────────────

def replace(existing: object, new: object) -> object:
    """Last-writer-wins reducer: always returns *new*."""
    return new


# ── State ────────────────────────────────────────────────────────────────

class ProjectState(TypedDict):
    """Central state schema for the AV lifecycle orchestrator graph."""

    messages: Annotated[list, add_messages]
    workflow_phase: Annotated[str, replace]
    project_context: Annotated[Optional[dict], replace]
    bod_narrative: Annotated[Optional[str], replace]
    spatial_data: Annotated[Optional[dict], replace]
    bom_json: Annotated[Optional[dict], replace]
    rom_estimate: Annotated[Optional[dict], replace]
    financials: Annotated[Optional[dict], replace]
    compliance_status: Annotated[Optional[dict], replace]
    deal_registrations: Annotated[list[dict], operator.add]
    error: Annotated[Optional[str], replace]


# ── Phase ordering ───────────────────────────────────────────────────────

PHASE_ORDER: list[str] = [
    "intake",
    "spatial_analysis",
    "design",
    "estimation",
    "compliance",
    "deal_registration",
    "complete",
]


def get_next_phase(current_phase: str) -> str | None:
    """Return the next phase after *current_phase*, or ``None`` if at the end.

    Raises:
        ValueError: If *current_phase* is not found in ``PHASE_ORDER``.
    """
    try:
        idx = PHASE_ORDER.index(current_phase)
    except ValueError:
        raise ValueError(
            f"Unknown phase '{current_phase}'. "
            f"Valid phases: {PHASE_ORDER}"
        ) from None

    if idx + 1 < len(PHASE_ORDER):
        return PHASE_ORDER[idx + 1]
    return None


def create_initial_state() -> dict:
    """Return a fresh state dictionary suitable for graph invocation."""
    return {
        "messages": [],
        "workflow_phase": PHASE_ORDER[0],
        "project_context": None,
        "bod_narrative": None,
        "spatial_data": None,
        "bom_json": None,
        "rom_estimate": None,
        "financials": None,
        "compliance_status": None,
        "deal_registrations": [],
        "error": None,
    }
