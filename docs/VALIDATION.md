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
