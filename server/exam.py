"""Launch the self-contained local exam server; no account or token setup."""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
os.environ["MINATO_EXAM_MODE"] = "1"
os.environ["MINATO_DEBUG"] = "1"
os.environ.setdefault("MINATO_DATABASE", str(ROOT / "exam.sqlite3"))
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
import django
django.setup()
from django.core.management import call_command
call_command("migrate", interactive=False, verbosity=0)
print("Minato Examen : http://127.0.0.1:8000 — rapports locaux sans connexion", flush=True)
call_command("runserver", "127.0.0.1:8000", use_reloader=False)
