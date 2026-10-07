# Minato Observatoire

**Connexion PostgreSQL Supabase et génération de la clé Django :** voir [SUPABASE.md](SUPABASE.md).

**Variante de présentation distante éphémère :** voir [VERCEL-DEMO.md](../docs/VERCEL-DEMO.md). Elle utilise un SQLite `/tmp` distinct par processus et des rapports sans connexion, avec expiration. Les instructions PostgreSQL/persistance ci-dessous concernent le mode administré normal.


**Mode examen local sans configuration :** [lancer le navigateur et les rapports Django sans connexion](../docs/EXAMEN.md).


Application Django monolithe : connexion administrateur, rapports HTML rendus côté serveur, SQLite et API JSON pour les profils Minato d’un parc administré. Aucun service de messagerie n’est nécessaire.

## Démarrage local

Python **3.10 à 3.14**, Django 5.2 LTS et Waitress. Les versions réellement testées sont Django 5.2.17 et Python 3.14. Installez les derniers correctifs compatibles avec `requirements.txt` avant déploiement.

### Windows (PowerShell)

Depuis le dossier `server` :

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
$env:MINATO_DEBUG = "1"
.\.venv\Scripts\python.exe manage.py migrate
.\.venv\Scripts\python.exe manage.py createsuperuser
.\.venv\Scripts\python.exe manage.py runserver 127.0.0.1:8000
```

### Linux

Depuis `server` :

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
export MINATO_DEBUG=1
.venv/bin/python manage.py migrate
.venv/bin/python manage.py createsuperuser
.venv/bin/python manage.py runserver 127.0.0.1:8000
```

Sur Ubuntu, installer `python3-venv` si nécessaire. Le serveur de développement écoute seulement sur votre machine. Il n’est pas destiné à Internet. Ouvrir **http://127.0.0.1:8000**.

## Associer un navigateur

1. Se connecter avec le compte administrateur créé ci-dessus.
2. Dans **Appareils**, nommer un appareil/profil et créer son jeton. Le copier immédiatement : il n’est affiché qu’une fois.
3. Recompiler Minato (`build-windows.cmd` sous Windows).
4. Ouvrir **minato://sync** (menu « Partage du parc », ou page Réseau).
5. Saisir `http://127.0.0.1:8000` pour un test sur la même machine ; utiliser `https://votre-serveur.example` pour un serveur distant. Saisir l’origine seule, sans `/api/`.
6. Saisir le jeton, lire l’information de partage, cocher l’autorisation et confirmer l’activation. Le nom du serveur est affiché lors de la confirmation.
7. Effectuer une recherche ou visiter une page. Le tableau de rapports affiche les événements reçus ; **Détails** affiche les interfaces réseau du moment.

Le navigateur mémorise l’adresse du serveur dans les paramètres du profil. Le jeton et l’autorisation restent **limités à la session** : saisir le jeton et autoriser à nouveau après chaque lancement. Aucun secret n’est écrit dans `settings.ini`. Une intégration ultérieure au coffre système pourra permettre une association persistante.

Pour un autre poste du réseau, `127.0.0.1` désigne ce poste et non le serveur Django : configurer un domaine HTTPS avec un certificat reconnu. Le client refuse HTTP distant et les redirections API, et ne contourne pas les erreurs de certificat.

## Données et fonctionnement

| Événement | Déclenchement | Contenu |
|---|---|---|
| Recherche | Validation dans la barre d’adresse ou la nouvelle page | Terme soumis et URL du moteur |
| Adresse saisie | Validation d’une adresse HTTP(S) | URL résolue |
| Page visitée | Chargement Web réussi | URL finale et titre |

Chaque événement contient un UUID, la date UTC déclarée par le navigateur, le nom du profil, les interfaces fournies par Qt (nom, type, état, IPv4, IPv6, MAC lorsqu’exposée). Le serveur ajoute sa date de réception, l’appareil authentifié et l’IP de la connexion. Cette IP peut appartenir à un NAT, VPN ou proxy ; ce n’est pas une preuve d’identité. La MAC n’est ni devinée ni recherchée au-delà de ce qu’expose le système.

