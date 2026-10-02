import os
from pathlib import Path
from django.core.exceptions import ImproperlyConfigured

BASE_DIR = Path(__file__).resolve().parent.parent
EXAM_MODE = os.environ.get("MINATO_EXAM_MODE", "0") == "1"
DEBUG = EXAM_MODE or os.environ.get("MINATO_DEBUG", "0") == "1"
SECRET_KEY = os.environ.get("MINATO_SECRET_KEY", "")
if not SECRET_KEY:
    if not DEBUG:
        raise ImproperlyConfigured("Définissez MINATO_SECRET_KEY, ou lancez exam.py pour la démonstration locale.")
    import secrets
    SECRET_KEY = secrets.token_urlsafe(64)
ALLOWED_HOSTS = ["localhost", "127.0.0.1", "[::1]"] if EXAM_MODE else os.environ.get("MINATO_ALLOWED_HOSTS", "localhost,127.0.0.1,[::1]").split(",")
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
DATABASES = {"default": {"ENGINE": "django.db.backends.sqlite3", "NAME": os.environ.get("MINATO_DATABASE", str(BASE_DIR / "db.sqlite3")),
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
# Only enable with a reverse proxy that strips/replaces the incoming header.
if os.environ.get("MINATO_TRUST_PROXY_HTTPS") == "1":
    SECURE_PROXY_SSL_HEADER = ("HTTP_X_FORWARDED_PROTO", "https")
CSRF_TRUSTED_ORIGINS = [s for s in os.environ.get("MINATO_CSRF_ORIGINS", "").split(",") if s]
# Default is the direct peer. See deployment guide before enabling proxy IP trust.
TRUST_PROXY_IP = os.environ.get("MINATO_TRUST_PROXY_IP") == "1"
RETENTION_DAYS = max(1, int(os.environ.get("MINATO_RETENTION_DAYS", "30")))

if EXAM_MODE:
    MIDDLEWARE.insert(0, "monitor.exam.ExamLocalOnlyMiddleware")
    TRUST_PROXY_IP = False
    SECURE_PROXY_SSL_HEADER = None
TEMPLATES[0]["OPTIONS"]["context_processors"].append("monitor.exam.context")
