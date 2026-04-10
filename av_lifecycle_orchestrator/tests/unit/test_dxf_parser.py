"""Tests for DXF parser geometry utilities (without actual DXF files)."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.spatial.dxf_parser import DXFParser


class TestBoundingBox:
    """Verify _bounding_box with known vertices."""

    def test_bounding_box(self):
        vertices = [(0, 0), (10, 0), (10, 5), (0, 5)]
        bbox = DXFParser._bounding_box(vertices)
        assert bbox["min_x"] == 0
        assert bbox["max_x"] == 10
        assert bbox["min_y"] == 0
        assert bbox["max_y"] == 5

    def test_bounding_box_negative_coords(self):
        vertices = [(-5, -3), (5, -3), (5, 7), (-5, 7)]
        bbox = DXFParser._bounding_box(vertices)
        assert bbox["min_x"] == -5
        assert bbox["max_x"] == 5
        assert bbox["min_y"] == -3
        assert bbox["max_y"] == 7

    def test_bounding_box_single_point(self):
        vertices = [(3, 4)]
        bbox = DXFParser._bounding_box(vertices)
        assert bbox["min_x"] == 3
        assert bbox["max_x"] == 3
        assert bbox["min_y"] == 4
        assert bbox["max_y"] == 4


class TestPolygonAreaSquare:
    """10x10 square = 100 sqft."""

    def test_polygon_area_square(self):
        # 10x10 square
        vertices = [(0, 0), (10, 0), (10, 10), (0, 10)]
        area = DXFParser._polygon_area(vertices)
        assert area == pytest.approx(100.0)

    def test_polygon_area_rectangle(self):
        # 20x30 rectangle
        vertices = [(0, 0), (20, 0), (20, 30), (0, 30)]
        area = DXFParser._polygon_area(vertices)
        assert area == pytest.approx(600.0)


class TestPolygonAreaTriangle:
    """Known triangle area."""

    def test_polygon_area_triangle(self):
        # Right triangle with base=6, height=4 -> area = 12.0
        vertices = [(0, 0), (6, 0), (0, 4)]
        area = DXFParser._polygon_area(vertices)
        assert area == pytest.approx(12.0)

    def test_polygon_area_degenerate(self):
        # Fewer than 3 vertices -> 0.0
        vertices = [(0, 0), (5, 5)]
        area = DXFParser._polygon_area(vertices)
        assert area == 0.0


class TestPolygonPerimeter:
    """Known perimeter calculation."""

    def test_polygon_perimeter_square(self):
        # 10x10 square, perimeter = 40
        vertices = [(0, 0), (10, 0), (10, 10), (0, 10)]
        perimeter = DXFParser._polygon_perimeter(vertices)
        assert perimeter == pytest.approx(40.0)

    def test_polygon_perimeter_triangle(self):
        # Right triangle: sides 3, 4, 5
        vertices = [(0, 0), (3, 0), (0, 4)]
        perimeter = DXFParser._polygon_perimeter(vertices)
        assert perimeter == pytest.approx(12.0)  # 3 + 4 + 5

    def test_polygon_perimeter_single_point(self):
        # Single vertex -> 0.0 (only one vertex, but the method returns distance
        # from vertex to itself for n=1 which is 0)
        vertices = [(5, 5)]
        perimeter = DXFParser._polygon_perimeter(vertices)
        assert perimeter == pytest.approx(0.0)
