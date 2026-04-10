"""Integration tests for the full LangGraph workflow."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from state import create_initial_state, PHASE_ORDER


class TestGraphCompiles:
    """build_graph() does not raise."""

    def test_graph_compiles(self):
        # Import inside test so failures are captured as test errors.
        # The graph module imports agent modules which may attempt to
        # instantiate LLM clients at import time.  Skip gracefully
        # when optional heavy dependencies (langchain_openai, etc.)
        # are not installed.
        try:
            from graph import build_graph
        except ImportError as exc:
            pytest.skip(f"Skipping graph compilation test due to missing dependency: {exc}")
        graph = build_graph()
        assert graph is not None


class TestInitialStateStructure:
    """create_initial_state returns valid state."""

    def test_initial_state_structure(self):
        state = create_initial_state()
        # Must have all required keys
        assert "messages" in state
        assert "workflow_phase" in state
        assert "project_context" in state
        assert "bod_narrative" in state
        assert "spatial_data" in state
        assert "bom_json" in state
        assert "rom_estimate" in state
        assert "financials" in state
        assert "compliance_status" in state
        assert "deal_registrations" in state
        assert "error" in state
        # workflow_phase should start at the first phase
        assert state["workflow_phase"] == PHASE_ORDER[0]
        assert state["workflow_phase"] == "intake"
        # Mutable defaults should be separate instances
        state2 = create_initial_state()
        assert state["messages"] is not state2["messages"]
        assert state["deal_registrations"] is not state2["deal_registrations"]
