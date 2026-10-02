# Démonstration d’examen locale

Ce mode relie automatiquement **Minato Examen** au serveur Django **http://127.0.0.1:8000**. Pas de compte, de jeton à saisir ni de configuration d’envoi. La page d’accueil Django affiche l’historique, le site, l’IP observée, les IP locales et les MAC exposées.

## Windows

1. Compiler la nouvelle version avec `build-windows.cmd`.
2. Sur **la même machine**, lancer `server\start-exam.cmd`. Le script crée l’environnement Python si nécessaire, installe les dépendances et prépare automatiquement une base `server\exam.sqlite3`. Python 3.10–3.14 doit être installé. Laisser ce terminal ouvert.
3. Lancer `launch-exam.cmd` depuis le projet ou le paquet Windows installé. Il exécute `minato.exe --exam --profile Examen` sans réglage dans le navigateur.
4. Consulter un site dans Minato.
5. Ouvrir **http://127.0.0.1:8000** et actualiser la page pour voir les nouveaux événements, sans connexion. Le lien Détails affiche toutes les interfaces associées.

Le serveur écoute exclusivement sur loopback, pas sur le réseau local. `127.0.0.1` désigne toujours la machine sur laquelle Minato est lancé. Pour un serveur sur un autre ordinateur, utiliser le mode parc administré HTTPS documenté dans [le guide serveur](../server/README.md).

## Linux

```bash
# Terminal 1, racine du projet
bash server/start-exam.sh
# Terminal 2, après compilation du navigateur
bash launch-exam.sh
```

Si les dépendances Python sont déjà installées : `server/.venv/bin/python server/exam.py`.

## Fonctionnement

- Le bandeau **EXAMEN LOCAL** informe en permanence de la transmission. La page native `minato://sync` affiche les informations de fonctionnement, sans champs de configuration ni bouton d’activation/désactivation.
- Les événements futurs sont envoyés automatiquement : adresses et recherches validées, puis pages chargées avec succès. Une recherche suivie d’un chargement correspond à deux événements distincts.
- Chaque événement contient la date, le profil, l’URL, le titre et les interfaces (nom, état, type, IPv4, IPv6, MAC disponibles). Les MAC absentes ou masquées restent indisponibles.
- L’**IP observée par Django est 127.0.0.1** dans cette démonstration locale. Les véritables IP locales figurent dans la colonne des interfaces. L’IP publique Internet n’est pas recherchée automatiquement.
- Le profil `Examen` est distinct du profil personnel et ses anciens onglets ne sont pas restaurés automatiquement. L’historique antérieur n’est pas importé.
- Aucun mot de passe de formulaire, cookie, contenu de page ou fichier n’est envoyé. Les fragments et paramètres courants de secrets sont retirés des URL ; une recherche, un titre ou une URL peut néanmoins contenir des données sensibles.
- Le mode privé est toujours exclu. Fermer la session Examen arrête la transmission. Le navigateur normal, sans `--exam`, conserve son fonctionnement avec activation explicite du partage.
- En cas d’arrêt du serveur, le navigateur conserve jusqu’à 200 événements en mémoire et réessaie. La fermeture perd les envois encore en attente. L’état d’envoi est visible dans la barre inférieure.
- Django conserve les rapports dans `exam.sqlite3` entre deux lancements. `MINATO_EXAM_MODE` active uniquement la dérogation locale ; les accès directs non loopback sont rejetés, même si un en-tête prétend être local. L’API n’autorise pas CORS et refuse les requêtes munies d’un Origin ; le client natif utilise un en-tête spécifique, sans secret.
- **Toute personne et tout programme ayant accès à cette machine peuvent consulter les rapports locaux.** Ce mode est une démonstration d’examen, pas un déploiement public et pas une preuve inviolable de l’activité d’un candidat.

## Tests

```bash
MINATO_DEBUG=1 server/.venv/bin/python server/manage.py test monitor
ctest --test-dir build -R '^(core|sync)$' --output-on-failure
# Fermer le serveur local sur le port 8000 avant ce test isolé :
server/.venv/bin/python server/test_exam_integration.py
QT_QPA_PLATFORM=xcb xvfb-run -a ./build/browser_tests examUiWithoutConfiguration
```

Le test d’intégration utilise une base temporaire, le véritable client C++ et le port local 8000. Il vérifie les sites, l’IP et les MAC dans la page Django sans session de connexion puis supprime les données temporaires.
