"""
Application settings via pydantic-settings with .env loading.
"""

from __future__ import annotations

from functools import lru_cache

from pydantic_settings import BaseSettings
from pydantic import Field


class Settings(BaseSettings):
    """Central configuration loaded from environment variables / .env file."""

    # ── LLM ──────────────────────────────────────────────────────────────
    llm_provider: str = Field(default="openai", description="LLM provider: 'openai' or 'anthropic'")
    llm_model: str = Field(default="gpt-4o", description="Model name passed to the provider")
    llm_temperature: float = Field(default=0.1, description="Sampling temperature")

    # ── API keys ─────────────────────────────────────────────────────────
    openai_api_key: str = Field(default="", description="OpenAI API key")
    anthropic_api_key: str = Field(default="", description="Anthropic API key")

    # ── Jetbuilt ─────────────────────────────────────────────────────────
    jetbuilt_api_token: str = Field(default="", description="Jetbuilt API bearer token")
    jetbuilt_base_url: str = Field(default="https://app.jetbuilt.com/api", description="Jetbuilt API base URL")

    # ── Xavia (xten-av) ─────────────────────────────────────────────────
    xavia_api_key: str = Field(default="", description="Xavia / xten-av API key")
    xavia_base_url: str = Field(default="https://api.xten-av.com/v1", description="Xavia API base URL")

    # ── Gmail / Google OAuth ─────────────────────────────────────────────
    gmail_credentials_json: str = Field(default="credentials.json", description="Path to Google OAuth credentials file")
    gmail_token_json: str = Field(default="token.json", description="Path to cached OAuth token file")
    gmail_sender_email: str = Field(default="", description="Sender email address for outgoing mail")
    gmail_sender_name: str = Field(default="", description="Friendly sender name")

    # ── ChromaDB ─────────────────────────────────────────────────────────
    chroma_persist_dir: str = Field(default="./chroma_data", description="ChromaDB persistence directory")

    # ── Runtime flags ────────────────────────────────────────────────────
    mock_mode: bool = Field(default=False, description="When True, use mock/stub integrations")
    log_level: str = Field(default="INFO", description="Logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)")

    model_config = {
        "env_file": ".env",
        "env_file_encoding": "utf-8",
        "extra": "ignore",
    }

    # ── Helpers ──────────────────────────────────────────────────────────

    def get_llm(self):
        """Return the appropriate LangChain chat model based on provider config.

        Returns:
            A LangChain ``BaseChatModel`` instance configured with the current
            provider, model name, temperature, and API key.

        Raises:
            ValueError: If *llm_provider* is not ``"openai"`` or ``"anthropic"``.
        """
        provider = self.llm_provider.lower()

        if provider == "openai":
            from langchain_openai import ChatOpenAI

            return ChatOpenAI(
                model=self.llm_model,
                temperature=self.llm_temperature,
                api_key=self.openai_api_key or None,
            )

        if provider == "anthropic":
            from langchain_anthropic import ChatAnthropic

            return ChatAnthropic(
                model=self.llm_model,
                temperature=self.llm_temperature,
                api_key=self.anthropic_api_key or None,
            )

        raise ValueError(
            f"Unsupported llm_provider '{self.llm_provider}'. "
            "Choose 'openai' or 'anthropic'."
        )


@lru_cache(maxsize=1)
def get_settings() -> Settings:
    """Return a cached singleton ``Settings`` instance."""
    return Settings()
