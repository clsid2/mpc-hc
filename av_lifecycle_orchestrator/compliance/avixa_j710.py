"""AVIXA J-STD-710 symbology validation for AV system design documents.

Validates that BOM items and equipment symbols conform to the AVIXA J-STD-710
standard for AV system drawing symbology, ensuring all required attributes
are present and use valid values.
"""

from models.compliance import AVIXAComplianceResult, ComplianceCheckResult

# Required symbol attributes per J-STD-710
REQUIRED_ATTRIBUTES: dict[str, str] = {
    "M": "Mounting Type",
    "T": "Primary Technology",
    "T2": "Secondary Technology",
    "X": "Legend/Schedule Reference",
}

# Valid mounting type codes
VALID_MOUNTING_TYPES: dict[str, str] = {
    "W": "Wall",
    "C": "Ceiling",
    "F": "Floor",
    "R": "Rack",
    "T": "Table/Surface",
    "P": "Pendant",
}

# Valid primary technology codes
VALID_PRIMARY_TECHNOLOGIES: dict[str, str] = {
    "D": "Display",
    "S": "Speaker",
    "C": "Camera",
    "M": "Microphone",
    "P": "Projector",
    "DSP": "Digital Signal Processor",
    "CTL": "Control System",
    "SW": "Switcher/Matrix",
}


def validate_symbol(symbol: dict) -> ComplianceCheckResult:
    """Check if a BOM item/symbol has all required J-STD-710 attributes.

    Parameters
    ----------
    symbol:
        Dictionary representing a BOM item or equipment symbol.  Expected
        keys include ``"M"``, ``"T"``, ``"T2"``, ``"X"`` corresponding to
        the required J-STD-710 attributes, plus an optional ``"id"`` or
        ``"name"`` for identification.

    Returns
    -------
    ComplianceCheckResult
        Result with pass/fail status and a list of missing attributes in
        the message.
    """
    item_id = symbol.get("id") or symbol.get("name") or "unknown"
    missing: list[str] = []

    for attr_code, attr_name in REQUIRED_ATTRIBUTES.items():
        value = symbol.get(attr_code)
        if not value:
            missing.append(f"{attr_code} ({attr_name})")

    if missing:
        return ComplianceCheckResult(
            check_name="AVIXA J-STD-710 Symbol Attributes",
            passed=False,
            severity="warning",
            message=(
                f"Item '{item_id}' is missing required J-STD-710 attributes: "
                f"{', '.join(missing)}"
            ),
            affected_items=[str(item_id)],
        )

    # Validate mounting type value if present
    mounting = symbol.get("M", "")
    if mounting and mounting not in VALID_MOUNTING_TYPES:
        return ComplianceCheckResult(
            check_name="AVIXA J-STD-710 Symbol Attributes",
            passed=False,
            severity="warning",
            message=(
                f"Item '{item_id}' has invalid mounting type '{mounting}'. "
                f"Valid types: {', '.join(f'{k} ({v})' for k, v in VALID_MOUNTING_TYPES.items())}"
            ),
            affected_items=[str(item_id)],
        )

    # Validate primary technology value if present
    tech = symbol.get("T", "")
    if tech and tech not in VALID_PRIMARY_TECHNOLOGIES:
        return ComplianceCheckResult(
            check_name="AVIXA J-STD-710 Symbol Attributes",
            passed=False,
            severity="warning",
            message=(
                f"Item '{item_id}' has invalid primary technology '{tech}'. "
                f"Valid types: {', '.join(f'{k} ({v})' for k, v in VALID_PRIMARY_TECHNOLOGIES.items())}"
            ),
            affected_items=[str(item_id)],
        )

    return ComplianceCheckResult(
        check_name="AVIXA J-STD-710 Symbol Attributes",
        passed=True,
        severity="info",
        message=f"Item '{item_id}' has all required J-STD-710 attributes.",
        affected_items=[str(item_id)],
    )


def validate_bom(bom_items: list[dict]) -> AVIXAComplianceResult:
    """Validate all items in a BOM against AVIXA J-STD-710 requirements.

    Parameters
    ----------
    bom_items:
        List of dictionaries, each representing a BOM item with symbol
        attributes.

    Returns
    -------
    AVIXAComplianceResult
        Aggregated compliance result.  An item fails if any required
        attribute is missing.
    """
    checks: list[ComplianceCheckResult] = []
    missing_attributes: list[dict] = []

    for item in bom_items:
        result = validate_symbol(item)
        checks.append(result)

        if not result.passed:
            item_id = item.get("id") or item.get("name") or "unknown"
            item_missing: list[str] = []
            for attr_code, attr_name in REQUIRED_ATTRIBUTES.items():
                if not item.get(attr_code):
                    item_missing.append(attr_code)
            missing_attributes.append({
                "item_id": str(item_id),
                "missing": item_missing,
            })

    return AVIXAComplianceResult(
        standard="J-STD-710",
        checks=checks,
        missing_attributes=missing_attributes,
    )
