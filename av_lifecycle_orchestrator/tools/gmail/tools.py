"""
LangChain ``@tool`` wrappers for Gmail operations.
"""

from __future__ import annotations

import logging
import os

from langchain_core.tools import tool

from av_lifecycle_orchestrator.config import get_settings
from av_lifecycle_orchestrator.tools.gmail.client import GmailClient

logger = logging.getLogger(__name__)


def _get_client() -> GmailClient:
    """Create a ``GmailClient`` from application settings."""
    settings = get_settings()
    return GmailClient(
        credentials_json=settings.gmail_credentials_json,
        token_json=settings.gmail_token_json,
        sender_email=settings.gmail_sender_email,
        sender_name=settings.gmail_sender_name,
        mock_mode=settings.mock_mode,
    )


@tool
async def send_deal_registration_email(
    to: str,
    manufacturer: str,
    subject: str,
    body: str,
    attachment_paths: str,
) -> str:
    """Send a deal registration email with optional PDF attachments.

    Args:
        to: Recipient email address.
        manufacturer: Manufacturer the deal registration is for.
        subject: Email subject line.
        body: HTML body of the email.
        attachment_paths: Comma-separated file paths of attachments to
            include (e.g. "/tmp/deal_reg.pdf,/tmp/quote.pdf"). Use an
            empty string for no attachments.

    Returns:
        A confirmation or error message.
    """
    client = _get_client()

    attachments: list[dict] = []
    if attachment_paths.strip():
        for path in attachment_paths.split(","):
            path = path.strip()
            if not os.path.isfile(path):
                return f"Error: attachment file not found: {path}"
            with open(path, "rb") as fh:
                attachments.append(
                    {
                        "filename": os.path.basename(path),
                        "content": fh.read(),
                        "content_type": "application/pdf",
                    }
                )

    try:
        result = await client.send_email(
            to=to,
            subject=subject,
            html_body=body,
            attachments=attachments or None,
        )
        return (
            f"Deal registration email for {manufacturer} sent successfully.\n"
            f"Message ID: {result.get('id')}\n"
            f"Recipient: {to}"
        )
    except Exception as exc:
        logger.exception("Failed to send deal registration email")
        return f"Error sending email: {exc}"


@tool
async def send_notification_email(to: str, subject: str, body: str) -> str:
    """Send a simple notification email (no attachments).

    Args:
        to: Recipient email address.
        subject: Email subject line.
        body: HTML body of the email.

    Returns:
        A confirmation or error message.
    """
    client = _get_client()

    try:
        result = await client.send_email(
            to=to,
            subject=subject,
            html_body=body,
        )
        return (
            f"Notification email sent successfully.\n"
            f"Message ID: {result.get('id')}\n"
            f"Recipient: {to}"
        )
    except Exception as exc:
        logger.exception("Failed to send notification email")
        return f"Error sending email: {exc}"
