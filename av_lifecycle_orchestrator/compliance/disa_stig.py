"""DISA RME Vendor STIG validation for networked AV equipment.

Post-2025, the DISA Risk Management Executive (RME) Vendor STIG process
replaces the former DoDIN APL (Department of Defense Information Network
Approved Products List).  This module parses XCCDF-format STIG checklists
and validates networked AV devices against applicable security rules.
"""

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from typing import Literal, Optional

from models.compliance import ComplianceCheckResult, STIGComplianceResult


@dataclass
class STIGRule:
    """A single rule from a STIG checklist."""

    rule_id: str
    severity: Literal["CAT_I", "CAT_II", "CAT_III"]
    title: str
    description: str
    check_content: str = ""
    fix_text: str = ""


class STIGParser:
    """Parse XCCDF-format STIG checklist files."""

    XCCDF_NS = "http://checklists.nist.gov/xccdf/1.2"

    @classmethod
    def parse_xccdf(cls, file_path: str) -> list[STIGRule]:
        """Parse an XCCDF XML file and extract STIG rules.

        Parameters
        ----------
        file_path:
            Path to the XCCDF XML file.

        Returns
        -------
        list[STIGRule]
            List of parsed STIG rules.
        """
        ns = {"xccdf": cls.XCCDF_NS}
        tree = ET.parse(file_path)  # noqa: S314
        root = tree.getroot()

        rules: list[STIGRule] = []

        for rule_el in root.iter(f"{{{cls.XCCDF_NS}}}Rule"):
            rule_id = rule_el.get("id", "")

            # Map XCCDF severity to CAT level
            severity_raw = rule_el.get("severity", "medium").lower()
            severity_map = {
                "high": "CAT_I",
                "medium": "CAT_II",
                "low": "CAT_III",
            }
            severity: Literal["CAT_I", "CAT_II", "CAT_III"] = severity_map.get(
                severity_raw, "CAT_II"
            )

            # Extract child elements
            title_el = rule_el.find("xccdf:title", ns)
            title = title_el.text.strip() if title_el is not None and title_el.text else ""

            desc_el = rule_el.find("xccdf:description", ns)
            description = desc_el.text.strip() if desc_el is not None and desc_el.text else ""

            check_el = rule_el.find(".//xccdf:check-content", ns)
            check_content = (
                check_el.text.strip() if check_el is not None and check_el.text else ""
            )

            fix_el = rule_el.find("xccdf:fixtext", ns)
            fix_text = fix_el.text.strip() if fix_el is not None and fix_el.text else ""

            rules.append(
                STIGRule(
                    rule_id=rule_id,
                    severity=severity,
                    title=title,
                    description=description,
                    check_content=check_content,
                    fix_text=fix_text,
                )
            )

        return rules


