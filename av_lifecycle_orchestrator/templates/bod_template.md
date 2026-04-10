# Basis of Design (BoD)

## Project: {{ project_name }}
## Client: {{ client_name }}
## Date: {{ date }}

---

## 1. Executive Summary

{{ executive_summary }}

## 2. Owner's Project Requirements (OPR) Summary

{{ opr_summary }}

## 3. Design Principles and Rationale

{{ design_rationale }}

## 4. Room-by-Room Analysis

{% for room in rooms %}
### {{ room.name }} ({{ room.type }})

- **Capacity**: {{ room.capacity }} occupants
- **Dimensions**: {{ room.length_ft }}' x {{ room.width_ft }}' ({{ room.area_sqft }} sq ft)
- **Ceiling Height**: {{ room.ceiling_height_ft | default('Standard 9\'') }}

#### Intended Systems
{{ room.system_description }}

#### Equipment Rationale
{{ room.equipment_rationale }}

{% endfor %}

## 5. Life Cycle Cost Analysis (LCCA)

{{ lcca_summary }}

## 6. Acoustic Considerations

{{ acoustic_notes }}

## 7. Sightline Verification

{{ sightline_notes }}

## 8. Power and Infrastructure Requirements

{{ power_notes }}

## 9. Assumptions and Constraints

{% for assumption in assumptions %}
- {{ assumption }}
{% endfor %}

## 10. Appendices

{{ appendices }}
