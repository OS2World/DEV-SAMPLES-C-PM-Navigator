#include "warpnav.h"

static CHAR s_hist[MAX_HISTORY][CCHMAXPATH];
static INT  s_histIdx   = -1;
static INT  s_histCount =  0;

static LONG s_cx = 0, s_cy = 0;
static BOOL s_bDrag   = FALSE;
static LONG s_dragOfs = 0;

static HWND s_hwndBtnBack    = NULLHANDLE;
static HWND s_hwndBtnFwd     = NULLHANDLE;
static HWND s_hwndBtnUp      = NULLHANDLE;
static HWND s_hwndBtnRefresh = NULLHANDLE;
static HWND s_hwndBtnIcons   = NULLHANDLE;
static HWND s_hwndBtnList    = NULLHANDLE;
static HWND s_hwndBtnDetails = NULLHANDLE;

static PFNWP s_pfnOrigPath    = NULL;
static PFNWP s_pfnOrigBtn     = NULL;
static BOOL  s_btnHilite[24];        /* indexed by control ID (IDs 14-19) */
static BOOL  s_bEmbedMode     = FALSE;
static HWND  s_hwndEmbedded   = NULLHANDLE;
static HWND  s_hwndEmbedOldParent = NULLHANDLE;
static LONG  s_xSep1 = 0;   /* x of toolbar group separator after nav buttons */
static LONG  s_xSep2 = 0;   /* x of toolbar group separator after path bar */

/* Status bar three-column text */
static CHAR s_szStat1[300] = "  Ready";
static CHAR s_szStat2[64]  = "";
static CHAR s_szStat3[CCHMAXPATH + 4] = "";

static VOID InvalidateStatus(VOID)
{
    RECTL rcl;
    if (!g_hwndClient) return;
    rcl.xLeft   = 0;   rcl.xRight  = s_cx;
    rcl.yBottom = 0;   rcl.yTop    = STATUSBAR_H;
    WinInvalidateRect(g_hwndClient, &rcl, FALSE);
}

/* unit circle * 100 for 0,60,120,180,240,300 deg  (used by refresh icon) */
static LONG s_ux[] = {100,  50, -50, -100, -50,  50};
static LONG s_uy[] = {  0,  87,  87,    0, -87, -87};

static VOID FillTriangle(HPS hps, POINTL *pts)
{
    POLYGON poly;
    poly.ulPoints = 3;
    poly.aPointl  = pts;
    GpiMove(hps, &pts[0]);
    GpiPolygons(hps, 1, &poly, POLYGON_FILL | POLYGON_BOUNDARY, POLYGON_INCL);
}

