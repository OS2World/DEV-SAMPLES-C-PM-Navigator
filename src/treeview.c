#include "warpnav.h"

/* sentinel label for placeholder "loading" children */
static CHAR s_szEllipsis[] = "...";

/*
 * Lazy-load trick for the tree view:
 *
 * The PM container shows an expand (+) button next to a node only if the
 * node already has at least one child record.  To avoid enumerating every
 * subdirectory at startup, we insert a single placeholder ("...") child
 * under each drive/folder node.  This makes the + appear immediately.
 *
 * When the user clicks + (CN_EXPANDTREE), PopulateTreeChildren removes the
 * placeholder and inserts the real subdirectory children on demand.
 * The placeholder is identified by szPath[0] == '\0'.
 */

/* Insert a single dummy "..." child under prec so the + button appears. */
static VOID InsertDummy(PTREERECORD prec)
{
    PTREERECORD  pdummy;
    RECORDINSERT ri;

    pdummy = (PTREERECORD)WinSendMsg(g_hwndTree, CM_ALLOCRECORD,
        MPFROMLONG(sizeof(TREERECORD) - sizeof(MINIRECORDCORE)),
        MPFROMLONG(1));
    if (!pdummy) return;

    pdummy->szPath[0]        = '\0';   /* empty path = dummy marker */
    pdummy->bLoaded          = TRUE;
    pdummy->mrc.cb           = sizeof(MINIRECORDCORE);
    pdummy->mrc.pszIcon      = s_szEllipsis;
    pdummy->mrc.hptrIcon     = NULLHANDLE;
    pdummy->mrc.flRecordAttr = 0;

    memset(&ri, 0, sizeof(ri));
    ri.cb                = sizeof(ri);
    ri.pRecordOrder      = (PRECORDCORE)CMA_END;
    ri.pRecordParent     = (PRECORDCORE)prec;
    ri.fInvalidateRecord = FALSE;
    ri.cRecordsInsert    = 1;
    WinSendMsg(g_hwndTree, CM_INSERTRECORD, MPFROMP(pdummy), MPFROMP(&ri));
}

/* Remove the dummy child (szPath[0]=='\0') from under prec, if present. */
static VOID RemoveDummy(PTREERECORD prec)
{
    PTREERECORD pchild;

    pchild = (PTREERECORD)WinSendMsg(g_hwndTree, CM_QUERYRECORD,
        MPFROMP(prec), MPFROM2SHORT(CMA_FIRSTCHILD, CMA_ITEMORDER));
    if (pchild && pchild->szPath[0] == '\0') {
        WinSendMsg(g_hwndTree, CM_REMOVERECORD,
            MPFROMP(&pchild),
            MPFROM2SHORT(1, CMA_FREE | CMA_INVALIDATE));
    }
}

/* Populate top-level drive nodes. */
VOID PopulateDrives(VOID)
{
    ULONG    ulCurDrive;
    ULONG    ulDriveMap;
    CHAR     chDrive;
    CNRINFO  cnri;

    WinSendMsg(g_hwndTree, CM_REMOVERECORD, MPFROMP(NULL),
               MPFROM2SHORT(0, CMA_FREE | CMA_INVALIDATE));

    DosQueryCurrentDisk(&ulCurDrive, &ulDriveMap);

    for (chDrive = 'A'; chDrive <= 'Z'; chDrive++) {
        PTREERECORD  prec;
        RECORDINSERT ri;
        FSINFO       fsi;
        CHAR         szPath[4];

        if (!(ulDriveMap & (1UL << (chDrive - 'A')))) continue;

        szPath[0] = chDrive; szPath[1] = ':'; szPath[2] = '\\'; szPath[3] = '\0';

        prec = (PTREERECORD)WinSendMsg(g_hwndTree, CM_ALLOCRECORD,
            MPFROMLONG(sizeof(TREERECORD) - sizeof(MINIRECORDCORE)),
            MPFROMLONG(1));
        if (!prec) continue;

        strncpy(prec->szPath, szPath, CCHMAXPATH - 1);
        prec->bLoaded = FALSE;

        prec->szLabel[0] = chDrive; prec->szLabel[1] = ':'; prec->szLabel[2] = '\0';
        if (DosQueryFSInfo((ULONG)(chDrive - 'A' + 1), FSIL_VOLSER,
                           &fsi, sizeof(fsi)) == NO_ERROR
            && fsi.vol.szVolLabel[0]) {
            strncat(prec->szLabel, "  [",
                    sizeof(prec->szLabel) - strlen(prec->szLabel) - 1);
            strncat(prec->szLabel, fsi.vol.szVolLabel,
                    sizeof(prec->szLabel) - strlen(prec->szLabel) - 1);
            strncat(prec->szLabel, "]",
                    sizeof(prec->szLabel) - strlen(prec->szLabel) - 1);
        }

        prec->mrc.cb           = sizeof(MINIRECORDCORE);
        prec->mrc.pszIcon      = prec->szLabel;
        prec->mrc.hptrIcon     = WinLoadFileIcon((PCSZ)szPath, FALSE);
        if (!prec->mrc.hptrIcon)
            prec->mrc.hptrIcon = WinQuerySysPointer(HWND_DESKTOP, SPTR_FOLDER, FALSE);
        prec->mrc.flRecordAttr = CRA_COLLAPSED;

        memset(&ri, 0, sizeof(ri));
        ri.cb                = sizeof(ri);
        ri.pRecordOrder      = (PRECORDCORE)CMA_END;
        ri.pRecordParent     = NULL;
        ri.fInvalidateRecord = FALSE;
        ri.cRecordsInsert    = 1;
        WinSendMsg(g_hwndTree, CM_INSERTRECORD, MPFROMP(prec), MPFROMP(&ri));

        /* dummy child forces the + button to appear */
        InsertDummy(prec);
    }

    /* switch container to tree view */
    memset(&cnri, 0, sizeof(cnri));
    cnri.cb           = sizeof(cnri);
    cnri.pszCnrTitle  = (PSZ)"Drives";
    cnri.flWindowAttr = CV_TREE | CV_TEXT | CA_TREELINE
                        | CA_CONTAINERTITLE | CA_TITLESEPARATOR;
    cnri.cxTreeIndent = 18;
    WinSendMsg(g_hwndTree, CM_SETCNRINFO, MPFROMP(&cnri),
               MPFROMLONG(CMA_CNRTITLE | CMA_FLWINDOWATTR | CMA_CXTREEINDENT));

    WinSendMsg(g_hwndTree, CM_INVALIDATERECORD,
               MPFROMP(NULL), MPFROM2SHORT(0, CMA_REPOSITION));
}

