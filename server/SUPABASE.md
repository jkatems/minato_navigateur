# Connecter Minato à Supabase

Le serveur Django utilise PostgreSQL via `DATABASE_URL`. L'authentification des
administrateurs reste gérée par Django. Aucune clé API Supabase n'est nécessaire.

## Projet Minato créé

- [Tableau de bord](https://supabase.com/dashboard/project/uxoutaopkjopiithbtfg)
- Organisation : `jkatems's` ; région : Paris (`eu-west-3`).
- Identifiant : `uxoutaopkjopiithbtfg` ; coût de création annoncé : 0 $/mois.
- Rôle serveur : `minato_app`, sans privilèges d'administration globale.
- Schéma privé : `minato`, utilisé automatiquement par ce rôle (`search_path`).
  Les rôles `anon`, `authenticated` et `service_role` n'y ont pas accès.
- Secrets générés : `server/.env.supabase`, ignoré par Git et protégé en mode 600.
- Les 16 migrations Django sont appliquées ; les 12 tables sont dans `minato`.
  RLS est activée sans politique publique : seul leur propriétaire `minato_app`
  (le serveur Django) y accède pour l'application. Les droits de consultation
  des administrateurs sont contrôlés par Django.

L'audit Supabase signale uniquement des informations « RLS Enabled No Policy »
sur ces tables privées. Ce refus d'accès aux autres rôles est intentionnel ;
ne pas ajouter de politique publique pour faire disparaître ce message.
[Explication du contrôle](https://supabase.com/docs/guides/database/database-linter?lint=0008_rls_enabled_no_policy).

Le fichier utilise le **Session pooler** IPv4
`aws-1-eu-west-3.pooler.supabase.com:5432`, avec l'utilisateur
`minato_app.uxoutaopkjopiithbtfg`. La connexion Django (`SELECT 1`) est vérifiée.
Ne pas relancer le générateur de secrets : conserver les valeurs existantes.
Le schéma `minato` doit rester absent des schémas exposés par la Data API ; pour ce
projet déjà initialisé, il n'est donc pas nécessaire de désactiver cette API.

Pour charger la configuration dans le terminal qui lance le serveur :

```bash
source server/.env.supabase
server/.venv/bin/python server/manage.py check_database
server/.venv/bin/python server/manage.py migrate
server/.venv/bin/python server/manage.py createsuperuser
```

Définir ensuite `MINATO_ALLOWED_HOSTS` et `MINATO_CSRF_ORIGINS` avec le domaine
HTTPS du serveur. Pour Vercel, transférer les variables dans le projet Vercel ;
le fichier local n'est pas envoyé au déploiement.

Pour retrouver uniquement la clé Django déjà générée dans votre terminal :

```bash
source server/.env.supabase
printf '%s\n' "$MINATO_SECRET_KEY"
```

Ne pas partager cette sortie. Pour un runtime Vercel, utiliser le même hôte et
les mêmes identifiants avec le port **6543** (Transaction pooler) ; conserver
le port **5432** pour les migrations.

## Préparer le projet

Les étapes ci-dessous décrivent la préparation d'un autre projet.

Dans Supabase, ouvrir **Connect** et copier l'URI PostgreSQL :

- Serveur persistant sur IPv4 : **Session pooler**, port 5432.
- Vercel : **Transaction pooler**, port 6543.
- Migrations : connexion directe si IPv6 disponible, sinon Session pooler.

Copier exactement l'hôte et l'utilisateur proposés par Supabase. Utiliser le
mot de passe de la base PostgreSQL, pas le mot de passe du compte Supabase.

Avant les migrations, désactiver la **Data API** dans les paramètres du projet
Supabase dédié à Minato : Django accède directement à PostgreSQL et ses tables
de comptes, sessions et rapports ne doivent pas être accessibles par REST.
Si le projet utilise déjà la Data API pour une autre application, ne pas la
désactiver globalement : prévoir un schéma privé et un rôle dédié avant les
migrations (cette configuration partagée n'est pas automatisée ici).

## Configurer et vérifier

Depuis la racine du dépôt :

```bash
python3 server/configure_supabase.py
source server/.env.supabase
server/.venv/bin/python server/manage.py check_database
```

Le script demande l'URI et le mot de passe en saisie masquée, encode le mot de
passe si l'URI contient `[YOUR-PASSWORD]`, puis génère `MINATO_SECRET_KEY`.
Il crée `server/.env.supabase` avec des permissions privées sur Linux et refuse
d'écraser un fichier existant. Ce fichier n'est pas chargé automatiquement.
`check_database` exécute uniquement `SELECT 1` ; il refuse une configuration SQLite.

Après une connexion réussie, avec une URI directe ou Session pooler :

```bash
server/.venv/bin/python server/manage.py migrate
server/.venv/bin/python server/manage.py createsuperuser
```

Pour Vercel, reporter les valeurs du fichier dans les variables d'environnement
du projet, puis utiliser l'URI Transaction pooler pour `DATABASE_URL` en exécution.
Ne pas inclure `export` ni les guillemets de shell dans les valeurs Vercel.
Redéployer après modification. Les données SQLite existantes ne sont pas copiées
automatiquement dans Supabase.

Le port 6543 active automatiquement la désactivation des requêtes préparées et
des curseurs serveur. `MINATO_DB_TRANSACTION_POOL=1` permet de sélectionner ce
comportement pour un pooler sur un autre port. SSL vaut `require` par défaut ;
`sslmode` et `sslrootcert` dans l'URI sont pris en compte, et `MINATO_DB_SSLMODE`
peut remplacer le mode SSL.

## Générer uniquement la clé secrète Django

```bash
python3 -c 'import secrets; print(secrets.token_urlsafe(64))'
```

Enregistrer le résultat comme `MINATO_SECRET_KEY` côté serveur et conserver la
même valeur entre les déploiements. La changer invalide notamment les sessions.

Si vous cherchez la **clé API secrète Supabase** (`sb_secret_...`), elle se crée
ou se récupère dans **Settings > API Keys** du projet. Le script ci-dessus ne
génère pas une clé API Supabase ; celle-ci n'est pas utilisée par ce serveur.
Ne placer aucun de ces secrets dans le navigateur Minato ni dans Git.

Références : [connexion PostgreSQL Supabase](https://supabase.com/docs/guides/database/connecting-to-postgres),
[clés Supabase](https://supabase.com/docs/guides/getting-started/api-keys),
[curseurs Django et poolers](https://docs.djangoproject.com/en/5.2/ref/databases/#transaction-pooling-and-server-side-cursors).
