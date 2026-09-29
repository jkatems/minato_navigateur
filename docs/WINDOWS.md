# Compiler et distribuer Minato sous Windows

## Option 1 — laisser GitHub compiler

Le dépôt contient `.github/workflows/windows.yml`. Il utilise une machine Windows 2022 x64, installe Qt 6.11.2 et WebEngine, puis exécute le script commun de compilation.

1. Mettre le projet dans un dépôt GitHub avec le workflow sur la branche par défaut.
2. Ouvrir **Actions → Windows distributable → Run workflow**.
3. Attendre la fin du job Windows.
4. Télécharger **Minato-Windows-x64** dans les artefacts de l’exécution.
5. Extraire cet artefact pour récupérer `Minato-0.1.0-windows-x64.zip`, `Minato-0.1.0-windows-x64.exe` et leurs fichiers `.sha256`.

Le ZIP contient le dossier complet du navigateur. L’EXE est l’installateur. Le workflow est également déclenché pour les tags `v*` et les pull requests touchant le code de compilation. Les artefacts restent disponibles 30 jours ; ils ne sont pas publiés automatiquement dans Releases. Les quotas/frais éventuels dépendent du compte GitHub.

La version Qt est fixée pour rendre le build reproductible ; la mettre à jour régulièrement pour bénéficier des correctifs WebEngine. Cette automatisation n’a pas encore été exécutée sur GitHub depuis cet environnement.

## Option 2 — compiler sur Windows en une commande

Installer une fois :

- Visual Studio 2022 ou Build Tools 2022, charge **Desktop development with C++**, MSVC x64 et SDK Windows ;
- CMake, accessible dans PATH ou via les outils Visual Studio ;
- Qt MSVC 2022 x64 avec Qt WebEngine et ses dépendances, via Qt Online Installer ;
- NSIS uniquement si un installateur est souhaité.

Depuis le dossier du projet, double-cliquer sur `build-windows.cmd`, ou lancer :

```powershell
.\build-windows.cmd
.\build-windows.cmd -QtRoot "D:\Qt\6.11.2\msvc2022_64"
.\build-windows.cmd -Installer
```

Le lanceur utilise PowerShell 5.1 ou supérieur. Son option ExecutionPolicy s’applique uniquement au processus lancé et ne change pas la politique du système ; une politique d’entreprise peut néanmoins interdire son exécution.

Le script initialise MSVC avec `vswhere` et `VsDevCmd`, cherche Qt dans `QT_ROOT_DIR`, puis `QTDIR`, puis les kits stables de `C:\Qt`. `-QtRoot` permet un choix explicite. Pas d’installation ni de téléchargement automatique sur votre PC par ce script.

`-Jobs 4` permet quatre compilations parallèles. `-RunBrowserTests` ajoute les tests WebEngine, à exécuter dans une session Windows graphique ; les tests unitaires sont toujours exécutés. GitHub exécute les tests unitaires et les contrôles de packaging, pas le scénario d’interaction graphique complet.

## Ce qui est produit

`build-windows/` contient la compilation Visual Studio Release. Chaque sortie de packaging possède un dossier neuf sous `dist/windows/<date-identifiant>/` :

- `Minato/` : application déployée, directement testable via `bin/minato.exe` ;
- `Minato-<version>-windows-x64.zip` : application avec dépendances ;
- `Minato-<version>-windows-x64.exe` : installateur si `-Installer` ;
- fichiers `.sha256` : contrôle d’intégrité des archives.

Les profils utilisateur restent dans le dossier de données Windows. « ZIP portable » désigne ici l’absence d’installation requise, pas le stockage des profils à côté de l’exécutable. La désinstallation ne supprime pas volontairement ces données.

## Vérifications et dépannage

Le script arrête le build en cas d’échec de CMake, des tests ou du déploiement. Il vérifie notamment le runtime MSVC, `qwindows`, SQLite, Schannel pour HTTPS via Qt Network, `QtWebEngineProcess`, ICU, les ressources et locales WebEngine. Le lancement `--version` est testé avec un PATH débarrassé du kit Qt. C’est un contrôle de chargement, pas un test complet de Chromium ou de l’installateur.

Si CMake signale un ancien générateur ou kit incompatible, renommer le dossier `build-windows` avant de réessayer. Si Qt est introuvable, fournir `-QtRoot` vers le kit, pas vers `C:\Qt` ni vers son sous-dossier `bin`. Si NSIS manque, installer NSIS ou omettre `-Installer`.

Avant diffusion publique, essayer le paquet sur un Windows 10/11 propre, vérifier navigation HTTPS, réseau, SQLite et téléchargements, et joindre les notices/licences correspondant à la distribution Qt/Chromium. La signature Authenticode n’est pas configurée ; Windows peut afficher un avertissement d’éditeur inconnu.

La compilation et le packaging Windows restent à valider nativement. Les vérifications réalisées ici sous Fedora portent sur CMake/Linux et la structure des fichiers d’automatisation.

Références : [déploiement Qt Windows](https://doc.qt.io/qt-6/windows-deployment.html), [ressources WebEngine](https://doc.qt.io/qt-6/qtwebengine-deploying.html), [action d’installation Qt](https://github.com/jurplel/install-qt-action).