static VOID DrawToolButton(USHORT usId, HPS hps, PRECTL prcl, ULONG fsState)
{
    BOOL   bDown;
    LONG   cx, cy, mx, my, s;
    RECTL  rcl;
    POINTL pt;
    POINTL pts[3];

    bDown = (fsState & BDS_HILITED) != 0;

    /* for view toggle buttons, treat the active mode as "depressed" */
    if (usId == ID_BTN_ICONS   && g_viewMode == 0) bDown = TRUE;
    if (usId == ID_BTN_LIST    && g_viewMode == 1) bDown = TRUE;
    if (usId == ID_BTN_DETAILS && g_viewMode == 2) bDown = TRUE;

    cx = prcl->xRight - prcl->xLeft;
    cy = prcl->yTop   - prcl->yBottom;
    mx = cx / 2 + (bDown ? 1 : 0);
    my = cy / 2 - (bDown ? 1 : 0);
    s  = (cy < cx ? cy : cx) / 4;
    if (s < 3) s = 3;
    rcl = *prcl;

    /* background */
    WinFillRect(hps, &rcl, SYSCLR_BUTTONMIDDLE);

    /* 3-D border: highlight top-left, shadow bottom-right */
    GpiSetMix(hps, FM_OVERPAINT);
    GpiSetColor(hps, bDown ? SYSCLR_BUTTONDARK : SYSCLR_BUTTONLIGHT);
    pt.x = rcl.xLeft;      pt.y = rcl.yBottom;    GpiMove(hps, &pt);
    pt.x = rcl.xLeft;      pt.y = rcl.yTop - 1;   GpiLine(hps, &pt);
    pt.x = rcl.xRight - 1; pt.y = rcl.yTop - 1;   GpiLine(hps, &pt);
    GpiSetColor(hps, bDown ? SYSCLR_BUTTONLIGHT : SYSCLR_BUTTONDARK);
    pt.x = rcl.xRight - 1; pt.y = rcl.yTop - 1;   GpiMove(hps, &pt);
    pt.x = rcl.xRight - 1; pt.y = rcl.yBottom;     GpiLine(hps, &pt);
    pt.x = rcl.xLeft;      pt.y = rcl.yBottom;     GpiLine(hps, &pt);

    GpiSetColor(hps, CLR_BLACK);

    switch (usId) {

    case ID_BTN_BACK:
        /* solid left-pointing triangle */
        pts[0].x = mx - s; pts[0].y = my;
        pts[1].x = mx + s; pts[1].y = my + s;
        pts[2].x = mx + s; pts[2].y = my - s;
        FillTriangle(hps, pts);
        break;

    case ID_BTN_FWD:
        /* solid right-pointing triangle */
        pts[0].x = mx + s; pts[0].y = my;
        pts[1].x = mx - s; pts[1].y = my + s;
        pts[2].x = mx - s; pts[2].y = my - s;
        FillTriangle(hps, pts);
        break;

    case ID_BTN_UP:
    {
        /* upward triangle + stem */
        RECTL rs;
        pts[0].x = mx;     pts[0].y = my + s;
        pts[1].x = mx + s; pts[1].y = my;
        pts[2].x = mx - s; pts[2].y = my;
        FillTriangle(hps, pts);
        rs.xLeft   = mx - s/3; rs.xRight  = mx + s/3;
        rs.yBottom = my - s;   rs.yTop    = my + 1;
        WinFillRect(hps, &rs, CLR_BLACK);
        break;
    }

    case ID_BTN_REFRESH:
    {
        /* 5-segment arc + small arrowhead at bottom-right */
        POINTL apts[5];
        POINTL tip[3];
        POLYGON tpoly;
        INT i;
        for (i = 0; i < 5; i++) {
            apts[i].x = mx + (LONG)s_ux[i] * s / 100;
            apts[i].y = my + (LONG)s_uy[i] * s / 100;
        }
        GpiMove(hps, &apts[0]);
        for (i = 1; i < 5; i++) GpiLine(hps, &apts[i]);
        /* arrowhead pointing clockwise from last segment */
        tip[0].x = apts[4].x;        tip[0].y = apts[4].y;
        tip[1].x = apts[4].x + s/2;  tip[1].y = apts[4].y + s/3;
        tip[2].x = apts[4].x + s/2;  tip[2].y = apts[4].y - s/3;
        GpiMove(hps, &tip[0]);
        tpoly.ulPoints = 3; tpoly.aPointl = tip;
        GpiPolygons(hps, 1, &tpoly, POLYGON_FILL, POLYGON_INCL);
        break;
    }

    case ID_BTN_ICONS:
    {
        /* 2×2 grid of small squares */
        RECTL r;
        LONG  d = s - 1;
        if (d < 1) d = 1;
        r.xLeft = mx-s; r.xRight = mx-1; r.yBottom = my+1;   r.yTop = my+d+1;
        WinFillRect(hps, &r, CLR_BLACK);
        r.xLeft = mx+1; r.xRight = mx+s;
        WinFillRect(hps, &r, CLR_BLACK);
        r.xLeft = mx-s; r.xRight = mx-1; r.yBottom = my-d;   r.yTop = my;
        WinFillRect(hps, &r, CLR_BLACK);
        r.xLeft = mx+1; r.xRight = mx+s;
        WinFillRect(hps, &r, CLR_BLACK);
        break;
    }

    case ID_BTN_LIST:
    {
        /* small square + line per row (list view with mini icons) */
        RECTL r;
        LONG  sq = s / 2;
        INT   i;
        for (i = -1; i <= 1; i++) {
            LONG y = my + i * (s - 1);
            r.xLeft = mx - s; r.xRight = mx - s + sq + 1;
            r.yBottom = y - sq/2; r.yTop = y + sq/2 + 1;
            WinFillRect(hps, &r, CLR_BLACK);
            r.xLeft = mx - s + sq + 3; r.xRight = mx + s;
            r.yBottom = y; r.yTop = y + 2;
            WinFillRect(hps, &r, CLR_BLACK);
        }
        break;
    }

    case ID_BTN_DETAILS:
    {
        /* column grid: narrow left column + wide right column, 3 rows */
        RECTL r;
        LONG  col1w = s / 2;
        INT   i;
        for (i = -1; i <= 1; i++) {
            LONG y = my + i * (s - 1);
            r.xLeft = mx - s;          r.xRight = mx - s + col1w;
            r.yBottom = y;             r.yTop   = y + 2;
            WinFillRect(hps, &r, CLR_BLACK);
            r.xLeft = mx - s + col1w + 2; r.xRight = mx + s;
            WinFillRect(hps, &r, CLR_BLACK);
        }
        break;
    }

    default:
        break;
    }
}

static USHORT BtnHwndToId(HWND hwnd)
{
    if (hwnd == s_hwndBtnBack)    return ID_BTN_BACK;
    if (hwnd == s_hwndBtnFwd)     return ID_BTN_FWD;
    if (hwnd == s_hwndBtnUp)      return ID_BTN_UP;
    if (hwnd == s_hwndBtnRefresh) return ID_BTN_REFRESH;
    if (hwnd == s_hwndBtnIcons)   return ID_BTN_ICONS;
    if (hwnd == s_hwndBtnList)    return ID_BTN_LIST;
    if (hwnd == s_hwndBtnDetails) return ID_BTN_DETAILS;
    return 0;
}

