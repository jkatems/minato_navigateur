from datetime import timedelta
from django.conf import settings
from django.core.management.base import BaseCommand
from django.utils import timezone
from monitor.models import Event, LoginThrottle

class Command(BaseCommand):
    help = "Supprime les rapports plus anciens que MINATO_RETENTION_DAYS et les anciens compteurs de connexion."
    def handle(self, *args, **kwargs):
        count, _ = Event.objects.filter(received_at__lt=timezone.now() - timedelta(days=settings.RETENTION_DAYS)).delete()
        LoginThrottle.objects.filter(window_start__lt=timezone.now() - timedelta(days=1)).delete()
        self.stdout.write(f"{count} rapport(s) supprimé(s).")
