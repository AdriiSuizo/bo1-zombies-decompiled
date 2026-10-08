@echo off
rem Black Ops 1 Zombies with the euphoria mod (active-ragdoll zombies). NOT part of the original game.
rem   Play-Euphoria.cmd                  Kino der Toten, physics 1
rem   Play-Euphoria.cmd zombie_pentagon  another map (zombie_theater, zombie_pentagon, zombie_cosmodrome, zombie_coast, ...)
rem   Play-Euphoria.cmd zombie_theater 1.5 0.5   physics 1.5 (harder hits), plus the drunk weave at 0.5
rem Needs the build (build\Release\BO1Zombies.exe, see README.md) and the game files copied by setup.ps1.
set "GAME=%~dp0build\Release"
if not exist "%GAME%\BO1Zombies.exe" (echo Game not found in %GAME% - build the project first ^(cmake, Visual Studio^). & pause & exit /b 1)
if not exist "%GAME%\mods\euphoria\maps\euphoria\_euphoria.gsc" (echo mods\euphoria is not next to the exe - rebuild ^(the build copies mods\^). & pause & exit /b 1)
set "MAP=%~1"
if "%MAP%"=="" set "MAP=zombie_theater"
set "PHYS=%~2"
if "%PHYS%"=="" set "PHYS=1"
set "DRUNK=%~3"
if "%DRUNK%"=="" set "DRUNK=0"
cd /d "%GAME%"
start "" "%GAME%\BO1Zombies.exe" +set bo1_zombies 1 +set fs_b "%GAME%" +set fs_h "%GAME%" +set logfile 2 +set net_ip 127.0.0.1 +set r_fullscreen 0 +set r_borderless 1 +set vid_xpos 0 +set vid_ypos 0 +set bo1_mod_sharedconfig 1 +set bo1_mod_quickrestart 1 +set fs_game mods/euphoria +set fs_mods euphoria +set euphoria_physics %PHYS% +set euphoria_level %DRUNK% +devmap %MAP%
