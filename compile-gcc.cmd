@echo off
rem ── WarpNav – GCC 9 build script ──────────────────────────────────────────
rem
rem  Compiler : GCC 9 for OS/2 (available via ANPM on ArcaOS)
rem  Linker   : wlink (OpenWatcom) – driven by GCC's own linker front-end
rem  Resources: wrc from OpenWatcom (WATCOM env var must be set)
rem
rem  EMXOMFLD_TYPE=WLINK tells GCC's OS/2 linker driver to invoke wlink
rem  instead of ilink.  EMXOMFLD_LINKER points to the wlink executable.
rem  src\warpnav.def (NAME … WINDOWAPI) is passed straight to GCC; the
rem  driver forwards it to wlink which marks the LX EXE as a PM app and
rem  links kLIBC (C runtime + startup code) automatically.
rem
rem  Requires watcom-wlink-hll from ANPM (provides wl.exe).
rem
rem  Output: bin-gcc\warpnav.exe

if not exist bin-gcc md bin-gcc

set EMXOMFLD_TYPE=WLINK
set EMXOMFLD_LINKER=wl.exe

echo Compiling with GCC 9... > bin-gcc\compile.log

gcc -Zomf -Wall -Wno-pointer-sign -std=c99 -c -o bin-gcc\main.obj     src\main.c     >> bin-gcc\compile.log 2>&1
gcc -Zomf -Wall -Wno-pointer-sign -std=c99 -c -o bin-gcc\mainwnd.obj  src\mainwnd.c  >> bin-gcc\compile.log 2>&1
gcc -Zomf -Wall -Wno-pointer-sign -std=c99 -c -o bin-gcc\treeview.obj src\treeview.c >> bin-gcc\compile.log 2>&1
gcc -Zomf -Wall -Wno-pointer-sign -std=c99 -c -o bin-gcc\fileview.obj src\fileview.c >> bin-gcc\compile.log 2>&1
gcc -Zomf -Wall -Wno-pointer-sign -std=c99 -c -o bin-gcc\fileops.obj  src\fileops.c  >> bin-gcc\compile.log 2>&1

echo. >> bin-gcc\compile.log
echo Linking... >> bin-gcc\compile.log

gcc -Zomf -o bin-gcc\warpnav.exe bin-gcc\main.obj bin-gcc\mainwnd.obj bin-gcc\treeview.obj bin-gcc\fileview.obj bin-gcc\fileops.obj src\warpnav.def >> bin-gcc\compile.log 2>&1

echo. >> bin-gcc\compile.log
echo Resource compile... >> bin-gcc\compile.log

wrc -r -I%WATCOM%\h\os2 -I%WATCOM%\h -fo=bin-gcc\warpnav.res src\warpnav.rc >> bin-gcc\compile.log 2>&1
wrc bin-gcc\warpnav.res bin-gcc\warpnav.exe >> bin-gcc\compile.log 2>&1

echo. >> bin-gcc\compile.log
if exist bin-gcc\warpnav.exe echo BUILD SUCCESSFUL >> bin-gcc\compile.log
if not exist bin-gcc\warpnav.exe echo BUILD FAILED >> bin-gcc\compile.log