static MRESULT EXPENTRY ToolBtnSubProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    USHORT usId = BtnHwndToId(hwnd);

    if (msg == WM_BUTTON1DOWN) {
        if (usId) s_btnHilite[usId] = TRUE;
        WinInvalidateRect(hwnd, NULL, FALSE);
        return s_pfnOrigBtn(hwnd, msg, mp1, mp2);
    }
    if (msg == WM_BUTTON1UP) {
        if (usId) s_btnHilite[usId] = FALSE;
        WinInvalidateRect(hwnd, NULL, FALSE);
        return s_pfnOrigBtn(hwnd, msg, mp1, mp2);
    }
    if (msg == WM_PAINT) {
        HPS   hps;
        RECTL rcl;
        ULONG fsState;
        fsState = (usId && s_btnHilite[usId]) ? BDS_HILITED : 0;
        hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
        WinQueryWindowRect(hwnd, &rcl);
        DrawToolButton(usId, hps, &rcl, fsState);
        WinEndPaint(hps);
        return 0;
    }
    return s_pfnOrigBtn(hwnd, msg, mp1, mp2);
}

static MRESULT EXPENTRY PathEntrySubProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    if (msg == WM_CHAR) {
        USHORT fsFlags = SHORT1FROMMP(mp1);
        USHORT vkey    = SHORT2FROMMP(mp2);
        if ((fsFlags & KC_VIRTUALKEY) && (fsFlags & KC_KEYUP) == 0
            && vkey == VK_ENTER)
        {
            CHAR szPath[CCHMAXPATH];
            INT  len;
            WinQueryWindowText(hwnd, sizeof(szPath), szPath);
            len = (INT)strlen(szPath);
            if (len > 0 && szPath[len - 1] != '\\') {
                szPath[len]     = '\\';
                szPath[len + 1] = '\0';
            }
            NavigateTo(szPath);
            return (MRESULT)TRUE;
        }
    }
    return s_pfnOrigPath(hwnd, msg, mp1, mp2);
}

static VOID PushHistory(PSZ pszPath)
{
    s_histCount = s_histIdx + 1;
    if (s_histCount >= MAX_HISTORY) {
        memmove(s_hist[0], s_hist[1], (MAX_HISTORY - 1) * CCHMAXPATH);
        s_histCount--;
        s_histIdx = s_histCount - 1;
    }
    strncpy(s_hist[s_histCount], pszPath, CCHMAXPATH - 1);
    s_histIdx   = s_histCount;
    s_histCount = s_histCount + 1;
}

static VOID UpdateNavButtons(VOID)
{
    BOOL bAtRoot = (strlen(g_szCurPath) <= 3);
    WinEnableWindow(s_hwndBtnBack, s_histIdx > 0);
    WinEnableWindow(s_hwndBtnFwd,  s_histIdx < s_histCount - 1);
    WinEnableWindow(s_hwndBtnUp, !bAtRoot);
}

VOID LayoutChildren(HWND hwnd, LONG cx, LONG cy)
{
    LONG contentY = STATUSBAR_H;
    LONG contentH = cy - TOOLBAR_H - STATUSBAR_H;
    LONG filesX;
    LONG tbY, btnY, x;
    LONG pathW, pathY;

    if (contentH < 1) contentH = 1;

    if (g_splitX < MIN_PANEL)                    g_splitX = MIN_PANEL;
    if (g_splitX > cx - MIN_PANEL - SPLITTER_W)  g_splitX = cx - MIN_PANEL - SPLITTER_W;

    WinSetWindowPos(g_hwndTree, NULLHANDLE,
        0, contentY, g_splitX, contentH,
        SWP_MOVE | SWP_SIZE | SWP_NOADJUST);

    filesX = g_splitX + SPLITTER_W;
    if (s_hwndEmbedded != NULLHANDLE) {
        WinShowWindow(g_hwndFiles, FALSE);
        WinSetWindowPos(s_hwndEmbedded, NULLHANDLE,
            filesX, contentY, cx - filesX, contentH,
            SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_NOADJUST);
    } else {
        WinShowWindow(g_hwndFiles, TRUE);
        WinSetWindowPos(g_hwndFiles, NULLHANDLE,
            filesX, contentY, cx - filesX, contentH,
            SWP_MOVE | SWP_SIZE | SWP_NOADJUST);
    }

    tbY  = cy - TOOLBAR_H;
    btnY = tbY + (TOOLBAR_H - BTN_H) / 2;
    x    = 4;

    WinSetWindowPos(s_hwndBtnBack,    NULLHANDLE, x, btnY, BTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST); x += BTN_W + 2;
    WinSetWindowPos(s_hwndBtnFwd,     NULLHANDLE, x, btnY, BTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST); x += BTN_W + 2;
    WinSetWindowPos(s_hwndBtnUp,      NULLHANDLE, x, btnY, BTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST); x += BTN_W + 2;
    WinSetWindowPos(s_hwndBtnRefresh, NULLHANDLE, x, btnY, BTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST); x += BTN_W + 4;
    s_xSep1 = x;
    x += 6;   /* 2px groove + 2px margin each side */

    pathW = cx - x - (VIEWBTN_W * 3 + 20);
    if (pathW < 40) pathW = 40;
    pathY = btnY;
    WinSetWindowPos(g_hwndPath, NULLHANDLE, x, pathY, pathW, BTN_H,
        SWP_MOVE | SWP_SIZE | SWP_NOADJUST);
    x += pathW + 4;
    s_xSep2 = x;
    x += 6;   /* 2px groove + 2px margin each side */

    WinSetWindowPos(s_hwndBtnIcons,   NULLHANDLE, x,                     btnY, VIEWBTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST);
    WinSetWindowPos(s_hwndBtnList,    NULLHANDLE, x + VIEWBTN_W + 2,     btnY, VIEWBTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST);
    WinSetWindowPos(s_hwndBtnDetails, NULLHANDLE, x + (VIEWBTN_W + 2)*2, btnY, VIEWBTN_W, BTN_H, SWP_MOVE | SWP_SIZE | SWP_NOADJUST);

    WinInvalidateRect(hwnd, NULL, FALSE);
}

