#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "CDropTarget.hpp"
#include "CDropSource.hpp"
#include "templa/templa.hpp"
#include "resource.h"

#define CLASSNAME TEXT("FolderDeTemple")
#define WM_SHELLCHANGE (WM_USER + 100)

HWND g_hWnd = NULL;
HWND g_hListView = NULL;
HIMAGELIST g_hImageList = NULL;
INT g_iDialog = 0;
HWND g_hwndDialogs[2] = { NULL };
HWND g_hStatusBar = NULL;
TCHAR g_szDir[MAX_PATH] = TEXT("");
CDropTarget* g_pDropTarget = NULL;
CDropSource* g_pDropSource = NULL;
UINT g_nNotifyID = 0;

LPCTSTR doLoadStr(LPCTSTR text)
{
    static TCHAR s_szText[1024] = TEXT("");
    if (HIWORD(text))
        return text;
    s_szText[0] = 0;
    LoadString(NULL, LOWORD(text), s_szText, _countof(s_szText));
    return s_szText;
}

LPCTSTR doLoadStr(UINT text)
{
    return doLoadStr(MAKEINTRESOURCE(text));
}

static void InitListView(HWND hListView, HIMAGELIST hImageList, LPCTSTR pszDir)
{
    TCHAR spec[MAX_PATH];

    GetFullPathName(pszDir, _countof(g_szDir), g_szDir, NULL);

    StringCchCopy(spec, _countof(spec), g_szDir);
    PathAddBackslash(spec);
    StringCchCat(spec, _countof(spec), TEXT("*"));

    INT i = 0;
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(spec, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (lstrcmp(find.cFileName, TEXT(".")) == 0 ||
                lstrcmp(find.cFileName, TEXT("..")) == 0)
            {
                continue;
            }

            SHFILEINFO info;
            SHGetFileInfo(find.cFileName, find.dwFileAttributes, &info, sizeof(info),
                          SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

            LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE };
            item.iItem    = i++;
            item.iSubItem = 0;
            item.pszText  = find.cFileName;
            item.iImage   = info.iIcon;
            ListView_InsertItem(hListView, &item);
        } while (FindNextFile(hFind, &find));

        FindClose(hFind);
    }
}

static INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    }
    return 0;
}

static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    g_hWnd = hwnd;

    ::OleInitialize(NULL);

    DWORD style = WS_CHILD | WS_VISIBLE | LVS_ICON |
        LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_SINGLESEL |
        WS_BORDER | WS_VSCROLL | WS_HSCROLL;
    DWORD exstyle = WS_EX_CLIENTEDGE;
    g_hListView = ::CreateWindowEx(exstyle, WC_LISTVIEW, NULL, style,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)1, GetModuleHandle(NULL), NULL);
    if (g_hListView == NULL)
        return FALSE;

    SHFILEINFO info;
    g_hImageList = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &info, sizeof(info), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
    if (g_hImageList == NULL)
        return FALSE;
    ListView_SetImageList(g_hListView, g_hImageList, LVSIL_NORMAL);

    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("Templates"));

    InitListView(g_hListView, g_hImageList, szPath);

    g_hwndDialogs[0] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_TOP), hwnd, DialogProc);
    if (!g_hwndDialogs[0])
        return FALSE;
    g_hwndDialogs[1] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SUBST), hwnd, DialogProc);
    if (!g_hwndDialogs[1])
        return FALSE;
    ::ShowWindow(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);

    style = WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP;
    g_hStatusBar = ::CreateStatusWindow(style, doLoadStr(IDS_SELECTITEM), hwnd, 2);
    if (g_hStatusBar == NULL)
        return FALSE;

    LPITEMIDLIST pidlDesktop;
    SHGetSpecialFolderLocation(hwnd, CSIDL_DESKTOP, &pidlDesktop);
    SHChangeNotifyEntry  entry;
    entry.pidl = pidlDesktop;
    entry.fRecursive = TRUE;
    LONG nEvents = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR |
                   SHCNE_RENAMEFOLDER | SHCNE_RENAMEITEM;
    g_nNotifyID = SHChangeNotifyRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_NewDelivery,
                                         nEvents, WM_SHELLCHANGE, 1, &entry);

    g_pDropTarget = new CDropTarget(hwnd);
    ::RegisterDragDrop(hwnd, g_pDropTarget);
    g_pDropSource = new CDropSource();

    ::PostMessage(hwnd, WM_SIZE, 0, 0);
    return TRUE;
}

