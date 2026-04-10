"""Tests for AVIXA J-STD-710 symbology validation."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from compliance.avixa_j710 import validate_symbol, validate_bom


class TestValidSymbolPasses:
    """Symbol with all M/T/T2/X attributes passes."""

    def test_valid_symbol_passes(self):
        symbol = {
            "id": "item-001",
            "M": "W",
            "T": "CTL",
            "T2": "TS",
            "X": "C-01",
        }
        result = validate_symbol(symbol)
        assert result.passed is True
        assert result.severity == "info"


class TestMissingMountingTypeFails:
    """Symbol without M attribute fails."""

    def test_missing_mounting_type_fails(self):
        symbol = {
            "id": "item-002",
            # M is missing
            "T": "DSP",
            "T2": "VoIP",
            "X": "D-01",
        }
        result = validate_symbol(symbol)
        assert result.passed is False
        assert "M (Mounting Type)" in result.message


class TestMissingPrimaryTechFails:
    """Symbol without T attribute fails."""

    def test_missing_primary_tech_fails(self):
        symbol = {
            "id": "item-003",
            "M": "R",
            # T is missing
            "T2": "4K",
            "X": "V-01",
        }
        result = validate_symbol(symbol)
        assert result.passed is False
        assert "T (Primary Technology)" in result.message


class TestPartialAttributesFails:
    """Symbol with only some attributes fails."""

    def test_partial_attributes_fails(self):
        symbol = {
            "id": "item-004",
            "M": "C",
            # T, T2, X all missing
        }
        result = validate_symbol(symbol)
        assert result.passed is False
        # Should mention multiple missing attributes
        assert "T (Primary Technology)" in result.message
        assert "T2 (Secondary Technology)" in result.message
        assert "X (Legend/Schedule Reference)" in result.message


class TestFullBomValidation:
    """Full BOM validation with mix of valid/invalid items."""

    def test_full_bom_validation(self):
        items = [
            # Valid item
            {"id": "ok-1", "M": "W", "T": "D", "T2": "4K", "X": "V-01"},
            # Invalid item -- missing M and T
            {"id": "bad-1", "T2": "Array", "X": "A-01"},
            # Valid item
            {"id": "ok-2", "M": "R", "T": "DSP", "T2": "VoIP", "X": "D-01"},
        ]
        result = validate_bom(items)
        assert result.passed is False  # one item failed
        assert len(result.checks) == 3
        assert result.checks[0].passed is True
        assert result.checks[1].passed is False
        assert result.checks[2].passed is True
        # missing_attributes should capture the bad item
        assert len(result.missing_attributes) == 1
        assert result.missing_attributes[0]["item_id"] == "bad-1"
        assert "M" in result.missing_attributes[0]["missing"]
        assert "T" in result.missing_attributes[0]["missing"]


class TestEmptyBom:
    """Empty BOM passes (no items to fail)."""

    def test_empty_bom(self):
        result = validate_bom([])
        assert result.passed is True
        assert len(result.checks) == 0
        assert len(result.missing_attributes) == 0
