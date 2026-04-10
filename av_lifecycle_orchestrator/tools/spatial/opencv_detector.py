import os
from typing import Optional
import logging

import cv2
import numpy as np

logger = logging.getLogger(__name__)


class AVSymbolDetector:
    """OpenCV template matching for AV symbols on floor plan images."""

    def __init__(self, confidence_threshold: float = 0.7):
        self.confidence_threshold = confidence_threshold

    def detect_symbols(
        self, image_path: str, template_dir: str
    ) -> list[dict]:
        """Load a floor-plan image and match AV-symbol templates against it.

        Args:
            image_path: Path to the floor-plan image.
            template_dir: Directory containing template images (PNG/JPG).

        Returns:
            List of dicts with keys: x, y, w, h, confidence, template_name.
        """
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if image is None:
            raise FileNotFoundError(f"Could not load image: {image_path}")

        detections: list[dict] = []

        for filename in os.listdir(template_dir):
            if not filename.lower().endswith((".png", ".jpg", ".jpeg", ".bmp")):
                continue

            template_path = os.path.join(template_dir, filename)
            template = cv2.imread(template_path, cv2.IMREAD_GRAYSCALE)
            if template is None:
                logger.warning("Skipping unreadable template: %s", template_path)
                continue

            t_h, t_w = template.shape[:2]

            # Skip templates larger than the source image
            if t_h > image.shape[0] or t_w > image.shape[1]:
                logger.warning(
                    "Template %s is larger than source image, skipping.", filename
                )
                continue

            result = cv2.matchTemplate(image, template, cv2.TM_CCOEFF_NORMED)
            locations = np.where(result >= self.confidence_threshold)

            template_name = os.path.splitext(filename)[0]

            for pt_y, pt_x in zip(*locations):
                confidence = float(result[pt_y, pt_x])
                detections.append(
                    {
                        "x": int(pt_x),
                        "y": int(pt_y),
                        "w": int(t_w),
                        "h": int(t_h),
                        "confidence": round(confidence, 4),
                        "template_name": template_name,
                    }
                )

        detections = self._non_maximum_suppression(detections)
        return detections

    def _non_maximum_suppression(
        self, detections: list[dict], overlap_threshold: float = 0.3
    ) -> list[dict]:
        """Remove duplicate / overlapping detections using NMS.

        For every pair of detections the Intersection-over-Union (IoU) is
        computed. When IoU exceeds *overlap_threshold* the lower-confidence
        detection is discarded.

        Args:
            detections: Raw detection list.
            overlap_threshold: IoU threshold above which a detection is
                considered a duplicate.

        Returns:
            Filtered list of detections.
        """
        if not detections:
            return []

        # Sort by confidence descending so we keep the best ones
        sorted_dets = sorted(detections, key=lambda d: d["confidence"], reverse=True)
        keep: list[dict] = []

        for det in sorted_dets:
            is_duplicate = False
            for kept in keep:
                iou = self._compute_iou(det, kept)
                if iou > overlap_threshold:
                    is_duplicate = True
                    break
            if not is_duplicate:
                keep.append(det)

        return keep

    @staticmethod
    def _compute_iou(a: dict, b: dict) -> float:
        """Compute Intersection-over-Union between two bounding boxes."""
        ax1, ay1, ax2, ay2 = a["x"], a["y"], a["x"] + a["w"], a["y"] + a["h"]
        bx1, by1, bx2, by2 = b["x"], b["y"], b["x"] + b["w"], b["y"] + b["h"]

        inter_x1 = max(ax1, bx1)
        inter_y1 = max(ay1, by1)
        inter_x2 = min(ax2, bx2)
        inter_y2 = min(ay2, by2)

        inter_area = max(0, inter_x2 - inter_x1) * max(0, inter_y2 - inter_y1)
        area_a = a["w"] * a["h"]
        area_b = b["w"] * b["h"]
        union_area = area_a + area_b - inter_area

        if union_area == 0:
            return 0.0
        return inter_area / union_area

    def detect_from_image(
        self, image_path: str, template_dir: Optional[str] = None
    ) -> dict:
        """Higher-level detection entry point.

        Args:
            image_path: Path to the floor-plan image.
            template_dir: Optional directory of template images. If ``None``
                an empty detection list is returned.

        Returns:
            Dict with keys: source, detections, total_detected.
        """
        if template_dir is None or not os.path.isdir(template_dir):
            logger.info(
                "No valid template_dir provided; returning empty detections."
            )
            return {
                "source": image_path,
                "detections": [],
                "total_detected": 0,
            }

        detections = self.detect_symbols(image_path, template_dir)
        return {
            "source": image_path,
            "detections": detections,
            "total_detected": len(detections),
        }