static void OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized)
{
    ::SetFocus(g_hListView);
}

static void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;
    ::GetWindowRect(g_hwndDialogs[g_iDialog], &rc);
    INT cxDialog = rc.right - rc.left;
    ::SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    ::GetWindowRect(g_hStatusBar, &rc);
    INT cyStatusBar = rc.bottom - rc.top;

    ::GetClientRect(hwnd, &rc);
    rc.bottom -= cyStatusBar;
    ::MoveWindow(g_hwndDialogs[g_iDialog], 0, 0, cxDialog, rc.bottom, TRUE);
    rc.left += cxDialog;
    ::MoveWindow(g_hListView, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

HRESULT ExecuteContextCommand(HWND hwnd, IContextMenu *pContextMenu, UINT nCmd)
{
    CMINVOKECOMMANDINFO info = { sizeof(info) };
    info.lpVerb = MAKEINTRESOURCEA(nCmd);
    info.hwnd = hwnd;
    info.fMask = CMIC_MASK_UNICODE;
    info.nShow = SW_SHOWNORMAL;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        info.fMask |= CMIC_MASK_SHIFT_DOWN;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        info.fMask |= CMIC_MASK_CONTROL_DOWN;
    return pContextMenu->InvokeCommand(&info);
}

BOOL ShowContextMenu(HWND hwnd, INT iItem, INT xPos, INT yPos, UINT uFlags = CMF_NORMAL)
{
    HRESULT hr;
    IShellFolder *pFolder = NULL;
    LPCITEMIDLIST pidlChild;
    IContextMenu *pContextMenu = NULL;
    LPITEMIDLIST pidl = NULL;
    TCHAR szPath[MAX_PATH];
    TCHAR szItem[MAX_PATH];
    HMENU hMenu = CreatePopupMenu();
    UINT nID;

    if (iItem == -1)
    {
        StringCchCopy(szPath, _countof(szPath), g_szDir);
    }
    else
    {
        ListView_EnsureVisible(g_hListView, iItem, FALSE);
        ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
        StringCchCopy(szPath, _countof(szPath), g_szDir);
        PathAppend(szPath, szItem);
    }

    hr = SHParseDisplayName(szPath, 0, &pidl, 0, 0);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = SHBindToParent(pidl, IID_IShellFolder, (void **)&pFolder, &pidlChild);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = pFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, NULL, (void **)&pContextMenu);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = pContextMenu->QueryContextMenu(hMenu, 0, FCIDM_SHVIEWFIRST, FCIDM_SHVIEWLAST, uFlags);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    if (iItem == -1)
    {
        // properties only
        while (GetMenuItemCount(hMenu) > 1)
        {
            DeleteMenu(hMenu, 0, MF_BYPOSITION);
        }
    }

    if (uFlags & CMF_DEFAULTONLY)
    {
        nID = GetMenuItemID(hMenu, 0);
        ExecuteContextCommand(hwnd, pContextMenu, nID);
    }
    else
    {
        SetForegroundWindow(hwnd);
        nID = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             xPos, yPos, 0, hwnd, NULL);
        if (nID != 0 && nID != -1)
        {
            ExecuteContextCommand(hwnd, pContextMenu, nID);
        }
    }

Finish:
    if (hMenu)
        DestroyMenu(hMenu);
    if (pidl)
        CoTaskMemFree(pidl);
    if (pFolder)
        pFolder->Release();
    if (pContextMenu)
        pContextMenu->Release();
    return TRUE;
}

static void OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1)
    {
        ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_NODEFAULT | CMF_NOVERBS);
        return;
    }

    if (xPos == 0xFFFF && yPos == 0xFFFF)
    {
        RECT rc;
        ListView_GetItemRect(g_hListView, iItem, &rc, LVIR_ICON);
        POINT pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        ::ClientToScreen(hwnd, &pt);
        xPos = pt.x;
        yPos = pt.y;
    }

    ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_EXPLORE | CMF_NODEFAULT);
}

