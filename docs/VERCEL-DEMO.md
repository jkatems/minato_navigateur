# Démonstration Vercel avec SQLite éphémère

Cette variante est volontairement **temporaire et non fiable comme stockage**. Elle convient uniquement à une présentation consentie où la perte d’événements est acceptée. Les rapports sont accessibles sans connexion à toute personne possédant l’URL. Minato demande l’accord explicite du testeur avant l’envoi et affiche un bandeau permanent ; seuls des sites de démonstration devraient être utilisés.

## 1. Déployer Django

Dans Vercel, importer le dépôt et choisir :

- **Root Directory : `server`** (pas la racine C++).
- **Framework Preset : Django**.
- Python : `3.12`, indiqué par `.python-version`.
- Laisser les commandes d’installation et de build automatiques. Ne pas lancer `exam.py`, `runserver` ou `migrate` dans la commande de build.

Vercel détecte `manage.py`, utilise `config/wsgi.py` et collecte les fichiers statiques. Le fichier `server/vercel.json` configure le framework et la durée maximale de la fonction. [Documentation Django sur Vercel](https://vercel.com/docs/frameworks/full-stack/django).

Ajouter ces variables d’environnement au projet Vercel :

| Variable | Valeur |
|---|---|
| `MINATO_EPHEMERAL_DEMO` | `1` |
| `MINATO_EXAM_MODE` | `0` |
| `MINATO_DEBUG` | `0` |
| `MINATO_SECRET_KEY` | Une valeur aléatoire stable, créée ci-dessous |
| `MINATO_DEMO_UNTIL` | L’heure UTC de fin, créée ci-dessous, au plus une heure dans le futur |
| `MINATO_ALLOWED_HOSTS` | Le domaine de production, sans `https://`, par exemple `minato-demo.vercel.app` |

Aucun PostgreSQL, `DATABASE_URL`, compte Django ni jeton d’appareil n’est nécessaire pour ce mode. Le nom de domaine fourni par Vercel dans `VERCEL_URL` et `VERCEL_PROJECT_PRODUCTION_URL` est aussi accepté automatiquement.

Créer la clé sur votre machine (la saisir uniquement dans les variables Vercel, pas dans le code) :

```bash
python3 -c "import secrets; print(secrets.token_urlsafe(64))"
```

Créer l’heure de fin **juste avant de déployer** :

```bash
python3 -c "from datetime import datetime,timedelta,timezone; print((datetime.now(timezone.utc)+timedelta(hours=1)).isoformat())"
```

Sous Windows, remplacer `python3` par `py -3`. L’expiration est une date fixe, commune aux instances, et non un délai qui recommence à chaque redémarrage. Le temps du déploiement est donc inclus dans cette heure. Pour recommencer une démonstration, définir une nouvelle date puis redéployer.

Utiliser l’URL HTTPS de production. Si Vercel renvoie sa propre page d’authentification/Deployment Protection, le client natif ne peut pas la traverser : configurer la protection de ce **projet de démonstration dédié** pour rendre son URL de production accessible aux participants. Les autres projets ne doivent pas être modifiés.

## 2. Mettre l’URL dans Minato

Modifier uniquement `src/sync/DemoConfig.h` :

```cpp
inline constexpr const char *ServerUrl = "https://votre-projet.vercel.app";
```

Mettre l’origine HTTPS seule, **sans `/api/v1/events/`**, sans chemin ni paramètres. Aucun secret ne doit figurer dans ce fichier. Recompiler :

```bash
# Linux, racine du projet
cmake --build build --parallel 2
bash launch-remote-demo.sh
```

Sous Windows : `build-windows.cmd`, puis `launch-remote-demo.cmd` (également inclus dans le paquet distribué). Le lanceur utilise `--remote-demo --profile Demonstration`, un profil distinct. Il ne restaure pas les anciens onglets et n’importe pas l’historique existant.

Au lancement, le testeur doit accepter le message indiquant le serveur, les données transmises et leur accès sans connexion. Un refus n’envoie rien et ferme ce lancement de démonstration. Le mode privé n’envoie rien. La page native `minato://sync` permet d’arrêter l’envoi.

## 3. Présenter

1. Sur votre ordinateur, ouvrir l’URL Vercel de production : les rapports s’affichent sans connexion.
2. Sur l’ordinateur du testeur, lancer Minato avec le lanceur distant et accepter le partage.
3. Consulter un site de démonstration ; le navigateur transmet la visite, les IP locales/MAC exposées et le serveur ajoute l’IP source observée par Vercel. Une recherche validée suivie d’un chargement produit deux événements.
4. Actualiser les rapports sur votre ordinateur.
5. Comparer l’identifiant **instance** dans la barre d’état Minato après l’envoi avec celui affiché en haut des rapports.

**Si les identifiants diffèrent, les deux requêtes ont atteint des bases distinctes.** Vercel ne garantit pas une instance unique ou une affinité entre les deux ordinateurs. Recharger peut parfois atteindre la bonne instance, mais aucun réglage de ce code ne garantit cela. Un acquittement API confirme l’écriture dans la base de l’instance qui a reçu la requête, pas sa visibilité depuis une autre instance. [Limites officielles de SQLite sur Vercel](https://vercel.com/kb/guide/is-sqlite-supported-in-vercel).

## Données temporaires et limites

- Chaque processus crée un SQLite vide sous `/tmp/minato-demo-<identifiant>.sqlite3`, avec migrations au premier accès HTTPS. Aucun fichier SQLite du dépôt n’est envoyé dans le déploiement.
- Aucun partage ni synchronisation entre instances. Un redémarrage peut faire disparaître l’historique. Les premiers accès peuvent être plus lents à cause des migrations.
- Maximum 10 000 événements avant refus des nouveaux lots (un lot concurrent peut faire dépasser légèrement ce seuil). La limite de débit et la validation habituelles restent actives.
- L’API distante de démo exige HTTPS et un en-tête natif ; elle refuse les requêtes avec Origin et ne fournit pas CORS. **Cet en-tête n’est pas une authentification** : un programme tiers qui connaît l’URL peut envoyer de faux événements. Ce système ne constitue pas une preuve d’examen inviolable.
- Pas de cookies, mots de passe de formulaires, fichiers ni contenus HTML transmis. URL/titres/recherches peuvent néanmoins contenir des informations personnelles. Les MAC peuvent être absentes ou aléatoires et l’IP peut être celle d’un VPN/NAT.
- Les rapports portent `no-store` et `noindex` ; cela ne rend pas l’URL privée et ne garantit pas qu’un tiers n’en copie pas le contenu.
- Après `MINATO_DEMO_UNTIL`, lectures et écritures renvoient HTTP 410. Les événements de l’instance déjà initialisée sont supprimés logiquement lors de sa prochaine requête. **Ce n’est pas une tâche planifiée ni une garantie d’effacement physique** des fichiers temporaires ou des journaux du fournisseur.

## Après la présentation

Arrêter Minato, puis **supprimer le déploiement de démonstration dans Vercel**, y compris ses anciennes URLs de preview le cas échéant. Le code ne supprime pas automatiquement votre projet Vercel. Vérifier également vos copies/captures et la politique de logs du fournisseur. Pour revenir au navigateur sans collecte automatique, lancer Minato normalement, sans `--remote-demo` ni `--exam`.

## Vérifications locales

```bash
MINATO_DEBUG=1 server/.venv/bin/python server/manage.py test monitor
cmake --build build --parallel 2
ctest --test-dir build -R '^(core|sync)$' --output-on-failure
server/.venv/bin/python server/test_remote_demo.py
```

Le dernier test crée un serveur HTTPS local avec un certificat de test, utilise le vrai client C++, vérifie le rapport anonyme et les fichiers SQLite temporaires, puis supprime son dossier de test. Il ne déploie rien sur Vercel et ne transmet aucune donnée à un serveur externe.
