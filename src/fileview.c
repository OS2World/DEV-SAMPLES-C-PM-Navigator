#include "warpnav.h"

VOID SetupFileColumns(VOID)
{
    PFIELDINFO      pfi, pfiFirst;
    FIELDINFOINSERT fii;
    ULONG           cCols = 4;

    pfiFirst = (PFIELDINFO)WinSendMsg(g_hwndFiles, CM_ALLOCDETAILFIELDINFO,
                                      MPFROMLONG(cCols), NULL);
    if (!pfiFirst) return;

    pfi = pfiFirst;

    pfi->cb         = sizeof(FIELDINFO);
    pfi->flData     = CFA_STRING | CFA_LEFT | CFA_HORZSEPARATOR;
    pfi->flTitle    = CFA_STRING | CFA_CENTER | CFA_HORZSEPARATOR;
    pfi->pTitleData = (PVOID)"Name";
    pfi->offStruct  = offsetof(FILERECORD, pszName);
    pfi->cxWidth    = 220;
    pfi = pfi->pNextFieldInfo;

    pfi->cb         = sizeof(FIELDINFO);
    pfi->flData     = CFA_STRING | CFA_RIGHT | CFA_HORZSEPARATOR;
    pfi->flTitle    = CFA_STRING | CFA_CENTER | CFA_HORZSEPARATOR;
    pfi->pTitleData = (PVOID)"Size";
    pfi->offStruct  = offsetof(FILERECORD, pszSize);
    pfi->cxWidth    = 80;
    pfi = pfi->pNextFieldInfo;

    pfi->cb         = sizeof(FIELDINFO);
    pfi->flData     = CFA_STRING | CFA_LEFT | CFA_HORZSEPARATOR;
    pfi->flTitle    = CFA_STRING | CFA_CENTER | CFA_HORZSEPARATOR;
    pfi->pTitleData = (PVOID)"Date modified";
    pfi->offStruct  = offsetof(FILERECORD, pszDate);
    pfi->cxWidth    = 130;
    pfi = pfi->pNextFieldInfo;

    pfi->cb         = sizeof(FIELDINFO);
    pfi->flData     = CFA_STRING | CFA_LEFT | CFA_HORZSEPARATOR;
    pfi->flTitle    = CFA_STRING | CFA_CENTER | CFA_HORZSEPARATOR;
    pfi->pTitleData = (PVOID)"Type";
    pfi->offStruct  = offsetof(FILERECORD, pszType);
    pfi->cxWidth    = 70;

    memset(&fii, 0, sizeof(fii));
    fii.cb                   = sizeof(fii);
    fii.pFieldInfoOrder      = (PFIELDINFO)CMA_FIRST;
    fii.cFieldInfoInsert     = cCols;
    fii.fInvalidateFieldInfo = TRUE;
    WinSendMsg(g_hwndFiles, CM_INSERTDETAILFIELDINFO,
               MPFROMP(pfiFirst), MPFROMP(&fii));
}

VOID ClearFiles(VOID)
{
    WinSendMsg(g_hwndFiles, CM_REMOVERECORD, MPFROMP(NULL),
               MPFROM2SHORT(0, CMA_FREE | CMA_INVALIDATE));
}

