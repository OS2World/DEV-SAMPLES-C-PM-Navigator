@echo off
rem ── WarpNav – OpenWatcom build script ─────────────────────────────────────
rem
rem  Requires: OpenWatcom 2.x (WATCOM env var must be set)
rem  Output: bin-ow\warpnav.exe

if not exist bin-ow md bin-ow

echo Compiling... > bin-ow\compile.log

wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin-ow\main.obj     src\main.c     >> bin-ow\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin-ow\mainwnd.obj  src\mainwnd.c  >> bin-ow\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin-ow\treeview.obj src\treeview.c >> bin-ow\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin-ow\fileview.obj src\fileview.c >> bin-ow\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin-ow\fileops.obj  src\fileops.c  >> bin-ow\compile.log 2>&1

echo. >> bin-ow\compile.log
echo Linking... >> bin-ow\compile.log

wlink system os2v2_pm name bin-ow\warpnav.exe file { bin-ow\main.obj bin-ow\mainwnd.obj bin-ow\treeview.obj bin-ow\fileview.obj bin-ow\fileops.obj } >> bin-ow\compile.log 2>&1

echo. >> bin-ow\compile.log
echo Resource compile... >> bin-ow\compile.log

wrc -r -I%WATCOM%\h\os2 -I%WATCOM%\h -fo=bin-ow\warpnav.res src\warpnav.rc >> bin-ow\compile.log 2>&1
wrc bin-ow\warpnav.res bin-ow\warpnav.exe >> bin-ow\compile.log 2>&1

echo. >> bin-ow\compile.log
if exist bin-ow\warpnav.exe echo BUILD SUCCESSFUL >> bin-ow\compile.log
if not exist bin-ow\warpnav.exe echo BUILD FAILED >> bin-ow\compile.log