static void OnDestroy(HWND hwnd)
{
    if (g_nNotifyID)
    {
        SHChangeNotifyDeregister(g_nNotifyID);
        g_nNotifyID = 0;
    }
    ::RevokeDragDrop(hwnd);
    if (g_pDropTarget)
    {
        g_pDropTarget->Release();
        g_pDropTarget = NULL;
    }
    if (g_pDropSource)
    {
        g_pDropSource->Release();
        g_pDropSource = NULL;
    }
    if (g_hImageList)
    {
        ImageList_Destroy(g_hImageList);
        g_hImageList = NULL;
    }
    if (g_hListView)
    {
        ::DestroyWindow(g_hListView);
        g_hListView = NULL;
    }
    if (g_hwndDialogs[0])
    {
        ::DestroyWindow(g_hwndDialogs[0]);
        g_hwndDialogs[0] = NULL;
    }
    if (g_hwndDialogs[1])
    {
        ::DestroyWindow(g_hwndDialogs[1]);
        g_hwndDialogs[1] = NULL;
    }
    OleUninitialize();
    PostQuitMessage(0);
}

static LRESULT OnNotify(HWND hwnd, int idFrom, LPNMHDR pnmhdr)
{
    if (idFrom != 1)
        return 0;

    switch (pnmhdr->code)
    {
    case NM_DBLCLK:
        {
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (iItem != -1)
                ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY);
        }
        break;

    case LVN_KEYDOWN:
        {
            LV_KEYDOWN* pKeyDown = (LV_KEYDOWN*)pnmhdr;
            if (pKeyDown->wVKey == VK_RETURN)
            {
                INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY);
            }
            else if (pKeyDown->wVKey == VK_DELETE)
            {
                INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                if (iItem != -1)
                {
                    TCHAR szPath[MAX_PATH], szItem[MAX_PATH];
                    ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));

                    ZeroMemory(szPath, sizeof(szPath));
                    StringCchCopy(szPath, _countof(szPath), g_szDir);
                    PathAppend(szPath, szItem);

                    if (::GetKeyState(VK_SHIFT) < 0)
                    {
                        ::DeleteFile(szPath);
                    }
                    else
                    {
                        SHFILEOPSTRUCT op = { hwnd, FO_DELETE };
                        op.pFrom = szPath;
                        op.fFlags = FOF_ALLOWUNDO;
                        ::SHFileOperation(&op);
                    }
                }
            }
        }
        break;

    case LVN_ITEMCHANGED:
        {
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (iItem != -1)
            {
                g_iDialog = 1;
                ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_TYPESUBST));
            }
            else
            {
                g_iDialog = 0;
                ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_SELECTITEM));
            }

            ::ShowWindow(g_hwndDialogs[0], SW_HIDE);
            ::ShowWindow(g_hwndDialogs[1], SW_HIDE);
            ::ShowWindow(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);
            ::PostMessage(hwnd, WM_SIZE, 0, 0);
        }
        break;

    case LVN_BEGINDRAG:
    case LVN_BEGINRDRAG:
        {
            INT cSelected = ListView_GetSelectedCount(g_hListView);
            if (cSelected == 0)
                break;

            PIDLIST_ABSOLUTE* ppidlAbsolute;
            ppidlAbsolute = (PIDLIST_ABSOLUTE*)CoTaskMemAlloc(sizeof(PIDLIST_ABSOLUTE) * cSelected);

            PITEMID_CHILD* ppidlChild;
            ppidlChild = (PITEMID_CHILD*)CoTaskMemAlloc(sizeof(PITEMID_CHILD) * cSelected);

            if (!ppidlAbsolute || !ppidlChild)
            {
                CoTaskMemFree(ppidlAbsolute);
                CoTaskMemFree(ppidlChild);
                break;
            }

            INT cItems = ListView_GetItemCount(g_hListView);
            for (UINT i = 0, j = 0; i < cItems; ++i)
            {
                if (ListView_GetItemState(g_hListView, i, LVIS_SELECTED))
                {
                    TCHAR szPath[MAX_PATH], szItem[MAX_PATH];
                    ListView_GetItemText(g_hListView, i, 0, szItem, _countof(szItem));
                    StringCchCopy(szPath, _countof(szPath), g_szDir);
                    PathAppend(szPath, szItem);
                    ppidlAbsolute[j++] = ILCreateFromPath(szPath);
                }
            }

            IShellFolder* pShellFolder = NULL;
            SHBindToParent(ppidlAbsolute[0], IID_PPV_ARGS(&pShellFolder), NULL);

            for (UINT i = 0; i < cSelected; i++)
                ppidlChild[i] = ILFindLastID(ppidlAbsolute[i]);

            IDataObject *pDataObject = NULL;
            pShellFolder->GetUIObjectOf(NULL, cSelected, (LPCITEMIDLIST*)ppidlChild, IID_IDataObject, NULL,
                                        (void **)&pDataObject);
            if (pDataObject)
            {
                DWORD dwEffect;
                DoDragDrop(pDataObject, g_pDropSource, DROPEFFECT_COPY, &dwEffect);
            }

            for (UINT i = 0; i < cSelected; ++i)
                CoTaskMemFree(ppidlAbsolute[i]);

            CoTaskMemFree(ppidlAbsolute);
            CoTaskMemFree(ppidlChild);
        }
        break;
    }

    return 0;
}

