"""Tests for DISA STIG (Security Technical Implementation Guide) validation."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from compliance.disa_stig import STIGValidator


@pytest.fixture
def validator():
    """Return a fresh STIGValidator with no pre-loaded rules."""
    return STIGValidator()


def _make_device(overrides: dict | None = None) -> dict:
    """Build a device dict with sensible defaults, then apply overrides."""
    device = {
        "id": "test-device",
        "manufacturer": "Crestron",
        "model": "DM-NVX-350",
        "network_config": {
            "default_credentials_changed": True,
            "encryption_enabled": True,
            "open_ports": [443, 22],
            "required_ports": [443, 22],
            "firmware_date": "2026-01-15",
            "snmp_version": "v3",
            "syslog_enabled": True,
        },
    }
    if overrides:
        device["network_config"].update(overrides)
    return device


class TestDefaultCredentialsCat1Fail:
    """Device with default creds fails CAT_I."""

    def test_default_credentials_cat1_fail(self, validator):
        device = _make_device({"default_credentials_changed": False})
        results = validator.validate_device(device)
        cred_check = next(r for r in results if "Default Credentials" in r.check_name)
        assert cred_check.passed is False
        assert cred_check.severity == "critical"
        assert "CAT_I" in cred_check.message


class TestEncryptionRequiredCat1:
    """Device without encryption fails CAT_I."""

    def test_encryption_required_cat1(self, validator):
        device = _make_device({"encryption_enabled": False})
        results = validator.validate_device(device)
        enc_check = next(r for r in results if "Encryption" in r.check_name)
        assert enc_check.passed is False
        assert enc_check.severity == "critical"
        assert "CAT_I" in enc_check.message


class TestSnmpV3Required:
    """SNMPv1/v2c fails CAT_II."""

    def test_snmpv1_fails(self, validator):
        device = _make_device({"snmp_version": "v1"})
        results = validator.validate_device(device)
        snmp_check = next(r for r in results if "SNMP" in r.check_name)
        assert snmp_check.passed is False
        assert "v3" in snmp_check.message.lower() or "CAT_II" in snmp_check.message

    def test_snmpv2c_fails(self, validator):
        device = _make_device({"snmp_version": "v2c"})
        results = validator.validate_device(device)
        snmp_check = next(r for r in results if "SNMP" in r.check_name)
        assert snmp_check.passed is False

    def test_snmpv3_passes(self, validator):
        device = _make_device({"snmp_version": "v3"})
        results = validator.validate_device(device)
        snmp_check = next(r for r in results if "SNMP" in r.check_name)
        assert snmp_check.passed is True


class TestCompliantDevicePasses:
    """Fully hardened device passes all checks."""

    def test_compliant_device_passes(self, validator):
        device = _make_device()  # All defaults are compliant
        results = validator.validate_device(device)
        assert all(r.passed for r in results), (
            f"Expected all checks to pass but got failures: "
            f"{[r.message for r in results if not r.passed]}"
        )


class TestAuditLoggingRequired:
    """Device without syslog gets CAT_III warning."""

    def test_audit_logging_required(self, validator):
        device = _make_device({"syslog_enabled": False})
        results = validator.validate_device(device)
        syslog_check = next(r for r in results if "Syslog" in r.check_name or "Audit" in r.check_name)
        assert syslog_check.passed is False
        assert "CAT_III" in syslog_check.message
