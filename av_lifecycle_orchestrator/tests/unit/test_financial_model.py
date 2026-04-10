"""Tests for ROM and financial calculations."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from models.financial import LaborLineItem, LaborEstimate, ROMEstimate


class TestROMLowerBound:
    """lower = base * 0.75"""

    def test_rom_lower_bound(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=100000.0,
            equipment_subtotal=70000.0,
            labor_subtotal=30000.0,
            labor_breakdown=labor,
        )
        assert rom.lower_bound == pytest.approx(75000.0)

    def test_rom_lower_bound_small(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=10000.0,
            equipment_subtotal=8000.0,
            labor_subtotal=2000.0,
            labor_breakdown=labor,
        )
        assert rom.lower_bound == pytest.approx(7500.0)


class TestROMUpperBound:
    """upper = base * 1.75"""

    def test_rom_upper_bound(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=100000.0,
            equipment_subtotal=70000.0,
            labor_subtotal=30000.0,
            labor_breakdown=labor,
        )
        assert rom.upper_bound == pytest.approx(175000.0)

    def test_rom_upper_bound_small(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=10000.0,
            equipment_subtotal=8000.0,
            labor_subtotal=2000.0,
            labor_breakdown=labor,
        )
        assert rom.upper_bound == pytest.approx(17500.0)


class TestLaborCalculation:
    """hours * rate = total for each role."""

    def test_labor_calculation(self):
        item = LaborLineItem(role="lead_installer", hourly_rate=85.0, estimated_hours=80)
        assert item.total == pytest.approx(6800.0)

    def test_labor_calculation_engineer(self):
        item = LaborLineItem(role="system_engineer", hourly_rate=125.0, estimated_hours=60)
        assert item.total == pytest.approx(7500.0)

    def test_labor_calculation_programmer(self):
        item = LaborLineItem(role="dsp_programmer", hourly_rate=150.0, estimated_hours=38)
        assert item.total == pytest.approx(5700.0)

    def test_labor_estimate_total(self):
        items = [
            LaborLineItem(role="lead_installer", hourly_rate=85.0, estimated_hours=80),
            LaborLineItem(role="system_engineer", hourly_rate=125.0, estimated_hours=60),
            LaborLineItem(role="dsp_programmer", hourly_rate=150.0, estimated_hours=38),
        ]
        estimate = LaborEstimate(line_items=items)
        assert estimate.total_labor_cost == pytest.approx(6800.0 + 7500.0 + 5700.0)


class TestContingencyCalculation:
    """contingency = base * percentage."""

    def test_contingency_calculation(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=85000.0,
            equipment_subtotal=65000.0,
            labor_subtotal=20000.0,
            labor_breakdown=labor,
            contingency_pct=0.10,
        )
        assert rom.contingency_amount == pytest.approx(8500.0)

    def test_contingency_calculation_15pct(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=100000.0,
            equipment_subtotal=70000.0,
            labor_subtotal=30000.0,
            labor_breakdown=labor,
            contingency_pct=0.15,
        )
        assert rom.contingency_amount == pytest.approx(15000.0)

    def test_contingency_default_10pct(self):
        labor = LaborEstimate(line_items=[])
        rom = ROMEstimate(
            base_estimate=50000.0,
            equipment_subtotal=35000.0,
            labor_subtotal=15000.0,
            labor_breakdown=labor,
        )
        # Default contingency_pct is 0.10
        assert rom.contingency_amount == pytest.approx(5000.0)
