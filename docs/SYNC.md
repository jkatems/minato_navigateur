# Synchronisation avec le parc administré

Le rapport SMTP a été remplacé dans l’interface par **minato://sync**. Le serveur est maintenant une application Django monolithe (connexion, rapports, appareils et API), située dans `server/`.

Voir le [guide complet du serveur](../server/README.md) pour le lancement Windows/Linux, l’association du navigateur, les données partagées, les limites de la V1 et le déploiement HTTPS.

Le partage exige une activation informée et reste visible dans la barre d’état. Le mode privé est exclu. Le jeton n’est pas conservé sur disque : l’activation se renouvelle à chaque lancement. La version actuelle partage les nouveaux événements, sans importer automatiquement l’historique antérieur. Les mots de passe, cookies et contenus des pages ne sont pas envoyés.
