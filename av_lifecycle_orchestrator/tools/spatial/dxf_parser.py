import ezdxf
from typing import Optional
import logging

logger = logging.getLogger(__name__)

class DXFParser:
    """Parses DXF/CAD files to extract room layouts, dimensions, and AV symbol data."""

    def __init__(self):
        self.doc = None
        self.msp = None  # modelspace

    def load(self, file_path: str) -> None:
        """Load a DXF file."""
        self.doc = ezdxf.readfile(file_path)
        self.msp = self.doc.modelspace()

    def extract_rooms(self) -> list[dict]:
        """Extract room boundaries and dimensions from polylines and closed shapes.
        Returns list of dicts with room_name, vertices, dimensions."""
        rooms = []
        for entity in self.msp.query("LWPOLYLINE"):
            if entity.is_closed:
                vertices = list(entity.get_points(format="xy"))
                bbox = self._bounding_box(vertices)
                room = {
                    "layer": entity.dxf.layer,
                    "vertices": vertices,
                    "length_ft": abs(bbox["max_x"] - bbox["min_x"]),
                    "width_ft": abs(bbox["max_y"] - bbox["min_y"]),
                    "area_sqft": self._polygon_area(vertices),
                    "perimeter_ft": self._polygon_perimeter(vertices),
                }
                rooms.append(room)
        return rooms

    def extract_blocks(self) -> list[dict]:
        """Extract block references (AV symbols, fixtures, etc.).
        Returns list of dicts with block_name, position, layer, attributes."""
        blocks = []
        for insert in self.msp.query("INSERT"):
            attrs = {}
            if insert.attribs:
                for attrib in insert.attribs:
                    attrs[attrib.dxf.tag] = attrib.dxf.text
            blocks.append({
                "block_name": insert.dxf.name,
                "x": insert.dxf.insert.x,
                "y": insert.dxf.insert.y,
                "layer": insert.dxf.layer,
                "attributes": attrs,
                "rotation": getattr(insert.dxf, "rotation", 0),
                "scale_x": getattr(insert.dxf, "xscale", 1.0),
                "scale_y": getattr(insert.dxf, "yscale", 1.0),
            })
        return blocks

    def extract_text_labels(self) -> list[dict]:
        """Extract text entities for room names, labels, etc."""
        texts = []
        for text in self.msp.query("TEXT"):
            texts.append({
                "text": text.dxf.text,
                "x": text.dxf.insert.x,
                "y": text.dxf.insert.y,
                "layer": text.dxf.layer,
                "height": text.dxf.height,
            })
        for mtext in self.msp.query("MTEXT"):
            texts.append({
                "text": mtext.text,
                "x": mtext.dxf.insert.x,
                "y": mtext.dxf.insert.y,
                "layer": mtext.dxf.layer,
                "height": mtext.dxf.char_height,
            })
        return texts

    def extract_layers(self) -> list[dict]:
        """List all layers and their entity counts."""
        layers = {}
        for entity in self.msp:
            layer = entity.dxf.layer
            layers[layer] = layers.get(layer, 0) + 1
        return [{"name": k, "entity_count": v} for k, v in sorted(layers.items())]

    def quantity_takeoff(self) -> dict:
        """Perform automated quantity takeoff.
        Counts blocks by type, measures dimensions, documents constraints."""
        blocks = self.extract_blocks()
        rooms = self.extract_rooms()

        block_counts = {}
        for b in blocks:
            name = b["block_name"]
            block_counts[name] = block_counts.get(name, 0) + 1

        return {
            "total_rooms": len(rooms),
            "rooms": rooms,
            "total_blocks": len(blocks),
            "block_counts": block_counts,
            "blocks": blocks,
            "layers": self.extract_layers(),
            "text_labels": self.extract_text_labels(),
        }

    def full_extraction(self, file_path: str) -> dict:
        """One-shot: load file and extract everything."""
        self.load(file_path)
        return self.quantity_takeoff()

    @staticmethod
    def _bounding_box(vertices: list[tuple]) -> dict:
        xs = [v[0] for v in vertices]
        ys = [v[1] for v in vertices]
        return {"min_x": min(xs), "max_x": max(xs), "min_y": min(ys), "max_y": max(ys)}

    @staticmethod
    def _polygon_area(vertices: list[tuple]) -> float:
        """Shoelace formula for polygon area."""
        n = len(vertices)
        if n < 3:
            return 0.0
        area = 0.0
        for i in range(n):
            j = (i + 1) % n
            area += vertices[i][0] * vertices[j][1]
            area -= vertices[j][0] * vertices[i][1]
        return abs(area) / 2.0

    @staticmethod
    def _polygon_perimeter(vertices: list[tuple]) -> float:
        n = len(vertices)
        if n < 2:
            return 0.0
        perimeter = 0.0
        for i in range(n):
            j = (i + 1) % n
            dx = vertices[j][0] - vertices[i][0]
            dy = vertices[j][1] - vertices[i][1]
            perimeter += (dx**2 + dy**2) ** 0.5
        return perimeter
