"""
Async Gmail API client using Google OAuth 2.0.
"""

from __future__ import annotations

import asyncio
import logging
import os
from functools import partial
from typing import Any

from av_lifecycle_orchestrator.tools.gmail.mime_builder import build_mime_message

logger = logging.getLogger(__name__)


class GmailClient:
    """Send emails through the Gmail API with OAuth 2.0 authentication.

    Parameters
    ----------
    credentials_json:
        Path to the Google OAuth ``credentials.json`` file.
    token_json:
        Path to the cached OAuth token file.
    sender_email:
        Email address used in the ``From`` header.
    sender_name:
        Friendly display name for the sender.
    mock_mode:
        When ``True``, log the email instead of sending it.
    """

    SCOPES = ["https://www.googleapis.com/auth/gmail.send"]

    def __init__(
        self,
        credentials_json: str,
        token_json: str,
        sender_email: str,
        sender_name: str,
        mock_mode: bool = False,
    ) -> None:
        self.credentials_json = credentials_json
        self.token_json = token_json
        self.sender_email = sender_email
        self.sender_name = sender_name
        self.mock_mode = mock_mode
        self._service: Any = None

    # ------------------------------------------------------------------
    # Authentication
    # ------------------------------------------------------------------

    def _authenticate(self):
        """Handle the OAuth 2.0 flow: load, refresh, or create credentials."""
        from google.auth.transport.requests import Request
        from google.oauth2.credentials import Credentials
        from google_auth_oauthlib.flow import InstalledAppFlow

        creds: Credentials | None = None

        # 1. Try to load an existing token
        if os.path.exists(self.token_json):
            creds = Credentials.from_authorized_user_file(
                self.token_json, self.SCOPES
            )

        # 2. Refresh or run the consent flow
        if not creds or not creds.valid:
            if creds and creds.expired and creds.refresh_token:
                creds.refresh(Request())
            else:
                flow = InstalledAppFlow.from_client_secrets_file(
                    self.credentials_json, self.SCOPES
                )
                creds = flow.run_local_server(port=0)

            # 3. Persist for next run
            with open(self.token_json, "w") as token_file:
                token_file.write(creds.to_json())

        return creds

    def _get_service(self):
        """Return a cached Gmail API service resource."""
        if self._service is None:
            from googleapiclient.discovery import build

            creds = self._authenticate()
            self._service = build("gmail", "v1", credentials=creds)
        return self._service

    # ------------------------------------------------------------------
    # Send email
    # ------------------------------------------------------------------

    async def send_email(
        self,
        to: str,
        subject: str,
        html_body: str,
        attachments: list[dict[str, Any]] | None = None,
    ) -> dict:
        """Send an email via the Gmail API.

        Parameters
        ----------
        to:
            Recipient email address.
        subject:
            Email subject.
        html_body:
            HTML content body.
        attachments:
            Optional list of dicts with keys ``filename`` (str),
            ``content`` (bytes), and optionally ``content_type`` (str,
            default ``"application/pdf"``).

        Returns
        -------
        dict
            Gmail API response containing the message ``id`` and other
            metadata, or a mock confirmation when running in mock mode.
        """
        if self.mock_mode:
            attachment_names = [
                a["filename"] for a in (attachments or [])
            ]
            logger.info(
                "MOCK send_email: to=%s, subject=%s, attachments=%s",
                to,
                subject,
                attachment_names,
            )
            return {
                "id": "mock-message-id-001",
                "threadId": "mock-thread-id-001",
                "labelIds": ["SENT"],
                "status": "sent (mock)",
            }

        mime_message = build_mime_message(
            to=to,
            subject=subject,
            html_body=html_body,
            sender_email=self.sender_email,
            sender_name=self.sender_name,
            attachments=attachments,
        )

        service = self._get_service()

        # Run the synchronous Google API call in a thread pool so we
        # don't block the event loop.
        loop = asyncio.get_running_loop()
        result = await loop.run_in_executor(
            None,
            partial(
                service.users()
                .messages()
                .send(userId="me", body=mime_message)
                .execute
            ),
        )
        logger.info("Email sent: id=%s to=%s", result.get("id"), to)
        return result
