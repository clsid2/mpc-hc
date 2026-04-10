"""Task Planner & Research AI agent.

Handles the ``intake`` and ``spatial_analysis`` workflow phases:
- **Intake**: extracts client requirements from unstructured messages and
  builds a structured ``project_context`` dictionary.
- **Spatial Analysis**: orchestrates floor-plan analysis (DXF, OpenCV,
  Vision), builds ``spatial_data``, generates a Basis of Design narrative
  via Jinja2 + LLM, and computes a rough-order-of-magnitude estimate.
"""

import json
import logging
from datetime import datetime, timezone
from typing import Any, Optional

from jinja2 import Environment, BaseLoader

from agents.base import BaseAgent, get_llm_from_config

logger = logging.getLogger(__name__)

# ── Jinja2 template for Basis of Design narrative ───────────────────────────

BOD_TEMPLATE = """\
# Basis of Design (BoD) Narrative

**Project:** {{ project_name }}
**Client:** {{ client_name }}
**Date:** {{ date }}

## 1. Project Overview

{{ project_description }}

## 2. Spatial Summary

{% for room in rooms -%}
### {{ room.name }}
- **Dimensions:** {{ room.length_ft }} ft x {{ room.width_ft }} ft ({{ room.area_sqft }} sq ft)
- **Capacity:** {{ room.capacity }} occupants
- **Room Type:** {{ room.room_type }}
{% if room.notes %}- **Notes:** {{ room.notes }}{% endif %}

{% endfor %}

## 3. Design Intent

The AV system design shall provide {{ design_intent }}.

## 4. Standards & Compliance

All equipment and installation shall conform to AVIXA J-STD-710 symbology
standards. {% if is_federal %}Federal procurement requirements including TAA
compliance and DISA RME Vendor STIG validation apply to this project.{% endif %}

## 5. Rough Order of Magnitude

- **Base Estimate:** ${{ "%.2f"|format(base_estimate) }}
- **ROM Range:** ${{ "%.2f"|format(lower_bound) }} -- ${{ "%.2f"|format(upper_bound) }}

*This ROM is preliminary and subject to refinement during detailed design.*
"""

_jinja_env = Environment(loader=BaseLoader(), autoescape=False)

# ── LLM prompts ────────────────────────────────────────────────────────────

_INTAKE_SYSTEM_PROMPT = """\
You are an AV project intake specialist. Given unstructured client messages,
extract structured project requirements.

Return a JSON object with the following keys:
{
  "project_name": "string",
  "client_name": "string",
  "project_description": "string",
  "rooms": [
    {
      "name": "string",
      "room_type": "conference|huddle|auditorium|classroom|lobby|other",
      "capacity": int,
      "length_ft": float,
      "width_ft": float,
      "notes": "string or null"
    }
  ],
  "is_federal": bool,
  "special_requirements": ["string"],
  "floor_plan_files": ["string"],
  "design_intent": "string",
  "budget_range": "string or null",
  "timeline": "string or null"
}

If information is missing, use reasonable defaults. Always return valid JSON.
"""

_SPATIAL_SYSTEM_PROMPT = """\
You are an AV spatial analysis expert. Given room data from floor-plan
analysis, produce a structured spatial data summary.

Return a JSON object with:
{
  "rooms": [
    {
      "name": "string",
      "room_type": "string",
      "length_ft": float,
      "width_ft": float,
      "area_sqft": float,
      "capacity": int,
      "ceiling_height_ft": float,
      "av_requirements": ["string"],
      "existing_infrastructure": ["string"],
      "notes": "string or null"
    }
  ],
  "total_rooms": int,
  "total_area_sqft": float,
  "analysis_source": "dxf|opencv|vision|manual"
}

Always return valid JSON.
"""


# ── Helper functions ────────────────────────────────────────────────────────

def _extract_messages_text(state: dict) -> str:
    """Concatenate all human message content from state messages."""
    messages = state.get("messages", [])
    parts: list[str] = []
    for msg in messages:
        if hasattr(msg, "content"):
            parts.append(str(msg.content))
        elif isinstance(msg, dict):
            parts.append(str(msg.get("content", "")))
        elif isinstance(msg, str):
            parts.append(msg)
    return "\n\n".join(parts)


def _compute_rom(spatial_data: dict) -> dict:
    """Compute a rough-order-of-magnitude estimate from spatial data.

    Heuristic: $150 per sq ft for AV fit-out as a baseline, adjusted by
    room type multipliers.
    """
    room_type_multipliers = {
        "conference": 1.0,
        "huddle": 0.7,
        "auditorium": 1.8,
        "classroom": 1.2,
        "lobby": 0.5,
        "boardroom": 1.4,
        "other": 1.0,
    }

    base_rate_per_sqft = 150.0
    total_base = 0.0

    rooms = spatial_data.get("rooms", [])
    for room in rooms:
        area = room.get("area_sqft", 0.0)
        room_type = room.get("room_type", "other").lower()
        multiplier = room_type_multipliers.get(room_type, 1.0)
        total_base += area * base_rate_per_sqft * multiplier

    # Minimum floor
    if total_base < 5000.0:
        total_base = 5000.0

    return {
        "base_estimate": round(total_base, 2),
        "lower_bound": round(total_base * 0.75, 2),
        "upper_bound": round(total_base * 1.75, 2),
        "methodology": "spatial_heuristic",
        "rate_per_sqft": base_rate_per_sqft,
        "computed_at": datetime.now(timezone.utc).isoformat(),
    }


