"""LangGraph StateGraph definition for the AV Lifecycle Orchestration pipeline.

Implements a supervisor hub-and-spoke topology where all worker agents
return to the supervisor, which decides the next phase based on state
completeness and the strict phase ordering FSM.
"""

from langgraph.graph import StateGraph, END

from state import ProjectState
from agents.supervisor import supervisor_node, route_supervisor
from agents.task_planner import task_planner_node
from agents.av_drafter import av_drafter_node
from agents.estimator import estimator_node
from agents.compliance import compliance_node
from agents.communications import communications_node


def build_graph() -> StateGraph:
    """Build and compile the AV lifecycle orchestration graph."""
    graph = StateGraph(ProjectState)

    # Add all nodes
    graph.add_node("supervisor", supervisor_node)
    graph.add_node("task_planner", task_planner_node)
    graph.add_node("av_drafter", av_drafter_node)
    graph.add_node("estimator", estimator_node)
    graph.add_node("compliance", compliance_node)
    graph.add_node("communications", communications_node)

    # Entry point is always the supervisor
    graph.set_entry_point("supervisor")

    # Supervisor uses conditional routing based on workflow_phase
    graph.add_conditional_edges(
        "supervisor",
        route_supervisor,
        {
            "task_planner": "task_planner",
            "av_drafter": "av_drafter",
            "estimator": "estimator",
            "compliance": "compliance",
            "communications": "communications",
            END: END,
        },
    )

    # All worker nodes return to supervisor after completing their work
    for worker in ["task_planner", "av_drafter", "estimator",
                    "compliance", "communications"]:
        graph.add_edge(worker, "supervisor")

    return graph.compile()


# Pre-compiled graph instance
app_graph = build_graph()
