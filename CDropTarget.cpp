#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include "CDropTarget.hpp"
#include "CDropSource.hpp"
#include "f-templa.hpp"
#include "resource.h"

/* initialisation for FORMATETC */
#define InitFormatEtc(fe, cf, med) do { \
    (fe).cfFormat = cf; \
    (fe).dwAspect = DVASPECT_CONTENT; \
    (fe).ptd      = NULL; \
    (fe).tymed    = med; \
    (fe).lindex   = -1; \
} while (0)

inline LPBYTE byte_cast(LPIDA pIDA, UINT p)
{
    return reinterpret_cast<LPBYTE>(pIDA) + pIDA->aoffset[p];
}

CDropTarget::CDropTarget(HWND hwnd)
    : m_cRef(1)
    , m_hwnd(hwnd)
    , m_bRight(FALSE)
{
    printf("CDropTarget::CDropTarget\n");
}

CDropTarget::~CDropTarget()
{
    printf("CDropTarget::~CDropTarget\n");
    g_pDropTarget = NULL;
}

void CDropTarget::UpdateEffect(DWORD *pdwEffect, DWORD grfKeyState)
{
    if (g_pDropSource && g_pDropSource->IsDragging())
    {
        *pdwEffect &= DROPEFFECT_NONE;
        return;
    }
    *pdwEffect &= DROPEFFECT_COPY;
}

STDMETHODIMP CDropTarget::QueryInterface(REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if (riid == IID_IUnknown || riid == IID_IDropTarget)
        *ppvObject = static_cast<IDropTarget *>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CDropTarget::AddRef()
{
    return ::InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CDropTarget::Release()
{
    if (::InterlockedDecrement(&m_cRef) == 0)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

STDMETHODIMP
CDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    printf("CDropTarget::DragEnter: 0x%08X\n", grfKeyState);

    *pdwEffect = DROPEFFECT_COPY;

    FORMATETC fmt1;
    InitFormatEtc(fmt1, ::RegisterClipboardFormat(CFSTR_SHELLIDLIST), TYMED_HGLOBAL);

    HRESULT hr = pDataObj->QueryGetData(&fmt1);
    if (FAILED(hr))
    {
        printf("QueryGetData failed: 0x%08X\n", hr);
        *pdwEffect = DROPEFFECT_NONE;
        return hr;
    }

    m_bRight = (grfKeyState & MK_RBUTTON);
    return S_OK;
}

STDMETHODIMP
CDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    printf("CDropTarget::DragOver: 0x%08X\n", grfKeyState);

    UpdateEffect(pdwEffect, grfKeyState);

    return S_OK;
}

STDMETHODIMP
CDropTarget::DragLeave()
{
    printf("CDropTarget::DragLeave\n");

    return S_OK;
}

LPCTSTR doLoadStr(UINT text);

STDMETHODIMP
CDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    printf("CDropTarget::Drop: 0x%08X\n", grfKeyState);

    UpdateEffect(pdwEffect, grfKeyState);

    FORMATETC fmt1;
    InitFormatEtc(fmt1, ::RegisterClipboardFormat(CFSTR_SHELLIDLIST), TYMED_HGLOBAL);

    STGMEDIUM medium;
    HRESULT hr = pDataObj->GetData(&fmt1, &medium);
    if (FAILED(hr))
    {
        printf("GetData failed: 0x%08X\n", hr);
        *pdwEffect = DROPEFFECT_NONE;
        return E_FAIL;
    }

    if (m_bRight)
    {
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, 1, doLoadStr(IDS_ADDITEM2));
        AppendMenu(hMenu, MF_STRING, 2, doLoadStr(IDS_CANCEL));

        SetForegroundWindow(m_hwnd);
        INT nID = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
            pt.x, pt.y, 0, m_hwnd, NULL);
        PostMessage(m_hwnd, WM_NULL, 0, 0);
        if (nID == 0 || nID == -1 || nID == 2)
        {
            *pdwEffect = DROPEFFECT_NONE;
            return E_FAIL;
        }
    }

    LPIDA pIDA = (LPIDA)GlobalLock(medium.hGlobal);

    PIDLIST_ABSOLUTE pidl_root =
        reinterpret_cast<PIDLIST_ABSOLUTE>(byte_cast(pIDA, 0));

    for (UINT i = 1; i <= pIDA->cidl; ++i)
    {
        PIDLIST_RELATIVE pidl_file = 
            reinterpret_cast<PIDLIST_RELATIVE>(byte_cast(pIDA, i));

        PIDLIST_ABSOLUTE pidl = ::ILCombine(pidl_root, pidl_file);

        TCHAR szSrcFilePath[MAX_PATH + 1], szDestFilePath[MAX_PATH + 1];
        ZeroMemory(szSrcFilePath, sizeof(szSrcFilePath));
        ZeroMemory(szDestFilePath, sizeof(szDestFilePath));

        SHGetPathFromIDList(pidl, szSrcFilePath);

        TCHAR szTempDir[MAX_PATH + 1];
        GetLongPathName(g_temp_dir, szTempDir, _countof(szTempDir));
        PathAddBackslash(szTempDir);

        StringCchCopy(szDestFilePath, _countof(szDestFilePath), g_root_dir);
        PathAppend(szDestFilePath, PathFindFileName(szSrcFilePath));
        printf("%ls: %ls\n", szSrcFilePath, szDestFilePath);

        SHFILEOPSTRUCT op = { m_hwnd, FO_COPY, szSrcFilePath, szDestFilePath };
        op.fFlags = FOF_ALLOWUNDO;
        ::SHFileOperation(&op);

        CoTaskMemFree(pidl);
    }

    GlobalUnlock(medium.hGlobal);

    ReleaseStgMedium(&medium);
    return S_OK;
}
