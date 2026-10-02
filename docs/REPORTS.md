> **Document historique :** l’envoi SMTP n’est plus accessible dans l’interface. Utiliser [le partage avec le serveur Django](SYNC.md). Le code SMTP est conservé pour compatibilité et tests, sans envoi automatique.

# Rapport réseau et historique par e-mail

Ouvrir `minato://network`, puis **Rapport par e-mail**. Le destinataire est `jkatemskatema@gmail.com`.

1. Si l’IP publique doit figurer dans le rapport, cliquer d’abord sur **Consulter mon IP publique**. Sans cette consultation, le rapport indique `null` pour l’IP publique.
2. Vérifier l’aperçu : interfaces, type, état, IPv4/IPv6 et MAC réellement exposée par le système.
3. Cocher **Inclure l’historique** pour ajouter les 1 000 dernières entrées du profil (URL complète, titre, date UTC, nombre de visites). Les URL peuvent contenir des informations sensibles. Aucun historique d’un autre profil n’est inclus.
4. Renseigner le serveur SMTP, le port, le chiffrement, l’expéditeur, l’identifiant et le mot de passe d’application.
5. Cliquer **Envoyer**, puis confirmer le destinataire et l’inclusion éventuelle de l’historique.

L’aperçu correspond exactement au document JSON envoyé comme texte UTF-8 dans le corps du message. Ni cookies, ni mots de passe de sites, ni mot de passe SMTP ne font partie du rapport. Il n’y a aucun envoi au démarrage ou en arrière-plan, et aucun bouton SMTP n’est exposé aux pages Web distantes.

## Configuration comparable aux paramètres Django

| Paramètre Django | Champ Minato |
| --- | --- |
| `EMAIL_HOST` | Serveur SMTP |
| `EMAIL_PORT` | Port |
| `EMAIL_HOST_USER` | Identifiant SMTP |
| `EMAIL_HOST_PASSWORD` | Mot de passe SMTP, saisi à chaque envoi |
| `EMAIL_USE_SSL=True` | TLS direct, généralement 465 |
| `EMAIL_USE_TLS=True` | STARTTLS obligatoire, généralement 587 |
| `DEFAULT_FROM_EMAIL` | Adresse de l’expéditeur |

Un serveur Django supplémentaire n’est pas nécessaire : Minato est directement client du serveur SMTP. Les paramètres non secrets sont conservés dans le profil normal. Le mot de passe n’est écrit ni dans QSettings, ni dans SQLite, ni dans un journal. En mode invité, les paramètres ne sont pas enregistrés.

Exemple Gmail : `smtp.gmail.com`, port `587`, **STARTTLS**, expéditeur et identifiant correspondant au compte qui envoie, avec un mot de passe d’application lorsque le compte permet cette méthode. Le compte expéditeur peut être distinct du destinataire. La disponibilité des mots de passe d’application dépend du compte et de sa configuration. Voir [les paramètres officiels Gmail](https://support.google.com/mail/answer/7104828?hl=fr) et [les mots de passe d’application](https://support.google.com/mail/answer/185833?hl=fr).

## Sécurité et limites

- TLS est obligatoire et les certificats sont vérifiés. L’authentification ne commence jamais sur une connexion non chiffrée.
- Les mécanismes pris en charge sont AUTH LOGIN et AUTH PLAIN sous TLS. OAuth2 et les relais sans authentification ne sont pas pris en charge.
- Les réponses SMTP et les identifiants ne sont pas journalisés. L’utilisateur voit les codes de refus, sans le texte arbitraire du serveur.
- Délai de 30 secondes par échange et 60 secondes pour la confirmation après transfert ; annulation depuis le dialogue.
- Taille maximale du rapport : 4 Mo. Une MAC absente n’est jamais inventée.
- Une confirmation SMTP signifie que le serveur a accepté le message, pas qu’il est arrivé dans la boîte de réception. Une coupure après transfert peut rendre le résultat incertain : vérifier la messagerie avant de réessayer.
- Le transport SMTP utilise TLS ; le message n’est pas chiffré de bout en bout. L’expéditeur, son fournisseur et le destinataire peuvent accéder au contenu.
- Aucun identifiant de messagerie n’est embarqué dans le binaire. L’utilisateur renseigne son propre compte au moment de l’envoi.

## Vérification

`report_tests` vérifie le contenu du rapport et l’exclusion de l’historique tant que sa case n’est pas cochée. `smtp_tests` utilise un serveur TLS local fictif, des identifiants synthétiques et un certificat de test, sans aucun relais vers Internet : TLS direct, STARTTLS, certificat non approuvé, refus d’authentification et annulation. Ces tests TLS serveur demandent le backend OpenSSL et sont ignorés s’il n’est pas disponible ; le client de production peut utiliser Schannel sous Windows.

`browser_tests reportPreviewIsExplicit` vérifie l’aperçu natif et le champ de mot de passe. Aucun envoi réel à Gmail n’a été effectué pendant le développement, faute de compte SMTP configuré.
