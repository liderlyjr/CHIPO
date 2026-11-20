param(
    [string]$WorkDir = "$PSScriptRoot\..\build",
    # Placa: "2.16" (480x480) o "1.8-v2" (368x448)
    [ValidateSet("1.8-v2", "2.16")]
    [string]$Board = "2.16"
)
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

if (-not (Test-Path (Join-Path $XiaoZhiDir ".git"))) {
    git clone --depth 1 https://github.com/78/xiaozhi-esp32.git $XiaoZhiDir
    if ($LASTEXITCODE -ne 0) { throw "Falló la descarga de XiaoZhi." }
} else {
    Write-Host "Usando el checkout XiaoZhi existente: $XiaoZhiDir"
    # Quita los cambios de integraciones anteriores: el script de integración parte siempre de XiaoZhi limpio.
    git -C $XiaoZhiDir reset --hard
    if ($LASTEXITCODE -ne 0) { throw "No se pudo limpiar el checkout de XiaoZhi." }
    git -C $XiaoZhiDir clean -fd
}

python (Join-Path $ChipoRoot "scripts\integrate_chipo.py") $ChipoRoot $XiaoZhiDir "--board=$Board"
if ($LASTEXITCODE -ne 0) { throw "No se pudo integrar CHIPO. No se ha iniciado la compilación." }

Push-Location $XiaoZhiDir
try {
    # Si se cambia de placa hay que partir de cero: sdkconfig recuerda la placa anterior.
    $boardMark = if ($Board -eq "2.16") { "AMOLED_2_16=y" } else { "AMOLED_1_8_v2=y" }
    if ((Test-Path "sdkconfig") -and -not (Select-String -Path "sdkconfig" -SimpleMatch $boardMark -Quiet)) {
        Write-Host "Cambio de placa: limpiando la configuración anterior."
        Remove-Item "sdkconfig" -Force
        if (Test-Path "build") { Remove-Item "build" -Recurse -Force }
    }
    idf.py set-target esp32s3
    if ($LASTEXITCODE -ne 0) { throw "idf.py set-target falló." }
    idf.py build
    if ($LASTEXITCODE -ne 0) { throw "La compilación falló. Revisa el primer error de C/C++ mostrado arriba." }
    Write-Host ""
    Write-Host "COMPILACIÓN TERMINADA. Busca los archivos .bin en build\."
} finally {
    Pop-Location
}
