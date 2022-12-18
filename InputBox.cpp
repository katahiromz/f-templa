#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include "f-templa.hpp"
#include "InputBox.hpp"
#include "resource.h"

typedef struct tagINPUTBOX
{
    string_t input;
    LPCTSTR title, error_msg;
    INPUTBOXCALLBACK verify;
} INPUTBOX, *PINPUTBOX;

static LPCTSTR doString(LPCTSTR text)
{
    static TCHAR s_szText[1024] = TEXT("");
    if (HIWORD(text) != 0)
        return text;

    s_szText[0] = 0;
    ::LoadString(NULL, LOWORD(text), s_szText, _countof(s_szText));
    return s_szText;
}

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    PINPUTBOX pInputBox = (PINPUTBOX)lParam;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)lParam);

    SetDlgItemText(hwnd, edt1, pInputBox->input.c_str());

    if (pInputBox->title)
    {
        SetWindowText(hwnd, doString(pInputBox->title));
    }

    return TRUE;
}

static BOOL OnOK(HWND hwnd)
{
    PINPUTBOX pInputBox = (PINPUTBOX)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    TCHAR text[1024];
    GetDlgItemText(hwnd, edt1, text, _countof(text));

    pInputBox->input = text;

    if (pInputBox->verify && !pInputBox->verify(text))
    {
        if (pInputBox->error_msg == NULL)
            pInputBox->error_msg = MAKEINTRESOURCE(IDS_INVALIDSTRING);

        SetDlgItemText(hwnd, stc1, doString(pInputBox->error_msg));
        return FALSE;
    }

    return TRUE;
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
        if (OnOK(hwnd))
        {
            EndDialog(hwnd, id);
        }
        break;
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    }
}

static INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

BOOL APIENTRY
InputBox(
    HWND hwnd,
    string_t& input,
    LPCTSTR title OPTIONAL,
    LPCTSTR error_msg OPTIONAL,
    INPUTBOXCALLBACK verify OPTIONAL)
{
    INPUTBOX box =
    {
        input, title, error_msg, verify
    };

    INT id = ::DialogBoxParam(::GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUTBOX),
                              hwnd, DialogProc, (LPARAM)&box);
    if (id == IDOK)
    {
        input = box.input;
        return TRUE;
    }

    return FALSE;
}
