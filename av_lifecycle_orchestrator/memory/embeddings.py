"""Embedding configuration for ChromaDB vector store.

Provides factory function for ChromaDB-compatible embedding functions,
supporting multiple providers.
"""

import logging
from typing import Optional

import chromadb.utils.embedding_functions as ef

logger = logging.getLogger(__name__)


def get_embedding_function(
    provider: str = "default",
) -> Optional[ef.EmbeddingFunction]:
    """Return a ChromaDB-compatible embedding function.

    Args:
        provider: Embedding provider to use.
            - "openai": Uses OpenAI text-embedding-ada-002 (requires OPENAI_API_KEY).
            - "default": Uses ChromaDB's built-in default embedding function.

    Returns:
        A ChromaDB EmbeddingFunction instance, or None for ChromaDB default.
    """
    if provider == "openai":
        try:
            return ef.OpenAIEmbeddingFunction(
                model_name="text-embedding-ada-002",
            )
        except Exception:
            logger.warning(
                "Failed to initialize OpenAI embeddings. "
                "Ensure OPENAI_API_KEY is set. Falling back to default."
            )
            return ef.DefaultEmbeddingFunction()

    # Default: use ChromaDB's built-in sentence-transformer embeddings
    return ef.DefaultEmbeddingFunction()
