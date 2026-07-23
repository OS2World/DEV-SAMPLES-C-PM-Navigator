@echo off
echo Compiling... > bin\compile.log

wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin\main.obj src\main.c >> bin\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin\mainwnd.obj src\mainwnd.c >> bin\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin\treeview.obj src\treeview.c >> bin\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin\fileview.obj src\fileview.c >> bin\compile.log 2>&1
wcc386 -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei -fo=bin\fileops.obj src\fileops.c >> bin\compile.log 2>&1

echo. >> bin\compile.log
echo Linking... >> bin\compile.log
wlink system os2v2_pm name bin\warpnav.exe file { bin\main.obj bin\mainwnd.obj bin\treeview.obj bin\fileview.obj bin\fileops.obj } >> bin\compile.log 2>&1

echo. >> bin\compile.log
echo Resource compile... >> bin\compile.log
wrc -r -I%WATCOM%\h\os2 -I%WATCOM%\h -fo=bin\warpnav.res src\warpnav.rc >> bin\compile.log 2>&1
wrc bin\warpnav.res bin\warpnav.exe >> bin\compile.log 2>&1

echo. >> bin\compile.log
if exist bin\warpnav.exe echo BUILD SUCCESSFUL >> bin\compile.log
if not exist bin\warpnav.exe echo BUILD FAILED >> bin\compile.log