/* Lazily expand a tree node: remove dummy, insert real subdirectory children. */
VOID PopulateTreeChildren(PTREERECORD prec)
{
    CHAR         szSearch[CCHMAXPATH];
    INT          len;
    HDIR         hdir;
    FILEFINDBUF3 ffb;
    ULONG        ulCount;
    APIRET       rc;
    HPOINTER     hptrFolder;

    if (!prec || prec->bLoaded) return;
    prec->bLoaded = TRUE;

    RemoveDummy(prec);

    hptrFolder = WinQuerySysPointer(HWND_DESKTOP, SPTR_FOLDER, FALSE);

    strncpy(szSearch, prec->szPath, CCHMAXPATH - 3);
    len = (INT)strlen(szSearch);
    if (len > 0 && szSearch[len - 1] != '\\') szSearch[len++] = '\\';
    szSearch[len++] = '*';
    szSearch[len]   = '\0';

    hdir    = HDIR_CREATE;
    ulCount = 1;

    rc = DosFindFirst((PCSZ)szSearch, &hdir,
        FILE_DIRECTORY | FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED,
        &ffb, sizeof(ffb), &ulCount, FIL_STANDARD);

    while (rc == NO_ERROR && ulCount > 0) {
        PTREERECORD  pchild;
        RECORDINSERT ri;
        CHAR         szChildPath[CCHMAXPATH];
        INT          clen;

        if (!(ffb.attrFile & FILE_DIRECTORY) ||
            strcmp(ffb.achName, ".")  == 0   ||
            strcmp(ffb.achName, "..") == 0) {
            ulCount = 1;
            rc = DosFindNext(hdir, &ffb, sizeof(ffb), &ulCount);
            continue;
        }

        pchild = (PTREERECORD)WinSendMsg(g_hwndTree, CM_ALLOCRECORD,
            MPFROMLONG(sizeof(TREERECORD) - sizeof(MINIRECORDCORE)),
            MPFROMLONG(1));
        if (!pchild) break;

        strncpy(szChildPath, prec->szPath, CCHMAXPATH - 1);
        clen = (INT)strlen(szChildPath);
        if (clen > 0 && szChildPath[clen - 1] != '\\')
            szChildPath[clen++] = '\\';
        strncat(szChildPath, ffb.achName, CCHMAXPATH - clen - 1);
        clen = (INT)strlen(szChildPath);
        if (clen < CCHMAXPATH - 1 && szChildPath[clen - 1] != '\\') {
            szChildPath[clen]     = '\\';
            szChildPath[clen + 1] = '\0';
        }

        strncpy(pchild->szPath,  szChildPath,    CCHMAXPATH - 1);
        strncpy(pchild->szLabel, ffb.achName, sizeof(pchild->szLabel) - 1);
        pchild->bLoaded = FALSE;

        pchild->mrc.cb           = sizeof(MINIRECORDCORE);
        pchild->mrc.pszIcon      = pchild->szLabel;
        pchild->mrc.hptrIcon     = WinLoadFileIcon((PCSZ)szChildPath, FALSE);
        if (!pchild->mrc.hptrIcon) pchild->mrc.hptrIcon = hptrFolder;
        pchild->mrc.flRecordAttr = CRA_COLLAPSED;

        memset(&ri, 0, sizeof(ri));
        ri.cb                = sizeof(ri);
        ri.pRecordOrder      = (PRECORDCORE)CMA_END;
        ri.pRecordParent     = (PRECORDCORE)prec;
        ri.fInvalidateRecord = FALSE;
        ri.cRecordsInsert    = 1;
        WinSendMsg(g_hwndTree, CM_INSERTRECORD, MPFROMP(pchild), MPFROMP(&ri));

        /* each subdir also gets a dummy so it shows a + */
        InsertDummy(pchild);

        ulCount = 1;
        rc = DosFindNext(hdir, &ffb, sizeof(ffb), &ulCount);
    }
    DosFindClose(hdir);

    WinSendMsg(g_hwndTree, CM_INVALIDATERECORD,
               MPFROMP(NULL), MPFROM2SHORT(0, CMA_REPOSITION));
}