VOID NavigateTo(PSZ pszPath)
{
    INT len;

    strncpy(g_szCurPath, pszPath, CCHMAXPATH - 1);
    len = (INT)strlen(g_szCurPath);
    if (len > 0 && g_szCurPath[len - 1] != '\\') {
        g_szCurPath[len]     = '\\';
        g_szCurPath[len + 1] = '\0';
    }

    PushHistory(g_szCurPath);
    ClearFiles();
    PopulateFiles(g_szCurPath);
    UpdatePathBar();
    UpdateNavButtons();
    UpdateStatus();
}

VOID NavigateBack(VOID)
{
    if (s_histIdx <= 0) return;
    s_histIdx--;
    strncpy(g_szCurPath, s_hist[s_histIdx], CCHMAXPATH - 1);
    ClearFiles();
    PopulateFiles(g_szCurPath);
    UpdatePathBar();
    UpdateNavButtons();
    UpdateStatus();
}

VOID NavigateForward(VOID)
{
    if (s_histIdx >= s_histCount - 1) return;
    s_histIdx++;
    strncpy(g_szCurPath, s_hist[s_histIdx], CCHMAXPATH - 1);
    ClearFiles();
    PopulateFiles(g_szCurPath);
    UpdatePathBar();
    UpdateNavButtons();
    UpdateStatus();
}

VOID NavigateUp(VOID)
{
    CHAR  szParent[CCHMAXPATH];
    INT   len;
    CHAR *p;

    strncpy(szParent, g_szCurPath, CCHMAXPATH - 1);
    len = (INT)strlen(szParent);
    if (len > 0 && szParent[len - 1] == '\\') { szParent[--len] = '\0'; }
    p = strrchr(szParent, '\\');
    if (!p) return;
    *(p + 1) = '\0';
    NavigateTo(szParent);
}

VOID UpdatePathBar(VOID)
{
    WinSetWindowText(g_hwndPath, (PCSZ)g_szCurPath);
}

VOID UpdateStatus(VOID)
{
    CNRINFO     cnri;
    PFILERECORD prec;

    prec = GetCursoredFile();
    if (prec) {
        sprintf(s_szStat1, "  \"%s\" (%s)", prec->szName, prec->szType);
        if (prec->bDir)
            s_szStat2[0] = '\0';
        else
            sprintf(s_szStat2, "%s", prec->szSize);
        sprintf(s_szStat3, "%s  ", prec->szDate);
    } else {
        memset(&cnri, 0, sizeof(cnri));
        cnri.cb = sizeof(cnri);
        WinSendMsg(g_hwndFiles, CM_QUERYCNRINFO,
                   MPFROMP(&cnri), MPFROMLONG(sizeof(cnri)));
        sprintf(s_szStat1, "  %lu object(s)", cnri.cRecords);
        s_szStat2[0] = '\0';
        sprintf(s_szStat3, "%s  ", g_szCurPath);
    }
    InvalidateStatus();
}

