# Périmètre progressif

## V1

Navigation et onglets Qt WebEngine, profil persistant, accueil natif, réseau local/IP publique volontaire, SQLite historique/favoris, paramètres et suivi des téléchargements de session. Compiler et valider ce socle avant les fonctions suivantes.

## Après validation de la V1

1. Gestion graphique des profils et création de fenêtres privées depuis le menu ; Bookmarks/History managers dédiés si les besoins dépassent la façade Store actuelle.
2. Effacement complet des données Web avec fermeture contrôlée des pages, traitement asynchrone et retour utilisateur fiable.
3. Historique persistant des téléchargements, reprise lorsque le serveur le permet, page intégrée multi-vues.
4. Adaptateurs SecureStorage Windows/Linux et tests de trousseau, puis PasswordManager. Aucun secret en SQLite.
5. Tests natifs Windows, Ubuntu et Fedora ; validation DNS/TLS, connexion OAuth, stockage Web, téléchargement et fermeture en charge.
6. Paquets installables signés, licences et notices embarquées, stratégie de mise à jour de WebEngine.

L’ajout de modules avancés ne doit pas masquer les limitations de la V1 dans l’interface ou le README.
