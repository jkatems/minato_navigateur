# Déploiement Vercel — Minato

Le mode local reste inchangé :

```bash
python exam.py
```

En production Vercel, l'application utilise PostgreSQL via `DATABASE_URL`.

Pour Supabase, suivre [SUPABASE.md](SUPABASE.md) : configuration interactive,
génération de `MINATO_SECRET_KEY`, pooler transactionnel et test de connexion.

## Variables Vercel obligatoires

- `MINATO_SECRET_KEY` : clé Django longue et aléatoire (50+ caractères).
- `DATABASE_URL` : URL PostgreSQL complète, par exemple `postgresql://user:password@host:5432/dbname`.

Optionnelles :

- `MINATO_ALLOWED_HOSTS` : votre domaine personnalisé, si vous en utilisez un.
- `MINATO_CSRF_ORIGINS` : origine HTTPS du domaine personnalisé.
- `MINATO_RETENTION_DAYS` : 30 par défaut.
- `MINATO_DB_SSLMODE` : `require` par défaut.

Ne définissez PAS `MINATO_EXAM_MODE=1` sur Vercel.
Ne définissez `MINATO_EPHEMERAL_DEMO=1` que si vous voulez explicitement l'ancien mode de démonstration temporaire.

## Avant / après le premier déploiement

Les migrations doivent être exécutées contre la même `DATABASE_URL` :

```bash
python manage.py migrate
python manage.py createsuperuser
```

Vous pouvez exécuter ces commandes localement en définissant `DATABASE_URL` avec la même URL PostgreSQL que Vercel.

Ensuite, dans l'interface Django, créez un appareil et récupérez son jeton. Le navigateur distant utilise :

- serveur : `https://<votre-projet>.vercel.app`
- endpoint final : `/api/v1/events/`
- authentification : `Authorization: Bearer <jeton>`

## Vérification locale avant push

```bash
python -m pip install -r requirements.txt
MINATO_DEBUG=1 python manage.py check
python manage.py test monitor
```
