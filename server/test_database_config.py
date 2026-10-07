"""Run with MINATO_DEBUG=1 python -m unittest test_database_config."""
import os
import unittest
from unittest.mock import patch

os.environ.setdefault("MINATO_DEBUG", "1")
from config.settings import database_from_url
from django.core.exceptions import ImproperlyConfigured


class DatabaseConfigTests(unittest.TestCase):
    @patch.dict(os.environ, {}, clear=True)
    def test_transaction_pool_and_encoded_password(self):
        config = database_from_url("postgresql://postgres.ref:p%40ss%23word@pooler.example:6543/postgres")
        self.assertEqual(config["PASSWORD"], "p@ss#word")
        self.assertTrue(config["DISABLE_SERVER_SIDE_CURSORS"])
        self.assertIsNone(config["OPTIONS"]["prepare_threshold"])
        self.assertEqual(config["CONN_MAX_AGE"], 0)
        self.assertEqual(config["OPTIONS"]["sslmode"], "require")

    @patch.dict(os.environ, {}, clear=True)
    def test_session_pool_and_certificate(self):
        config = database_from_url("postgresql://user:pass@pooler.example:5432/postgres?sslmode=verify-full&sslrootcert=%2Ftmp%2Fca.pem")
        self.assertFalse(config["DISABLE_SERVER_SIDE_CURSORS"])
        self.assertNotIn("prepare_threshold", config["OPTIONS"])
        self.assertEqual(config["OPTIONS"]["sslmode"], "verify-full")
        self.assertEqual(config["OPTIONS"]["sslrootcert"], "/tmp/ca.pem")

    @patch.dict(os.environ, {"MINATO_DB_TRANSACTION_POOL": "1", "MINATO_DB_SSLMODE": "require"}, clear=True)
    def test_explicit_overrides(self):
        config = database_from_url("postgresql://user:pass@pooler.example:5432/postgres?sslmode=prefer")
        self.assertTrue(config["DISABLE_SERVER_SIDE_CURSORS"])
        self.assertEqual(config["OPTIONS"]["sslmode"], "require")

    def test_invalid_urls_do_not_leak_credentials(self):
        for uri in ("https://user:secret@host/db", "postgresql://user:secret@host:bad/db",
                    "postgresql:///db", "postgresql://user:secret@host/db#fragment"):
            with self.subTest(uri=uri), self.assertRaises(ImproperlyConfigured) as error:
                database_from_url(uri)
            self.assertNotIn("secret", str(error.exception))
