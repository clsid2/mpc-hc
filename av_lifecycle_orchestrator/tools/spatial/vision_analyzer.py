import base64
import os
from typing import Optional
import logging

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Mock / sample data returned when running without a live API key
# ---------------------------------------------------------------------------
_MOCK_FLOOR_PLAN_RESULT: dict = {
    "rooms": [
        {
            "name": "Conference Room A",
            "type": "conference",
            "dimensions": {"length_ft": 30, "width_ft": 20},
            "area_sqft": 600,
        },
        {
            "name": "Huddle Room B",
            "type": "huddle",
            "dimensions": {"length_ft": 12, "width_ft": 10},
            "area_sqft": 120,
        },
        {
            "name": "Open Office Area",
            "type": "open_office",
            "dimensions": {"length_ft": 60, "width_ft": 40},
            "area_sqft": 2400,
        },
    ],
    "av_equipment": [
        {"type": "display", "location": "Conference Room A", "count": 2},
        {"type": "speaker", "location": "Conference Room A", "count": 4},
        {"type": "camera", "location": "Conference Room A", "count": 1},
        {"type": "display", "location": "Huddle Room B", "count": 1},
        {"type": "soundbar", "location": "Huddle Room B", "count": 1},
    ],
    "data_drops": [
        {"location": "Conference Room A", "count": 6},
        {"location": "Huddle Room B", "count": 2},
        {"location": "Open Office Area", "count": 24},
    ],
    "power_outlets": [
        {"location": "Conference Room A", "count": 8},
        {"location": "Huddle Room B", "count": 4},
        {"location": "Open Office Area", "count": 30},
    ],
    "existing_infrastructure": [
        "Ceiling-mounted projector screen in Conference Room A",
        "Cable tray running along north wall of Open Office Area",
        "Network switch closet adjacent to Huddle Room B",
    ],
}

_MOCK_ROOM_PARAMS: list[dict] = [
    {
        "name": "Conference Room A",
        "type": "conference",
        "length_ft": 30,
        "width_ft": 20,
        "ceiling_height_ft": 10,
    },
    {
        "name": "Huddle Room B",
        "type": "huddle",
        "length_ft": 12,
        "width_ft": 10,
        "ceiling_height_ft": 9,
    },
    {
        "name": "Open Office Area",
        "type": "open_office",
        "length_ft": 60,
        "width_ft": 40,
        "ceiling_height_ft": 12,
    },
]


class VisionAnalyzer:
    """GPT-4o Vision multimodal analysis for floor plans and AV layouts."""

    def __init__(self, api_key: str = "", provider: str = "openai"):
        self.api_key = api_key or os.environ.get("OPENAI_API_KEY", "")
        self.provider = provider
        self._mock_mode = not bool(self.api_key)
        if self._mock_mode:
            logger.info(
                "VisionAnalyzer running in mock mode (no API key provided)."
            )

    # ------------------------------------------------------------------
    # Public async interface
    # ------------------------------------------------------------------

    async def analyze_floor_plan(self, image_path: str) -> dict:
        """Analyze a floor-plan image and return structured AV-relevant data.

        Uses GPT-4o vision to identify rooms, AV equipment, data drops,
        power outlets, and existing infrastructure.

        Args:
            image_path: Path to the floor-plan image file.

        Returns:
            Dict with keys: rooms, av_equipment, data_drops, power_outlets,
            existing_infrastructure.
        """
        if self._mock_mode:
            logger.info("Mock mode: returning sample floor-plan analysis.")
            return _MOCK_FLOOR_PLAN_RESULT

        from langchain_openai import ChatOpenAI

        b64_image = self._encode_image(image_path)

        prompt_text = (
            "You are an expert AV and building systems analyst. "
            "Analyze this floor plan image and identify the following in structured JSON:\n"
            "1. rooms - list of rooms with name, type (conference/huddle/open_office/lobby/etc), "
            "and estimated dimensions (length_ft, width_ft) and area_sqft.\n"
            "2. av_equipment - list of AV equipment you can see with type, location, and count.\n"
            "3. data_drops - list of data drop locations with location name and count.\n"
            "4. power_outlets - list of power outlet locations with location name and count.\n"
            "5. existing_infrastructure - list of notable infrastructure items.\n"
            "Return valid JSON only."
        )

        llm = ChatOpenAI(
            model="gpt-4o",
            api_key=self.api_key,
            max_tokens=4096,
        )

        message = {
            "role": "user",
            "content": [
                {"type": "text", "text": prompt_text},
                {
                    "type": "image_url",
                    "image_url": {
                        "url": f"data:image/png;base64,{b64_image}",
                        "detail": "high",
                    },
                },
            ],
        }

        response = await llm.ainvoke([message])

        # Parse the response content as JSON
        import json

        try:
            result = json.loads(response.content)
        except json.JSONDecodeError:
            logger.warning(
                "Could not parse structured JSON from vision response; "
                "returning raw text."
            )
            result = {
                "raw_response": response.content,
                "rooms": [],
                "av_equipment": [],
                "data_drops": [],
                "power_outlets": [],
                "existing_infrastructure": [],
            }

        return result

    async def extract_room_parameters(self, image_path: str) -> list[dict]:
        """Focused extraction of room names, types, and dimensions.

        Args:
            image_path: Path to the floor-plan image file.

        Returns:
            List of dicts with keys: name, type, length_ft, width_ft,
            ceiling_height_ft.
        """
        if self._mock_mode:
            logger.info("Mock mode: returning sample room parameters.")
            return _MOCK_ROOM_PARAMS

        from langchain_openai import ChatOpenAI

        b64_image = self._encode_image(image_path)

        prompt_text = (
            "You are an expert building analyst. "
            "Look at this floor plan and list every room you can identify.\n"
            "For each room return a JSON object with:\n"
            "- name (string)\n"
            "- type (conference | huddle | open_office | lobby | utility | restroom | other)\n"
            "- length_ft (number, estimated)\n"
            "- width_ft (number, estimated)\n"
            "- ceiling_height_ft (number, estimated or null)\n"
            "Return a JSON array only."
        )

        llm = ChatOpenAI(
            model="gpt-4o",
            api_key=self.api_key,
            max_tokens=2048,
        )

        message = {
            "role": "user",
            "content": [
                {"type": "text", "text": prompt_text},
                {
                    "type": "image_url",
                    "image_url": {
                        "url": f"data:image/png;base64,{b64_image}",
                        "detail": "high",
                    },
                },
            ],
        }

        response = await llm.ainvoke([message])

        import json

        try:
            result = json.loads(response.content)
        except json.JSONDecodeError:
            logger.warning(
                "Could not parse structured JSON from vision response; "
                "returning empty list."
            )
            result = []

        return result

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _encode_image(image_path: str) -> str:
        """Read an image file and return its base64-encoded string."""
        with open(image_path, "rb") as f:
            return base64.b64encode(f.read()).decode("utf-8")
