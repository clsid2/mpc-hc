"""Pydantic domain models for the AV Lifecycle Orchestrator."""

from .bom import BillOfMaterials, BOMLineItem
from .compliance import (
    AVIXAComplianceResult,
    ComplianceCheckResult,
    FullComplianceReport,
    STIGComplianceResult,
    TAAComplianceResult,
)
from .deal_registration import DealRegistration, DealRegistrationBatch, DealResult
from .financial import FinancialModel, LaborEstimate, LaborLineItem, ROMEstimate
from .project import ClientRequirements, ProjectContext, RoomRequirement
from .spatial import DetectedSymbol, FloorPlanData, QuantityTakeoff, RoomDimensions

__all__ = [
    # project
    "ClientRequirements",
    "RoomRequirement",
    "ProjectContext",
    # spatial
    "DetectedSymbol",
    "RoomDimensions",
    "FloorPlanData",
    "QuantityTakeoff",
    # bom
    "BOMLineItem",
    "BillOfMaterials",
    # financial
    "LaborLineItem",
    "LaborEstimate",
    "ROMEstimate",
    "FinancialModel",
    # compliance
    "ComplianceCheckResult",
    "AVIXAComplianceResult",
    "TAAComplianceResult",
    "STIGComplianceResult",
    "FullComplianceReport",
    # deal registration
    "DealRegistration",
    "DealResult",
    "DealRegistrationBatch",
]
