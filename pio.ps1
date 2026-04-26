$ErrorActionPreference = 'Stop'

$py = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\python.exe"

if (-not (Test-Path $py)) {
  throw "PlatformIO penv python not found at: $py. Run 'python -m platformio run' once to initialize PlatformIO, or install PlatformIO IDE extension."
}

& $py -m platformio @Args