class STIGValidator:
    """Validate networked AV devices against STIG rules."""

    def __init__(self, stig_rules: list[STIGRule] | None = None) -> None:
        self.stig_rules: list[STIGRule] = stig_rules or []

    def validate_device(self, device: dict) -> list[ComplianceCheckResult]:
        """Check a device against applicable STIG rules.

        Parameters
        ----------
        device:
            Device dictionary with keys: ``device_type``, ``manufacturer``,
            ``model``, ``firmware_version``, ``network_config`` (dict with
            ``open_ports``, ``encryption_enabled``,
            ``default_credentials_changed``, ``snmp_version``,
            ``syslog_enabled``, ``firmware_date``, etc.).

        Returns
        -------
        list[ComplianceCheckResult]
            List of individual check results for the device.
        """
        results = self._check_common_stig_rules(device)
        return results

    def validate_bom(
        self,
        bom_items: list[dict],
        stig_file: Optional[str] = None,
    ) -> STIGComplianceResult:
        """Validate all networked items in a BOM against STIG rules.

        Parameters
        ----------
        bom_items:
            List of BOM item dictionaries.  Only items with a
            ``network_config`` key are evaluated (networked devices).
        stig_file:
            Optional path to an XCCDF STIG file.  If provided, rules are
            parsed and loaded before validation.

        Returns
        -------
        STIGComplianceResult
            Aggregated STIG compliance result.
        """
        if stig_file:
            self.stig_rules = STIGParser.parse_xccdf(stig_file)

        stig_version = "DISA RME Vendor STIG v1"
        checks: list[ComplianceCheckResult] = []
        uncertified_items: list[str] = []

        for item in bom_items:
            # Only validate networked devices
            if "network_config" not in item:
                continue

            device_results = self.validate_device(item)
            checks.extend(device_results)

            # If any CAT_I (critical) check fails, mark as uncertified
            item_id = (
                item.get("id") or item.get("name") or item.get("model") or "unknown"
            )
            has_critical_failure = any(
                not r.passed and r.severity == "critical" for r in device_results
            )
            if has_critical_failure:
                uncertified_items.append(str(item_id))

        return STIGComplianceResult(
            stig_version=stig_version,
            checks=checks,
            uncertified_items=uncertified_items,
        )

    def _check_common_stig_rules(self, device: dict) -> list[ComplianceCheckResult]:
        """Check common STIG rules applicable to most networked AV devices.

        Parameters
        ----------
        device:
            Device dictionary (see ``validate_device``).

        Returns
        -------
        list[ComplianceCheckResult]
            Results for each common STIG rule checked.
        """
        results: list[ComplianceCheckResult] = []
        item_id = (
            device.get("id") or device.get("name") or device.get("model") or "unknown"
        )
        network_config = device.get("network_config", {})

        # --- CAT_I: Default credentials must be changed ---
        default_creds_changed = network_config.get("default_credentials_changed", False)
        results.append(
            ComplianceCheckResult(
                check_name="STIG: Default Credentials Changed",
                passed=bool(default_creds_changed),
                severity="critical",
                message=(
                    f"Device '{item_id}': Default credentials "
                    f"{'have been changed' if default_creds_changed else 'have NOT been changed (CAT_I violation)'}."
                ),
                affected_items=[str(item_id)],
            )
        )

        # --- CAT_I: Encryption must be enabled for management interfaces ---
        encryption_enabled = network_config.get("encryption_enabled", False)
        results.append(
            ComplianceCheckResult(
                check_name="STIG: Management Interface Encryption",
                passed=bool(encryption_enabled),
                severity="critical",
                message=(
                    f"Device '{item_id}': Management interface encryption "
                    f"{'is enabled' if encryption_enabled else 'is NOT enabled (CAT_I violation)'}."
                ),
                affected_items=[str(item_id)],
            )
        )

        # --- CAT_II: Unused network ports must be disabled ---
        open_ports = network_config.get("open_ports", [])
        required_ports = network_config.get("required_ports", [])
        unnecessary_ports = [p for p in open_ports if p not in required_ports] if required_ports else []
        ports_ok = len(unnecessary_ports) == 0
        if not required_ports:
            # If no required ports specified, we can't validate -- warn
            results.append(
                ComplianceCheckResult(
                    check_name="STIG: Unused Network Ports Disabled",
                    passed=True,
                    severity="warning",
                    message=(
                        f"Device '{item_id}': No required ports list specified; "
                        f"cannot verify unused ports are disabled. "
                        f"Open ports: {open_ports or 'none reported'}."
                    ),
                    affected_items=[str(item_id)],
                )
            )
        else:
            results.append(
                ComplianceCheckResult(
                    check_name="STIG: Unused Network Ports Disabled",
                    passed=ports_ok,
                    severity="warning",
                    message=(
                        f"Device '{item_id}': "
                        + (
                            "All open ports are required."
                            if ports_ok
                            else f"Unnecessary open ports detected: {unnecessary_ports} (CAT_II violation)."
                        )
                    ),
                    affected_items=[str(item_id)],
                )
            )

        # --- CAT_II: Firmware must be current (within last 12 months) ---
        firmware_date_str = network_config.get("firmware_date", "")
        if firmware_date_str:
            try:
                firmware_date = datetime.strptime(firmware_date_str, "%Y-%m-%d")
                cutoff = datetime.utcnow() - timedelta(days=365)
                firmware_current = firmware_date >= cutoff
            except ValueError:
                firmware_current = False
        else:
            firmware_current = False

        results.append(
            ComplianceCheckResult(
                check_name="STIG: Firmware Currency",
                passed=firmware_current,
                severity="warning",
                message=(
                    f"Device '{item_id}': Firmware "
                    + (
                        f"is current (date: {firmware_date_str})."
                        if firmware_current
                        else f"is NOT current or date not provided (CAT_II violation). "
                        f"Firmware date: {firmware_date_str or 'not specified'}."
                    )
                ),
                affected_items=[str(item_id)],
            )
        )

        # --- CAT_II: SNMP v3 required (v1/v2c not allowed) ---
        snmp_version = network_config.get("snmp_version", "")
        if snmp_version:
            snmp_ok = snmp_version.lower() in ("v3", "3", "snmpv3")
            results.append(
                ComplianceCheckResult(
                    check_name="STIG: SNMP Version",
                    passed=snmp_ok,
                    severity="warning",
                    message=(
                        f"Device '{item_id}': SNMP "
                        + (
                            "v3 is in use."
                            if snmp_ok
                            else f"version '{snmp_version}' is not allowed (CAT_II violation). "
                            f"SNMP v3 is required."
                        )
                    ),
                    affected_items=[str(item_id)],
                )
            )
        else:
            # SNMP not configured -- not necessarily a failure
            results.append(
                ComplianceCheckResult(
                    check_name="STIG: SNMP Version",
                    passed=True,
                    severity="info",
                    message=f"Device '{item_id}': SNMP is not configured.",
                    affected_items=[str(item_id)],
                )
            )

        # --- CAT_III: Syslog/audit logging must be enabled ---
        syslog_enabled = network_config.get("syslog_enabled", False)
        results.append(
            ComplianceCheckResult(
                check_name="STIG: Syslog/Audit Logging",
                passed=bool(syslog_enabled),
                severity="info",
                message=(
                    f"Device '{item_id}': Syslog/audit logging "
                    f"{'is enabled' if syslog_enabled else 'is NOT enabled (CAT_III violation)'}."
                ),
                affected_items=[str(item_id)],
            )
        )

        return results
