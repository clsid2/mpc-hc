"""
MIME message construction utilities for the Gmail API.
"""

from __future__ import annotations

import base64
from email.mime.application import MIMEApplication
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from typing import Any


def build_mime_message(
    to: str,
    subject: str,
    html_body: str,
    sender_email: str,
    sender_name: str,
    attachments: list[dict[str, Any]] | None = None,
) -> dict[str, str]:
    """Build an RFC 2822 MIME message and return it base64url-encoded.

    Parameters
    ----------
    to:
        Recipient email address.
    subject:
        Email subject line.
    html_body:
        HTML content of the email body.
    sender_email:
        Sender's email address.
    sender_name:
        Friendly sender display name.
    attachments:
        Optional list of attachment dicts, each with keys:
        ``filename`` (str), ``content`` (bytes),
        ``content_type`` (str, default ``"application/pdf"``).

    Returns
    -------
    dict
        ``{"raw": "<base64url-encoded-message>"}`` ready for the Gmail API.
    """
    message = MIMEMultipart("mixed")
    message["To"] = to
    message["From"] = f"{sender_name} <{sender_email}>"
    message["Subject"] = subject

    # -- HTML body --
    body_part = MIMEText(html_body, "html", "utf-8")
    message.attach(body_part)

    # -- Attachments --
    for attachment in attachments or []:
        filename = attachment["filename"]
        content: bytes = attachment["content"]
        content_type = attachment.get("content_type", "application/pdf")

        maintype, _, subtype = content_type.partition("/")
        part = MIMEApplication(content, _subtype=subtype or "octet-stream")
        part.add_header(
            "Content-Disposition",
            "attachment",
            filename=filename,
        )
        part.set_type(content_type)
        message.attach(part)

    # -- Encode --
    raw_bytes = message.as_bytes()
    encoded = base64.urlsafe_b64encode(raw_bytes).decode("ascii")

    return {"raw": encoded}
