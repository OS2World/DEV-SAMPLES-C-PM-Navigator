#ifndef WARPNAV_H
#define WARPNAV_H

/*
 * INCL_* selectors must be defined BEFORE including <os2.h>.
 * Each one gates a block of declarations inside the OS/2 headers.
 * Omitting one causes "undefined" errors for any API in that subsystem.
 */
#define INCL_WIN          /* WinXxx window-manager functions      */
#define INCL_WINMENUS     /* menu API (MM_xxx, WM_COMMAND …)      */
#define INCL_WINPOINTERS  /* WinLoadFileIcon, WinQuerySysPointer  */
#define INCL_WINWORKPLACE /* WinQueryObject, WinOpenObject        */
#define INCL_DOS          /* DosXxx file/process API              */
#define INCL_GPI          /* GpiLine, GpiPolygons … (drawing)     */
#define INCL_DOSERRORS    /* NO_ERROR, ERROR_xxx constants        */
#include <os2.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* WPS object view constants (not declared in all OpenWatcom pmwp.h versions) */
#ifndef OPEN_DEFAULT
#define OPEN_DEFAULT  0UL
#define OPEN_CONTENTS 1UL
#define OPEN_SETTINGS 2UL
#endif

/*
 * CA_AUTOPOSITION – tells the container to auto-arrange icons in a grid.
 * The correct value is 0x0800.  0x0020 looks plausible but is actually
 * CA_CONTAINERTITLE; using the wrong value causes all icons to pile up
 * in the bottom-left corner instead of forming a grid.
 * Some OpenWatcom header versions omit this constant, so we guard it.
 */
#ifndef CA_AUTOPOSITION
#define CA_AUTOPOSITION 0x0800
#endif

/* BDS_HILITED: button depressed/pressed state flag */
#ifndef BDS_HILITED
#define BDS_HILITED 0x0002
#endif

/* PUM_* constants missing from some OpenWatcom OS/2 headers */
#ifndef PUM_SELECTITEM
#define PUM_NONE         0x0000
#define PUM_SELECTITEM   0x0001
#define PUM_MOUSEBUTTON1 0x0002
#define PUM_MOUSEBUTTON2 0x0004
#define PUM_MOUSEBUTTON3 0x0008
#define PUM_KEYBOARD     0x0010
#endif

/* ── Window class names ────────────────────────────────────────────────────── */
#define WC_WARPNAVCLIENT  "WarpNavClient"

/* ── Resource IDs ──────────────────────────────────────────────────────────── */
#define IDR_MAINMENU      1
#define IDR_ACCEL         1
#define IDR_CONTEXT       2
#define IDD_RENAME        10
#define IDC_RENAME_ENTRY  11

/* ── Menu command IDs ──────────────────────────────────────────────────────── */
#define IDM_FOLDER        100
#define IDM_FOLDER_BACK   101
#define IDM_FOLDER_FWD    102
#define IDM_FOLDER_UP     103
#define IDM_FOLDER_EXIT   109

#define IDM_VIEW          200
#define IDM_VIEW_ICONS    201
#define IDM_VIEW_LIST     202
#define IDM_VIEW_DETAILS  203
#define IDM_VIEW_REFRESH  204
#define IDM_VIEW_EMBED    205
#define IDM_VIEW_RELEASE  206

#define IDM_SELECTED      300
#define IDM_SEL_OPEN      301
#define IDM_SEL_DELETE    302
#define IDM_SEL_RENAME    303
#define IDM_SEL_PROPS     304

#define IDM_HELP          400
#define IDM_HELP_ABOUT    401

/* legacy aliases so existing case labels still compile */
#define IDM_FILE_OPEN     IDM_SEL_OPEN
#define IDM_FILE_DELETE   IDM_SEL_DELETE
#define IDM_FILE_RENAME   IDM_SEL_RENAME
#define IDM_FILE_EXIT     IDM_FOLDER_EXIT
#define IDM_GO_BACK       IDM_FOLDER_BACK
#define IDM_GO_FORWARD    IDM_FOLDER_FWD
#define IDM_GO_UP         IDM_FOLDER_UP

