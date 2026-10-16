param([string]$ProjectRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))

# Reconstruye el pack aprobado desde fuentes locales sin invocar servicios de imágenes.
$ErrorActionPreference = 'Stop'
$taskRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$bundledPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
if (Test-Path -LiteralPath $bundledPython) {
    $artPython = $bundledPython
} else {
    $artPython = (Get-Command python -ErrorAction Stop).Source
}
& $artPython -c 'import PIL, numpy'
if ($LASTEXITCODE -ne 0) { throw 'El generador necesita Python con Pillow y NumPy.' }
Push-Location $taskRoot
try {
    foreach ($builder in @('BuildUnits.py', 'BuildEnvironment.py', 'BuildCasterEffects.py')) {
        $builderPath = Join-Path $taskRoot ('Tools/Art/' + $builder)
        if (-not (Test-Path -LiteralPath $builderPath)) { throw "Falta el generador: $builderPath" }
        & $artPython -B $builderPath
        if ($LASTEXITCODE -ne 0) { throw "Falló el generador de arte: $builder" }
    }
    Copy-Item -LiteralPath (Join-Path $taskRoot 'Tools/Art/Sources/menu-approved.png') -Destination (Join-Path $taskRoot 'Content/Textures/Plaguefront/Pixel/menu.png')
    Write-Output 'Arte aprobado reconstruido localmente: unidades, terreno, props, magia y portada; retratos conservados.'
} finally { Pop-Location }
