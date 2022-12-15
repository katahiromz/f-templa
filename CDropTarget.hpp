#pragma once

class CDropTarget : public IDropTarget
{
public:
    CDropTarget(HWND hwnd);
    ~CDropTarget();

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    
    STDMETHODIMP DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;

protected:
    LONG m_cRef;
    HWND m_hwnd;
    IDropTargetHelper *m_pDropTargetHelper;
    BOOL m_bRight;

    void UpdateEffect(DWORD *pdwEffect, DWORD grfKeyState);
};
