param([switch]$Test, [switch]$Smoke)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $projectRoot
try {
  $candidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
  )
  $developerScript = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
  if (-not $developerScript) {
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
