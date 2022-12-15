#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include "CDropSource.hpp"
#include "resource.h"

CDropSource::CDropSource() : m_cRef(1)
{
}

CDropSource::~CDropSource()
{
}

STDMETHODIMP CDropSource::QueryInterface(REFIID riid, void **ppvObject)
{
    *ppvObject = NULL;

    if (riid == IID_IUnknown || riid == IID_IDropSource)
        *ppvObject = static_cast<IDropSource *>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CDropSource::AddRef()
{
    return ::InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CDropSource::Release()
{
    if (::InterlockedDecrement(&m_cRef) == 0)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

STDMETHODIMP CDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{
    if (fEscapePressed)
        return DRAGDROP_S_CANCEL;

    if ((grfKeyState & (MK_LBUTTON | MK_RBUTTON)) == 0)
        return DRAGDROP_S_DROP;

    return S_OK;
}

STDMETHODIMP CDropSource::GiveFeedback(DWORD dwEffect)
{
    return DRAGDROP_S_USEDEFAULTCURSORS;
}
