from django.core.management.base import BaseCommand, CommandError
from django.db import connection, DatabaseError


class Command(BaseCommand):
    help = "Vérifie la connexion PostgreSQL par SELECT 1, sans afficher les secrets."

    def handle(self, *args, **options):
        if connection.vendor != "postgresql":
            raise CommandError("PostgreSQL non configuré : définissez DATABASE_URL.")
        try:
            with connection.cursor() as cursor:
                cursor.execute("SELECT 1")
                if cursor.fetchone() != (1,):
                    raise CommandError("Résultat de vérification inattendu.")
        except DatabaseError:
            raise CommandError("Connexion impossible : vérifiez DATABASE_URL, le mot de passe, SSL et l'accès réseau.") from None
        finally:
            connection.close()
        self.stdout.write(self.style.SUCCESS("Connexion PostgreSQL vérifiée (SELECT 1)."))
