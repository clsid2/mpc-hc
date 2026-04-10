"""Supervisor agent -- the brain of the hub-and-spoke orchestration graph.

Routes workflow phases to specialized worker agents, evaluates phase
completeness, and manages transitions through the AV project lifecycle.
"""

import json
import logging
from typing import Optional

from langgraph.graph import END

from agents.base import BaseAgent, get_llm_from_config

logger = logging.getLogger(__name__)

# ── Phase ordering ──────────────────────────────────────────────────────────

PHASE_ORDER: list[str] = [
    "intake",
    "spatial_analysis",
    "design",
    "estimation",
    "compliance",
    "deal_registration",
    "complete",
]

# Map each phase to the state keys it must populate before advancing.
_PHASE_REQUIRED_KEYS: dict[str, list[str]] = {
    "intake": ["project_context"],
    "spatial_analysis": ["spatial_data", "bod_narrative", "rom_estimate"],
    "design": ["bom_json"],
    "estimation": ["financials", "rom_estimate"],
    "compliance": ["compliance_status"],
    "deal_registration": ["deal_registrations"],
}

# Map workflow_phase values to the LangGraph node names for routing.
_PHASE_TO_NODE: dict[str, str] = {
    "intake": "task_planner",
    "spatial_analysis": "task_planner",
    "design": "av_drafter",
    "estimation": "estimator",
    "compliance": "compliance",
    "deal_registration": "communications",
}

# ── Internal helpers ────────────────────────────────────────────────────────

_supervisor_agent = BaseAgent(llm=None)
_supervisor_agent.system_prompt = (
    "You are the Supervisor AI for an AV lifecycle orchestrator. "
    "Your job is to evaluate the quality and completeness of worker output. "
    "When given a state summary, respond with a JSON object containing: "
    '{"quality": "sufficient" | "insufficient", "reason": "<brief explanation>"}. '
    "Be concise."
)


def _check_phase_completeness(state: dict, phase: str) -> bool:
    """Return True if all required state keys for *phase* are populated."""
    required = _PHASE_REQUIRED_KEYS.get(phase, [])
    for key in required:
        value = state.get(key)
        if value is None:
            return False
        # For lists, ensure they are non-empty
        if isinstance(value, list) and len(value) == 0:
            return False
        # For dicts, ensure they are non-empty
        if isinstance(value, dict) and len(value) == 0:
            return False
    return True


def _get_next_phase(current: str) -> Optional[str]:
    """Return the next phase after *current*, or None if at the end."""
    try:
        idx = PHASE_ORDER.index(current)
    except ValueError:
        logger.error("Unknown phase '%s'; defaulting to 'intake'", current)
        return "intake"
    if idx + 1 < len(PHASE_ORDER):
        return PHASE_ORDER[idx + 1]
    return None


# ── Supervisor node ─────────────────────────────────────────────────────────

async def supervisor_node(state: dict) -> dict:
    """Evaluate the current workflow phase and decide the next step.

    1. Checks whether the current phase's required state keys are populated.
    2. If an error is set, decides whether to retry or halt.
    3. If the current phase is complete, advances to the next phase.
    4. Critical gate: blocks advancement to ``deal_registration`` unless
       ``compliance_status.passed`` is ``True``.
    5. Optionally uses the LLM to evaluate quality when keys exist but
       may contain insufficient data.

    Returns a partial state update with ``workflow_phase`` and ``error``.
    """
    current_phase = state.get("workflow_phase", "intake")
    error = state.get("error")

    logger.info("Supervisor evaluating phase '%s'", current_phase)

    # ── Handle errors ───────────────────────────────────────────────────
    if error:
        logger.warning("Error detected in phase '%s': %s", current_phase, error)
        # If we have already retried (error contains "retry"), halt.
        if "RETRY_ATTEMPTED" in str(error):
            logger.error("Retry already attempted; halting at phase '%s'", current_phase)
            return {
                "workflow_phase": current_phase,
                "error": f"HALTED: {error}",
            }
        # Otherwise, clear error and let the phase retry once.
        logger.info("Clearing error and retrying phase '%s'", current_phase)
        return {
            "workflow_phase": current_phase,
            "error": None,
        }

    # ── Check phase completeness ────────────────────────────────────────
    if current_phase == "complete":
        logger.info("Workflow is complete.")
        return {"workflow_phase": "complete", "error": None}

    phase_complete = _check_phase_completeness(state, current_phase)

    if not phase_complete:
        # Phase is not yet complete -- keep it as-is so the worker runs.
        logger.info("Phase '%s' is not yet complete; dispatching worker.", current_phase)
        return {"workflow_phase": current_phase, "error": None}

    # ── LLM quality evaluation (optional, lightweight) ──────────────────
    try:
        quality_ok = await _evaluate_quality(state, current_phase)
        if not quality_ok:
            logger.info(
                "LLM flagged phase '%s' output as insufficient; "
                "keeping phase for rework.",
                current_phase,
            )
            return {
                "workflow_phase": current_phase,
                "error": f"RETRY_ATTEMPTED: Quality check failed for phase '{current_phase}'",
            }
    except Exception as exc:
        # Quality evaluation is best-effort; don't block the pipeline.
        logger.warning("Quality evaluation failed (non-blocking): %s", exc)

    # ── Advance to next phase ───────────────────────────────────────────
    next_phase = _get_next_phase(current_phase)

    if next_phase is None:
        return {"workflow_phase": "complete", "error": None}

    # Gate: compliance must pass before deal_registration
    if next_phase == "deal_registration":
        compliance_status = state.get("compliance_status") or {}
        if not compliance_status.get("passed", False):
            logger.warning(
                "Compliance has not passed; blocking advancement to "
                "deal_registration. Keeping phase at 'compliance'."
            )
            return {
                "workflow_phase": "compliance",
                "error": "Compliance checks have not passed. "
                         "Non-compliant items must be resolved before deal registration.",
            }

    logger.info("Advancing from '%s' to '%s'", current_phase, next_phase)
    return {"workflow_phase": next_phase, "error": None}


