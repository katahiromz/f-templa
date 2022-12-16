#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include "CDropTarget.hpp"
#include "resource.h"

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

extern TCHAR g_szRootDir[MAX_PATH];

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
        AppendMenu(hMenu, MF_STRING, 1, TEXT("Add Item"));
        AppendMenu(hMenu, MF_STRING, 2, TEXT("Cancel"));

        SetForegroundWindow(m_hwnd);
        INT nID = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
            pt.x, pt.y, 0, m_hwnd, NULL);
        PostMessage(m_hwnd, WM_NULL, 0, 0);
        if (nID == 0 || nID == -1 || nID == 2)
        {
            return E_FAIL;
        }
    }

    LPIDA lpIda = (LPIDA)GlobalLock(medium.hGlobal);
    for (UINT i = 0; i < lpIda->cidl; ++i)
    {
        PIDLIST_ABSOLUTE pidl = (PIDLIST_ABSOLUTE)(((LPBYTE)lpIda) + lpIda->aoffset[1 + i]);
        TCHAR szSrcFilePath[MAX_PATH];
        SHGetPathFromIDList(pidl, szSrcFilePath);

        TCHAR szDestFilePath[MAX_PATH];
        StringCchCopy(szDestFilePath, _countof(szDestFilePath), g_szRootDir);
        PathAppend(szDestFilePath, PathFindFileName(szSrcFilePath));

        MessageBox(NULL, szDestFilePath, szSrcFilePath, 0);
    }

    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return S_OK;
}
