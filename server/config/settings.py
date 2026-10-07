import os
from pathlib import Path
from urllib.parse import urlparse, unquote, parse_qs
from django.core.exceptions import ImproperlyConfigured

BASE_DIR = Path(__file__).resolve().parent.parent
IS_VERCEL = os.environ.get("VERCEL") == "1"
EPHEMERAL_DEMO = os.environ.get("MINATO_EPHEMERAL_DEMO", "0") == "1"
EXAM_MODE = os.environ.get("MINATO_EXAM_MODE", "0") == "1"

if EPHEMERAL_DEMO and EXAM_MODE:
    raise ImproperlyConfigured("Choisir soit le mode local, soit la démonstration distante.")
if IS_VERCEL and EXAM_MODE:
    raise ImproperlyConfigured("Le mode examen local ne peut pas être exposé sur Vercel.")

DEBUG = not IS_VERCEL and not EPHEMERAL_DEMO and (EXAM_MODE or os.environ.get("MINATO_DEBUG", "0") == "1")
SECRET_KEY = os.environ.get("MINATO_SECRET_KEY", "")
if not SECRET_KEY:
    if not DEBUG:
        raise ImproperlyConfigured("Définissez MINATO_SECRET_KEY en production, ou lancez exam.py en local.")
    import secrets
    SECRET_KEY = secrets.token_urlsafe(64)

ALLOWED_HOSTS = [h.strip() for h in os.environ.get(
    "MINATO_ALLOWED_HOSTS", "localhost,127.0.0.1,[::1]"
).split(",") if h.strip()]
if EXAM_MODE:
    ALLOWED_HOSTS = ["localhost", "127.0.0.1", "[::1]"]
if IS_VERCEL:
    ALLOWED_HOSTS.extend([".vercel.app"])
    for variable in ("VERCEL_URL", "VERCEL_PROJECT_PRODUCTION_URL"):
        hostname = os.environ.get(variable, "").strip()
        if hostname and "/" not in hostname:
            ALLOWED_HOSTS.append(hostname)
ALLOWED_HOSTS = list(dict.fromkeys(ALLOWED_HOSTS))

INSTALLED_APPS = ["django.contrib.auth", "django.contrib.contenttypes", "django.contrib.sessions",
                  "django.contrib.messages", "django.contrib.staticfiles", "monitor"]
MIDDLEWARE = ["django.middleware.security.SecurityMiddleware", "django.contrib.sessions.middleware.SessionMiddleware",
              "django.middleware.common.CommonMiddleware", "django.middleware.csrf.CsrfViewMiddleware",
              "django.contrib.auth.middleware.AuthenticationMiddleware", "django.contrib.messages.middleware.MessageMiddleware",
              "django.middleware.clickjacking.XFrameOptionsMiddleware"]
ROOT_URLCONF = "config.urls"
TEMPLATES = [{"BACKEND": "django.template.backends.django.DjangoTemplates", "DIRS": [BASE_DIR / "templates"],
              "APP_DIRS": True, "OPTIONS": {"context_processors": ["django.template.context_processors.request",
              "django.contrib.auth.context_processors.auth", "django.contrib.messages.context_processors.messages"]}}]
WSGI_APPLICATION = "config.wsgi.application"

# Local/exam keeps SQLite. Vercel/production uses a persistent PostgreSQL DATABASE_URL.
def database_from_url(value):
    try:
        parsed = urlparse(value)
        port = parsed.port or 5432
    except ValueError:
        raise ImproperlyConfigured("DATABASE_URL contient un hôte ou un port invalide.") from None
    if parsed.scheme not in ("postgres", "postgresql"):
        raise ImproperlyConfigured("DATABASE_URL doit être une URL PostgreSQL (postgresql://...).")
    if not parsed.hostname or not parsed.username or not parsed.path.strip("/") or parsed.fragment:
        raise ImproperlyConfigured("DATABASE_URL doit contenir hôte, utilisateur et base ; encodez les caractères spéciaux du mot de passe.")
    query = parse_qs(parsed.query)
    sslmode = os.environ.get("MINATO_DB_SSLMODE") or query.get("sslmode", ["require"])[-1]
    if sslmode not in ("disable", "allow", "prefer", "require", "verify-ca", "verify-full"):
        raise ImproperlyConfigured("Mode SSL PostgreSQL invalide.")
    # Supabase transaction poolers listen on 6543; allow other poolers explicitly.
    transaction_pool = os.environ.get("MINATO_DB_TRANSACTION_POOL", "1" if port == 6543 else "0") == "1"
    options = {"sslmode": sslmode, "connect_timeout": 10}
    if "sslrootcert" in query:
        options["sslrootcert"] = query["sslrootcert"][-1]
    if transaction_pool:
        options["prepare_threshold"] = None
    return {
        "ENGINE": "django.db.backends.postgresql",
        "NAME": unquote(parsed.path.lstrip("/")),
        "USER": unquote(parsed.username or ""),
        "PASSWORD": unquote(parsed.password or ""),
        "HOST": parsed.hostname or "",
        "PORT": str(port),
        "CONN_MAX_AGE": 0 if IS_VERCEL or transaction_pool else 60,
        "CONN_HEALTH_CHECKS": True,
        "DISABLE_SERVER_SIDE_CURSORS": transaction_pool,
        "OPTIONS": options,
    }