VOID PopulateFiles(PSZ pszPath)
{
    CHAR         szSearch[CCHMAXPATH];
    CHAR         szFull[CCHMAXPATH];
    INT          len;
    HPOINTER     hptrFolder;
    HPOINTER     hptrFile;
    HDIR         hdir;
    FILEFINDBUF3 ffb;
    ULONG        ulCount;
    APIRET       rc;
    PFILERECORD  pFirst;
    PFILERECORD  pLast;
    ULONG        cInsert;
    CNRINFO      cnri;
    ULONG        flView;

    strncpy(szSearch, pszPath, CCHMAXPATH - 3);
    len = (INT)strlen(szSearch);
    if (len > 0 && szSearch[len - 1] != '\\') szSearch[len++] = '\\';
    szSearch[len++] = '*';
    szSearch[len]   = '\0';

    hptrFolder = WinQuerySysPointer(HWND_DESKTOP, SPTR_FOLDER, FALSE);
    hptrFile   = WinQuerySysPointer(HWND_DESKTOP, SPTR_FILE,   FALSE);
    if (!hptrFolder) hptrFolder = WinQuerySysPointer(HWND_DESKTOP, SPTR_ICONINFORMATION, FALSE);
    if (!hptrFile)   hptrFile   = WinQuerySysPointer(HWND_DESKTOP, SPTR_APPICON, FALSE);

    /*
     * HDIR_CREATE is a constant (not a real handle) that tells DosFindFirst
     * to allocate a new directory-search handle internally.  After the call,
     * hdir holds the real handle for subsequent DosFindNext calls.
     * Always close it with DosFindClose when done, even after an error.
     */
    hdir     = HDIR_CREATE;
    ulCount  = 1;
    pFirst   = NULL;
    pLast    = NULL;
    cInsert  = 0;

    rc = DosFindFirst((PCSZ)szSearch, &hdir,
                      FILE_NORMAL | FILE_DIRECTORY |
                      FILE_HIDDEN | FILE_SYSTEM    |
                      FILE_READONLY | FILE_ARCHIVED,
                      &ffb, sizeof(ffb), &ulCount, FIL_STANDARD);

    while (rc == NO_ERROR && ulCount > 0) {
        BOOL        bDir;
        HPOINTER    hptr;
        PFILERECORD prec;

        if (strcmp(ffb.achName, ".") == 0 || strcmp(ffb.achName, "..") == 0) {
            ulCount = 1;
            rc = DosFindNext(hdir, &ffb, sizeof(ffb), &ulCount);
            continue;
        }

        bDir = (ffb.attrFile & FILE_DIRECTORY) != 0;

        /*
         * CM_ALLOCRECORD allocates a container record.  The first argument is
         * the number of EXTRA bytes beyond the base RECORDCORE that the
         * container should allocate.  We subtract sizeof(RECORDCORE) because
         * FILERECORD already embeds RECORDCORE as its first member.
         */
        prec = (PFILERECORD)WinSendMsg(g_hwndFiles,
            CM_ALLOCRECORD,
            MPFROMLONG(sizeof(FILERECORD) - sizeof(RECORDCORE)),
            MPFROMLONG(1));

        if (!prec) break;

        strncpy(prec->szName, ffb.achName, sizeof(prec->szName) - 1);
        prec->pszName = prec->szName;

        if (bDir) {
            strcpy(prec->szSize, "");
        } else if (ffb.cbFile >= 1024UL * 1024UL) {
            sprintf(prec->szSize, "%lu MB", ffb.cbFile / (1024UL * 1024UL));
        } else if (ffb.cbFile >= 1024UL) {
            sprintf(prec->szSize, "%lu KB", ffb.cbFile / 1024UL);
        } else {
            sprintf(prec->szSize, "%lu B", ffb.cbFile);
        }
        prec->pszSize = prec->szSize;

        /* OS/2 stores the year as an offset from 1980, so add 1980 to display it. */
        sprintf(prec->szDate, "%04d-%02d-%02d %02d:%02d",
                (INT)ffb.fdateLastWrite.year + 1980,
                (INT)ffb.fdateLastWrite.month,
                (INT)ffb.fdateLastWrite.day,
                (INT)ffb.ftimeLastWrite.hours,
                (INT)ffb.ftimeLastWrite.minutes);
        prec->pszDate = prec->szDate;

        if (bDir) {
            strcpy(prec->szType, "Folder");
        } else {
            CHAR *pDot = strrchr(prec->szName, '.');
            if (pDot && strlen(pDot) < sizeof(prec->szType)) {
                CHAR *c;
                strncpy(prec->szType, pDot + 1, sizeof(prec->szType) - 1);
                c = prec->szType;
                while (*c) { if (*c >= 'a' && *c <= 'z') *c -= 32; c++; }
            } else {
                strcpy(prec->szType, "File");
            }
        }
        prec->pszType = prec->szType;
        prec->bDir    = bDir;

        /*
         * WinLoadFileIcon asks the Workplace Shell for the icon registered to
         * this specific file path (by extension or EA type).  If the WPS has
         * no icon registered, it returns NULLHANDLE and we fall back to the
         * generic folder/file system pointer.
         */
        strncpy(szFull, pszPath, CCHMAXPATH - 1);
        strncat(szFull, prec->szName, CCHMAXPATH - (INT)strlen(szFull) - 1);

        hptr = WinLoadFileIcon((PCSZ)szFull, FALSE);
        if (!hptr) hptr = bDir ? hptrFolder : hptrFile;

        prec->rc.cb           = sizeof(RECORDCORE);
        prec->rc.flRecordAttr = bDir ? CRA_RECORDREADONLY : 0;
        /*
         * pszIcon  – the text label shown in CV_ICON (icon) view.
         * pszName  – the text label shown in CV_NAME (list) view.
         * Both must be set; they can point to the same string.
         * hptrIcon     is the large icon used in CV_ICON view.
         * hptrMiniIcon is the small icon used in CV_NAME and CV_DETAIL views.
         */
        prec->rc.pszIcon      = prec->szName;
        prec->rc.pszName      = prec->szName;
        prec->rc.hptrIcon     = hptr;
        prec->rc.hptrMiniIcon = hptr;

        prec->rc.preccNextRecord = NULL;
        if (!pFirst) {
            pFirst = prec;
            pLast  = prec;
        } else {
            pLast->rc.preccNextRecord = (PRECORDCORE)prec;
            pLast = prec;
        }
        cInsert++;

        ulCount = 1;
        rc = DosFindNext(hdir, &ffb, sizeof(ffb), &ulCount);
    }
    DosFindClose(hdir);

    if (pFirst && cInsert > 0) {
        RECORDINSERT ri;
        memset(&ri, 0, sizeof(ri));
        ri.cb                = sizeof(ri);
        ri.pRecordOrder      = (PRECORDCORE)CMA_END;
        ri.zOrder            = (USHORT)CMA_TOP;
        ri.fInvalidateRecord = TRUE;
        ri.cRecordsInsert    = cInsert;
        WinSendMsg(g_hwndFiles, CM_INSERTRECORD, MPFROMP(pFirst), MPFROMP(&ri));
    }

    /* apply current view mode */
    switch (g_viewMode) {
    case 0:  flView = CV_ICON | CA_AUTOPOSITION;        break;
    case 2:  flView = CV_DETAIL | CA_DETAILSVIEWTITLES; break;
    default: flView = CV_NAME;                          break;
    }
    memset(&cnri, 0, sizeof(cnri));
    cnri.cb           = sizeof(cnri);
    cnri.pszCnrTitle  = NULL;
    cnri.flWindowAttr = flView;
    WinSendMsg(g_hwndFiles, CM_SETCNRINFO, MPFROMP(&cnri),
               MPFROMLONG(CMA_CNRTITLE | CMA_FLWINDOWATTR));

    /* CM_ARRANGE forces icon grid layout after records are already inserted */
    if (g_viewMode == 0)
        WinSendMsg(g_hwndFiles, CM_ARRANGE, NULL, NULL);
}

PFILERECORD GetCursoredFile(VOID)
{
    return (PFILERECORD)WinSendMsg(g_hwndFiles, CM_QUERYRECORDEMPHASIS,
        MPFROMP(CMA_FIRST), MPFROMSHORT(CRA_CURSORED));
}
