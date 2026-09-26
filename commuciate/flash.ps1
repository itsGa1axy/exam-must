param(
    [string]$BuildDir = "build/flash-release",
    [string]$ProgrammerCli = "",
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([IO.Path]::IsPathRooted($BuildDir)) {
    $buildPath = $BuildDir
} else {
    $buildPath = Join-Path $projectRoot $BuildDir
}
$firmwarePath = Join-Path $buildPath "commuicate.elf"

# 自动寻找 STM32Cube 安装的工具链，仅在本次脚本运行期间加入 PATH。
if (-not (Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue)) {
    $compiler = Get-ChildItem -Path "$env:LOCALAPPDATA/stm32cube/bundles/gnu-tools-for-stm32/*/bin/arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $compiler) { throw 'ARM GCC not found. Add arm-none-eabi-gcc.exe to PATH.' }
    $env:Path = "$($compiler.DirectoryName);$env:Path"
}
# 旧缓存不删除；搬家后指定新的构建目录，避免烧录过期固件。
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
$configureArgs = @('-S', $projectRoot, '-B', $buildPath, '-DCMAKE_BUILD_TYPE=Release')
if (Test-Path -LiteralPath $cachePath) {
    $sourceLine = Get-Content -LiteralPath $cachePath | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } | Select-Object -First 1
    if ($sourceLine -and ($sourceLine.Split('=', 2)[1].Replace('\', '/') -ne $projectRoot.Replace('\', '/'))) {
        throw 'Build cache belongs to another source directory. Use -BuildDir build/flash-release or another new directory.'
    }
} elseif (Get-Command ninja.exe -ErrorAction SilentlyContinue) {
    $configureArgs += @('-G', 'Ninja')
} elseif (Get-Command mingw32-make.exe -ErrorAction SilentlyContinue) {
    $configureArgs += @('-G', 'MinGW Makefiles')
} else {
    throw 'Ninja or mingw32-make is required in PATH.'
}
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'Host configure failed; programming was stopped.' }
& cmake --build $buildPath --parallel
if ($LASTEXITCODE -ne 0) { throw 'Host build failed; programming was stopped.' }
if (-not (Test-Path -LiteralPath $firmwarePath)) {
    throw "Firmware file not found: $firmwarePath"
}

# 查找参数、PATH 或 STM32Cube 安装目录中的烧录工具。
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

Write-Host "Host firmware: $firmwarePath"
Write-Host "Programmer: $ProgrammerCli"
if ($BuildOnly) {
    Write-Host 'Build and programmer path check succeeded; target was not programmed.'
    return
}
Write-Host "Connect ST-Link to the host board and power the target before continuing."
& $ProgrammerCli -c port=SWD -w $firmwarePath -v -rst
if ($LASTEXITCODE -ne 0) {
    throw "Host programming/verification failed; STM32CubeProgrammer exit code: $LASTEXITCODE"
}
Write-Host "Host programming and verification succeeded."