VOID SetViewMode(INT iMode)
{
    CNRINFO cnri;
    ULONG   flView;

    g_viewMode = iMode;

    switch (iMode) {
    case 0:  flView = CV_ICON | CA_AUTOPOSITION;        break;
    case 2:  flView = CV_DETAIL | CA_DETAILSVIEWTITLES; break;
    default: flView = CV_NAME;                          break;
    }

    memset(&cnri, 0, sizeof(cnri));
    cnri.cb           = sizeof(cnri);
    cnri.flWindowAttr = flView;
    WinSendMsg(g_hwndFiles, CM_SETCNRINFO,
               MPFROMP(&cnri), MPFROMLONG(CMA_FLWINDOWATTR));

    /*
     * CM_ARRANGE re-tiles existing icon records into a grid.  It must be sent
     * AFTER CM_SETCNRINFO when records are already in the container; otherwise
     * icons stay wherever they were and do not form a grid.
     */
    if (iMode == 0)
        WinSendMsg(g_hwndFiles, CM_ARRANGE, NULL, NULL);

    WinInvalidateRect(s_hwndBtnIcons,   NULL, FALSE);
    WinInvalidateRect(s_hwndBtnList,    NULL, FALSE);
    WinInvalidateRect(s_hwndBtnDetails, NULL, FALSE);
}

static VOID ReleaseEmbedded(VOID)
{
    if (s_hwndEmbedded == NULLHANDLE) return;
    WinSetParent(s_hwndEmbedded, s_hwndEmbedOldParent, FALSE);
    WinShowWindow(s_hwndEmbedded, TRUE);
    s_hwndEmbedded       = NULLHANDLE;
    s_hwndEmbedOldParent = NULLHANDLE;
    WinShowWindow(g_hwndFiles, TRUE);
    LayoutChildren(g_hwndClient, s_cx, s_cy);
}

static VOID EmbedWindow(HWND hwndNew)
{
    if (s_hwndEmbedded != NULLHANDLE) ReleaseEmbedded();
    s_hwndEmbedOldParent = WinQueryWindow(hwndNew, QW_PARENT);
    s_hwndEmbedded       = hwndNew;
    WinSetParent(hwndNew, g_hwndClient, FALSE);
    WinShowWindow(g_hwndFiles, FALSE);
    LayoutChildren(g_hwndClient, s_cx, s_cy);
    WinShowWindow(hwndNew, TRUE);
}

static VOID ShowContextMenu(PFILERECORD prec, HWND hwndOwner)
{
    HWND   hwndMenu;
    POINTL ptl;

    hwndMenu = WinLoadMenu(hwndOwner, NULLHANDLE, IDR_CONTEXT);
    if (!hwndMenu) return;

    if (!prec) {
        WinSendMsg(hwndMenu, MM_SETITEMATTR,
            MPFROM2SHORT(IDM_FILE_OPEN,   TRUE),
            MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));
        WinSendMsg(hwndMenu, MM_SETITEMATTR,
            MPFROM2SHORT(IDM_SEL_PROPS,   TRUE),
            MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));
        WinSendMsg(hwndMenu, MM_SETITEMATTR,
            MPFROM2SHORT(IDM_FILE_DELETE, TRUE),
            MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));
        WinSendMsg(hwndMenu, MM_SETITEMATTR,
            MPFROM2SHORT(IDM_FILE_RENAME, TRUE),
            MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));
    }

    WinQueryPointerPos(HWND_DESKTOP, &ptl);
    WinPopupMenu(HWND_DESKTOP, hwndOwner, hwndMenu,
                 ptl.x, ptl.y, 0, PUM_KEYBOARD);
}

