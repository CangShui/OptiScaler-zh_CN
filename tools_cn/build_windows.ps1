param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$Package
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "找不到 vswhere.exe，请安装 Visual Studio 2022 Build Tools。"
}

$install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $install) {
    throw "找不到 Visual Studio 或 Build Tools 安装。"
}

$msbuild = Join-Path $install "MSBuild\Current\Bin\MSBuild.exe"
$vcvars = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $msbuild) -or -not (Test-Path $vcvars)) {
    throw "Visual Studio C++ 工具链不完整：$install"
}

$projectPath = Join-Path $root "OptiScaler\OptiScaler.vcxproj"
[xml]$project = Get-Content -LiteralPath $projectPath -Encoding utf8
$condition = "'`$(Configuration)|`$(Platform)'=='$Configuration|$Platform'"
$propertyGroup = $project.Project.PropertyGroup |
    Where-Object { $_.Condition -eq $condition -and $_.IncludePath } |
    Select-Object -First 1
if (-not $propertyGroup) {
    throw "在 vcxproj 中找不到 $Configuration|$Platform 的 IncludePath"
}

$include = $propertyGroup.IncludePath.Replace('$(ProjectDir)', "$root\OptiScaler\").Replace('$(SolutionDir)', "$root\")
$lib = $propertyGroup.LibraryPath.Replace('$(ProjectDir)', "$root\OptiScaler\").Replace('$(SolutionDir)', "$root\")

$batch = @"
@echo off
chcp 65001 >nul
call "$vcvars" >nul
set "INCLUDE=$include;%INCLUDE%"
set "LIB=$lib;%LIB%"
"$msbuild" "$root\OptiScaler.sln" /p:Configuration=$Configuration /p:Platform=$Platform /p:UseEnv=true /m /v:minimal /nologo
"@

$tempBatch = Join-Path $env:TEMP "optiscaler_build_$PID.bat"
[System.IO.File]::WriteAllText($tempBatch, $batch.Replace("`r`n", "`n").Replace("`n", "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
try {
    & cmd.exe /c $tempBatch
    $exitCode = $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $tempBatch -Force -ErrorAction SilentlyContinue
}
if ($exitCode -ne 0) {
    exit $exitCode
}

$outputDir = Join-Path $root "x64\$Configuration\a"
$dll = Join-Path $outputDir "OptiScaler.dll"
if (-not (Test-Path $dll)) {
    throw "编译结束但找不到产物：$dll"
}
Write-Host "编译完成：$dll"

if ($Package) {
    $resource = Get-Content -LiteralPath (Join-Path $root "OptiScaler\resource.h") -Encoding utf8
    $major = [regex]::Match($resource, '#define VER_MAJOR_VERSION (\d+)').Groups[1].Value
    $minor = [regex]::Match($resource, '#define VER_MINOR_VERSION (\d+)').Groups[1].Value
    $hotfix = [regex]::Match($resource, '#define VER_HOTFIX_VERSION (\d+)').Groups[1].Value
    $date = Get-Date -Format "yyyyMMdd"
    $releaseDir = Join-Path $root "release"
    New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
    $baseName = "OptiScaler_v$major.$minor.$hotfix-nightly-$($date)_zh-CN"
    $sevenZip = "C:\Program Files\7-Zip\7z.exe"
    if (Test-Path $sevenZip) {
        $archive = Join-Path $releaseDir "$baseName.7z"
        & $sevenZip a -t7z -mx=9 $archive "$outputDir\*" | Out-Null
    }
    else {
        $archive = Join-Path $releaseDir "$baseName.zip"
        Compress-Archive -Path "$outputDir\*" -DestinationPath $archive -Force
    }
    Write-Host "发布包：$archive"
}
