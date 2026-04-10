"""Trade Agreements Act (TAA) designated and non-designated country lists.

TAA compliance under FAR 52.225-5 requires products to be manufactured or
substantially transformed in the United States or a designated country.
"""

# WTO Government Procurement Agreement (GPA) countries
WTO_GPA_COUNTRIES: set[str] = {
    "United States", "Armenia", "Australia", "Austria", "Belgium",
    "Bulgaria", "Canada", "Croatia", "Cyprus", "Czech Republic",
    "Denmark", "Estonia", "Finland", "France", "Germany", "Greece",
    "Hong Kong", "Hungary", "Iceland", "Ireland", "Israel", "Italy",
    "Japan", "Korea (South)", "Latvia", "Liechtenstein", "Lithuania",
    "Luxembourg", "Malta", "Moldova", "Montenegro", "Netherlands",
    "New Zealand", "North Macedonia", "Norway", "Poland", "Portugal",
    "Romania", "Singapore", "Slovak Republic", "Slovenia", "Spain",
    "Sweden", "Switzerland", "Taiwan", "Ukraine", "United Kingdom",
}

# Caribbean Basin countries
CARIBBEAN_BASIN_COUNTRIES: set[str] = {
    "Antigua and Barbuda", "Aruba", "Bahamas", "Barbados", "Belize",
    "British Virgin Islands", "Costa Rica", "Dominica", "Dominican Republic",
    "El Salvador", "Grenada", "Guatemala", "Guyana", "Haiti", "Honduras",
    "Jamaica", "Montserrat", "Netherlands Antilles", "Nicaragua", "Panama",
    "St. Kitts and Nevis", "St. Lucia", "St. Vincent and the Grenadines",
    "Trinidad and Tobago",
}

# Least developed countries
LEAST_DEVELOPED_COUNTRIES: set[str] = {
    "Afghanistan", "Angola", "Bangladesh", "Benin", "Bhutan",
    "Burkina Faso", "Burundi", "Cambodia", "Central African Republic",
    "Chad", "Comoros", "Democratic Republic of Congo", "Djibouti",
    "East Timor", "Equatorial Guinea", "Eritrea", "Ethiopia",
    "Gambia", "Guinea", "Guinea-Bissau", "Haiti", "Kiribati",
    "Laos", "Lesotho", "Liberia", "Madagascar", "Malawi", "Mali",
    "Mauritania", "Mozambique", "Myanmar", "Nepal", "Niger",
    "Rwanda", "Samoa", "Sao Tome and Principe", "Senegal",
    "Sierra Leone", "Solomon Islands", "Somalia", "South Sudan",
    "Sudan", "Tanzania", "Togo", "Tuvalu", "Uganda", "Vanuatu",
    "Yemen", "Zambia",
}

# All TAA-designated countries combined
TAA_DESIGNATED_COUNTRIES: set[str] = (
    WTO_GPA_COUNTRIES | CARIBBEAN_BASIN_COUNTRIES | LEAST_DEVELOPED_COUNTRIES
)

# Known non-compliant countries for AV equipment
NON_COMPLIANT_COUNTRIES: set[str] = {
    "China", "Russia", "Iran", "North Korea", "Belarus",
    "Venezuela", "Cuba", "Syria",
}

def is_taa_compliant(country_of_origin: str) -> bool:
    """Check if a country of origin is TAA compliant."""
    if not country_of_origin:
        return False
    normalized = country_of_origin.strip().title()
    return normalized in TAA_DESIGNATED_COUNTRIES

def is_restricted_country(country_of_origin: str) -> bool:
    """Check if country is on the restricted list."""
    if not country_of_origin:
        return False
    normalized = country_of_origin.strip().title()
    return normalized in NON_COMPLIANT_COUNTRIES
