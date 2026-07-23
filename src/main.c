#include "warpnav.h"

/* ── Shared globals ────────────────────────────────────────────────────────── */
HAB   g_hab           = NULLHANDLE;
HWND  g_hwndFrame     = NULLHANDLE;
HWND  g_hwndClient    = NULLHANDLE;
HWND  g_hwndTree      = NULLHANDLE;
HWND  g_hwndFiles     = NULLHANDLE;
HWND  g_hwndPath      = NULLHANDLE;
CHAR  g_szCurPath[CCHMAXPATH] = "C:\\";
INT   g_viewMode      = 0;     /* 0=icons  1=list  2=details */
INT   g_splitX        = 180;

int main(void)
{
    HMQ   hmq;
    QMSG  qmsg;
    ULONG flCreate;

    /*
     * PM startup sequence (must follow this order):
     *   1. WinInitialize  – connect to PM; returns the anchor-block handle (HAB)
     *   2. WinCreateMsgQueue – create the thread's message queue
     *   3. WinRegisterClass  – register any custom window classes
     *   4. WinCreateStdWindow / WinCreateWindow – create the window hierarchy
     *   5. Message loop (WinGetMsg / WinDispatchMsg)
     *   6. Cleanup in reverse: DestroyWindow → DestroyMsgQueue → WinTerminate
     */
    g_hab = WinInitialize(0);
    if (!g_hab) return 1;

    hmq = WinCreateMsgQueue(g_hab, 0);
    if (!hmq) { WinTerminate(g_hab); return 1; }

    WinRegisterClass(g_hab, WC_WARPNAVCLIENT, ClientWndProc,
                     CS_SIZEREDRAW | CS_CLIPCHILDREN, 0);

    flCreate = FCF_TITLEBAR    | FCF_SYSMENU     | FCF_MINMAX    |
               FCF_SIZEBORDER  | FCF_MENU         | FCF_TASKLIST  |
               FCF_SHELLPOSITION | FCF_ACCELTABLE;

    /*
     * WinCreateStdWindow creates a frame + client window pair.
     * The RETURN VALUE is the frame window handle.
     * The last argument receives the CLIENT window handle (our WndProc window).
     * FCF_MENU + resource ID loads the menu from the .rc file.
     * FCF_ACCELTABLE loads the matching accelerator table (same resource ID).
     */
    g_hwndFrame = WinCreateStdWindow(
        HWND_DESKTOP, WS_VISIBLE, &flCreate,
        WC_WARPNAVCLIENT, "Warp Navigator 1.0",
        0, NULLHANDLE, IDR_MAINMENU, &g_hwndClient);

    if (!g_hwndFrame) {
        WinDestroyMsgQueue(hmq);
        WinTerminate(g_hab);
        return 1;
    }

    /* DosQueryCurrentDisk returns the 1-based drive number (1=A, 2=B, 3=C …) */
    {
        ULONG ulDisk, ulMap;
        DosQueryCurrentDisk(&ulDisk, &ulMap);
        g_szCurPath[0] = (CHAR)('A' + ulDisk - 1);
        g_szCurPath[1] = ':';
        g_szCurPath[2] = '\\';
        g_szCurPath[3] = '\0';
    }

    WinSetWindowPos(g_hwndFrame, HWND_TOP, 60, 60, 780, 540,
                    SWP_SIZE | SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);

    NavigateTo(g_szCurPath);

    while (WinGetMsg(g_hab, &qmsg, NULLHANDLE, 0, 0))
        WinDispatchMsg(g_hab, &qmsg);

    WinDestroyWindow(g_hwndFrame);
    WinDestroyMsgQueue(hmq);
    WinTerminate(g_hab);
    return 0;
}
