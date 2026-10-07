# Validation V1 — 29 septembre 2026

## Environnement effectivement testé

Fedora 44 x86_64, GCC 16.2.1, Qt 6.11.2, CMake, build Debug. Tests graphiques sous Xvfb avec le sandbox Chromium actif. Le confinement de l’outil de commande empêchait le premier démarrage graphique ; les tests ont été exécutés hors de ce confinement, sans désactiver la sécurité du navigateur.

## Résultats

- Configuration CMake et compilation de `minato`, `core_tests`, `browser_tests` réussies ; aucun avertissement du compilateur dans la dernière compilation.
- Suite CoreTests : 17 résultats réussis (incluant initialisation et nettoyage), aucun échec.
- Suite BrowserTests : 7 résultats réussis (incluant initialisation et nettoyage), aucun échec.
- Installation locale dans `dist/` réussie ; l’exécutable installé répond `Minato 0.1.0` à `--version`.
- Captures réelles de l’accueil et du réseau inspectées. La capture d’accueil est incluse ; les identifiants réseau de la machine ne sont pas ajoutés à la documentation.

## Cas couverts

| Domaine | Vérifications automatisées |
| --- | --- |
| Adresse | Domaines, HTTP explicite, IPv6, recherches encodées, pages internes, refus file/data/javascript/FTP et URL avec identifiants |
| SQLite | Création, visites cumulées, favoris/modifications/suppression, requêtes préparées, recherche, persistance, isolation, dossier inaccessible |
| Navigation réelle | Chargement HTTP via un serveur local, titre, historique, précédent, refus de navigation distante vers une page privilégiée |
| Onglets et interface | Création, fermeture Ctrl+W, création Ctrl+T, rendu des pages internes, adresse mise à jour lors du changement d’onglet |
| Profil | Cookies de session, cookies persistants et localStorage conservés après réouverture ; isolation d’un autre profil |
| Redémarrage | Deux processus indépendants écrivent puis relisent un cookie de session et localStorage dans le même profil |
| Réseau | Requête d’IP asynchrone et terminaison en erreur dans le délai prévu face à un serveur TLS silencieux |

Une lecture inutile d’une réponse réseau déjà annulée a produit un avertissement Qt pendant le test de timeout ; elle a été supprimée et le test concerné relancé.

## Limites de cette validation

- Pas de machine Windows ou Ubuntu utilisée ici. La portabilité est préparée dans le code/CMake et les procédures sont documentées ; elle reste à confirmer par compilation native.
- Pas de connexion à des comptes personnels ni de test OAuth/DRM ou de codec propriétaire.
- Pas de test automatisé du dialogue de téléchargement, de la réussite du fournisseur public ipify, ou de l’ensemble des erreurs DNS/TLS possibles.
- La persistance testée ne garantit pas qu’un site maintienne une session : le serveur garde le contrôle de l’expiration.
- La suppression complète de toutes les données de sites et le coffre-fort de mots de passe sont hors V1.

## Reproduire

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
./build/core_tests -platform offscreen
MINATO_SCREENSHOT_DIR=/tmp/minato-captures xvfb-run -a ./build/browser_tests
cmake --install build --prefix dist
QT_QPA_PLATFORM=offscreen ./dist/bin/minato --version
```

Créer `/tmp/minato-captures` avant la commande si des captures sont souhaitées. Les tests Web utilisent des profils temporaires, distincts du profil Personnel.

## Ajout du rapport SMTP

Compilation du client SMTP et du dialogue de rapport réussie sous Fedora avec Qt 6.11.2.

- `ctest --test-dir build -R '^(core|reports|smtp)$' --output-on-failure` : trois suites réussies.
- Serveur SMTP simulé sur loopback : TLS direct et STARTTLS réussis ; certificat non approuvé, absence de STARTTLS et authentification refusée traités ; annulation et refus d’injection dans l’expéditeur vérifiés.
- Aperçu : l’historique apparaît seulement après sélection de sa case ; le mot de passe est masqué, absent du rapport et absent des préférences persistantes.
- Test graphique `browser_tests reportPreviewIsExplicit` exécuté sous Xvfb.

Aucun e-mail réel n’a été expédié et aucun compte SMTP réel n’a été utilisé. La livraison via le fournisseur de l’utilisateur et la nouvelle compilation Windows restent à valider sur sa machine.

## Mode examen local (2 octobre 2026)

- Compilation Qt/C++ réussie sous Fedora.
- 19 tests Django réussis : accès anonyme local aux rapports, réception sans jeton, affichage IP/MAC, refus des clients distants et des requêtes web avec Origin, maintien des protections du mode administré.
- Suites CTest `core` et `sync` réussies.
- `server/test_exam_integration.py` réussi : véritable client C++ → API Django sur 127.0.0.1:8000 → SQLite temporaire → rapport HTTP sans connexion contenant site, IP et MAC exposées.
- Le lanceur Windows est fourni et empaqueté ; son exécution native reste à vérifier sur Windows.
- Contrôle Qt sous Xvfb/X11 réussi : bandeau permanent, aucun champ de jeton ni bouton de configuration en mode examen, mode privé exclu (4 résultats QtTest réussis).

## Démonstration distante SQLite éphémère

- Compilation C++ réussie sous Linux.
- 23 tests Django réussis, dont rapports anonymes HTTPS, refus d’Origin, expiration et suppression logique à la requête suivante.
- Suites CTest `core` et `sync` réussies ; consentement et HTTPS obligatoires pour le client distant.
- `test_remote_demo.py` réussi : vrai client Qt → serveur HTTPS local avec certificat de test → SQLite temporaire → rapport anonyme. Les données de test et le serveur sont supprimés à la fin.
- Les réglages Vercel et la collecte des fichiers statiques ont été validés localement. Aucun déploiement réel Vercel effectué ; l’affinité d’instance n’est pas garantie et ne peut pas être validée par le seul test local.
