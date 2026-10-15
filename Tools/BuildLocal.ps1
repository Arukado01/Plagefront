param([switch]$Test, [switch]$Smoke)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $projectRoot
try {
  $developerScript = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
  if (-not (Test-Path -LiteralPath $developerScript)) {
    throw 'No se encuentra VsDevCmd.bat. Abre una Developer PowerShell de MSVC y ejecuta los comandos de README.md.'
  }
  $buildLine = '"' + $developerScript + '" -no_logo -arch=x64 -host_arch=x64 && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release && cmake --build build --parallel 4'
  & $env:ComSpec /d /s /c $buildLine
  if ($LASTEXITCODE -ne 0) { throw 'La compilación local falló.' }
  if ($Test) {
    & ctest --test-dir build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Las pruebas fallaron.' }
  }
  if ($Smoke) {
    & (Join-Path $projectRoot 'build/Plaguefront.exe') --smoke
    if ($LASTEXITCODE -ne 0) { throw 'La comprobación visual falló.' }
  }
} finally { Pop-Location }
