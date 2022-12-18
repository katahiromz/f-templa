#pragma once

typedef BOOL (CALLBACK *INPUTBOXCALLBACK)(LPTSTR text);

BOOL APIENTRY
InputBox(
    HWND hwnd,
    string_t& input,
    LPCTSTR title OPTIONAL,
    LPCTSTR error_msg OPTIONAL,
    INPUTBOXCALLBACK verify OPTIONAL);
