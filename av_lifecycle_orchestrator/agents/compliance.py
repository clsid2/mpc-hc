"""Cyber Risk & Compliance AI agent.

Handles the ``compliance`` workflow phase. Validates all BOM items against:
- AVIXA J-STD-710 symbology attributes
- Trade Agreements Act (TAA) country-of-origin requirements
- DISA RME Vendor STIG rules (for federal/military projects only)

Non-compliant items are flagged and optionally stripped from the BOM.
"""

import logging
from datetime import datetime, timezone
from typing import Any

from agents.base import BaseAgent

logger = logging.getLogger(__name__)


def _run_avixa_validation(bom_items: list[dict]) -> dict:
    """Run AVIXA J-STD-710 validation on all BOM items.

    Returns a serializable dict summarizing the AVIXA compliance result.
    """
    from compliance.avixa_j710 import validate_bom

    result = validate_bom(bom_items)

    return {
        "standard": result.standard,
        "passed": result.passed,
        "total_checks": len(result.checks),
        "failed_checks": sum(1 for c in result.checks if not c.passed),
        "missing_attributes": result.missing_attributes,
        "details": [
            {
                "check_name": c.check_name,
                "passed": c.passed,
                "severity": c.severity,
                "message": c.message,
                "affected_items": c.affected_items,
            }
            for c in result.checks
            if not c.passed
        ],
    }


def _run_taa_validation(bom_items: list[dict]) -> dict:
    """Run TAA country-of-origin validation on all BOM items.

    Returns a serializable dict summarizing the TAA compliance result.
    """
    from compliance.taa import validate_bom, get_compliant_alternatives

    result = validate_bom(bom_items)

    # Collect alternative suggestions for non-compliant manufacturers
    alternatives: dict[str, list[str]] = {}
    for item_id in result.non_compliant_items:
        # Find the original item to get manufacturer
        for item in bom_items:
            check_id = (
                item.get("id") or item.get("name")
                or item.get("model") or item.get("model_number") or "unknown"
            )
            if str(check_id) == item_id:
                mfr = item.get("manufacturer", "")
                if mfr:
                    alts = get_compliant_alternatives(mfr)
                    if alts:
                        alternatives[mfr] = alts

    return {
        "passed": result.passed,
        "total_checks": len(result.checks),
        "failed_checks": sum(1 for c in result.checks if not c.passed),
        "non_compliant_items": result.non_compliant_items,
        "non_compliant_countries": result.non_compliant_countries,
        "compliant_alternatives": alternatives,
        "details": [
            {
                "check_name": c.check_name,
                "passed": c.passed,
                "severity": c.severity,
                "message": c.message,
                "affected_items": c.affected_items,
            }
            for c in result.checks
            if not c.passed
        ],
    }


def _run_stig_validation(bom_items: list[dict]) -> dict:
    """Run DISA RME Vendor STIG validation on networked BOM items.

    Returns a serializable dict summarizing the STIG compliance result.
    """
    from compliance.disa_stig import STIGValidator

    validator = STIGValidator()
    result = validator.validate_bom(bom_items)

    return {
        "stig_version": result.stig_version,
        "passed": result.passed,
        "total_checks": len(result.checks),
        "failed_checks": sum(1 for c in result.checks if not c.passed),
        "uncertified_items": result.uncertified_items,
        "details": [
            {
                "check_name": c.check_name,
                "passed": c.passed,
                "severity": c.severity,
                "message": c.message,
                "affected_items": c.affected_items,
            }
            for c in result.checks
            if not c.passed
        ],
    }


def _strip_non_compliant_items(bom_json: dict, non_compliant_ids: list[str]) -> dict:
    """Remove non-compliant items from the BOM and recalculate totals.

    Returns a new bom_json dict with non-compliant items removed and a
    ``compliance_warnings`` list added.
    """
    if not non_compliant_ids:
        return bom_json

    cleaned_bom = dict(bom_json)
    original_items = cleaned_bom.get("items", [])
    warnings: list[str] = []

    kept_items: list[dict] = []
    for item in original_items:
        item_id = (
            item.get("id") or item.get("name")
            or item.get("model_number") or item.get("model") or "unknown"
        )
        if str(item_id) in non_compliant_ids:
            warnings.append(
                f"Removed non-compliant item: {item.get('manufacturer', '?')} "
                f"{item.get('model_number', '?')} ({item_id})"
            )
            logger.warning(
                "Stripping non-compliant item from BOM: %s %s",
                item.get("manufacturer", "?"),
                item.get("model_number", "?"),
            )
        else:
            kept_items.append(item)

    # Recalculate totals
    equipment_subtotal = sum(
        item.get("unit_price", item.get("dealer_price", 0)) * item.get("quantity", 1)
        for item in kept_items
    )

    cleaned_bom["items"] = kept_items
    cleaned_bom["equipment_subtotal"] = round(equipment_subtotal, 2)
    cleaned_bom["total_items"] = len(kept_items)
    cleaned_bom["compliance_warnings"] = warnings
    cleaned_bom["compliance_cleaned_at"] = datetime.now(timezone.utc).isoformat()

    return cleaned_bom