- Le partage est désactivé par défaut, visible dans la barre d’état et interruptible. Les sites visités n’ont aucun accès à sa configuration ni à ses jetons.
- L’historique **antérieur à l’activation** n’est pas importé. Une recherche suivie d’un chargement produit deux événements distincts. Les recherches faites directement dans un formulaire de site sont représentées uniquement par leurs pages chargées, sans interception des formulaires.
- Le mode invité/privé est exclu, ainsi que les pages `minato://` et l’origine du serveur de rapports.
- Aucun cookie, mot de passe de formulaire, stockage de site, contenu HTML, fichier téléchargé ou secret du coffre n’est capturé. Les favoris et paramètres restent locaux.
- Les URL avec identifiants sont refusées ; fragments et paramètres courants de secrets (`token`, `password`, `code`, etc.) sont retirés. Ce filtre ne peut pas garantir qu’une URL, un titre ou une recherche ne contient jamais de donnée sensible.
- La file d’attente contient au maximum 200 événements **en mémoire**. À saturation, les nouveaux sont ignorés avec un état visible. Arrêter/fermer le navigateur vide la file ; les événements non reçus sont perdus. Une requête déjà reçue peut avoir été enregistrée même si le client est arrêté ensuite.
- En cas de panne, tentatives espacées jusqu’à 60 secondes, délai réseau de 15 secondes. Les UUID évitent les doublons après une réponse perdue. Un jeton refusé arrête le partage et vide la file.
- Les erreurs de navigation ne créent pas de « page visitée », mais l’adresse/recherche validée a déjà été enregistrée.
- Désactiver l’historique **local** dans les paramètres n’arrête pas le partage du parc : utiliser « Arrêter le partage ».

## Accès et suppression

Tous les comptes Django actifs `is_staff` peuvent consulter **tout le parc de cette instance** ; il n’y a pas de séparation multi-entreprises. Aucun compte public ne peut s’inscrire. `createsuperuser` crée le premier administrateur ; la gestion de nouveaux administrateurs se fait via les commandes Django, pas par une inscription publique.

Chaque jeton permet uniquement l’écriture pour un appareil, jamais la consultation. Empreinte SHA-256 d’un jeton aléatoire de 256 bits en base. **Révoquer** bloque ses prochains envois ; créer un nouvel appareil pour remplacer un jeton perdu. **Effacer les rapports** supprime les données reçues de l’appareil après confirmation. La révocation seule ne supprime pas ses anciens rapports.

L’administrateur doit configurer une purge quotidienne :

```bash
.venv/bin/python manage.py purge_reports
```

La durée par défaut est 30 jours, réglable par `MINATO_RETENTION_DAYS`. Utiliser cron/systemd timer sur Linux ou le Planificateur de tâches sous Windows, avec le même environnement que le serveur. La purge n’est pas automatique sans cette planification ; la politique affichée dans les rapports le précise. Les sauvegardes doivent avoir leur propre durée d’expiration.

## Déploiement HTTPS

1. Définir `MINATO_DEBUG=0`, `MINATO_SECRET_KEY` aléatoire d’au moins 50 caractères, `MINATO_ALLOWED_HOSTS=rapports.example.org` et `MINATO_CSRF_ORIGINS=https://rapports.example.org`. Sans clé, le serveur refuse de démarrer hors développement. Ne pas committer les secrets ; les fichiers `.env` ne sont pas chargés automatiquement.
2. Définir `MINATO_DATABASE` vers un chemin persistant accessible uniquement au compte système de l’application. SQLite convient à un petit parc ; prévoir PostgreSQL et une stratégie de débit pour une grande flotte.
3. Exécuter `migrate`, `collectstatic --noinput` et `check --deploy` avec cet environnement.
4. Démarrer Waitress sur loopback, derrière Nginx/Caddy avec un certificat HTTPS reconnu :

