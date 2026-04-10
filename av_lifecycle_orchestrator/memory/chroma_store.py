"""ChromaDB vector memory store for AV project data.

Provides persistent vector storage for project documents, enabling
similarity search across historical projects for reference and reuse.
"""

import json
import logging
from typing import Optional

import chromadb

from memory.embeddings import get_embedding_function

logger = logging.getLogger(__name__)


class ChromaProjectStore:
    """ChromaDB-backed vector store for AV project data."""

    def __init__(
        self,
        persist_dir: str = "./chroma_data",
        collection_name: str = "av_projects",
    ):
        self.persist_dir = persist_dir
        self.collection_name = collection_name
        self._client = chromadb.PersistentClient(path=persist_dir)
        self._collection = self._get_or_create_collection()

    def _get_or_create_collection(self) -> chromadb.Collection:
        """Get or create the ChromaDB collection for AV projects."""
        embedding_fn = get_embedding_function()
        return self._client.get_or_create_collection(
            name=self.collection_name,
            embedding_function=embedding_fn,
            metadata={"description": "AV project lifecycle documents"},
        )

    def add_project(
        self,
        project_id: str,
        project_data: dict,
        metadata: Optional[dict] = None,
    ) -> None:
        """Add a project to the vector store.

        Args:
            project_id: Unique identifier for the project.
            project_data: Dictionary of project data to store.
            metadata: Optional metadata dict for filtering.
        """
        document = json.dumps(project_data, default=str)
        meta = metadata or {}
        # ChromaDB metadata values must be str, int, float, or bool
        sanitized_meta = {
            k: v for k, v in meta.items()
            if isinstance(v, (str, int, float, bool))
        }
        self._collection.upsert(
            ids=[project_id],
            documents=[document],
            metadatas=[sanitized_meta],
        )
        logger.info(f"Added/updated project '{project_id}' in vector store")

    def search_similar_projects(
        self,
        query: str,
        n_results: int = 5,
    ) -> list[dict]:
        """Query the collection for similar historical projects.

        Args:
            query: Natural-language query describing the project or need.
            n_results: Maximum number of results to return.

        Returns:
            List of dicts with keys 'id', 'document', 'metadata', 'distance'.
        """
        results = self._collection.query(
            query_texts=[query],
            n_results=n_results,
        )
        projects = []
        for i in range(len(results["ids"][0])):
            doc_str = results["documents"][0][i]
            try:
                doc = json.loads(doc_str)
            except (json.JSONDecodeError, TypeError):
                doc = doc_str
            projects.append({
                "id": results["ids"][0][i],
                "document": doc,
                "metadata": results["metadatas"][0][i] if results["metadatas"] else {},
                "distance": results["distances"][0][i] if results["distances"] else None,
            })
        return projects

    def get_project(self, project_id: str) -> Optional[dict]:
        """Retrieve a specific project by ID.

        Args:
            project_id: The project identifier.

        Returns:
            Parsed project data dict, or None if not found.
        """
        try:
            result = self._collection.get(ids=[project_id])
            if not result["ids"]:
                return None
            doc_str = result["documents"][0]
            try:
                return json.loads(doc_str)
            except (json.JSONDecodeError, TypeError):
                return {"raw": doc_str}
        except Exception:
            logger.warning(f"Project '{project_id}' not found in vector store")
            return None

    def delete_project(self, project_id: str) -> None:
        """Remove a project from the collection.

        Args:
            project_id: The project identifier to delete.
        """
        try:
            self._collection.delete(ids=[project_id])
            logger.info(f"Deleted project '{project_id}' from vector store")
        except Exception:
            logger.warning(f"Could not delete project '{project_id}' — may not exist")