MRESULT EXPENTRY ClientWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    switch (msg) {

    case WM_CREATE:
    {
        g_hwndTree = WinCreateWindow(hwnd, WC_CONTAINER, "",
            WS_VISIBLE | CCS_READONLY | CCS_MINIRECORDCORE,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_TREEVIEW, NULL, NULL);

        g_hwndFiles = WinCreateWindow(hwnd, WC_CONTAINER, "",
            WS_VISIBLE,   /* no CCS_MINIRECORDCORE — needed for CV_ICON */
            0, 0, 0, 0, hwnd, HWND_TOP, ID_FILEVIEW, NULL, NULL);

        SetupFileColumns();

        /* view mode is applied by PopulateFiles on first NavigateTo */
        /* tree view is configured inside PopulateDrives via CM_SETCNRINFO */

        s_hwndBtnBack = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_BACK, NULL, NULL);
        s_hwndBtnFwd = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_FWD, NULL, NULL);
        s_hwndBtnUp = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_UP, NULL, NULL);
        s_hwndBtnRefresh = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_REFRESH, NULL, NULL);

        g_hwndPath = WinCreateWindow(hwnd, WC_ENTRYFIELD, "",
            WS_VISIBLE | ES_AUTOSCROLL | ES_MARGIN,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_PATH, NULL, NULL);
        WinSendMsg(g_hwndPath, EM_SETTEXTLIMIT, MPFROMSHORT(CCHMAXPATH - 1), 0);
        s_pfnOrigPath = WinSubclassWindow(g_hwndPath, PathEntrySubProc);

        s_hwndBtnIcons = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_ICONS, NULL, NULL);
        s_hwndBtnList = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_LIST, NULL, NULL);
        s_hwndBtnDetails = WinCreateWindow(hwnd, WC_BUTTON, "",
            WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, HWND_TOP, ID_BTN_DETAILS, NULL, NULL);

        /*
         * WinSubclassWindow replaces a window's procedure with our own and
         * returns the original PFNWP so we can forward unhandled messages.
         * We call it on the first button and reuse that one saved PFNWP for
         * all buttons — they all share the same original WC_BUTTON procedure.
         */
        s_pfnOrigBtn = WinSubclassWindow(s_hwndBtnBack,    ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnFwd,     ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnUp,      ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnRefresh, ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnIcons,   ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnList,    ToolBtnSubProc);
        WinSubclassWindow(s_hwndBtnDetails, ToolBtnSubProc);

        PopulateDrives();
        UpdateNavButtons();
        return 0;
    }

    case WM_SIZE:
        s_cx = SHORT1FROMMP(mp2);
        s_cy = SHORT2FROMMP(mp2);
        LayoutChildren(hwnd, s_cx, s_cy);
        return 0;

    case WM_PAINT:
    {
        /*
         * PM coordinate system: Y increases UPWARD.  (0,0) is the bottom-left
         * corner of the client area — the opposite of Windows.  STATUSBAR_H
         * pixels above the bottom is where the status bar lives; the toolbar
         * sits at the top (s_cy - TOOLBAR_H).
         */
        HPS    hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
        RECTL  rcl;
        POINTL pt;
        LONG   tbY  = s_cy - TOOLBAR_H;
        LONG   btnY = tbY + (TOOLBAR_H - BTN_H) / 2;
        LONG   sepY = btnY - 2;
        LONG   sepH = BTN_H + 4;

        /* ── toolbar background ────────────────────────────────────────── */
        rcl.xLeft = 0; rcl.xRight = s_cx;
        rcl.yBottom = tbY; rcl.yTop = s_cy;
        WinFillRect(hps, &rcl, SYSCLR_BUTTONMIDDLE);

        GpiSetMix(hps, FM_OVERPAINT);

        /* toolbar bottom edge: raised ridge (light above, dark below) */
        GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
        pt.x = 0; pt.y = tbY + 1; GpiMove(hps, &pt);
        pt.x = s_cx; GpiLine(hps, &pt);
        GpiSetColor(hps, SYSCLR_BUTTONDARK);
        pt.x = 0; pt.y = tbY; GpiMove(hps, &pt);
        pt.x = s_cx; GpiLine(hps, &pt);

        /* toolbar group separators (dark line + light line = groove) */
        if (s_xSep1 > 0) {
            GpiSetColor(hps, SYSCLR_BUTTONDARK);
            pt.x = s_xSep1;     pt.y = sepY;        GpiMove(hps, &pt);
            pt.x = s_xSep1;     pt.y = sepY + sepH; GpiLine(hps, &pt);
            GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
            pt.x = s_xSep1 + 1; pt.y = sepY;        GpiMove(hps, &pt);
            pt.x = s_xSep1 + 1; pt.y = sepY + sepH; GpiLine(hps, &pt);
        }
        if (s_xSep2 > 0) {
            GpiSetColor(hps, SYSCLR_BUTTONDARK);
            pt.x = s_xSep2;     pt.y = sepY;        GpiMove(hps, &pt);
            pt.x = s_xSep2;     pt.y = sepY + sepH; GpiLine(hps, &pt);
            GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
            pt.x = s_xSep2 + 1; pt.y = sepY;        GpiMove(hps, &pt);
            pt.x = s_xSep2 + 1; pt.y = sepY + sepH; GpiLine(hps, &pt);
        }

        /* ── splitter: 3-D raised strip ───────────────────────────────── */
        rcl.xLeft   = g_splitX + 1;
        rcl.xRight  = g_splitX + SPLITTER_W - 1;
        rcl.yBottom = STATUSBAR_H;
        rcl.yTop    = tbY;
        WinFillRect(hps, &rcl, SYSCLR_BUTTONMIDDLE);

        GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
        pt.x = g_splitX; pt.y = STATUSBAR_H; GpiMove(hps, &pt);
        pt.x = g_splitX; pt.y = tbY;         GpiLine(hps, &pt);

        GpiSetColor(hps, SYSCLR_BUTTONDARK);
        pt.x = g_splitX + SPLITTER_W - 1; pt.y = STATUSBAR_H; GpiMove(hps, &pt);
        pt.x = g_splitX + SPLITTER_W - 1; pt.y = tbY;         GpiLine(hps, &pt);

        /* ── status bar ───────────────────────────────────────────────── */
        /* sunken separator at the top edge of the status bar */
        GpiSetColor(hps, SYSCLR_BUTTONDARK);
        pt.x = 0; pt.y = STATUSBAR_H;     GpiMove(hps, &pt);
        pt.x = s_cx;                       GpiLine(hps, &pt);
        GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
        pt.x = 0; pt.y = STATUSBAR_H - 1; GpiMove(hps, &pt);
        pt.x = s_cx;                       GpiLine(hps, &pt);

        /*
         * Status bar: three columns drawn with WinDrawText.
         *
         * DT_VCENTER vertically centres text in the rectangle.
         * OS/2 PM has no DT_SINGLELINE (that is a Windows-only flag);
         * DT_VCENTER works on its own to centre a single line of text.
         *
         * DT_ERASERECT fills the rectangle with the background colour
         * before drawing the text, so we do not need a separate WinFillRect.
         *
         * Passing cchText=-1 tells WinDrawText to measure the string with
         * strlen internally.
         */
        if (s_cx > 0) {
            RECTL rclS;
            LONG  x1 = (s_cx * 55L) / 100L;
            LONG  x2 = (s_cx * 76L) / 100L;

            rclS.yBottom = 1; rclS.yTop = STATUSBAR_H - 2;

            rclS.xLeft = 0;   rclS.xRight = x1;
            WinDrawText(hps, -1, s_szStat1, &rclS,
                        CLR_BLACK, SYSCLR_BUTTONMIDDLE,
                        DT_LEFT | DT_VCENTER | DT_ERASERECT);

            rclS.xLeft = x1;  rclS.xRight = x2;
            WinDrawText(hps, -1, s_szStat2, &rclS,
                        CLR_BLACK, SYSCLR_BUTTONMIDDLE,
                        DT_RIGHT | DT_VCENTER | DT_ERASERECT);

            rclS.xLeft = x2;  rclS.xRight = s_cx;
            WinDrawText(hps, -1, s_szStat3, &rclS,
                        CLR_BLACK, SYSCLR_BUTTONMIDDLE,
                        DT_RIGHT | DT_VCENTER | DT_ERASERECT);

            /* column groove separators */
            GpiSetColor(hps, SYSCLR_BUTTONDARK);
            pt.x = x1;     pt.y = 3; GpiMove(hps, &pt);
            pt.x = x1;     pt.y = STATUSBAR_H - 3; GpiLine(hps, &pt);
            pt.x = x2;     pt.y = 3; GpiMove(hps, &pt);
            pt.x = x2;     pt.y = STATUSBAR_H - 3; GpiLine(hps, &pt);
            GpiSetColor(hps, SYSCLR_BUTTONLIGHT);
            pt.x = x1 + 1; pt.y = 3; GpiMove(hps, &pt);
            pt.x = x1 + 1; pt.y = STATUSBAR_H - 3; GpiLine(hps, &pt);
            pt.x = x2 + 1; pt.y = 3; GpiMove(hps, &pt);
            pt.x = x2 + 1; pt.y = STATUSBAR_H - 3; GpiLine(hps, &pt);
        }

        WinEndPaint(hps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        LONG x = SHORT1FROMMP(mp1);
        if (s_bEmbedMode) {
            WinSetPointer(HWND_DESKTOP,
                WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE));
            return (MRESULT)TRUE;
        }
        if (s_bDrag) {
            g_splitX = x - s_dragOfs;
            LayoutChildren(hwnd, s_cx, s_cy);
        } else if (x >= g_splitX && x < g_splitX + SPLITTER_W) {
            WinSetPointer(HWND_DESKTOP,
                WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE));
            return (MRESULT)TRUE;
        }
        break;
    }
    case WM_BUTTON1DOWN:
    {
        LONG x = SHORT1FROMMP(mp1);
        if (x >= g_splitX && x < g_splitX + SPLITTER_W) {
            s_bDrag   = TRUE;
            s_dragOfs = x - g_splitX;
            WinSetCapture(HWND_DESKTOP, hwnd);
            return (MRESULT)TRUE;
        }
        break;
    }
    case WM_BUTTON1UP:
        if (s_bEmbedMode) {
            POINTL ptl;
            HWND   hwndPick;
            HWND   hwndFrame;
            s_bEmbedMode = FALSE;
            WinSetCapture(HWND_DESKTOP, NULLHANDLE);
            WinQueryPointerPos(HWND_DESKTOP, &ptl);
            hwndPick = WinWindowFromPoint(HWND_DESKTOP, &ptl, TRUE);
            hwndFrame = hwndPick;
            while (hwndFrame != NULLHANDLE &&
                   WinQueryWindow(hwndFrame, QW_PARENT) != HWND_DESKTOP)
                hwndFrame = WinQueryWindow(hwndFrame, QW_PARENT);
            if (hwndFrame != NULLHANDLE && hwndFrame != g_hwndFrame)
                EmbedWindow(hwndFrame);
            else
                UpdateStatus();
        } else if (s_bDrag) {
            s_bDrag = FALSE;
            WinSetCapture(HWND_DESKTOP, NULLHANDLE);
        }
        break;

    case WM_CONTROL:
    {
        USHORT usId     = SHORT1FROMMP(mp1);
        USHORT usNotify = SHORT2FROMMP(mp1);

        if (usId == ID_TREEVIEW) {
            if (usNotify == CN_EXPANDTREE) {
                PTREERECORD prec = (PTREERECORD)PVOIDFROMMP(mp2);
                if (prec && !prec->bLoaded)
                    PopulateTreeChildren(prec);
            } else if (usNotify == CN_EMPHASIS) {
                PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)PVOIDFROMMP(mp2);
                if (pnre && pnre->pRecord &&
                    (pnre->fEmphasisMask & CRA_SELECTED) &&
                    (pnre->pRecord->flRecordAttr & CRA_SELECTED))
                {
                    PTREERECORD prec = (PTREERECORD)pnre->pRecord;
                    if (prec->szPath[0]) NavigateTo(prec->szPath);
                }
            } else if (usNotify == CN_ENTER) {
                PNOTIFYRECORDENTER pnre = (PNOTIFYRECORDENTER)PVOIDFROMMP(mp2);
                PTREERECORD prec = (PTREERECORD)pnre->pRecord;
                if (prec && prec->szPath[0]) NavigateTo(prec->szPath);
            }
        } else if (usId == ID_FILEVIEW) {
            if (usNotify == CN_ENTER) {
                PNOTIFYRECORDENTER pnre = PVOIDFROMMP(mp2);
                PFILERECORD        prec = (PFILERECORD)pnre->pRecord;
                if (prec) FileOpen(prec);
            } else if (usNotify == CN_CONTEXTMENU) {
                PFILERECORD prec = (PFILERECORD)PVOIDFROMMP(mp2);
                ShowContextMenu(prec, hwnd);
            } else if (usNotify == CN_EMPHASIS) {
                /*
                 * CN_EMPHASIS fires whenever a record gains or loses cursor or
                 * selection emphasis.  fEmphasisMask tells which attribute
                 * changed; we only care when the cursor or selection moves so
                 * we can refresh the status bar with the newly highlighted item.
                 */
                PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)PVOIDFROMMP(mp2);
                if (pnre && (pnre->fEmphasisMask & (CRA_CURSORED | CRA_SELECTED)))
                    UpdateStatus();
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        USHORT usCmd = SHORT1FROMMP(mp1);
        switch (usCmd) {
        case IDM_GO_BACK:
        case ID_BTN_BACK:       NavigateBack();    break;
        case IDM_GO_FORWARD:
        case ID_BTN_FWD:        NavigateForward(); break;
        case IDM_GO_UP:
        case ID_BTN_UP:         NavigateUp();      break;
        case IDM_VIEW_REFRESH:
        case ID_BTN_REFRESH:
            ClearFiles();
            PopulateFiles(g_szCurPath);
            UpdateStatus();
            break;
        case ID_BTN_ICONS:
        case IDM_VIEW_ICONS:    SetViewMode(0); break;
        case ID_BTN_LIST:
        case IDM_VIEW_LIST:     SetViewMode(1); break;
        case ID_BTN_DETAILS:
        case IDM_VIEW_DETAILS:  SetViewMode(2); break;
        case IDM_FILE_OPEN: {
            PFILERECORD prec = GetCursoredFile();
            if (prec) FileOpen(prec);
            break;
        }
        case IDM_SEL_PROPS: {
            PFILERECORD prec = GetCursoredFile();
            if (prec) FileProperties(prec);
            break;
        }
        case IDM_FILE_DELETE: {
            PFILERECORD prec = GetCursoredFile();
            if (prec) FileDelete(prec);
            break;
        }
        case IDM_FILE_RENAME: {
            PFILERECORD prec = GetCursoredFile();
            if (prec) FileRename(prec);
            break;
        }
        case IDM_VIEW_EMBED:
            s_bEmbedMode = TRUE;
            WinSetCapture(HWND_DESKTOP, hwnd);
            strncpy(s_szStat1, "  Click on a PM window to embed it in the right panel.",
                    sizeof(s_szStat1) - 1);
            s_szStat2[0] = '\0';
            s_szStat3[0] = '\0';
            InvalidateStatus();
            break;
        case IDM_VIEW_RELEASE:
            ReleaseEmbedded();
            UpdateStatus();
            break;
        case IDM_FILE_EXIT:
            WinPostMsg(g_hwndFrame, WM_CLOSE, 0, 0);
            break;
        case IDM_HELP_ABOUT:
            WinMessageBox(HWND_DESKTOP, hwnd,
                (PCSZ)"Warp Navigator 1.0\n\nA file manager for ArcaOS.\n\nhttps://www.os2world.com",
                (PCSZ)"About Warp Navigator",
                0, MB_OK | MB_INFORMATION);
            break;
        }
        return 0;
    }

    case WM_ERASEBACKGROUND:
        /*
         * Returning TRUE tells PM "I will erase my own background in WM_PAINT".
         * Without this, PM draws the default window background before WM_PAINT,
         * which causes a visible flash (flicker) on resize.
         */
        return (MRESULT)TRUE;
    }

    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}
