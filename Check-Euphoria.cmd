@echo off
rem Headless check of the euphoria mod (no window): the server side only - the balance model, the real damage path
rem (DoDamage from the self-test script), stumbles, falls, get-ups, the entity tilt. The client full body needs a real
rem client (play, or tools\headless.ps1 -Client). Prints the euphoria lines of games_mp.log; "euphoria PASS" = good.
rem   Check-Euphoria.cmd [map]   (default zombie_theater; about 3 minutes)
set "MAP=%~1"
if "%MAP%"=="" set "MAP=zombie_theater"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\headless.ps1" -Zombies -Commands "+set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_selftest 1 +set euphoria_physics 1 +devmap %MAP%" -AutoQuitMs 170000
echo.
echo ---- euphoria lines of build\Release\mods\euphoria\games_mp.log ----
findstr /C:"euphoria" "%~dp0build\Release\mods\euphoria\games_mp.log"
pause