def _generate_bod_narrative(project_context: dict, spatial_data: dict,
                            rom: dict) -> str:
    """Render the Basis of Design narrative using Jinja2."""
    rooms = spatial_data.get("rooms", [])
    template = _jinja_env.from_string(BOD_TEMPLATE)

    return template.render(
        project_name=project_context.get("project_name", "Untitled Project"),
        client_name=project_context.get("client_name", "Unknown Client"),
        date=datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        project_description=project_context.get(
            "project_description", "AV system design and integration project."
        ),
        rooms=rooms,
        design_intent=project_context.get(
            "design_intent",
            "reliable, high-quality audiovisual experiences across all spaces",
        ),
        is_federal=project_context.get("is_federal", False),
        base_estimate=rom.get("base_estimate", 0),
        lower_bound=rom.get("lower_bound", 0),
        upper_bound=rom.get("upper_bound", 0),
    )


async def _run_spatial_analysis(project_context: dict) -> dict:
    """Orchestrate floor-plan analysis using available tools.

    Tries DXF parser, OpenCV detector, and Vision analyzer in order,
    falling back to LLM-based analysis if no floor-plan files are found.
    """
    floor_plan_files = project_context.get("floor_plan_files", [])
    rooms_from_context = project_context.get("rooms", [])
    analysis_source = "manual"
    analyzed_rooms: list[dict] = []

    for file_path in floor_plan_files:
        file_lower = file_path.lower()
        try:
            if file_lower.endswith(".dxf"):
                from tools.spatial.dxf_parser import DXFParser
                parser = DXFParser()
                result = parser.full_extraction(file_path)
                raw_rooms = result.get("rooms", [])
                for room in raw_rooms:
                    analyzed_rooms.append({
                        "name": room.get("name", "Room"),
                        "room_type": room.get("room_type", "other"),
                        "length_ft": room.get("length_ft", 0),
                        "width_ft": room.get("width_ft", 0),
                        "area_sqft": room.get("area_sqft", 0),
                        "capacity": room.get("capacity", 0),
                        "ceiling_height_ft": room.get("ceiling_height_ft", 9.0),
                        "av_requirements": room.get("av_requirements", []),
                        "existing_infrastructure": room.get("existing_infrastructure", []),
                        "notes": room.get("notes"),
                    })
                analysis_source = "dxf"
                logger.info("DXF analysis completed for %s", file_path)

            elif file_lower.endswith((".png", ".jpg", ".jpeg", ".tiff", ".bmp")):
                # Try OpenCV symbol detection first
                try:
                    from tools.spatial.opencv_detector import AVSymbolDetector
                    detector = AVSymbolDetector()
                    detection_result = detector.detect_from_image(file_path)
                    if detection_result and not detection_result.get("error"):
                        logger.info("OpenCV detection completed for %s", file_path)
                except Exception as cv_exc:
                    logger.warning("OpenCV detection failed: %s", cv_exc)

                # Then use Vision analyzer for room identification
                try:
                    from tools.spatial.vision_analyzer import VisionAnalyzer
                    analyzer = VisionAnalyzer()
                    vision_result = await analyzer.analyze_floor_plan(file_path)
                    if vision_result and not vision_result.get("error"):
                        for room in vision_result.get("rooms", []):
                            analyzed_rooms.append({
                                "name": room.get("name", "Room"),
                                "room_type": room.get("room_type", "other"),
                                "length_ft": room.get("length_ft", 0),
                                "width_ft": room.get("width_ft", 0),
                                "area_sqft": room.get("area_sqft", 0),
                                "capacity": room.get("capacity", 0),
                                "ceiling_height_ft": room.get("ceiling_height_ft", 9.0),
                                "av_requirements": room.get("av_requirements", []),
                                "existing_infrastructure": room.get("existing_infrastructure", []),
                                "notes": room.get("notes"),
                            })
                        analysis_source = "vision"
                        logger.info("Vision analysis completed for %s", file_path)
                except Exception as vis_exc:
                    logger.warning("Vision analysis failed: %s", vis_exc)

        except Exception as exc:
            logger.warning("Failed to analyze floor plan '%s': %s", file_path, exc)

    # Fall back to rooms defined in project_context if no analysis succeeded
    if not analyzed_rooms and rooms_from_context:
        for room in rooms_from_context:
            length = room.get("length_ft", 20.0)
            width = room.get("width_ft", 15.0)
            analyzed_rooms.append({
                "name": room.get("name", "Room"),
                "room_type": room.get("room_type", "other"),
                "length_ft": length,
                "width_ft": width,
                "area_sqft": round(length * width, 2),
                "capacity": room.get("capacity", 10),
                "ceiling_height_ft": room.get("ceiling_height_ft", 9.0),
                "av_requirements": room.get("av_requirements", []),
                "existing_infrastructure": room.get("existing_infrastructure", []),
                "notes": room.get("notes"),
            })
        analysis_source = "manual"

    total_area = sum(r.get("area_sqft", 0) for r in analyzed_rooms)

    return {
        "rooms": analyzed_rooms,
        "total_rooms": len(analyzed_rooms),
        "total_area_sqft": round(total_area, 2),
        "analysis_source": analysis_source,
        "analyzed_at": datetime.now(timezone.utc).isoformat(),
    }