/* ── Child control IDs ─────────────────────────────────────────────────────── */
#define ID_TREEVIEW       10
#define ID_FILEVIEW       11
#define ID_STATUS         12
#define ID_PATH           13
#define ID_BTN_BACK       14
#define ID_BTN_FWD        15
#define ID_BTN_UP         16
#define ID_BTN_REFRESH    17
#define ID_BTN_ICONS      18
#define ID_BTN_LIST       19
#define ID_BTN_DETAILS    20

/* ── Layout constants (pixels) ─────────────────────────────────────────────── */
#define TOOLBAR_H         42
#define STATUSBAR_H       22
#define SPLITTER_W        6
#define BTN_W             28
#define BTN_H             28
#define VIEWBTN_W         28
#define MIN_PANEL         80

/* ── Navigation history ────────────────────────────────────────────────────── */
#define MAX_HISTORY       64

/*
 * File record for the right-panel container (CV_ICON / CV_NAME / CV_DETAIL).
 *
 * RECORDCORE MUST be the very first member.  The container casts PFILERECORD*
 * directly to PRECORDCORE* when it calls back into our code; any offset would
 * corrupt the pointer.
 *
 * The PSZ pointer fields (pszName …) are required for CV_DETAIL columns.
 * FIELDINFO.offStruct stores the byte offset of the PSZ within the record.
 * We point each PSZ at the corresponding CHAR array inside the same struct so
 * that no separate allocation is needed and the record is self-contained.
 */
typedef struct _FILERECORD {
    RECORDCORE  rc;           /* MUST be first — full record for CV_ICON */
    PSZ         pszName;      /* → szName  (detail column) */
    PSZ         pszSize;      /* → szSize  */
    PSZ         pszDate;      /* → szDate  */
    PSZ         pszType;      /* → szType  */
    BOOL        bDir;
    CHAR        szName[CCHMAXPATHCOMP];
    CHAR        szSize[20];
    CHAR        szDate[20];
    CHAR        szType[10];
} FILERECORD, *PFILERECORD;

/* ── Tree record (left panel container) ────────────────────────────────────── */
typedef struct _TREERECORD {
    MINIRECORDCORE  mrc;
    BOOL            bLoaded;              /* children already populated?  */
    CHAR            szPath[CCHMAXPATH];   /* full path, e.g. "C:\WarpNav\" */
    CHAR            szLabel[64];          /* display text                  */
} TREERECORD, *PTREERECORD;

/* ── Shared globals (defined in main.c) ────────────────────────────────────── */
extern HAB   g_hab;
extern HWND  g_hwndFrame;
extern HWND  g_hwndClient;
extern HWND  g_hwndTree;
extern HWND  g_hwndFiles;
extern HWND  g_hwndPath;
extern CHAR  g_szCurPath[CCHMAXPATH];
extern INT   g_viewMode;   /* 0=icons  1=list  2=details */
extern INT   g_splitX;

/* ── Function prototypes ───────────────────────────────────────────────────── */

/* mainwnd.c */
MRESULT EXPENTRY ClientWndProc(HWND, ULONG, MPARAM, MPARAM);
VOID LayoutChildren(HWND hwnd, LONG cx, LONG cy);
VOID NavigateTo(PSZ pszPath);
VOID NavigateBack(VOID);
VOID NavigateForward(VOID);
VOID NavigateUp(VOID);
VOID SetViewMode(INT iMode);   /* 0=icons 1=list 2=details */
VOID UpdateStatus(VOID);
VOID UpdatePathBar(VOID);

/* treeview.c */
VOID PopulateDrives(VOID);
VOID PopulateTreeChildren(PTREERECORD prec);

/* fileview.c */
VOID SetupFileColumns(VOID);
VOID PopulateFiles(PSZ pszPath);
VOID ClearFiles(VOID);
PFILERECORD GetCursoredFile(VOID);

/* fileops.c */
VOID FileOpen(PFILERECORD prec);
VOID FileProperties(PFILERECORD prec);
VOID FileDelete(PFILERECORD prec);
VOID FileRename(PFILERECORD prec);
MRESULT EXPENTRY RenameDlgProc(HWND, ULONG, MPARAM, MPARAM);

#endif /* WARPNAV_H */
