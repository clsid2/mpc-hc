"""Tests for MIME message construction utilities."""

import base64
import email
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.gmail.mime_builder import build_mime_message


class TestBuildSimpleEmail:
    """Message without attachments."""

    def test_build_simple_email(self):
        result = build_mime_message(
            to="recipient@example.com",
            subject="Test Subject",
            html_body="<p>Hello, World!</p>",
            sender_email="sender@example.com",
            sender_name="Test Sender",
        )
        assert "raw" in result
        # Decode the raw message and verify it's valid email
        raw_bytes = base64.urlsafe_b64decode(result["raw"])
        msg = email.message_from_bytes(raw_bytes)
        assert msg["To"] == "recipient@example.com"
        assert msg["Subject"] == "Test Subject"
        assert "Test Sender" in msg["From"]
        assert "sender@example.com" in msg["From"]


class TestBuildEmailWithPdfAttachment:
    """Message with PDF bytes."""

    def test_build_email_with_pdf_attachment(self):
        pdf_content = b"%PDF-1.4 fake pdf content for testing"
        result = build_mime_message(
            to="recipient@example.com",
            subject="Report Attached",
            html_body="<p>See attached report.</p>",
            sender_email="sender@example.com",
            sender_name="AV System",
            attachments=[
                {
                    "filename": "report.pdf",
                    "content": pdf_content,
                    "content_type": "application/pdf",
                }
            ],
        )
        assert "raw" in result
        raw_bytes = base64.urlsafe_b64decode(result["raw"])
        msg = email.message_from_bytes(raw_bytes)
        # Should be multipart
        assert msg.is_multipart()
        # Should have at least 2 parts (body + attachment)
        parts = list(msg.walk())
        filenames = [
            p.get_filename() for p in parts if p.get_filename() is not None
        ]
        assert "report.pdf" in filenames


class TestBase64urlEncoding:
    """Raw field is valid base64url."""

    def test_base64url_encoding(self):
        result = build_mime_message(
            to="test@example.com",
            subject="Encoding Test",
            html_body="<p>Test</p>",
            sender_email="sender@example.com",
            sender_name="Sender",
        )
        raw = result["raw"]
        # base64url should not contain + or / (only - and _)
        assert "+" not in raw
        assert "/" not in raw
        # Should be decodable without errors
        decoded = base64.urlsafe_b64decode(raw)
        assert len(decoded) > 0


class TestMimeHeaders:
    """Correct To, Subject headers."""

    def test_mime_headers(self):
        result = build_mime_message(
            to="jane@acme.com",
            subject="AV System Proposal",
            html_body="<p>Proposal details</p>",
            sender_email="system@integrator.com",
            sender_name="ProAV System",
        )
        raw_bytes = base64.urlsafe_b64decode(result["raw"])
        msg = email.message_from_bytes(raw_bytes)
        assert msg["To"] == "jane@acme.com"
        assert msg["Subject"] == "AV System Proposal"
        assert msg["From"] == "ProAV System <system@integrator.com>"