static LRESULT OnShellChange(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LPITEMIDLIST *ppidlAbsolute;
    LONG lEvent;
    HANDLE hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &ppidlAbsolute, &lEvent);
    if (!hLock)
    {
        MessageBoxA(NULL, "NG", NULL, 0);
        return 0;
    }

    TCHAR szPath[MAX_PATH];
    SHGetPathFromIDList(ppidlAbsolute[0], szPath);
    PathRemoveFileSpec(szPath);

    if (lstrcmpi(szPath, g_szDir) == 0)
    {
        SHGetPathFromIDList(ppidlAbsolute[0], szPath);
        LPTSTR pszFileName = PathFindFileName(szPath);
        LV_FINDINFO Find = { LVFI_STRING, pszFileName };
        INT iItem = ListView_FindItem(g_hListView, -1, &Find);
        switch (lEvent)
        {
        case SHCNE_CREATE:
        case SHCNE_MKDIR:
            if (iItem == -1)
            {
                DWORD dwAttrs = ((lEvent == SHCNE_MKDIR) ? FILE_ATTRIBUTE_DIRECTORY : 0);
                SHFILEINFO info;
                SHGetFileInfo(pszFileName, dwAttrs, &info, sizeof(info),
                              SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
                LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE };
                item.iItem    = ListView_GetItemCount(g_hListView);
                item.iSubItem = 0;
                item.pszText  = pszFileName;
                item.iImage   = info.iIcon;
                ListView_InsertItem(g_hListView, &item);
            }
            break;
        case SHCNE_DELETE:
        case SHCNE_RMDIR:
            ListView_DeleteItem(g_hListView, iItem);
            break;
        case SHCNE_RENAMEFOLDER:
        case SHCNE_RENAMEITEM:
            SHGetPathFromIDList(ppidlAbsolute[1], szPath);
            ListView_SetItemText(g_hListView, iItem, 0, PathFindFileName(szPath));
            break;
        }
    }

    SHChangeNotification_Unlock(hLock);
    return 0;
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_ACTIVATE, OnActivate);
        HANDLE_MSG(hwnd, WM_SIZE, OnSize);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, OnContextMenu);
        HANDLE_MSG(hwnd, WM_NOTIFY, OnNotify);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    case WM_SHELLCHANGE:
        return OnShellChange(hwnd, wParam, lParam);
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    InitCommonControls();

    WNDCLASSEX wcx = { sizeof(wcx) };
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = WindowProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = CLASSNAME;
    wcx.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!::RegisterClassEx(&wcx))
    {
        MessageBoxA(NULL, "RegisterClassEx failed", NULL, MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = 0;
    HWND hwnd = ::CreateWindowEx(exstyle, CLASSNAME, doLoadStr(IDS_APPVERSION), style,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
                                 NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBoxA(NULL, "CreateWindowEx failed", NULL, MB_ICONERROR);
        return 2;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (g_hwndDialogs[g_iDialog] && IsDialogMessage(g_hwndDialogs[g_iDialog], &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return INT(msg.wParam);
}
