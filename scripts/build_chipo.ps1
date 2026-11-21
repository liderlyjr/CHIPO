param(
    [string]$WorkDir = "$PSScriptRoot\..\build",
    # Commit, rama o etiqueta de XiaoZhi. Vacío = el commit fijado en XIAOZHI_REF (el mismo que usa GitHub Actions).
    [string]$XiaoZhiRef = ""
)
# CHIPO OS compila solo para la Waveshare ESP32-S3 Touch AMOLED 2.16 (480x480).
$ErrorActionPreference = "Stop"

function Require-Command($Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "No encuentro '$Name'. Abre primero ESP-IDF PowerShell/Command Prompt y verifica que Git esté instalado."
    }
}

Require-Command "git"
Require-Command "python"
Require-Command "idf.py"

$ChipoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$WorkDir = [System.IO.Path]::GetFullPath($WorkDir)
$XiaoZhiDir = Join-Path $WorkDir "xiaozhi-esp32"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

if (-not $XiaoZhiRef) {
    $XiaoZhiRef = (Get-Content (Join-Path $ChipoRoot "XIAOZHI_REF") -TotalCount 1).Trim()
}
Write-Host "XiaoZhi: $XiaoZhiRef"

if (-not (Test-Path (Join-Path $XiaoZhiDir ".git"))) {
    git init -q $XiaoZhiDir
    if ($LASTEXITCODE -ne 0) { throw "No se pudo crear la carpeta de XiaoZhi." }
    git -C $XiaoZhiDir remote add origin https://github.com/78/xiaozhi-esp32.git
} else {
    Write-Host "Usando el checkout XiaoZhi existente: $XiaoZhiDir"
}
git -C $XiaoZhiDir fetch -q --depth 1 origin $XiaoZhiRef
if ($LASTEXITCODE -ne 0) { throw "Falló la descarga de XiaoZhi ($XiaoZhiRef)." }
# Parte siempre de XiaoZhi limpio: quita los cambios de integraciones anteriores.
git -C $XiaoZhiDir checkout -q -f FETCH_HEAD
if ($LASTEXITCODE -ne 0) { throw "No se pudo preparar XiaoZhi." }
git -C $XiaoZhiDir clean -fdq
if ($LASTEXITCODE -ne 0) { throw "No se pudo limpiar el checkout de XiaoZhi." }

python (Join-Path $ChipoRoot "scripts\integrate_chipo.py") $ChipoRoot $XiaoZhiDir
if ($LASTEXITCODE -ne 0) { throw "No se pudo integrar CHIPO. No se ha iniciado la compilación." }

Push-Location $XiaoZhiDir
try {
    # set-target regenera sdkconfig desde sdkconfig.defaults (placa 2.16, palabra de activación, fuentes).
    idf.py set-target esp32s3
    if ($LASTEXITCODE -ne 0) { throw "idf.py set-target falló." }
    idf.py build
    if ($LASTEXITCODE -ne 0) { throw "La compilación falló. Revisa el primer error de C/C++ mostrado arriba." }
    Write-Host ""
    Write-Host "COMPILACIÓN TERMINADA. Busca los archivos .bin en build\."
} finally {
    Pop-Location
}
