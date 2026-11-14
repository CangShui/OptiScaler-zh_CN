@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

REM 配置
set "REPO=NVIDIA-RTX/Streamline"
set "DEST=%~dp0streamline"
set "WORKDIR=%TEMP%\streamline_dl_%RANDOM%"
set "ZIPFILE=%WORKDIR%\release.zip"
set "EXTRACTDIR=%WORKDIR%\extracted"

echo ==========================
echo  Streamline 文件下载器
echo ==========================
echo v1.1
echo.
echo.
echo [1] 下载最新 Streamline DLL
echo.
echo [2] 删除 "streamline" 文件夹
echo.
choice /c 12 /n /m "请选择（1 或 2）："
echo.

if errorlevel 2 goto :removal
if errorlevel 1 goto :downloader

:removal
echo.
if exist "%DEST%" (
    echo 正在删除 "streamline" 文件夹及其全部内容……
    rmdir /s /q "%DEST%"
    echo 完成。
) else (
    echo 文件夹 "streamline" 不存在，无需删除。
)
echo.
echo 按任意键继续……
pause >nul
exit /b 0

:downloader

mkdir "%WORKDIR%" 2>nul
mkdir "%EXTRACTDIR%" 2>nul
if not exist "%DEST%" mkdir "%DEST%"

REM 查找最新的 ZIP 发布包
echo --- 正在查询最新版本信息 ---
echo.
powershell -NoProfile -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$r = Invoke-RestMethod -Uri 'https://api.github.com/repos/%REPO%/releases/latest' -Headers @{ 'User-Agent' = 'batch-script' };" ^
    "$asset = $r.assets | Where-Object { $_.name -match '^streamline-sdk-v[0-9.]+\.zip$' } | Select-Object -First 1;" ^
    "if (-not $asset) { Write-Error '最新版本中未找到 .zip 资源。'; exit 1 }" ^
    "$asset.browser_download_url | Out-File -Encoding ascii '%WORKDIR%\asset_url.txt';" ^
    "$asset.name | Out-File -Encoding ascii '%WORKDIR%\asset_name.txt';" ^
    "Write-Host ('找到资源：' + $asset.name)"

if not exist "%WORKDIR%\asset_url.txt" (
    echo 错误：无法确定最新版本的资源地址。
    goto :cleanup_fail
)

set /p ASSET_URL=<"%WORKDIR%\asset_url.txt"
set /p ASSET_NAME=<"%WORKDIR%\asset_name.txt"

if "%ASSET_URL%"=="" (
    echo 错误：资源地址为空。
    goto :cleanup_fail
)

echo.
echo 下载地址：%ASSET_URL%
echo.

REM 下载 ZIP
echo --- 正在下载 %ASSET_NAME% ---
echo.
curl -L --fail -o "%ZIPFILE%" "%ASSET_URL%"
if errorlevel 1 (
    echo 错误：下载失败。
    goto :cleanup_fail
)
echo.

REM 解压 ZIP
echo --- 正在解压 ---
powershell -NoProfile -Command "Expand-Archive -LiteralPath '%ZIPFILE%' -DestinationPath '%EXTRACTDIR%' -Force"
if errorlevel 1 (
    echo 错误：解压失败。
    goto :cleanup_fail
)
echo.

REM 查找 DLL 并复制到目标文件夹
echo --- 正在查找 bin\x64 文件夹并复制 .dll 文件 ---
set FOUND=0
for /f "delims=" %%D in ('dir /ad /b /s "%EXTRACTDIR%" ^| findstr /i "\\bin\\x64$"') do (
    set FOUND=1
    echo   找到：%%D
    xcopy "%%D\*.dll" "%DEST%\" /Y /Q >nul
)

if "%FOUND%"=="0" (
    echo 警告：压缩包中未找到 bin\x64 文件夹。
    echo 压缩包结构可能已变化，请手动检查 "%EXTRACTDIR%"。
    goto :cleanup_fail
)

echo.
echo 完成。DLL 已复制到：%DEST%
echo.
dir "%DEST%\*.dll" /b

goto :cleanup_success

:cleanup_fail
echo.
echo 脚本执行失败。临时文件保留在：
echo   %WORKDIR%
echo.
echo 按任意键继续……
pause >nul
exit /b 1

:cleanup_success
rmdir /s /q "%WORKDIR%" 2>nul
echo.
echo 临时文件已清理。
echo.
echo 按任意键继续……
pause >nul
exit /b 0