# ── Quality evaluation via LLM ──────────────────────────────────────────────

async def _evaluate_quality(state: dict, phase: str) -> bool:
    """Use the LLM to judge whether phase output is sufficient.

    Returns True if quality is acceptable, False if rework is needed.
    """
    required_keys = _PHASE_REQUIRED_KEYS.get(phase, [])
    if not required_keys:
        return True

    # Build a summary of the relevant state for the LLM to evaluate.
    summary_parts: list[str] = [f"Phase: {phase}"]
    for key in required_keys:
        value = state.get(key)
        if value is None:
            summary_parts.append(f"  {key}: <missing>")
        elif isinstance(value, dict):
            # Truncate large dicts to keep prompt manageable
            truncated = json.dumps(value, default=str)[:2000]
            summary_parts.append(f"  {key}: {truncated}")
        elif isinstance(value, list):
            summary_parts.append(f"  {key}: list with {len(value)} item(s)")
        elif isinstance(value, str):
            summary_parts.append(f"  {key}: {value[:500]}")
        else:
            summary_parts.append(f"  {key}: {value}")

    summary = "\n".join(summary_parts)

    try:
        llm = get_llm_from_config()
        agent = BaseAgent(llm=llm)
        agent.system_prompt = _supervisor_agent.system_prompt

        response_text = await agent.invoke_llm(
            f"Evaluate the following phase output for completeness and quality:\n\n{summary}"
        )
        # Parse the LLM response -- expect JSON with "quality" key.
        response_text = response_text.strip()
        # Handle markdown code blocks
        if response_text.startswith("```"):
            response_text = response_text.split("```")[1]
            if response_text.startswith("json"):
                response_text = response_text[4:]
            response_text = response_text.strip()

        result = json.loads(response_text)
        quality = result.get("quality", "sufficient")
        reason = result.get("reason", "")

        if quality == "insufficient":
            logger.info("Quality evaluation: insufficient -- %s", reason)
            return False

        logger.info("Quality evaluation: sufficient -- %s", reason)
        return True

    except (json.JSONDecodeError, KeyError) as exc:
        logger.warning("Could not parse LLM quality response: %s", exc)
        # Default to accepting the output if we can't parse the response.
        return True


# ── Routing function ────────────────────────────────────────────────────────

def route_supervisor(state: dict) -> str:
    """Map the current ``workflow_phase`` to the appropriate worker node name.

    Returns the LangGraph node name string, or ``END`` for the ``complete``
    phase.
    """
    phase = state.get("workflow_phase", "intake")

    if phase == "complete":
        return END

    # If the workflow is halted due to an unrecoverable error, end.
    error = state.get("error", "")
    if error and "HALTED" in str(error):
        logger.error("Workflow halted: %s", error)
        return END

    node = _PHASE_TO_NODE.get(phase)
    if node is None:
        logger.error(
            "No worker node mapped for phase '%s'; ending workflow.", phase
        )
        return END

    logger.info("Routing to worker node '%s' for phase '%s'", node, phase)
    return node