def _generate_report_summary(avixa: dict, taa: dict, stig: dict | None,
                             is_federal: bool) -> str:
    """Generate a human-readable compliance report summary."""
    lines: list[str] = [
        "=== Compliance Report Summary ===",
        "",
        f"AVIXA J-STD-710: {'PASS' if avixa['passed'] else 'FAIL'} "
        f"({avixa['failed_checks']}/{avixa['total_checks']} checks failed)",
        "",
        f"TAA Compliance: {'PASS' if taa['passed'] else 'FAIL'} "
        f"({taa['failed_checks']}/{taa['total_checks']} checks failed)",
    ]

    if taa.get("non_compliant_items"):
        lines.append(f"  Non-compliant items: {', '.join(taa['non_compliant_items'])}")
    if taa.get("non_compliant_countries"):
        lines.append(f"  Non-compliant countries: {', '.join(taa['non_compliant_countries'])}")
    if taa.get("compliant_alternatives"):
        lines.append("  Suggested alternatives:")
        for mfr, alts in taa["compliant_alternatives"].items():
            lines.append(f"    {mfr} -> {', '.join(alts)}")

    if is_federal and stig:
        lines.append("")
        lines.append(
            f"DISA STIG ({stig['stig_version']}): "
            f"{'PASS' if stig['passed'] else 'FAIL'} "
            f"({stig['failed_checks']}/{stig['total_checks']} checks failed)"
        )
        if stig.get("uncertified_items"):
            lines.append(
                f"  Uncertified devices: {', '.join(stig['uncertified_items'])}"
            )
    elif is_federal:
        lines.append("")
        lines.append("DISA STIG: Not evaluated (no networked devices found)")

    overall = avixa["passed"] and taa["passed"]
    if is_federal and stig:
        overall = overall and stig["passed"]

    lines.append("")
    lines.append(f"Overall: {'PASS' if overall else 'FAIL'}")

    return "\n".join(lines)


# ── Main node function ─────────────────────────────────────────────────────

async def compliance_node(state: dict) -> dict:
    """Cyber Risk & Compliance AI node.

    1. Validates all BOM items against AVIXA J-STD-710.
    2. Validates all BOM items against TAA requirements.
    3. If the project is federal/military, runs DISA STIG validation.
    4. Aggregates results and strips non-compliant items from the BOM.
    """
    logger.info("Compliance agent starting validation")

    try:
        bom_json = state.get("bom_json")
        project_context = state.get("project_context") or {}

        if not bom_json or not bom_json.get("items"):
            return {"error": "No bom_json with items available for compliance validation."}

        bom_items = bom_json["items"]
        is_federal = project_context.get("is_federal", False)

        # Check for federal/military keywords in special_requirements
        special_reqs = project_context.get("special_requirements", [])
        federal_keywords = {"federal", "government", "military", "dod", "disa", "stig", "fisma"}
        if any(
            kw in req.lower()
            for req in special_reqs
            for kw in federal_keywords
        ):
            is_federal = True

        # ── Run AVIXA J-STD-710 validation ──────────────────────────────
        logger.info("Running AVIXA J-STD-710 validation on %d item(s)", len(bom_items))
        avixa_result = _run_avixa_validation(bom_items)

        # ── Run TAA compliance check ────────────────────────────────────
        logger.info("Running TAA compliance check on %d item(s)", len(bom_items))
        taa_result = _run_taa_validation(bom_items)

        # ── Run DISA STIG validation (federal/military only) ────────────
        stig_result: dict | None = None
        if is_federal:
            logger.info("Federal project detected; running DISA STIG validation")
            stig_result = _run_stig_validation(bom_items)
        else:
            logger.info("Non-federal project; skipping DISA STIG validation")

        # ── Determine overall pass/fail ─────────────────────────────────
        overall_passed = avixa_result["passed"] and taa_result["passed"]
        if is_federal and stig_result:
            overall_passed = overall_passed and stig_result["passed"]

        # ── Collect all non-compliant item IDs ──────────────────────────
        non_compliant_items: list[str] = list(taa_result.get("non_compliant_items", []))
        if stig_result:
            for item_id in stig_result.get("uncertified_items", []):
                if item_id not in non_compliant_items:
                    non_compliant_items.append(item_id)

        # ── Generate report summary ─────────────────────────────────────
        report_summary = _generate_report_summary(
            avixa_result, taa_result, stig_result, is_federal
        )
        logger.info("\n%s", report_summary)

        # ── Build compliance_status ─────────────────────────────────────
        compliance_status: dict[str, Any] = {
            "passed": overall_passed,
            "avixa": avixa_result,
            "taa": taa_result,
            "stig": stig_result or {},
            "non_compliant_items": non_compliant_items,
            "report_summary": report_summary,
            "is_federal": is_federal,
            "validated_at": datetime.now(timezone.utc).isoformat(),
        }

        # ── Strip non-compliant items from BOM ──────────────────────────
        cleaned_bom = bom_json
        if non_compliant_items:
            logger.warning(
                "Stripping %d non-compliant item(s) from BOM",
                len(non_compliant_items),
            )
            cleaned_bom = _strip_non_compliant_items(bom_json, non_compliant_items)

        logger.info(
            "Compliance validation complete: overall=%s, "
            "avixa=%s, taa=%s, stig=%s",
            overall_passed,
            avixa_result["passed"],
            taa_result["passed"],
            stig_result["passed"] if stig_result else "N/A",
        )

        return {
            "compliance_status": compliance_status,
            "bom_json": cleaned_bom,
        }

    except Exception as exc:
        logger.exception("Compliance agent failed")
        return {"error": f"Compliance error: {exc}"}