# ── Main node function ─────────────────────────────────────────────────────

async def task_planner_node(state: dict) -> dict:
    """Task Planner & Research AI node.

    Dispatches to intake or spatial analysis logic based on the current
    ``workflow_phase``.
    """
    phase = state.get("workflow_phase", "intake")
    logger.info("Task planner running for phase '%s'", phase)

    try:
        if phase == "intake":
            return await _handle_intake(state)
        elif phase == "spatial_analysis":
            return await _handle_spatial_analysis(state)
        else:
            logger.warning("Task planner called for unexpected phase '%s'", phase)
            return {"error": f"Task planner does not handle phase '{phase}'"}
    except Exception as exc:
        logger.exception("Task planner failed during phase '%s'", phase)
        return {"error": f"Task planner error in phase '{phase}': {exc}"}


async def _handle_intake(state: dict) -> dict:
    """Extract client requirements from messages and build project_context."""
    messages_text = _extract_messages_text(state)

    if not messages_text.strip():
        return {"error": "No client messages found for intake processing."}

    llm = get_llm_from_config()
    agent = BaseAgent(llm=llm)

    response_text = await agent.invoke_llm(
        user_message=messages_text,
        system_message=_INTAKE_SYSTEM_PROMPT,
    )

    # Parse the LLM JSON response
    response_text = response_text.strip()
    if response_text.startswith("```"):
        response_text = response_text.split("```")[1]
        if response_text.startswith("json"):
            response_text = response_text[4:]
        response_text = response_text.strip()

    try:
        project_context = json.loads(response_text)
    except json.JSONDecodeError as exc:
        logger.error("Failed to parse intake LLM response as JSON: %s", exc)
        # Build a minimal project_context from what we have
        project_context = {
            "project_name": "Untitled Project",
            "client_name": "Unknown",
            "project_description": messages_text[:500],
            "rooms": [],
            "is_federal": False,
            "special_requirements": [],
            "floor_plan_files": [],
            "design_intent": "reliable audiovisual experiences",
            "budget_range": None,
            "timeline": None,
            "raw_input": messages_text,
        }

    project_context["intake_completed_at"] = datetime.now(timezone.utc).isoformat()

    logger.info(
        "Intake complete: project='%s', %d room(s) identified",
        project_context.get("project_name", "?"),
        len(project_context.get("rooms", [])),
    )

    return {"project_context": project_context}


async def _handle_spatial_analysis(state: dict) -> dict:
    """Orchestrate floor-plan analysis and generate BoD narrative + ROM."""
    project_context = state.get("project_context")

    if not project_context:
        return {"error": "No project_context available for spatial analysis."}

    # Run spatial analysis (DXF, OpenCV, Vision, or fallback)
    spatial_data = await _run_spatial_analysis(project_context)

    if not spatial_data.get("rooms"):
        # If no rooms could be identified, use LLM to infer from context
        logger.warning("No rooms identified from floor plans; using LLM inference.")
        llm = get_llm_from_config()
        agent = BaseAgent(llm=llm)
        response_text = await agent.invoke_llm(
            user_message=json.dumps(project_context, default=str),
            system_message=_SPATIAL_SYSTEM_PROMPT,
        )

        response_text = response_text.strip()
        if response_text.startswith("```"):
            response_text = response_text.split("```")[1]
            if response_text.startswith("json"):
                response_text = response_text[4:]
            response_text = response_text.strip()

        try:
            spatial_data = json.loads(response_text)
            spatial_data["analysis_source"] = "llm_inference"
            spatial_data["analyzed_at"] = datetime.now(timezone.utc).isoformat()
        except json.JSONDecodeError:
            logger.error("LLM spatial inference returned invalid JSON.")
            spatial_data = {
                "rooms": [],
                "total_rooms": 0,
                "total_area_sqft": 0,
                "analysis_source": "failed",
                "analyzed_at": datetime.now(timezone.utc).isoformat(),
            }

    # Compute ROM estimate
    rom_estimate = _compute_rom(spatial_data)

    # Generate Basis of Design narrative
    bod_narrative = _generate_bod_narrative(project_context, spatial_data, rom_estimate)

    logger.info(
        "Spatial analysis complete: %d room(s), ROM=$%.2f (source=%s)",
        spatial_data.get("total_rooms", 0),
        rom_estimate.get("base_estimate", 0),
        spatial_data.get("analysis_source", "unknown"),
    )

    return {
        "spatial_data": spatial_data,
        "bod_narrative": bod_narrative,
        "rom_estimate": rom_estimate,
    }
