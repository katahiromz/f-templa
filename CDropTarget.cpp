#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include "CDropTarget.hpp"
#include "CDropSource.hpp"
#include "f-templa.hpp"
#include "resource.h"

inline LPBYTE byte_cast(LPIDA pIDA, UINT p)
{
    return reinterpret_cast<LPBYTE>(pIDA) + pIDA->aoffset[p];
}

CDropTarget::CDropTarget(HWND hwnd)
    : m_cRef(1)
    , m_hwnd(hwnd)
    , m_pDropTargetHelper(NULL)
    , m_bRight(FALSE)
{
    ::CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pDropTargetHelper));
}

CDropTarget::~CDropTarget()
{
    if (m_pDropTargetHelper)
    {
        m_pDropTargetHelper->Release();
        m_pDropTargetHelper = NULL;
    }
}

void CDropTarget::UpdateEffect(DWORD *pdwEffect, DWORD grfKeyState)
{
    if (g_pDropSource && g_pDropSource->IsDragging())
    {
        *pdwEffect = DROPEFFECT_NONE;
        return;
    }
    *pdwEffect = DROPEFFECT_COPY;
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
    UpdateEffect(pdwEffect, grfKeyState);

    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragEnter(m_hwnd, pDataObj, (LPPOINT)&pt, *pdwEffect);

    FORMATETC formatetc;
    formatetc.cfFormat = ::RegisterClipboardFormat(CFSTR_SHELLIDLIST);
    formatetc.ptd      = NULL;
    formatetc.dwAspect = DVASPECT_CONTENT;
    formatetc.lindex   = -1;
    formatetc.tymed    = TYMED_HGLOBAL;
    HRESULT hr = pDataObj->QueryGetData(&formatetc);
    if (FAILED(hr))
    {
        *pdwEffect = DROPEFFECT_NONE;
    }
    else
    {
        m_bRight = (grfKeyState & MK_RBUTTON);
    }

    return S_OK;
}

STDMETHODIMP
CDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    UpdateEffect(pdwEffect, grfKeyState);
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragOver((LPPOINT)&pt, *pdwEffect);
    return S_OK;
}

STDMETHODIMP
CDropTarget::DragLeave()
{
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragLeave();
    return S_OK;
}

LPCTSTR doLoadStr(UINT text);

STDMETHODIMP
CDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
    UpdateEffect(pdwEffect, grfKeyState);

    if (m_pDropTargetHelper)
        m_pDropTargetHelper->Drop(pDataObj, (LPPOINT)&pt, *pdwEffect);

    FORMATETC formatetc;
    formatetc.cfFormat = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
    formatetc.ptd      = NULL;
    formatetc.dwAspect = DVASPECT_CONTENT;
    formatetc.lindex   = -1;
    formatetc.tymed    = TYMED_HGLOBAL;

    STGMEDIUM medium;
    HRESULT hr = pDataObj->GetData(&formatetc, &medium);
    if (FAILED(hr))
    {
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
        StringCchCopy(szTempDir, _countof(szTempDir), g_temp_dir);
        PathAddBackslash(szTempDir);
        if (wcsstr(szSrcFilePath, szTempDir) == 0)
            break;

        StringCchCopy(szDestFilePath, _countof(szDestFilePath), g_root_dir);
        PathAppend(szDestFilePath, PathFindFileName(szSrcFilePath));

        SHFILEOPSTRUCT op = { m_hwnd, FO_COPY, szSrcFilePath, szDestFilePath };
        op.fFlags = FOF_ALLOWUNDO | FOF_RENAMEONCOLLISION;
        ::SHFileOperation(&op);

        CoTaskMemFree(pidl);
    }

    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return S_OK;
}
