# Modèle de sécurité

## Frontière Web / application

Les pages internes sont des QWidget dans un conteneur d’onglet, à côté de QWebEngineView. Aucun QWebChannel ni objet C++ privilégié n’est attaché aux pages distantes. Le routage interne n’est déclenché que par les contrôles natifs. `BrowserPage::acceptNavigationRequest` refuse les protocoles locaux/privilégiés et les navigations Web contenant un identifiant/mot de passe dans l’URL. `about:blank` est admis comme document vide ; il ne possède aucun pont natif.

HTTPS est choisi pour une adresse sans protocole. Une adresse HTTP explicite reste possible, notamment sur un intranet. Le refus des certificats invalides et le sandbox de WebEngine restent actifs. Les fenêtres automatiques sont refusées ; une fenêtre issue d’un geste utilisateur reçoit une page du même profil afin de respecter le fonctionnement normal des sessions.

Le module réseau est natif et ne transmet pas les interfaces à un site. La demande d’IP publique utilise Qt Network vers un fournisseur HTTPS isolé dans NetworkManager, avec redirections non dégradées et délai maximal. Aucun appel n’est réalisé à l’ouverture du navigateur ou de la page réseau ; le bouton indique le fournisseur. Le fournisseur reçoit nécessairement l’IP source de la requête.

## Données persistantes

Les profils normaux utilisent un nom et des répertoires distincts. `ForcePersistentCookies` conserve également les cookies de session. Cette option évite certaines déconnexions mais ne modifie pas la durée de validité imposée par le serveur. Les cookies, données Web, URLs et favoris doivent être considérés comme des données sensibles. La V1 n’ajoute pas de chiffrement aux fichiers de profil : protection par le compte OS et, idéalement, chiffrement du disque. Ne pas partager un dossier de profil entre comptes.

Les valeurs SQLite sont liées à des requêtes préparées. Le schéma est initialisé en transaction et une version inconnue supérieure est refusée. Les titres non fiables sont affichés en texte brut. Les profils normaux sont verrouillés avant la création du profil WebEngine. Le mode invité ne sauvegarde ni onglets ni historique sur disque et le profil WebEngine est off-the-record.

## Gestionnaire de mots de passe : conception prévue, pas implémentée

L’interface indiquera explicitement l’indisponibilité du coffre-fort tant que les adaptateurs ne sont pas validés. Aucun stockage de secours en clair ne sera autorisé.

Architecture prévue :

- `SecureStorage` : `available`, `storeSecret`, `readSecret`, `deleteSecret`, résultats typés (verrouillé, indisponible, refus, absent). Pas de journalisation des secrets.
- Windows : Credential Manager via les API Windows, isolées dans un adaptateur compilé uniquement sous Windows ; clés identifiées par profil et UUID.
- Linux : Secret Service via D-Bus/libsecret, compatible avec le trousseau de la session lorsque disponible. Si aucun service ou trousseau déverrouillé n’est disponible, la fonction reste désactivée avec une explication.
- `PasswordManager` : SQLite conserve uniquement UUID, origine normalisée, nom d’utilisateur et métadonnées. Les secrets restent dans le stockage système. Réconciliation des erreurs/annulations entre les deux stockages.
- Proposition d’enregistrement explicite ; remplissage sur action utilisateur ; vérification stricte de l’origine HTTPS, des redirections, de l’onglet actif et des frames avant tout remplissage. Pas d’API privilégiée persistante injectée dans chaque site.

Une validation dédiée devra couvrir changement d’origine, iframes, trousseau verrouillé, suppression, réutilisation des identifiants et absence de secrets dans fichiers/logs. Les limites de Qt WebEngine pour détecter les formulaires devront être évaluées avant de promettre un gestionnaire généraliste.

## Ancien module SMTP

Le module SMTP est conservé dans le code et les tests, mais il n’est plus accessible dans l’interface. Il n’effectue aucun envoi automatique.

## Partage avec un parc administré

Le partage API remplace le rapport SMTP dans l’interface. Désactivé par défaut, interdit en mode privé, il exige une autorisation dans la page native `minato://sync` et affiche son état en permanence. Le jeton d’appareil est gardé en mémoire seulement, jamais dans WebEngine ni QSettings. TLS vérifié, redirections refusées, file bornée et révocation côté serveur. Le serveur conserve une empreinte du jeton et réserve la lecture aux administrateurs. Les URL, recherches et titres peuvent contenir des informations sensibles malgré le retrait de paramètres de secrets courants. Les rapports SQLite ne sont pas chiffrés par Django ; protéger le volume et les sauvegardes. Voir [le guide serveur](../server/README.md) pour le modèle d’accès, les limites et la purge à planifier.


## Démonstration d’examen locale

`--exam` lance un partage automatique annoncé par un bandeau permanent, fixé à `127.0.0.1:8000`, sans jeton. Django ne dispense de connexion aux rapports qu’en `MINATO_EXAM_MODE=1`, avec contrôle de l’adresse directe loopback sur toutes les requêtes et hôtes locaux autorisés. Le lanceur lie le serveur à loopback et utilise une base distincte. Les API refusent les Origin et imposent un en-tête natif ; aucun CORS n’est activé. Tout processus/utilisateur local peut néanmoins lire ou soumettre des données : ce mode ne fournit pas d’authenticité des preuves d’examen. Aucun envoi en mode privé. Voir [EXAMEN.md](EXAMEN.md).


## Présentation distante éphémère

`MINATO_EPHEMERAL_DEMO=1` est une variante expressément non privée et non persistante : rapports anonymes, fichier SQLite par processus sous `/tmp`, expiration UTC fixe au plus une heure après configuration. Le testeur accepte au lancement `--remote-demo` le partage et l’accès sans connexion aux rapports. Le bandeau et la commande d’arrêt sont conservés. Aucune donnée historique antérieure n’est importée ; le mode privé est exclu. Cette API de démonstration n’authentifie pas les appareils et ses rapports ne constituent pas des preuves inviolables. Vercel peut distribuer les requêtes entre des bases distinctes ; voir [les limites et la procédure de suppression](VERCEL-DEMO.md).
