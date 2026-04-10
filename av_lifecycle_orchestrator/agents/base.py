"""Base agent providing shared LLM initialization and prompt construction."""

import logging
from typing import Optional
from langchain_core.language_models import BaseChatModel
from langchain_core.messages import SystemMessage, HumanMessage

logger = logging.getLogger(__name__)


def get_llm(provider: str = "openai", model: str = "gpt-4o",
            temperature: float = 0.1, api_key: str = "") -> BaseChatModel:
    """Create the appropriate LangChain chat model based on provider config."""
    if provider == "anthropic":
        from langchain_anthropic import ChatAnthropic
        return ChatAnthropic(
            model=model,
            temperature=temperature,
            anthropic_api_key=api_key,
        )
    else:
        from langchain_openai import ChatOpenAI
        return ChatOpenAI(
            model=model,
            temperature=temperature,
            openai_api_key=api_key,
        )


def get_llm_from_config() -> BaseChatModel:
    """Create LLM from environment configuration."""
    try:
        from config import Settings
        settings = Settings()
        api_key = (settings.anthropic_api_key if settings.llm_provider == "anthropic"
                   else settings.openai_api_key)
        return get_llm(
            provider=settings.llm_provider,
            model=settings.llm_model,
            temperature=settings.llm_temperature,
            api_key=api_key,
        )
    except Exception as e:
        logger.warning(f"Could not load settings, using defaults: {e}")
        return get_llm()


class BaseAgent:
    """Base class for all specialized agents."""

    agent_name: str = "base"
    system_prompt: str = "You are a helpful assistant."

    def __init__(self, llm: Optional[BaseChatModel] = None):
        self.llm = llm or get_llm_from_config()

    async def invoke_llm(self, user_message: str, system_message: Optional[str] = None) -> str:
        """Invoke the LLM with a system and user message."""
        messages = [
            SystemMessage(content=system_message or self.system_prompt),
            HumanMessage(content=user_message),
        ]
        response = await self.llm.ainvoke(messages)
        return response.content

    def _safe_state_update(self, updates: dict) -> dict:
        """Wrap state updates with error handling."""
        return {k: v for k, v in updates.items() if v is not None}
