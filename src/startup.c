/*
 * startup.c  --  GCC + wlink entry point for WarpNav
 *
 * When linking with wlink (not kLIBC), there is no C runtime startup stub
 * to call main().  This file provides the OS/2 EXE entry point wn_start(),
 * which calls main() and then calls DosExit() so the process terminates
 * cleanly instead of returning to an undefined address.
 *
 * wlink option: option start=wn_start
 *
 * Only compiled into the GCC build (compile-gcc.cmd / Makefile.gcc).
 */

#define INCL_DOSPROCESS
#include <os2.h>

extern int main(void);

void wn_start(void)
{
    DosExit(EXIT_PROCESS, (ULONG)main());
}
