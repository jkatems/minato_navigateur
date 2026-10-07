"""Local-only exam demonstration. No credentials accepted or needed."""
import ipaddress
from functools import wraps
from django.conf import settings
from django.contrib.auth import get_user_model
from django.http import HttpResponseForbidden
from django.views.decorators.cache import never_cache
from .models import Device


def is_local(request):
    try:
        return ipaddress.ip_address(request.META.get("REMOTE_ADDR", "")).is_loopback
    except ValueError:
        return False


class ExamLocalOnlyMiddleware:
    def __init__(self, get_response):
        self.get_response = get_response

    def __call__(self, request):
        # Never trust a forwarded IP: this mode is only for the direct local machine.
        if not is_local(request):
            return HttpResponseForbidden("Mode examen : accès limité à la machine locale.")
        response = self.get_response(request)
        response["Cache-Control"] = "no-store"
        return response


def context(request):
    return {"exam_mode": settings.EXAM_MODE, "ephemeral_demo": settings.EPHEMERAL_DEMO,
            "demo_instance": settings.DEMO_INSTANCE, "demo_until": settings.DEMO_UNTIL}


def local_device():
    user, created = get_user_model().objects.get_or_create(username="minato-exam-local", defaults={"is_active": False})
    if created:
        user.set_unusable_password()
        user.save(update_fields=["password"])
    device, _ = Device.objects.get_or_create(id="4e2c34e0-50e1-4c8a-aa99-401aaef009aa", defaults={
        "name": "Minato · Démonstration" if settings.EPHEMERAL_DEMO else "Minato · Examen local", "created_by": user,
        "token_hash": "exam-local-no-authentication"})
    return device


def report_access(staff_decorator):
    def decorate(view):
        protected = staff_decorator(view)
        @wraps(view)
        @never_cache
        def wrapped(request, *args, **kwargs):
            if settings.EPHEMERAL_DEMO:
                from .demo import expired
                if expired():
                    from django.http import HttpResponseGone
                    return HttpResponseGone("Démonstration terminée.")
                if not request.is_secure():
                    return HttpResponseForbidden("HTTPS requis.")
                return view(request, *args, **kwargs)
            if settings.EXAM_MODE:
                if not is_local(request):
                    return HttpResponseForbidden("Mode examen : accès local uniquement.")
                return view(request, *args, **kwargs)
            return protected(request, *args, **kwargs)
        return wrapped
    return decorate
