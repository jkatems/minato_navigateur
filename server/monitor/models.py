import hashlib
import secrets
import uuid
from django.db import models
from django.conf import settings

class Device(models.Model):
    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    name = models.CharField(max_length=120)
    token_hash = models.CharField(max_length=64, unique=True)
    active = models.BooleanField(default=True)
    created_at = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(settings.AUTH_USER_MODEL, on_delete=models.PROTECT)
    last_seen = models.DateTimeField(null=True, blank=True)

    @classmethod
    def issue(cls, name, user):
        token = secrets.token_urlsafe(32)
        return cls.objects.create(name=name, created_by=user, token_hash=hashlib.sha256(token.encode()).hexdigest()), token

class Event(models.Model):
    device = models.ForeignKey(Device, on_delete=models.CASCADE, related_name="events")
    event_id = models.UUIDField()
    kind = models.CharField(max_length=16, choices=[("search", "Recherche"), ("address", "Adresse saisie"), ("visit", "Page visitée")])
    occurred_at = models.DateTimeField()
    received_at = models.DateTimeField(auto_now_add=True, db_index=True)
    profile = models.CharField(max_length=120)
    url = models.TextField()
    title = models.CharField(max_length=512, blank=True)
    query = models.CharField(max_length=2048, blank=True)
    peer_ip = models.GenericIPAddressField(null=True)
    interfaces = models.JSONField(default=list)
    consent_version = models.PositiveSmallIntegerField(default=1)

    class Meta:
        ordering = ["-received_at", "-id"]
        constraints = [models.UniqueConstraint(fields=["device", "event_id"], name="unique_device_event")]
        indexes = [models.Index(fields=["device", "received_at"])]

class LoginThrottle(models.Model):
    key = models.CharField(max_length=64, primary_key=True)
    attempts = models.PositiveIntegerField(default=0)
    window_start = models.DateTimeField()
