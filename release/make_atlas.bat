@echo off
rem Builds BlockRender's block atlas (atlas.bin and atlas.json) into this folder, from your own Minecraft 26.3 jar.
rem The textures belong to Mojang, so CyberCraft doesn't ship them: this makes them from the copy you own.
rem
rem   make_atlas.bat                         finds 26.3's jar in the official launcher's or the Modrinth App's folders
rem   make_atlas.bat "D:\path\to\26.3.jar"   uses that jar (Prism, MultiMC, CurseForge: any 26.3 client jar)
rem
rem Needs uv (https://docs.astral.sh/uv/) or Python 3 on PATH; with plain Python it installs Pillow first.
setlocal
set "HERE=%~dp0"
set "JAR=%~1"
if defined JAR goto :have_jar
if exist "%APPDATA%\.minecraft\versions\26.3\26.3.jar" set "JAR=%APPDATA%\.minecraft\versions\26.3\26.3.jar"
if defined JAR goto :have_jar
for /d %%D in ("%APPDATA%\ModrinthApp\meta\versions\26.3-*") do if exist "%%D\%%~nxD.jar" set "JAR=%%D\%%~nxD.jar"
if defined JAR goto :have_jar
echo Couldn't find Minecraft 26.3's jar. Start 26.3 once in your launcher, or pass the jar's path:
echo   make_atlas.bat "D:\path\to\26.3.jar"
goto :fail

:have_jar
if exist "%JAR%" goto :jar_ok
echo There's no file at "%JAR%".
goto :fail

:jar_ok
echo Building the atlas from "%JAR%"
where uv >nul 2>nul
if errorlevel 1 goto :python
uv run --no-project --with pillow "%HERE%build_atlas.py" "%JAR%" "%HERE%."
goto :check

:python
python -c "import sys" >nul 2>nul
if errorlevel 1 goto :no_python
python -c "import PIL" >nul 2>nul
if errorlevel 1 python -m pip install --user pillow
python "%HERE%build_atlas.py" "%JAR%" "%HERE%."

:check
if errorlevel 1 goto :fail
if not exist "%HERE%atlas.bin" goto :fail
echo.
echo Done: atlas.bin is in "%HERE%."
if "%~1"=="" pause
exit /b 0

:no_python
echo Neither uv nor Python is on PATH. Install one of them (uv: https://docs.astral.sh/uv/) and run this again.

:fail
echo.
echo The atlas wasn't built.
if "%~1"=="" pause
exit /b 1
