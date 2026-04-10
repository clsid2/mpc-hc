"""LangChain @tool wrappers for spatial analysis utilities."""

import asyncio
import json
import logging
from typing import Optional

from langchain_core.tools import tool

from .dxf_parser import DXFParser
from .opencv_detector import AVSymbolDetector
from .vision_analyzer import VisionAnalyzer

logger = logging.getLogger(__name__)


@tool
def parse_dxf_floor_plan(file_path: str) -> str:
    """Parse a DXF/CAD floor-plan file and extract rooms, blocks, text labels, and layers.

    Args:
        file_path: Path to the DXF file on disk.

    Returns:
        JSON string containing the full extraction results including rooms,
        block counts, layers, and text labels.
    """
    parser = DXFParser()
    try:
        result = parser.full_extraction(file_path)
        return json.dumps(result, indent=2, default=str)
    except Exception as exc:
        logger.exception("Error parsing DXF file: %s", file_path)
        return json.dumps({"error": str(exc)})


@tool
def detect_av_symbols(image_path: str, template_dir: str = "") -> str:
    """Detect AV symbols in a floor-plan image using OpenCV template matching.

    Args:
        image_path: Path to the floor-plan image (PNG/JPG).
        template_dir: Directory containing AV symbol template images.
            If empty, returns an empty detection set.

    Returns:
        JSON string with detection results including bounding boxes and
        confidence scores.
    """
    detector = AVSymbolDetector()
    try:
        result = detector.detect_from_image(
            image_path, template_dir=template_dir or None
        )
        return json.dumps(result, indent=2, default=str)
    except Exception as exc:
        logger.exception("Error detecting AV symbols: %s", image_path)
        return json.dumps({"error": str(exc)})


@tool
def analyze_floor_plan_vision(image_path: str) -> str:
    """Analyze a floor-plan image using GPT-4o vision to identify rooms, AV equipment, and infrastructure.

    Args:
        image_path: Path to the floor-plan image file.

    Returns:
        JSON string with structured analysis including rooms, AV equipment,
        data drops, power outlets, and existing infrastructure.
    """
    analyzer = VisionAnalyzer()
    try:
        # Run the async method in a sync context
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            loop = None

        if loop and loop.is_running():
            # We are inside an already-running event loop; create a task
            import concurrent.futures

            with concurrent.futures.ThreadPoolExecutor() as pool:
                result = pool.submit(
                    asyncio.run, analyzer.analyze_floor_plan(image_path)
                ).result()
        else:
            result = asyncio.run(analyzer.analyze_floor_plan(image_path))

        return json.dumps(result, indent=2, default=str)
    except Exception as exc:
        logger.exception("Error analyzing floor plan with vision: %s", image_path)
        return json.dumps({"error": str(exc)})
