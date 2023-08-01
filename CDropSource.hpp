#pragma once

class CDropSource : public IDropSource
{
public:
    CDropSource();
    ~CDropSource();

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
    STDMETHODIMP GiveFeedback(DWORD dwEffect) override;

    void BeginDrag();
    void EndDrag();
    BOOL IsDragging() const;

protected:
    LONG m_cRef;
};

extern CDropSource* g_pDropSource;
