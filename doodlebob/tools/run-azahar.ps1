# Open a DoodleBob build in the project's own portable Azahar (separate from Fruit Slicer's).
# Usage: .\run-azahar.ps1 [path\to\build.3dsx]   (default: ..\dist\cinnamon.3dsx)
#        .\run-azahar.ps1 -Cia                    (..\dist\DoodleBob.cia: Azahar offers to install it, then it's in the game list)
param([string]$Game, [switch]$Cia)
$root = Split-Path $PSScriptRoot -Parent
if (-not $Game) {
    $Game = if ($Cia) { "$root\dist\DoodleBob.cia" } else { "$root\dist\cinnamon.3dsx" }
}
if (-not (Test-Path $Game)) { Write-Host "No build at $Game yet."; exit 1 }
if (Get-Process azahar -ErrorAction SilentlyContinue) { Write-Host "Azahar is already open. Close it first (one emulator at a time)."; exit 1 }
Start-Process "$PSScriptRoot\azahar\azahar.exe" -ArgumentList "`"$Game`""
