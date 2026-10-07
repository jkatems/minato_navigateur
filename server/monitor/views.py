import hashlib
import json
from datetime import timedelta
from functools import wraps
from django.conf import settings
from django.contrib import messages
from django.contrib.auth.decorators import login_required
from django.contrib.auth.forms import AuthenticationForm
from django.contrib.auth.views import LoginView
from django.core.exceptions import PermissionDenied, RequestDataTooBig
from django.core.paginator import Paginator
from django.db import transaction
from django.db.models import Q
from django.http import JsonResponse, HttpResponse
from django.shortcuts import get_object_or_404, redirect, render
from django.utils import timezone
from django.utils.decorators import method_decorator
from django.views.decorators.cache import never_cache
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.debug import sensitive_post_parameters, sensitive_variables
from django.views.decorators.http import require_POST
from .models import Device, Event, LoginThrottle
from .exam import report_access, is_local, local_device
from .validation import validate_event, valid_ip

def peer_ip(request):
    key = "HTTP_X_REAL_IP" if settings.TRUST_PROXY_IP else "REMOTE_ADDR"
    return valid_ip(request.META.get(key, ""))

class StaffAuthenticationForm(AuthenticationForm):
    def confirm_login_allowed(self, user):
        super().confirm_login_allowed(user)
        if not user.is_staff:
            raise self.get_invalid_login_error()

class AdminLoginView(LoginView):
    template_name = "registration/login.html"
    authentication_form = StaffAuthenticationForm

    @method_decorator(sensitive_post_parameters())
    def post(self, request, *args, **kwargs):
        now = timezone.now()
        key = hashlib.sha256((peer_ip(request) or "unknown").encode()).hexdigest()
        with transaction.atomic():
            attempt, _ = LoginThrottle.objects.select_for_update().get_or_create(key=key, defaults={"window_start": now})
            if now - attempt.window_start > timedelta(minutes=15):
                attempt.attempts = 0
                attempt.window_start = now
            if attempt.attempts >= 10:
                return HttpResponse("Trop de tentatives. Réessayez dans 15 minutes.", status=429)
            attempt.attempts += 1
            attempt.save()
        response = super().post(request, *args, **kwargs)
        if response.status_code == 302:
            LoginThrottle.objects.filter(key=key).delete()
        return response

def staff_only(view):
    @wraps(view)
    @login_required
    @never_cache
    def wrapped(request, *args, **kwargs):
        if not request.user.is_active or not request.user.is_staff:
            raise PermissionDenied
        return view(request, *args, **kwargs)
    return wrapped

@report_access(staff_only)
def dashboard(request):
    events = Event.objects.select_related("device")
    q = request.GET.get("q", "")[:200]
    device = request.GET.get("device", "")
    kind = request.GET.get("kind", "")
    if q:
        events = events.filter(Q(url__icontains=q) | Q(title__icontains=q) | Q(query__icontains=q) | Q(peer_ip__icontains=q))
    if device:
        # Only values present in the permitted device list can become a UUID filter.
        known = {str(d.id): d.id for d in Device.objects.all()}
        events = events.filter(device_id=known[device]) if device in known else events.none()
    if kind:
        events = events.filter(kind=kind)
    return render(request, "monitor/dashboard.html", {"page": Paginator(events, 50).get_page(request.GET.get("page")),
        "devices": Device.objects.order_by("name"), "q": q, "selected_device": device, "selected_kind": kind,
        "total": Event.objects.count(), "device_count": Device.objects.filter(active=True).count(), "retention": settings.RETENTION_DAYS})

@staff_only
@sensitive_variables("token")
def devices(request):
    token = None
    if request.method == "POST":
        name = request.POST.get("name", "").strip()
        if not 1 <= len(name) <= 120:
            messages.error(request, "Le nom doit contenir entre 1 et 120 caractères.")
        else:
            _, token = Device.issue(name, request.user)
    return render(request, "monitor/devices.html", {"devices": Device.objects.order_by("name"), "token": token})

