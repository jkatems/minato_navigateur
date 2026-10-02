import ipaddress
import uuid
from datetime import timedelta
from urllib.parse import urlsplit, urlunsplit, parse_qsl, urlencode
from django.utils import timezone
from django.utils.dateparse import parse_datetime

SENSITIVE_KEYS = {"password", "passwd", "pwd", "token", "access_token", "refresh_token", "id_token", "code", "secret", "api_key", "apikey", "authorization", "session", "sessionid", "auth"}

def safe_url(value):
    if not isinstance(value, str) or not 1 <= len(value) <= 8192:
        raise ValueError("URL invalide")
    parts = urlsplit(value)
    if parts.scheme not in ("http", "https") or not parts.hostname or parts.username or parts.password or any(ord(c) < 32 for c in value):
        raise ValueError("URL invalide")
    _ = parts.port
    query = [(k, v) for k, v in parse_qsl(parts.query, keep_blank_values=True, max_num_fields=100) if k.lower() not in SENSITIVE_KEYS]
    return urlunsplit((parts.scheme, parts.netloc, parts.path, urlencode(query), ""))

def text(obj, key, limit, required=False):
    value = obj.get(key, "")
    if not isinstance(value, str) or len(value) > limit or (required and not value.strip()):
        raise ValueError("Champ invalide : " + key)
    return value

def validate_event(item):
    if not isinstance(item, dict) or set(item) - {"id", "kind", "occurred_at", "profile", "url", "title", "query", "interfaces", "consent_version"}:
        raise ValueError("Événement invalide")
    if item.get("consent_version") != 1 or item.get("kind") not in ("search", "address", "visit"):
        raise ValueError("Consentement ou type invalide")
    identifier = uuid.UUID(text(item, "id", 36, True))
    stamp = parse_datetime(text(item, "occurred_at", 40, True))
    if stamp is None or timezone.is_naive(stamp) or not timezone.now() - timedelta(days=7) <= stamp <= timezone.now() + timedelta(minutes=10):
        raise ValueError("Horodatage invalide")
    interfaces = item.get("interfaces", [])
    if not isinstance(interfaces, list) or len(interfaces) > 64:
        raise ValueError("Interfaces invalides")
    validated = []
    for interface in interfaces:
        if not isinstance(interface, dict) or set(interface) - {"name", "type", "state", "ipv4", "ipv6", "mac"}:
            raise ValueError("Interface invalide")
        validated.append({k: text(interface, k, 2048 if k in ("ipv4", "ipv6") else 256) for k in ("name", "type", "state", "ipv4", "ipv6", "mac")})
    query = text(item, "query", 2048)
    if query and item["kind"] != "search":
        raise ValueError("Recherche inattendue")
    return {"event_id": identifier, "kind": item["kind"], "occurred_at": stamp,
            "profile": text(item, "profile", 120, True), "url": safe_url(item.get("url")),
            "title": text(item, "title", 512), "query": query,
            "interfaces": validated, "consent_version": 1}

def valid_ip(value):
    try:
        return str(ipaddress.ip_address(value))
    except ValueError:
        return None
