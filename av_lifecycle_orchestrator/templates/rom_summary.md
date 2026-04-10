# Rough Order of Magnitude (ROM) Estimate

## Project: {{ project_name }}
## Client: {{ client_name }}
## Date: {{ date }}

---

## Financial Summary

| Metric | Value |
|--------|-------|
| Base Estimate | ${{ "%.2f"|format(base_estimate) }} |
| Lower Bound (-25%) | ${{ "%.2f"|format(lower_bound) }} |
| Upper Bound (+75%) | ${{ "%.2f"|format(upper_bound) }} |
| Equipment Subtotal | ${{ "%.2f"|format(equipment_subtotal) }} |
| Labor Subtotal | ${{ "%.2f"|format(labor_subtotal) }} |
| Contingency ({{ contingency_pct }}%) | ${{ "%.2f"|format(contingency_amount) }} |

## Labor Breakdown

| Role | Rate | Hours | Total |
|------|------|-------|-------|
{% for item in labor_items %}
| {{ item.role }} | ${{ "%.2f"|format(item.hourly_rate) }}/hr | {{ "%.1f"|format(item.estimated_hours) }} | ${{ "%.2f"|format(item.total) }} |
{% endfor %}

## ROM Accuracy Range

This ROM estimate carries an accuracy range of **-25% to +75%** as defined by
AACE International Class 5 estimate guidelines. Detailed engineering and final
vendor pricing will refine this estimate during subsequent project phases.

## Notes

{% for note in notes %}
- {{ note }}
{% endfor %}
