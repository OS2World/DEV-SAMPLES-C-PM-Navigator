# WarpNav – OpenWatcom wmake build file
# Usage:  wmake
# Output: bin\warpnav.exe

CC     = wcc386
LINK   = wlink
RC     = wrc
RM     = del /f /q

CFLAGS = -bt=os2v2 -mf -3 -sg -zp4 -we -w4 -ei &
         -I$(%WATCOM)\h\os2 -I$(%WATCOM)\h

SRCS = src\main.c      &
       src\mainwnd.c   &
       src\treeview.c  &
       src\fileview.c  &
       src\fileops.c

OBJS = bin\main.obj      &
       bin\mainwnd.obj   &
       bin\treeview.obj  &
       bin\fileview.obj  &
       bin\fileops.obj

EXE  = bin\warpnav.exe
RES  = bin\warpnav.res

all : dirs $(EXE) .SYMBOLIC

dirs : .SYMBOLIC
    @if not exist bin md bin

$(EXE) : $(OBJS) $(RES)
    $(LINK) system os2v2_pm name $(EXE) file { $(OBJS) }
    $(RC) $(RES) $(EXE)

$(RES) : src\warpnav.rc src\warpnav.h
    $(RC) -r -I$(%WATCOM)\h\os2 -I$(%WATCOM)\h src\warpnav.rc $@

bin\main.obj : src\main.c src\warpnav.h
    $(CC) $(CFLAGS) -fo=$@ src\main.c

bin\mainwnd.obj : src\mainwnd.c src\warpnav.h
    $(CC) $(CFLAGS) -fo=$@ src\mainwnd.c

bin\treeview.obj : src\treeview.c src\warpnav.h
    $(CC) $(CFLAGS) -fo=$@ src\treeview.c

bin\fileview.obj : src\fileview.c src\warpnav.h
    $(CC) $(CFLAGS) -fo=$@ src\fileview.c

bin\fileops.obj : src\fileops.c src\warpnav.h
    $(CC) $(CFLAGS) -fo=$@ src\fileops.c

clean : .SYMBOLIC
    -$(RM) $(OBJS)
    -$(RM) $(RES)
    -$(RM) $(EXE)
