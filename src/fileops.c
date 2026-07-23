#include "warpnav.h"

VOID FileOpen(PFILERECORD prec)
{
    if (!prec) return;

    if (prec->bDir) {
        CHAR szNew[CCHMAXPATH];
        INT  len;

        len = (INT)strlen(g_szCurPath);
        strncpy(szNew, g_szCurPath, CCHMAXPATH - 1);
        if (len > 0 && szNew[len - 1] != '\\')
            strncat(szNew, "\\", CCHMAXPATH - len - 1);
        strncat(szNew, prec->szName, CCHMAXPATH - (INT)strlen(szNew) - 1);
        NavigateTo(szNew);
    } else {
        CHAR    szFull[CCHMAXPATH];
        INT     len;
        HOBJECT hobj;

        len = (INT)strlen(g_szCurPath);
        strncpy(szFull, g_szCurPath, CCHMAXPATH - 1);
        if (len > 0 && szFull[len - 1] != '\\')
            strncat(szFull, "\\", CCHMAXPATH - len - 1);
        strncat(szFull, prec->szName, CCHMAXPATH - (INT)strlen(szFull) - 1);

        hobj = WinQueryObject((PCSZ)szFull);
        if (hobj)
            WinOpenObject(hobj, OPEN_DEFAULT, FALSE);
    }
}

VOID FileProperties(PFILERECORD prec)
{
    CHAR    szFull[CCHMAXPATH];
    INT     len;
    HOBJECT hobj;

    if (!prec) return;

    len = (INT)strlen(g_szCurPath);
    strncpy(szFull, g_szCurPath, CCHMAXPATH - 1);
    if (len > 0 && szFull[len - 1] != '\\')
        strncat(szFull, "\\", CCHMAXPATH - len - 1);
    strncat(szFull, prec->szName, CCHMAXPATH - (INT)strlen(szFull) - 1);

    hobj = WinQueryObject((PCSZ)szFull);
    if (hobj)
        WinOpenObject(hobj, OPEN_SETTINGS, FALSE);
}

VOID FileDelete(PFILERECORD prec)
{
    CHAR   szMsg[CCHMAXPATH + 64];
    CHAR   szFull[CCHMAXPATH];
    INT    len;
    ULONG  rc;
    APIRET arc;

    if (!prec) return;

    sprintf(szMsg, "Delete \"%s\"?", prec->szName);

    rc = WinMessageBox(HWND_DESKTOP, g_hwndFrame, szMsg,
                       "Warp Navigator", 0,
                       MB_YESNO | MB_WARNING | MB_MOVEABLE);
    if (rc != MBID_YES) return;

    len = (INT)strlen(g_szCurPath);
    strncpy(szFull, g_szCurPath, CCHMAXPATH - 1);
    if (len > 0 && szFull[len - 1] != '\\')
        strncat(szFull, "\\", CCHMAXPATH - len - 1);
    strncat(szFull, prec->szName, CCHMAXPATH - (INT)strlen(szFull) - 1);

    if (prec->bDir)
        arc = DosDeleteDir((PCSZ)szFull);
    else
        arc = DosDelete((PCSZ)szFull);

    if (arc != NO_ERROR) {
        CHAR szErr[80];
        sprintf(szErr, "Delete failed (error %lu).", arc);
        WinMessageBox(HWND_DESKTOP, g_hwndFrame, szErr,
                      "Warp Navigator", 0, MB_OK | MB_ERROR | MB_MOVEABLE);
    }

    NavigateTo(g_szCurPath);
}

static CHAR s_szRenameOld[CCHMAXPATHCOMP];
static CHAR s_szRenameNew[CCHMAXPATHCOMP];

MRESULT EXPENTRY RenameDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    switch (msg) {
    case WM_INITDLG:
        WinSetDlgItemText(hwnd, IDC_RENAME_ENTRY, s_szRenameOld);
        WinSendDlgItemMsg(hwnd, IDC_RENAME_ENTRY, EM_SETSEL,
                          MPFROM2SHORT(0, (SHORT)strlen(s_szRenameOld)), NULL);
        return (MRESULT)FALSE;

    case WM_COMMAND:
        switch (SHORT1FROMMP(mp1)) {
        case DID_OK: {
            CHAR szNew[CCHMAXPATHCOMP];
            WinQueryDlgItemText(hwnd, IDC_RENAME_ENTRY, sizeof(szNew), szNew);
            strncpy(s_szRenameNew, szNew, sizeof(s_szRenameNew) - 1);
            WinDismissDlg(hwnd, DID_OK);
            return 0;
        }
        case DID_CANCEL:
            WinDismissDlg(hwnd, DID_CANCEL);
            return 0;
        }
        break;
    }
    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

VOID FileRename(PFILERECORD prec)
{
    CHAR   szOld[CCHMAXPATH];
    CHAR   szNew[CCHMAXPATH];
    INT    len;
    ULONG  rc;
    APIRET arc;

    if (!prec) return;

    strncpy(s_szRenameOld, prec->szName, sizeof(s_szRenameOld) - 1);
    s_szRenameNew[0] = '\0';

    rc = WinDlgBox(HWND_DESKTOP, g_hwndFrame, RenameDlgProc,
                   NULLHANDLE, IDD_RENAME, NULL);
    if (rc != DID_OK || s_szRenameNew[0] == '\0') return;
    if (strcmp(s_szRenameOld, s_szRenameNew) == 0) return;

    len = (INT)strlen(g_szCurPath);

    strncpy(szOld, g_szCurPath, CCHMAXPATH - 1);
    if (len > 0 && szOld[len - 1] != '\\')
        strncat(szOld, "\\", CCHMAXPATH - len - 1);
    strncat(szOld, s_szRenameOld, CCHMAXPATH - (INT)strlen(szOld) - 1);

    strncpy(szNew, g_szCurPath, CCHMAXPATH - 1);
    if (len > 0 && szNew[len - 1] != '\\')
        strncat(szNew, "\\", CCHMAXPATH - len - 1);
    strncat(szNew, s_szRenameNew, CCHMAXPATH - (INT)strlen(szNew) - 1);

    arc = DosMove((PCSZ)szOld, (PCSZ)szNew);
    if (arc != NO_ERROR) {
        CHAR szErr[80];
        sprintf(szErr, "Rename failed (error %lu).", arc);
        WinMessageBox(HWND_DESKTOP, g_hwndFrame, szErr,
                      "Warp Navigator", 0, MB_OK | MB_ERROR | MB_MOVEABLE);
    }

    NavigateTo(g_szCurPath);
}