```bash
.venv/bin/waitress-serve --listen=127.0.0.1:8000 --threads=4 --trusted-proxy=127.0.0.1 --trusted-proxy-headers=x-forwarded-proto config.wsgi:application
```

Sous Windows : `.\.venv\Scripts\waitress-serve.exe` avec les mêmes arguments. Installer le processus comme service selon votre système. Exposer uniquement le reverse proxy, jamais le port interne de Waitress.

5. Configurer le proxy pour **remplacer** `X-Forwarded-Proto` par `https` et `X-Real-IP` par l’IP du client direct. Activer alors `MINATO_TRUST_PROXY_HTTPS=1` et `MINATO_TRUST_PROXY_IP=1`. Sans proxy maîtrisé, laisser ces deux options désactivées : un en-tête fourni par le client n’est pas fiable. Derrière plusieurs proxies, définir explicitement les relais de confiance.
6. Servir `/static/` depuis `server/staticfiles/` et limiter le corps des requêtes à 256 Kio. Ajouter une limitation de débit au proxy, notamment sur `/connexion/` et `/api/`. Ne pas journaliser les corps, `Authorization`, cookies ou paramètres de recherche des rapports.
7. Sauvegarder la base et les secrets via un canal protégé, tester la restauration et planifier `purge_reports`. Le stockage SQLite n’est pas chiffré par l’application : utiliser les protections d’accès et le chiffrement du volume du serveur.

Voir [`deploy/nginx.conf.example`](deploy/nginx.conf.example). Les cookies de session sont HttpOnly, Secure en production, SameSite=Lax. CSRF est actif sur les formulaires. L’API est sans CSRF car elle **ignore l’authentification par session** et exige un jeton d’écriture. Les pages de rapports sont non mises en cache. L’authentification est limitée à 10 tentatives/15 minutes par IP observée, à compléter par la limitation du reverse proxy. La V1 n’inclut pas de MFA.

Références : [authentification Django](https://docs.djangoproject.com/en/5.2/topics/auth/default/), [checklist de déploiement](https://docs.djangoproject.com/en/5.2/howto/deployment/checklist/).

## API

`POST /api/v1/events/` avec `Authorization: Bearer <jeton>` et `Content-Type: application/json`. Limites : 20 événements/lot, 256 Kio, 240 événements/minute/appareil. Le client actuel envoie un événement par requête. Les dates doivent être récentes (7 jours maximum, tolérance future de 10 minutes) : synchroniser les horloges des postes.

```json
{
  "events": [{
    "id": "b1c98153-8bde-4d3c-9d0c-506a00c58ab1",
    "kind": "visit",
    "occurred_at": "2026-09-29T12:00:00Z",
    "profile": "Travail",
    "url": "https://example.org/",
    "title": "Example",
    "query": "",
    "interfaces": [],
    "consent_version": 1
  }]
}
```

Réponse 200 : `{"accepted": ["UUID"]}`. L’appareil est identifié par le jeton, jamais par un identifiant fourni dans le JSON. Les données déclarées et la version de consentement ne constituent pas une preuve d’identité ou un audit inviolable. Rejets : 400 validation, 401 jeton, 403 transport, 413 taille, 415 format, 429 débit.

## Vérification

Depuis la racine du dépôt, sous Linux :

```bash
MINATO_DEBUG=1 server/.venv/bin/python server/manage.py test monitor
cmake --build build --parallel 2
ctest --test-dir build -R '^(core|sync)$' --output-on-failure
server/.venv/bin/python server/test_integration.py
xvfb-run -a ./build/browser_tests syncPageAndPrivateMode syncPageNormalProfile
```

`test_integration.py` crée une base temporaire, lance Django sur loopback, exécute le **véritable client C++**, vérifie SQLite et les vues administrateur puis arrête le serveur. Aucun compte de démonstration ni mot de passe par défaut n’est installé dans la base réelle. Les informations réseau du poste de test passent uniquement par loopback vers la base temporaire, supprimée à la fin.
