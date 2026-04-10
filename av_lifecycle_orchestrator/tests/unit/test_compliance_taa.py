"""Tests for TAA (Trade Agreements Act) compliance validation."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from compliance.taa import validate_item, validate_bom, get_compliant_alternatives


class TestUSManufacturerCompliant:
    """US origin passes."""

    def test_us_manufacturer_compliant(self):
        item = {
            "id": "item-001",
            "manufacturer": "Crestron",
            "country_of_origin": "United States",
        }
        result = validate_item(item)
        assert result.passed is True
        assert result.severity == "info"
        assert "TAA-designated" in result.message


class TestJapanManufacturerCompliant:
    """Japan (WTO GPA) passes."""

    def test_japan_manufacturer_compliant(self):
        item = {
            "id": "item-002",
            "manufacturer": "Sony",
            "country_of_origin": "Japan",
        }
        result = validate_item(item)
        assert result.passed is True
        assert result.severity == "info"


class TestChinaManufacturerNonCompliant:
    """China origin fails."""

    def test_china_manufacturer_non_compliant(self):
        item = {
            "id": "item-003",
            "manufacturer": "Hikvision",
            "country_of_origin": "China",
        }
        result = validate_item(item)
        assert result.passed is False
        assert result.severity == "critical"
        assert "restricted" in result.message.lower() or "non-compliant" in result.message.lower()


class TestUnknownManufacturerWarning:
    """Unknown manufacturer gets warning."""

    def test_unknown_manufacturer_warning(self):
        item = {
            "id": "item-004",
            "manufacturer": "UnknownBrandXYZ",
            # no country_of_origin provided and not in KNOWN_MANUFACTURER_ORIGINS
        }
        result = validate_item(item)
        # Should pass with a warning since country can't be determined
        assert result.passed is True
        assert result.severity == "warning"
        assert "manual verification" in result.message.lower() or "not in the known origins" in result.message.lower()


class TestFullBomWithMixedCompliance:
    """BOM with mix of compliant/non-compliant items."""

    def test_full_bom_with_mixed_compliance(self):
        items = [
            {"id": "us-item", "manufacturer": "Crestron", "country_of_origin": "United States"},
            {"id": "jp-item", "manufacturer": "Sony", "country_of_origin": "Japan"},
            {"id": "cn-item", "manufacturer": "Hikvision", "country_of_origin": "China"},
        ]
        result = validate_bom(items)
        assert result.passed is False  # China item fails
        assert len(result.non_compliant_items) == 1
        assert "cn-item" in result.non_compliant_items
        assert "China" in result.non_compliant_countries


class TestCompliantAlternatives:
    """Hikvision suggests alternatives."""

    def test_compliant_alternatives(self):
        alternatives = get_compliant_alternatives("Hikvision")
        assert len(alternatives) > 0
        assert "Axis Communications" in alternatives
        assert "Hanwha Techwin" in alternatives
        assert "Bosch Security" in alternatives

    def test_unknown_manufacturer_no_alternatives(self):
        alternatives = get_compliant_alternatives("UnknownBrandXYZ")
        assert alternatives == []

    def test_empty_manufacturer_no_alternatives(self):
        alternatives = get_compliant_alternatives("")
        assert alternatives == []