@staff_only
@require_POST
def revoke(request, pk):
    Device.objects.filter(pk=pk).update(active=False)
    messages.success(request, "Jeton révoqué. Les nouveaux envois seront refusés.")
    return redirect("devices")

@staff_only
@require_POST
def erase(request, pk):
    device = get_object_or_404(Device, pk=pk)
    if request.POST.get("confirm") != "erase":
        return HttpResponse("Confirmation requise", status=400)
    device.events.all().delete()
    messages.success(request, "Rapports de cet appareil effacés. Révoquez aussi son jeton pour arrêter les futurs envois.")
    return redirect("devices")

@report_access(staff_only)
def event_detail(request, pk):
    return render(request, "monitor/event.html", {"event": get_object_or_404(Event.objects.select_related("device"), pk=pk)})

@csrf_exempt
@require_POST
@sensitive_variables()
def ingest(request):
    if settings.EPHEMERAL_DEMO:
        from .demo import expired
        if expired():
            return JsonResponse({"error": "demo_expired"}, status=410)
        if not request.is_secure() or request.headers.get("Origin") or request.headers.get("X-Minato-Demo") != "1":
            return JsonResponse({"error": "demo_native_https_required"}, status=403)
        device = local_device()
        if Event.objects.count() >= 10000:
            return JsonResponse({"error": "demo_capacity_reached"}, status=429)
    elif settings.EXAM_MODE:
        # Native client only. Cross-origin web pages cannot send this custom header
        # without a preflight, which this API does not permit.
        if not is_local(request) or request.headers.get("Origin") or request.headers.get("X-Minato-Exam") != "1":
            return JsonResponse({"error": "local_exam_client_required"}, status=403)
        device = local_device()
    else:
        # This endpoint never uses session/cookie authentication: only a device write token.
        if not request.is_secure() and not (settings.DEBUG and request.get_host().split(":")[0] in ("localhost", "127.0.0.1")):
            return JsonResponse({"error": "https_required"}, status=403)
        authorization = request.headers.get("Authorization", "")
        if not authorization.startswith("Bearer ") or not 32 <= len(authorization[7:]) <= 128:
            return JsonResponse({"error": "invalid_token"}, status=401)
        digest = hashlib.sha256(authorization[7:].encode()).hexdigest()
        device = Device.objects.filter(token_hash=digest, active=True).first()
        if device is None:
            return JsonResponse({"error": "invalid_token"}, status=401)
    if request.content_type != "application/json":
        return JsonResponse({"error": "json_required"}, status=415)
    try:
        body = request.body
    except RequestDataTooBig:
        return JsonResponse({"error": "payload_too_large"}, status=413)
    if len(body) > 262144:
        return JsonResponse({"error": "payload_too_large"}, status=413)
    try:
        payload = json.loads(body)
        if not isinstance(payload, dict) or set(payload) != {"events"} or not isinstance(payload["events"], list) or not 1 <= len(payload["events"]) <= 20:
            raise ValueError("Lot invalide")
        batch = [validate_event(item) for item in payload["events"]]
    except (ValueError, TypeError, OverflowError, RecursionError):
        return JsonResponse({"error": "invalid_payload"}, status=400)
    address = peer_ip(request)
    with transaction.atomic():
        # Recheck revocation under the same transaction as event insertion.
        device = Device.objects.select_for_update().filter(pk=device.pk, active=True).first()
        if device is None:
            return JsonResponse({"error": "invalid_token"}, status=401)
        recent = device.events.filter(received_at__gte=timezone.now() - timedelta(minutes=1)).count()
        if recent + len(batch) > 240:
            response = JsonResponse({"error": "rate_limited"}, status=429)
            response["Retry-After"] = "60"
            return response
        for event in batch:
            Event.objects.get_or_create(device=device, event_id=event["event_id"], defaults={k:v for k,v in event.items() if k != "event_id"} | {"peer_ip": address})
        device.last_seen = timezone.now()
        device.save(update_fields=["last_seen"])
    return JsonResponse({"accepted": [str(event["event_id"]) for event in batch], "instance": settings.DEMO_INSTANCE})