DATABASE_URL = os.environ.get("DATABASE_URL", "").strip()
if DATABASE_URL:
    DATABASES = {"default": database_from_url(DATABASE_URL)}
elif IS_VERCEL:
    raise ImproperlyConfigured("Définissez DATABASE_URL sur Vercel avec une base PostgreSQL persistante.")
else:
    DATABASES = {"default": {"ENGINE": "django.db.backends.sqlite3",
                             "NAME": os.environ.get("MINATO_DATABASE", str(BASE_DIR / "db.sqlite3")),
                             "OPTIONS": {"timeout": 20}}}

AUTH_PASSWORD_VALIDATORS = [{"NAME": "django.contrib.auth.password_validation.UserAttributeSimilarityValidator"},
                            {"NAME": "django.contrib.auth.password_validation.MinimumLengthValidator"},
                            {"NAME": "django.contrib.auth.password_validation.CommonPasswordValidator"},
                            {"NAME": "django.contrib.auth.password_validation.NumericPasswordValidator"}]
LANGUAGE_CODE = "fr-fr"
TIME_ZONE = "UTC"
USE_I18N = True
USE_TZ = True
STATIC_URL = "/static/"
STATICFILES_DIRS = [BASE_DIR / "static"]
STATIC_ROOT = BASE_DIR / "staticfiles"
DEFAULT_AUTO_FIELD = "django.db.models.BigAutoField"
LOGIN_URL = "login"
LOGIN_REDIRECT_URL = "dashboard"
LOGOUT_REDIRECT_URL = "login"
SESSION_COOKIE_HTTPONLY = True
SESSION_COOKIE_SAMESITE = "Lax"
SESSION_COOKIE_AGE = 3600
SESSION_EXPIRE_AT_BROWSER_CLOSE = True
SESSION_COOKIE_SECURE = not DEBUG
CSRF_COOKIE_SECURE = not DEBUG
SECURE_SSL_REDIRECT = not DEBUG
SECURE_HSTS_SECONDS = 31536000 if not DEBUG else 0
SECURE_CONTENT_TYPE_NOSNIFF = True
X_FRAME_OPTIONS = "DENY"
DATA_UPLOAD_MAX_MEMORY_SIZE = 262144

if IS_VERCEL or os.environ.get("MINATO_TRUST_PROXY_HTTPS") == "1":
    SECURE_PROXY_SSL_HEADER = ("HTTP_X_FORWARDED_PROTO", "https")

CSRF_TRUSTED_ORIGINS = [s.strip() for s in os.environ.get("MINATO_CSRF_ORIGINS", "").split(",") if s.strip()]
if IS_VERCEL:
    CSRF_TRUSTED_ORIGINS.append("https://*.vercel.app")
    for variable in ("VERCEL_URL", "VERCEL_PROJECT_PRODUCTION_URL"):
        hostname = os.environ.get(variable, "").strip()
        if hostname and "/" not in hostname:
            CSRF_TRUSTED_ORIGINS.append(f"https://{hostname}")
CSRF_TRUSTED_ORIGINS = list(dict.fromkeys(CSRF_TRUSTED_ORIGINS))

# Vercel is a trusted reverse proxy for transport/IP headers in this deployment.
TRUST_PROXY_IP = IS_VERCEL or os.environ.get("MINATO_TRUST_PROXY_IP") == "1"
RETENTION_DAYS = max(1, int(os.environ.get("MINATO_RETENTION_DAYS", "30")))

if EXAM_MODE:
    MIDDLEWARE.insert(0, "monitor.exam.ExamLocalOnlyMiddleware")
    TRUST_PROXY_IP = False
    SECURE_PROXY_SSL_HEADER = None
TEMPLATES[0]["OPTIONS"]["context_processors"].append("monitor.exam.context")

# Kept only for the optional short-lived demo mode. Normal Vercel deployments use PostgreSQL above.
DEMO_UNTIL = None
DEMO_INSTANCE = ""
if EPHEMERAL_DEMO:
    import tempfile
    import uuid
    from datetime import datetime, timezone, timedelta
    try:
        DEMO_UNTIL = datetime.fromisoformat(os.environ["MINATO_DEMO_UNTIL"].replace("Z", "+00:00"))
        if DEMO_UNTIL.tzinfo is None or DEMO_UNTIL > datetime.now(timezone.utc) + timedelta(hours=1, minutes=1):
            raise ValueError()
    except (KeyError, ValueError):
        raise ImproperlyConfigured("MINATO_DEMO_UNTIL doit être une date UTC explicite, au plus une heure dans le futur.") from None
    DEMO_INSTANCE = uuid.uuid4().hex[:12]
    if not DATABASE_URL:
        DATABASES["default"] = {"ENGINE": "django.db.backends.sqlite3",
                                "NAME": str(Path(tempfile.gettempdir()) / ("minato-demo-" + DEMO_INSTANCE + ".sqlite3")),
                                "OPTIONS": {"timeout": 20}}
    MIDDLEWARE.insert(1, "monitor.demo.EphemeralDemoMiddleware")
