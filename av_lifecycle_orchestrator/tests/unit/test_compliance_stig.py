"""Tests for DISA STIG compliance validation."""

import pytest
from compliance.disa_stig import STIGValidator


@pytest.fixture
def validator():
    return STIGValidator()


@pytest.fixture
def compliant_device():
    return {
        "device_type": "dsp",
        "manufacturer": "Biamp",
        "model": "TesiraFORTE-AVB-VT4",
        "firmware_version": "4.12.0",
        "network_config": {
            "default_credentials_changed": True,
            "encryption_enabled": True,
            "unused_ports_disabled": True,
            "snmp_version": "v3",
            "audit_logging_enabled": True,
        },
    }


@pytest.fixture
def non_compliant_device():
    return {
        "device_type": "controller",
        "manufacturer": "Generic",
        "model": "CTL-100",
        "firmware_version": "1.0.0",
        "network_config": {
            "default_credentials_changed": False,
            "encryption_enabled": False,
            "unused_ports_disabled": False,
            "snmp_version": "v2c",
            "audit_logging_enabled": False,
        },
    }


class TestSTIGValidator:
    def test_default_credentials_cat1_fail(self, validator, non_compliant_device):
        results = validator._check_common_stig_rules(non_compliant_device)
        cred_checks = [r for r in results if "credential" in r.message.lower()]
        assert any(not r.passed for r in cred_checks), "Default credentials should fail"
        assert any(r.severity == "critical" for r in cred_checks), "Should be critical"

    def test_encryption_required_cat1(self, validator, non_compliant_device):
        results = validator._check_common_stig_rules(non_compliant_device)
        enc_checks = [r for r in results if "encrypt" in r.message.lower()]
        assert any(not r.passed for r in enc_checks), "Missing encryption should fail"
        assert any(r.severity == "critical" for r in enc_checks), "Should be critical"

    def test_snmp_v3_required(self, validator, non_compliant_device):
        results = validator._check_common_stig_rules(non_compliant_device)
        snmp_checks = [r for r in results if "snmp" in r.message.lower()]
        assert any(not r.passed for r in snmp_checks), "SNMP v2c should fail"

    def test_compliant_device_passes(self, validator, compliant_device):
        results = validator._check_common_stig_rules(compliant_device)
        assert all(r.passed for r in results), (
            f"Compliant device should pass all, failures: "
            f"{[r.check_name for r in results if not r.passed]}"
        )

    def test_audit_logging_required(self, validator, non_compliant_device):
        results = validator._check_common_stig_rules(non_compliant_device)
        log_checks = [r for r in results if "log" in r.message.lower() or "audit" in r.message.lower()]
        assert any(not r.passed for r in log_checks), "Missing audit logging should fail"
