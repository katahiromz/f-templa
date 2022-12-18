#include <windows.h>
#include <windowsx.h>
#include <cassert>
#include "f-templa.hpp"

void RepositionPointDx(LPPOINT ppt, SIZE siz, LPCRECT prc)
{
    if (ppt->x + siz.cx > prc->right)
        ppt->x = prc->right - siz.cx;
    if (ppt->y + siz.cy > prc->bottom)
        ppt->y = prc->bottom - siz.cy;
    if (ppt->x < prc->left)
        ppt->x = prc->left;
    if (ppt->y < prc->top)
        ppt->y = prc->top;
}

SIZE SizeFromRectDx(LPCRECT prc)
{
    SIZE siz;
    siz.cx = prc->right - prc->left;
    siz.cy = prc->bottom - prc->top;
    return siz;
}

RECT WorkAreaFromWindowDx(HWND hwnd)
{
#if (WINVER >= 0x0500)
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfo(hMonitor, &mi))
    {
        return mi.rcWork;
    }
#endif
    RECT rc;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
    return rc;
}

void CenterWindowDx(HWND hwnd)
{
    assert(IsWindow(hwnd));

    BOOL bChild = !!(GetWindowStyle(hwnd) & WS_CHILD);

    HWND hwndParent;
    if (bChild)
        hwndParent = ::GetParent(hwnd);
    else
        hwndParent = ::GetWindow(hwnd, GW_OWNER);

    RECT rcWorkArea = WorkAreaFromWindowDx(hwnd);

    RECT rcParent;
    if (hwndParent)
        ::GetWindowRect(hwndParent, &rcParent);
    else
        rcParent = rcWorkArea;

    SIZE sizParent = SizeFromRectDx(&rcParent);

    RECT rc;
    ::GetWindowRect(hwnd, &rc);
    SIZE siz = SizeFromRectDx(&rc);

    POINT pt;
    pt.x = rcParent.left + (sizParent.cx - siz.cx) / 2;
    pt.y = rcParent.top + (sizParent.cy - siz.cy) / 2;

    if (bChild && hwndParent)
    {
        ::GetClientRect(hwndParent, &rcParent);
        ::MapWindowPoints(hwndParent, NULL, (LPPOINT)&rcParent, 2);
        RepositionPointDx(&pt, siz, &rcParent);

        ::ScreenToClient(hwndParent, &pt);
    }
    else
    {
        RepositionPointDx(&pt, siz, &rcWorkArea);
    }

    ::SetWindowPos(hwnd, NULL, pt.x, pt.y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
