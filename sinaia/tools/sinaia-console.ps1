<#
.SYNOPSIS
    Client de la console distante de sinaia (127.0.0.1:7777).

.DESCRIPTION
    sinaia ouvre un serveur de commandes au demarrage (SinaiaApp.cpp, port 7777).
    Il n'y a pas d'invite dans l'application : les commandes `glcheck`, `camera`,
    `shading`, `toggle`, `open`, `screenshot`, `info`... passent par ce socket.

    Ce script sert a deux choses :
      - piloter sinaia a la main, une commande a la fois ;
      - scripter une serie de captures de reference, ce pour quoi la console a
        ete etendue (camera deterministe + bascules idempotentes).

.EXAMPLE
    .\sinaia-console.ps1 "glcheck on"

.EXAMPLE
    # Plusieurs commandes dans l'ordre, une seule connexion.
    .\sinaia-console.ps1 "open C:\home\perso\cg\test\data\Duck.glb", "camera reset",
                         "camera azel 35 20", "shading materials",
                         "screenshot C:\tmp\duck-materials.png"

.EXAMPLE
    # Mode interactif : tapez vos commandes, `quit` pour sortir.
    .\sinaia-console.ps1
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Command,

    [int] $Port = 7777
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Chaque reponse du serveur se termine par une ligne "OK" ou "ERR ...". On lit
# jusque-la plutot qu'un nombre d'octets fixe : la longueur varie d'une commande
# a l'autre (`info` rend plusieurs dizaines de lignes, `glcheck` une seule).
function Read-Reply {
    param($Reader)

    $lines = @()
    while ($true) {
        $line = $Reader.ReadLine()
        if ($null -eq $line) { break }          # serveur ferme la connexion
        $lines += $line
        if ($line -eq 'OK' -or $line.StartsWith('ERR ')) { break }
    }
    return ($lines -join [Environment]::NewLine)
}

try {
    $client = New-Object System.Net.Sockets.TcpClient
    $client.Connect('127.0.0.1', $Port)
}
catch {
    Write-Error ("Connexion a 127.0.0.1:$Port impossible. sinaia est-il lance ? " +
                 "(le serveur demarre avec l'application, SinaiaApp.cpp)")
    exit 1
}

try {
    $stream = $client.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.AutoFlush = $true

    if ($Command) {
        foreach ($c in $Command) {
            Write-Host "> $c" -ForegroundColor DarkGray
            $writer.WriteLine($c)
            Read-Reply -Reader $reader
        }
        $writer.WriteLine('quit')
    }
    else {
        Write-Host "Console sinaia -- 'help' pour la liste, 'quit' pour sortir." -ForegroundColor DarkGray
        while ($true) {
            $line = Read-Host 'sinaia'
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $writer.WriteLine($line)
            if ($line.Trim() -eq 'quit') { break }
            Read-Reply -Reader $reader
        }
    }
}
finally {
    $client.Close()
}
