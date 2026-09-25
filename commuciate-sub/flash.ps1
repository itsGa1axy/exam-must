param(
    [string]$BuildDir = "cmake-build-mpu-check",
    [string]$ProgrammerCli = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildPath = Join-Path $projectRoot $BuildDir
$firmwarePath = Join-Path $buildPath "commuicate.elf"

# Build the slave project before programming.
cmake --build $buildPath --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Slave build failed; programming was stopped."
}
if (-not (Test-Path -LiteralPath $firmwarePath)) {
    throw "Firmware file not found: $firmwarePath"
}

# Find STM32CubeProgrammer from the parameter, PATH, or common install locations.
if ([string]::IsNullOrWhiteSpace($ProgrammerCli)) {
    $command = Get-Command "STM32_Programmer_CLI.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $ProgrammerCli = $command.Source
    }
}
if ([string]::IsNullOrWhiteSpace($ProgrammerCli)) {
    $candidates = @(
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
    $programmerRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\programmer"
    if (Test-Path -LiteralPath $programmerRoot) {
        Get-ChildItem -LiteralPath $programmerRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                $candidates += Join-Path $_.FullName "bin\STM32_Programmer_CLI.exe"
            }
    }
    $ProgrammerCli = $candidates |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($ProgrammerCli) -or
    -not (Test-Path -LiteralPath $ProgrammerCli)) {
    throw "STM32_Programmer_CLI.exe not found; specify its path with -ProgrammerCli."
}

Write-Host "Slave firmware: $firmwarePath"
Write-Host "Programmer: $ProgrammerCli"
Write-Host "Connect ST-Link to the slave board and power the target before continuing."
& $ProgrammerCli -c port=SWD -w $firmwarePath -v -rst
if ($LASTEXITCODE -ne 0) {
    throw "Slave programming/verification failed; STM32CubeProgrammer exit code: $LASTEXITCODE"
}
Write-Host "Slave programming and verification succeeded."
