"""FastAPI entry point for the AV Lifecycle Orchestration system."""

import logging
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Optional

from fastapi import FastAPI, HTTPException, BackgroundTasks
from pydantic import BaseModel

from config import Settings
from state import create_initial_state
from graph import build_graph

logger = logging.getLogger(__name__)

settings = Settings()

# Configure logging
logging.basicConfig(
    level=getattr(logging, settings.log_level.upper(), logging.INFO),
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan handler."""
    logger.info("AV Lifecycle Orchestrator starting up")
    logger.info(f"LLM Provider: {settings.llm_provider}, Model: {settings.llm_model}")
    logger.info(f"Mock Mode: {settings.mock_mode}")
    yield
    logger.info("AV Lifecycle Orchestrator shutting down")


app = FastAPI(
    title="AV Lifecycle Orchestrator",
    description="Multi-Agent AV Project Lifecycle Orchestration System",
    version="1.0.0",
    lifespan=lifespan,
)


class TriggerRequest(BaseModel):
    """Request body for triggering a new AV project workflow."""
    client_name: str
    project_name: str
    project_description: str
    rooms: list[dict] = []
    floor_plan_path: Optional[str] = None
    is_federal: bool = False
    is_military: bool = False
    budget_range_low: Optional[float] = None
    budget_range_high: Optional[float] = None


class TriggerResponse(BaseModel):
    """Response after triggering a workflow."""
    status: str
    project_name: str
    started_at: str
    message: str


class WorkflowResult(BaseModel):
    """Full workflow result."""
    status: str
    workflow_phase: str
    project_context: Optional[dict] = None
    bod_narrative: Optional[str] = None
    bom_summary: Optional[dict] = None
    financials: Optional[dict] = None
    compliance_status: Optional[dict] = None
    deal_registrations: list[dict] = []
    error: Optional[str] = None


# In-memory storage for workflow results (use a real DB in production)
_workflow_results: dict[str, dict] = {}


@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {
        "status": "healthy",
        "llm_provider": settings.llm_provider,
        "mock_mode": settings.mock_mode,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@app.post("/trigger", response_model=TriggerResponse)
async def trigger_workflow(request: TriggerRequest, background_tasks: BackgroundTasks):
    """Trigger a new AV project lifecycle workflow."""
    project_id = f"{request.project_name.lower().replace(' ', '_')}_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}"

    initial_state = create_initial_state()
    initial_state["messages"] = [{
        "role": "user",
        "content": (
            f"New AV opportunity: {request.project_description}\n"
            f"Client: {request.client_name}\n"
            f"Project: {request.project_name}\n"
            f"Rooms: {request.rooms}\n"
            f"Floor Plan: {request.floor_plan_path or 'Not provided'}\n"
            f"Federal: {request.is_federal}\n"
            f"Military: {request.is_military}\n"
            f"Budget: {request.budget_range_low} - {request.budget_range_high}"
        ),
    }]
    initial_state["project_context"] = {
        "client_name": request.client_name,
        "project_name": request.project_name,
        "project_description": request.project_description,
        "rooms": request.rooms,
        "floor_plan_path": request.floor_plan_path,
        "is_federal": request.is_federal,
        "is_military": request.is_military,
        "budget_range_low": request.budget_range_low,
        "budget_range_high": request.budget_range_high,
    }

    background_tasks.add_task(_run_workflow, project_id, initial_state)

    return TriggerResponse(
        status="started",
        project_name=request.project_name,
        started_at=datetime.now(timezone.utc).isoformat(),
        message=f"Workflow started with project ID: {project_id}",
    )


@app.get("/status/{project_id}", response_model=WorkflowResult)
async def get_workflow_status(project_id: str):
    """Get the status of a running or completed workflow."""
    if project_id not in _workflow_results:
        raise HTTPException(status_code=404, detail=f"Project '{project_id}' not found")
    return WorkflowResult(**_workflow_results[project_id])


@app.get("/projects")
async def list_projects():
    """List all tracked projects."""
    return {
        pid: {
            "status": data.get("status", "unknown"),
            "workflow_phase": data.get("workflow_phase", "unknown"),
        }
        for pid, data in _workflow_results.items()
    }


async def _run_workflow(project_id: str, initial_state: dict) -> None:
    """Execute the full workflow graph in the background."""
    _workflow_results[project_id] = {
        "status": "running",
        "workflow_phase": "intake",
    }
    try:
        graph = build_graph()
        final_state = await graph.ainvoke(initial_state)
        _workflow_results[project_id] = {
            "status": "completed",
            "workflow_phase": final_state.get("workflow_phase", "unknown"),
            "project_context": final_state.get("project_context"),
            "bod_narrative": final_state.get("bod_narrative"),
            "bom_summary": _summarize_bom(final_state.get("bom_json")),
            "financials": final_state.get("financials"),
            "compliance_status": final_state.get("compliance_status"),
            "deal_registrations": final_state.get("deal_registrations", []),
            "error": final_state.get("error"),
        }
    except Exception as e:
        logger.exception(f"Workflow failed for {project_id}")
        _workflow_results[project_id] = {
            "status": "failed",
            "workflow_phase": "error",
            "error": str(e),
        }


def _summarize_bom(bom: Optional[dict]) -> Optional[dict]:
    """Create a summary of the BOM for the API response."""
    if not bom:
        return None
    items = bom.get("items", [])
    return {
        "total_items": len(items),
        "equipment_subtotal": sum(i.get("unit_cost", 0) * i.get("quantity", 1) for i in items),
        "manufacturers": list(set(i.get("manufacturer", "") for i in items)),
        "categories": list(set(i.get("category", "") for i in items)),
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
