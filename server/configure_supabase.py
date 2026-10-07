#!/usr/bin/env python3
"""Create a private shell environment file without displaying credentials."""
import getpass
import os
from pathlib import Path
import secrets
import shlex
from urllib.parse import quote, urlsplit


def main():
    destination = Path(__file__).resolve().parent / ".env.supabase"
    if destination.exists():
        raise SystemExit("server/.env.supabase existe déjà : conservez-le ou renommez-le avant de recommencer.")
    print("Dans Supabase > Connect, copiez l'URI PostgreSQL avec [YOUR-PASSWORD].")
    uri = getpass.getpass("URI (saisie masquée) : ").strip()
    if "[YOUR-PASSWORD]" in uri:
        password = getpass.getpass("Mot de passe PostgreSQL (pas la clé API) : ")
        if not password:
            raise SystemExit("Le mot de passe est obligatoire.")
        uri = uri.replace("[YOUR-PASSWORD]", quote(password, safe=""))
    try:
        parsed = urlsplit(uri)
        port = parsed.port or 5432
        valid = (parsed.scheme in ("postgres", "postgresql") and parsed.hostname
                 and parsed.username and parsed.password and parsed.path.strip("/")
                 and not parsed.fragment and "[" not in uri and "]" not in uri)
    except ValueError:
        valid = False
    if not valid:
        raise SystemExit("URI invalide : utilisez l'URI complète du projet depuis Connect.")
    hostname = input("Domaine HTTPS du serveur (ex. minato.vercel.app) : ").strip()
    if not hostname or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-" for c in hostname):
        raise SystemExit("Saisissez uniquement le nom du domaine, sans https:// ni chemin.")
    values = {
        "DATABASE_URL": uri,
        "MINATO_SECRET_KEY": secrets.token_urlsafe(64),
        "MINATO_DEBUG": "0",
        "MINATO_EXAM_MODE": "0",
        "MINATO_EPHEMERAL_DEMO": "0",
        "MINATO_ALLOWED_HOSTS": hostname,
        "MINATO_CSRF_ORIGINS": "https://" + hostname,
        "MINATO_DB_TRANSACTION_POOL": "1" if port == 6543 else "0",
    }
    fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as stream:
        stream.write("# Secrets serveur uniquement. Charger explicitement dans Bash.\n")
        for name, value in values.items():
            stream.write(f"export {name}={shlex.quote(value)}\n")
    print("Configuration enregistrée dans server/.env.supabase (ignorée par Git).")
    print("Aucune connexion distante ni migration n'a encore été effectuée.")


if __name__ == "__main__":
    main()
