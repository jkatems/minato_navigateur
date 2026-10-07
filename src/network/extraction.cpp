#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>

int main() {
    // 1. Obtention du chemin du dossier temporaire
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring scriptPath = std::wstring(tempPath) + L"get_wifi.ps1";

        // 2. Écriture du script temporaire
        // Convertit le chemin wide string en UTF-8 pour std::ofstream
        int _len = WideCharToMultiByte(CP_UTF8, 0, scriptPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string scriptPathUtf8;
        if (_len > 0) {
            scriptPathUtf8.resize(_len - 1);
            WideCharToMultiByte(CP_UTF8, 0, scriptPath.c_str(), -1, &scriptPathUtf8[0], _len, nullptr, nullptr);
        }
    
        std::ofstream scriptFile(scriptPathUtf8, std::ios::binary);
        if (!scriptFile.is_open()) {
            std::cerr << "Erreur lors de la creation du fichier script temporaire.\n";
            return 1;
        }
    
        // Utilisation d'un littéral brut pour éviter d'échapper les guillemets PowerShell
                static const char scriptContent[] = R"PS1(
                        function Export-And-ReadProfile([string]$name) {
                        $tmpFolder = Join-Path $env:TEMP ("wifiexport_" + [guid]::NewGuid())
                        New-Item -ItemType Directory -Path $tmpFolder | Out-Null
        
                        netsh wlan export profile name="$name" key=clear folder="$tmpFolder" 2>&1 | Out-Null
        
                        $xmlFile = Get-ChildItem -Path $tmpFolder -Filter "*.xml" -ErrorAction SilentlyContinue | Select-Object -First 1
        
                        if (-not $xmlFile) {
                            Remove-Item $tmpFolder -Recurse -Force -ErrorAction SilentlyContinue
                            return $null
                        }
        
                        [xml]$xml = Get-Content -Path $xmlFile.FullName -Encoding UTF8
                        $ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
                        $ns.AddNamespace("w", "http://www.microsoft.com/networking/WLAN/profile/v1")
        
                        $ssidNode = $xml.SelectSingleNode("//w:SSIDConfig/w:SSID/w:name", $ns)
                        $keyNode  = $xml.SelectSingleNode("//w:MSM/w:security/w:sharedKey/w:keyMaterial", $ns)
        
                        Remove-Item $tmpFolder -Recurse -Force -ErrorAction SilentlyContinue
        
                        return [PSCustomObject]@{
                            SSID = if ($ssidNode) { $ssidNode.InnerText } else { $name }
                            Pass = if ($keyNode)  { $keyNode.InnerText } else { $null }
                        }
                    }
        
                    $profile = Get-NetConnectionProfile | Where-Object {
                        $_.InterfaceAlias -match "Wi-Fi|Wireless|WLAN"
                    } | Select-Object -First 1
        
                    if (-not $profile) {
                        Write-Host "Aucun reseau Wi-Fi connecte detecte."
                        exit
                    }
        
                    $displayName = $profile.Name
        
                    # Tentative 1 : nom exact
                    $result = Export-And-ReadProfile $displayName
        
                    # Tentative 2 : nom sans suffixe numerique ajoute par Windows (ex: "Reseau 45" -> "Reseau")
                    if (-not $result -and $displayName -match '^(.*\S)\s+\d+$') {
                        $trimmedName = $Matches[1]
                        $result = Export-And-ReadProfile $trimmedName
                    }
        
                    if ($result) {
                        if ($result.Pass) {
                            Write-Host "Reseau : $($result.SSID) | Mot de passe : $($result.Pass)"
                        } else {
                            Write-Host "Reseau : $($result.SSID) | Mot de passe : introuvable (reseau ouvert, ou lance le script en administrateur)"
                        }
                        exit
                    }
        
                    # Dernier recours : lister tous les profils Wi-Fi enregistres pour identification manuelle
                    Write-Host "Nom de connexion detecte : $displayName"
                    Write-Host "Impossible de retrouver automatiquement le profil correspondant."
                    Write-Host ""
                    Write-Host "Profils Wi-Fi enregistres sur ce PC :"
                    (netsh wlan show profiles) -split "`n" | Where-Object { $_ -match ":" }
        )PS1";
                        
                scriptFile << scriptContent;
                scriptFile.close();

    // 3. Configuration du Pipe de capture
    SECURITY_ATTRIBUTES securite{};
    securite.nLength = sizeof(securite);
    securite.bInheritHandle = TRUE;

    HANDLE lecture = nullptr;
    HANDLE ecriture = nullptr;

    if (!CreatePipe(&lecture, &ecriture, &securite, 0)) {
        DeleteFileW(scriptPath.c_str());
        return 1;
    }
    SetHandleInformation(lecture, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = ecriture;
    si.hStdError = ecriture;

    PROCESS_INFORMATION pi{};

    // 4. Lancement masqué du processus PowerShell
    std::wstring cmdLine = L"powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + scriptPath + L"\"";

    BOOL lance = CreateProcessW(
        nullptr, &cmdLine[0],
        nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    // Fermeture de l'écriture côté parent
    CloseHandle(ecriture);

    if (!lance) {
        CloseHandle(lecture);
        DeleteFileW(scriptPath.c_str());
        std::cerr << "Erreur de lancement : " << GetLastError() << '\n';
        return 1;
    }

    // 5. Extraction du résultat
    std::string resultat;
    char buffer[4096];
    DWORD octetsLus = 0;

    while (ReadFile(lecture, buffer, sizeof(buffer) - 1, &octetsLus, nullptr) && octetsLus > 0) {
        buffer[octetsLus] = '\0';
        resultat += buffer;
    }

    CloseHandle(lecture);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // 6. Suppression du script temporaire
    DeleteFileW(scriptPath.c_str());

    

    return 0;
};