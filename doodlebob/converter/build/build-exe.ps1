# Packs the converter into one Windows program: converter\dist\DoodleBob 3DS Converter.exe
# Needs: py -m pip install --user pillow pyinstaller, and converter\bin filled (build\prepare-bin.py).
# PyInstaller logs to stderr; judge it by its exit code, not by stderr.
$ErrorActionPreference = 'Continue'
$conv = Split-Path $PSScriptRoot -Parent
$work = Join-Path $PSScriptRoot 'pyi'
py -m PyInstaller --noconfirm --clean --onefile --windowed `
    --name 'DoodleBob 3DS Converter' `
    --add-data "$conv\bin;bin" `
    --distpath "$conv\dist" --workpath $work --specpath $work `
    "$conv\gui.py"
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller failed' }
Get-Item "$conv\dist\DoodleBob 3DS Converter.exe" | Select-Object Name, Length, LastWriteTime
