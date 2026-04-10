"""Shared test fixtures for the AV Lifecycle Orchestrator test suite."""

import json
import pytest
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock

FIXTURES_DIR = Path(__file__).parent / "fixtures"


@pytest.fixture
def mock_settings():
    """Settings configured for test/mock mode."""
    from config import Settings
    return Settings(
        llm_provider="openai",
        llm_model="gpt-4o",
        openai_api_key="test-key",
        anthropic_api_key="test-key",
        jetbuilt_api_token="test-token",
        xavia_api_key="test-key",
        mock_mode=True,
        log_level="DEBUG",
    )


@pytest.fixture
def sample_project_context():
    """Sample project context for testing."""
    return {
        "client_name": "Acme Corporation",
        "project_name": "HQ Conference Room Upgrade",
        "project_description": "Upgrade 3 conference rooms with modern AV systems",
        "rooms": [
            {
                "room_name": "Executive Boardroom",
                "room_type": "conference_room",
                "capacity": 20,
                "length_ft": 30.0,
                "width_ft": 20.0,
                "ceiling_height_ft": 10.0,
            },
            {
                "room_name": "Huddle Room A",
                "room_type": "huddle",
                "capacity": 6,
                "length_ft": 12.0,
                "width_ft": 10.0,
                "ceiling_height_ft": 9.0,
            },
            {
                "room_name": "All-Hands Space",
                "room_type": "all_hands",
                "capacity": 100,
                "length_ft": 60.0,
                "width_ft": 40.0,
                "ceiling_height_ft": 12.0,
            },
        ],
        "is_federal": False,
        "is_military": False,
        "floor_plan_path": None,
        "budget_range_low": 150000,
        "budget_range_high": 300000,
    }


@pytest.fixture
def sample_bom():
    """Sample Bill of Materials for testing."""
    return {
        "project_name": "HQ Conference Room Upgrade",
        "items": [
            {
                "item_id": "item-001",
                "manufacturer": "Crestron",
                "model_number": "TSW-1070-B-S",
                "description": "10.1 in. Touch Screen, Black",
                "category": "control",
                "quantity": 3,
                "unit_cost": 2100.0,
                "unit_price": 3500.0,
                "shipping_cost": 25.0,
                "country_of_origin": "United States",
                "room_assignment": "Executive Boardroom",
                "attributes": {"M": "W", "T": "CTL", "T2": "TS", "X": "C-01"},
            },
            {
                "item_id": "item-002",
                "manufacturer": "Biamp",
                "model_number": "TesiraFORTE-AVB-VT4",
                "description": "DSP with 4 analog inputs, VoIP",
                "category": "dsp",
                "quantity": 1,
                "unit_cost": 4200.0,
                "unit_price": 7000.0,
                "shipping_cost": 50.0,
                "country_of_origin": "United States",
                "room_assignment": "Executive Boardroom",
                "attributes": {"M": "R", "T": "DSP", "T2": "VoIP", "X": "D-01"},
            },
            {
                "item_id": "item-003",
                "manufacturer": "Samsung",
                "model_number": "QM85R-B",
                "description": "85 in. 4K UHD Display",
                "category": "display",
                "quantity": 2,
                "unit_cost": 3800.0,
                "unit_price": 5500.0,
                "shipping_cost": 150.0,
                "country_of_origin": "Korea (South)",
                "room_assignment": "All-Hands Space",
                "attributes": {"M": "W", "T": "D", "T2": "4K", "X": "V-01"},
            },
            {
                "item_id": "item-004",
                "manufacturer": "Shure",
                "model_number": "MXA920-S",
                "description": "Ceiling Array Microphone",
                "category": "audio",
                "quantity": 4,
                "unit_cost": 2800.0,
                "unit_price": 4200.0,
                "shipping_cost": 30.0,
                "country_of_origin": "United States",
                "room_assignment": "Executive Boardroom",
                "attributes": {"M": "C", "T": "M", "T2": "Array", "X": "A-01"},
            },
            {
                "item_id": "item-005",
                "manufacturer": "Hikvision",
                "model_number": "DS-2CD2147G2-SU",
                "description": "4MP AcuSense Fixed Dome Camera",
                "category": "other",
                "quantity": 2,
                "unit_cost": 180.0,
                "unit_price": 320.0,
                "shipping_cost": 10.0,
                "country_of_origin": "China",
                "room_assignment": "All-Hands Space",
                "attributes": {},
            },
        ],
        "equipment_subtotal": 33060.0,
        "total_items": 12,
        "generated_by": "xavia",
        "generated_at": "2026-04-10T12:00:00Z",
    }


@pytest.fixture
def sample_bom_federal(sample_bom):
    """Sample BOM for federal compliance testing (includes non-compliant items)."""
    bom = sample_bom.copy()
    bom["items"] = list(sample_bom["items"])  # shallow copy items list
    return bom


@pytest.fixture
def sample_spatial_data():
    """Sample spatial extraction data."""
    return {
        "source_file": "test_floorplan.dxf",
        "source_type": "dxf",
        "rooms": [
            {
                "room_name": "Conference Room 101",
                "dimensions": {
                    "length_ft": 30.0,
                    "width_ft": 20.0,
                    "area_sqft": 600.0,
                    "ceiling_height_ft": 10.0,
                    "perimeter_ft": 100.0,
                },
            }
        ],
        "detected_symbols": [
            {"symbol_type": "data_drop", "x": 10.0, "y": 5.0, "confidence": 0.95},
            {"symbol_type": "power_outlet", "x": 15.0, "y": 5.0, "confidence": 0.88},
        ],
        "data_drops": 8,
        "power_receptacles": 12,
        "existing_av_devices": 3,
    }


@pytest.fixture
def sample_financial_model():
    """Sample financial model."""
    return {
        "rom": {
            "base_estimate": 85000.0,
            "lower_bound": 63750.0,
            "upper_bound": 148750.0,
            "equipment_subtotal": 65000.0,
            "labor_subtotal": 20000.0,
            "contingency_pct": 0.10,
            "contingency_amount": 8500.0,
        },
        "labor_breakdown": [
            {"role": "lead_installer", "hourly_rate": 85.0, "estimated_hours": 80, "total": 6800.0},
            {"role": "system_engineer", "hourly_rate": 125.0, "estimated_hours": 60, "total": 7500.0},
            {"role": "dsp_programmer", "hourly_rate": 150.0, "estimated_hours": 38, "total": 5700.0},
        ],
    }


@pytest.fixture
def sample_deal_registration():
    """Sample deal registration data."""
    return {
        "manufacturer": "Crestron",
        "dealer_name": "ProAV Integrators Inc.",
        "dealer_id": "CREST-12345",
        "end_user_company": "Acme Corporation",
        "end_user_contact_name": "Jane Smith",
        "end_user_contact_email": "jsmith@acme.com",
        "end_user_contact_phone": "555-0123",
        "project_name": "HQ Conference Room Upgrade",
        "project_description": "Full AV refresh for 3 conference rooms",
        "estimated_close_date": "2026-07-15",
        "estimated_value": 150000.0,
        "product_lines": ["TSW-1070-B-S", "DM-NVX-350"],
        "quantities": {"TSW-1070-B-S": 3, "DM-NVX-350": 6},
        "requested_discount_pct": 12.0,
    }


@pytest.fixture
def mock_llm():
    """Mock LLM that returns predictable responses."""
    llm = AsyncMock()
    llm.ainvoke = AsyncMock(return_value=MagicMock(content="Mock LLM response"))
    return llm
