"""Deliberately ephemeral presentation storage. Never used by normal/local modes."""
import threading
from io import StringIO
from django.conf import settings
from django.core.management import call_command
from django.http import HttpResponse
from django.utils import timezone

_ready = False
_lock = threading.Lock()

def expired():
    return settings.DEMO_UNTIL is None or timezone.now() >= settings.DEMO_UNTIL

def ensure_database():
    global _ready
    with _lock:
        if not _ready:
            call_command("migrate", interactive=False, verbosity=0, stdout=StringIO())
            _ready = True

class EphemeralDemoMiddleware:
    def __init__(self, get_response):
        self.get_response = get_response

    def __call__(self, request):
        if expired():
            # Best-effort logical deletion on the next request, not a scheduled job.
            if _ready:
                from .models import Event
                Event.objects.all().delete()
            response = HttpResponse("Démonstration terminée. Supprimez le déploiement Vercel.", status=410)
        elif not request.is_secure():
            response = HttpResponse("HTTPS requis pour cette démonstration.", status=403)
        else:
            ensure_database()
            response = self.get_response(request)
        response["Cache-Control"] = "private, no-store, max-age=0"
        response["Vercel-CDN-Cache-Control"] = "no-store"
        response["X-Robots-Tag"] = "noindex, nofollow, noarchive"
        response["X-Minato-Instance"] = settings.DEMO_INSTANCE
        return response
