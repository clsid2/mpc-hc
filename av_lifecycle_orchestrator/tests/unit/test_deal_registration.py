"""Tests for deal registration handlers and registry."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from deal_registration.registry import get_handler, list_handlers, _REGISTRY
from deal_registration.handlers.crestron import CrestronDealHandler


class TestHandlerRegistry:
    """All 8 handlers are registered."""

    def test_handler_registry(self):
        handlers = list_handlers()
        expected = [
            "amx",
            "biamp",
            "crestron",
            "extron",
            "legrand",
            "qsys",
            "sennheiser",
            "shure",
        ]
        assert handlers == expected
        assert len(handlers) == 8


class TestGetHandlerByName:
    """get_handler('crestron') returns CrestronDealHandler."""

    def test_get_handler_by_name(self):
        handler = get_handler("crestron")
        assert isinstance(handler, CrestronDealHandler)
        assert handler.manufacturer_name == "crestron"


class TestUnknownHandlerRaises:
    """get_handler('unknown') raises KeyError."""

    def test_unknown_handler_raises(self):
        with pytest.raises(KeyError, match="No deal registration handler"):
            get_handler("unknown")


class TestSennheiserThreshold:
    """<50 users raises ValueError."""

    @pytest.mark.asyncio
    async def test_sennheiser_threshold(self):
        handler = get_handler("sennheiser")
        deal = {
            "dealer_id": "SENN-001",
            "dealer_name": "Test Dealer",
            "sennheiser_account_number": "SA-12345",
            "business_users": 10,  # below 50 threshold
            "end_user_company": "Acme Corp",
            "end_user_contact_name": "John",
            "end_user_contact_email": "john@acme.com",
            "project_name": "Test Project",
            "estimated_close_date": "2026-12-31",
            "estimated_value": 50000.0,
            "product_lines": ["MXA920"],
        }
        with pytest.raises(ValueError, match="minimum of 50"):
            await handler.validate_eligibility(deal)


class TestExtronApprovalWindow:
    """90-day deadline is set."""

    @pytest.mark.asyncio
    async def test_extron_approval_window(self):
        handler = get_handler("extron")
        assert handler.approval_window_days == 90

        deal = {
            "dealer_id": "EXT-001",
            "dealer_name": "Test Dealer",
            "insider_portal_id": "INS-12345",
            "end_user_company": "Acme Corp",
            "end_user_contact_name": "Jane",
            "end_user_contact_email": "jane@acme.com",
            "project_name": "Test Project",
            "project_description": "AV upgrade",
            "estimated_close_date": "2026-12-31",
            "estimated_value": 100000.0,
            "product_lines": ["DTP2"],
        }
        payload = await handler.prepare_payload(deal)
        assert payload["opportunity_registration_criteria"]["commitment_days"] == 90
        assert "commitment_deadline" in payload["opportunity_registration_criteria"]


class TestValidateRequiredFields:
    """Missing required fields are detected."""

    def test_validate_required_fields(self):
        handler = get_handler("crestron")
        # Provide an incomplete deal with no fields
        missing = handler.validate_required_fields({})
        # Should detect that required fields are missing
        assert len(missing) > 0
        # All required fields from the template should be in the missing list
        for field in handler.required_fields:
            assert field in missing

    def test_validate_required_fields_complete(self):
        handler = get_handler("crestron")
        # Build a deal with all required fields
        deal = {field: f"value-{field}" for field in handler.required_fields}
        missing = handler.validate_required_fields(deal)
        assert missing == []
