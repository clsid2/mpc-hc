"""Trade Agreements Act (TAA) country-of-origin verification for AV BOM items.

Validates that all items in a Bill of Materials originate from TAA-designated
countries, as required for U.S. federal government procurements under
FAR 52.225-5.
"""

from models.compliance import ComplianceCheckResult, TAAComplianceResult

from .countries import is_restricted_country, is_taa_compliant

# Known manufacturer -> country of origin / substantial transformation
KNOWN_MANUFACTURER_ORIGINS: dict[str, str] = {
    "Crestron": "United States",
    "Extron": "United States",
    "Biamp": "United States",
    "QSC": "United States",
    "Shure": "United States",
    "Sennheiser": "Germany",
    "Legrand": "France",
    "Harman/AMX": "United States",
    "Poly": "United States",
    "Logitech": "Switzerland",
    "Samsung": "Korea (South)",
    "LG": "Korea (South)",
    "Sony": "Japan",
    "Panasonic": "Japan",
    "Epson": "Japan",
    "NEC": "Japan",
    "Barco": "Belgium",
    "Christie": "Canada",
    "Bose": "United States",
    "JBL": "United States",
    "Yamaha": "Japan",
    "Atlona": "United States",
    "APC": "United States",
    "Middle Atlantic": "United States",
    "Chief": "United States",
    "Peerless-AV": "United States",
    "Hikvision": "China",
    "Dahua": "China",
    "ZTE": "China",
    "Huawei": "China",
}

# Compliant alternatives for non-compliant manufacturers
_COMPLIANT_ALTERNATIVES: dict[str, list[str]] = {
    "Hikvision": ["Axis Communications", "Hanwha Techwin", "Bosch Security"],
    "Dahua": ["Axis Communications", "Hanwha Techwin", "Bosch Security"],
    "ZTE": ["Cisco", "Juniper Networks", "Aruba Networks"],
    "Huawei": ["Cisco", "Juniper Networks", "Aruba Networks"],
}


def validate_item(item: dict) -> ComplianceCheckResult:
    """Check a single BOM item against TAA requirements.

    Parameters
    ----------
    item:
        Dictionary representing a BOM item.  Expected keys:
        ``"manufacturer"`` and optionally ``"country_of_origin"``,
        ``"id"``, ``"name"``, ``"model"``.

    Returns
    -------
    ComplianceCheckResult
        Pass, warning (unknown origin), or critical failure (non-compliant).
    """
    item_id = item.get("id") or item.get("name") or item.get("model") or "unknown"
    manufacturer = item.get("manufacturer", "")

    # Determine country of origin
    country = item.get("country_of_origin", "")
    if not country and manufacturer:
        country = KNOWN_MANUFACTURER_ORIGINS.get(manufacturer, "")

    # No manufacturer and no country -- unknown origin
    if not country and not manufacturer:
        return ComplianceCheckResult(
            check_name="TAA Country of Origin",
            passed=False,
            severity="warning",
            message=(
                f"Item '{item_id}' has no manufacturer or country of origin "
                f"specified. Cannot determine TAA compliance."
            ),
            affected_items=[str(item_id)],
        )

    # Manufacturer found but country still unknown
    if not country:
        return ComplianceCheckResult(
            check_name="TAA Country of Origin",
            passed=True,
            severity="warning",
            message=(
                f"Item '{item_id}' manufacturer '{manufacturer}' is not in the "
                f"known origins database. Manual verification of country of "
                f"origin is required."
            ),
            affected_items=[str(item_id)],
        )

    # Check restricted countries first (critical)
    if is_restricted_country(country):
        alternatives = get_compliant_alternatives(manufacturer)
        alt_msg = ""
        if alternatives:
            alt_msg = f" Consider TAA-compliant alternatives: {', '.join(alternatives)}."
        return ComplianceCheckResult(
            check_name="TAA Country of Origin",
            passed=False,
            severity="critical",
            message=(
                f"Item '{item_id}' (manufacturer: {manufacturer}) originates "
                f"from '{country}', which is a restricted/non-compliant "
                f"country under TAA.{alt_msg}"
            ),
            affected_items=[str(item_id)],
        )

    # Check TAA compliance
    if is_taa_compliant(country):
        return ComplianceCheckResult(
            check_name="TAA Country of Origin",
            passed=True,
            severity="info",
            message=(
                f"Item '{item_id}' (manufacturer: {manufacturer}) originates "
                f"from '{country}', which is TAA-designated."
            ),
            affected_items=[str(item_id)],
        )

    # Country exists but is not TAA-designated and not restricted
    return ComplianceCheckResult(
        check_name="TAA Country of Origin",
        passed=False,
        severity="critical",
        message=(
            f"Item '{item_id}' (manufacturer: {manufacturer}) originates "
            f"from '{country}', which is NOT a TAA-designated country."
        ),
        affected_items=[str(item_id)],
    )


def validate_bom(bom_items: list[dict]) -> TAAComplianceResult:
    """Validate all items in a BOM against TAA requirements.

    Overall result passes only if all items pass or are warnings (unknown
    origin).  Critical failures are non-compliant countries.

    Parameters
    ----------
    bom_items:
        List of BOM item dictionaries.

    Returns
    -------
    TAAComplianceResult
        Aggregated TAA compliance result.
    """
    checks: list[ComplianceCheckResult] = []
    non_compliant_items: list[str] = []
    non_compliant_countries: set[str] = set()

    for item in bom_items:
        result = validate_item(item)
        checks.append(result)

        if not result.passed and result.severity == "critical":
            item_id = item.get("id") or item.get("name") or item.get("model") or "unknown"
            non_compliant_items.append(str(item_id))

            # Extract country for the non-compliant list
            country = item.get("country_of_origin", "")
            if not country:
                manufacturer = item.get("manufacturer", "")
                country = KNOWN_MANUFACTURER_ORIGINS.get(manufacturer, "")
            if country:
                non_compliant_countries.add(country)

    return TAAComplianceResult(
        checks=checks,
        non_compliant_items=non_compliant_items,
        non_compliant_countries=sorted(non_compliant_countries),
    )


def get_compliant_alternatives(manufacturer: str) -> list[str]:
    """Suggest TAA-compliant alternatives for a non-compliant manufacturer.

    Parameters
    ----------
    manufacturer:
        Name of the manufacturer to find alternatives for.

    Returns
    -------
    list[str]
        List of suggested TAA-compliant alternative manufacturers.
        Empty list if no specific alternatives are known.
    """
    if not manufacturer:
        return []
    return _COMPLIANT_ALTERNATIVES.get(manufacturer, [])
