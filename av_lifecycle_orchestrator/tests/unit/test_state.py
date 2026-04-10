"""Tests for state schema, phase ordering, and initial state creation."""

import sys
from pathlib import Path

import pytest

# Ensure the package root is importable
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from state import ProjectState, PHASE_ORDER, get_next_phase, create_initial_state


class TestCreateInitialState:
    """Verify initial state has all required keys."""

    def test_create_initial_state(self):
        state = create_initial_state()
        required_keys = {
            "messages",
            "workflow_phase",
            "project_context",
            "bod_narrative",
            "spatial_data",
            "bom_json",
            "rom_estimate",
            "financials",
            "compliance_status",
            "deal_registrations",
            "error",
        }
        assert required_keys == set(state.keys())


class TestPhaseOrder:
    """Verify PHASE_ORDER contains all expected phases."""

    def test_phase_order(self):
        expected_phases = [
            "intake",
            "spatial_analysis",
            "design",
            "estimation",
            "compliance",
            "deal_registration",
            "complete",
        ]
        assert PHASE_ORDER == expected_phases


class TestGetNextPhase:
    """Test progression through all phases."""

    def test_get_next_phase(self):
        assert get_next_phase("intake") == "spatial_analysis"
        assert get_next_phase("spatial_analysis") == "design"
        assert get_next_phase("design") == "estimation"
        assert get_next_phase("estimation") == "compliance"
        assert get_next_phase("compliance") == "deal_registration"
        assert get_next_phase("deal_registration") == "complete"

    def test_get_next_phase_complete_returns_none(self):
        """Last phase returns None."""
        assert get_next_phase("complete") is None

    def test_get_next_phase_invalid_raises(self):
        with pytest.raises(ValueError, match="Unknown phase"):
            get_next_phase("nonexistent_phase")


class TestInitialStateValues:
    """All optional keys are None/empty."""

    def test_initial_state_values(self):
        state = create_initial_state()
        assert state["messages"] == []
        assert state["workflow_phase"] == "intake"
        assert state["project_context"] is None
        assert state["bod_narrative"] is None
        assert state["spatial_data"] is None
        assert state["bom_json"] is None
        assert state["rom_estimate"] is None
        assert state["financials"] is None
        assert state["compliance_status"] is None
        assert state["deal_registrations"] == []
        assert state["error"] is None
