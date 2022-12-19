#include <windows.h>
#include <algorithm>
#include <cassert>
#include "ScrollView.hpp"
#undef min
#undef max

BOOL ScrollView_Init(PSCROLLVIEW pSV, HWND hwnd, INT bars)
{
    pSV->hwnd = hwnd;
    pSV->bars = bars;
    assert(bars == SB_BOTH || bars == SB_HORZ || bars == SB_VERT);

    RECT rc;
    ::GetClientRect(hwnd, &rc);
    pSV->siz.cx = rc.right - rc.left;
    pSV->siz.cy = rc.bottom - rc.top;

    SCROLLINFO si = { sizeof(si), SIF_PAGE | SIF_POS | SIF_RANGE };
    si.nPos = si.nMin = 1;

    if (bars == SB_BOTH || bars == SB_HORZ)
    {
        si.nMax = pSV->siz.cx;
        si.nPage = pSV->siz.cx;
        ::SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);
    }

    if (bars == SB_BOTH || bars == SB_VERT)
    {
        si.nMax = pSV->siz.cy;
        si.nPage = pSV->siz.cy;
        ::SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
    }

    return TRUE;
}

INT ScrollView_GetScrollPos(PSCROLLVIEW pSV, INT bar, UINT code)
{
    HWND hwnd = pSV->hwnd;
    SCROLLINFO si = { sizeof(si), SIF_ALL };
    ::GetScrollInfo(hwnd, bar, &si);

    INT nPos = -1, step = 10;
    INT nMax = si.nMax - (si.nPage - 1);

    switch (code)
    {
    case -1:
        nPos = std::max(si.nPos, si.nMin);
        nPos = std::min(nPos, nMax);
        break;

    case SB_LINEUP:
        nPos = std::max(si.nPos - step, si.nMin);
        break;

    case SB_LINEDOWN:
        nPos = std::min(si.nPos + step, nMax);
        break;

    case SB_PAGEUP:
        nPos = std::max(si.nPos - (INT)si.nPage, si.nMin);
        break;

    case SB_PAGEDOWN:
        nPos = std::min(si.nPos + (INT)si.nPage, nMax);
        break;

    case SB_THUMBPOSITION:
        // Do nothing
        break;

    case SB_THUMBTRACK:
        nPos = si.nTrackPos;
        break;

    case SB_TOP:
        nPos = si.nMin;
        break;

    case SB_BOTTOM:
        nPos = nMax;
        break;

    case SB_ENDSCROLL:
        // Do nothing
        break;
    }

    return nPos;
}

void ScrollView_Scroll(PSCROLLVIEW pSV, INT bar, INT pos)
{
    HWND hwnd = pSV->hwnd;
    static INT s_xPrev = 1, s_yPrev = 1;

    INT cx = 0, cy = 0;
    INT& delta = (bar == SB_HORZ ? cx : cy);
    INT& prev = (bar == SB_HORZ ? s_xPrev : s_yPrev);

    delta = prev - pos;
    prev = pos;

    if (cx || cy)
    {
        ::ScrollWindow(hwnd, cx, cy, NULL, NULL);
    }
}

static void ScrollView_OnHVScroll(PSCROLLVIEW pSV, HWND hwndCtl, UINT code, INT pos, INT bar)
{
    HWND hwnd = pSV->hwnd;
    INT new_pos = ScrollView_GetScrollPos(pSV, bar, code);
    if (new_pos == -1)
        return;

    ::SetScrollPos(hwnd, bar, new_pos, TRUE);
    ScrollView_Scroll(pSV, bar, new_pos);
}

void ScrollView_OnHScroll(PSCROLLVIEW pSV, HWND hwndCtl, UINT code, INT pos)
{
    ScrollView_OnHVScroll(pSV, hwndCtl, code, pos, SB_HORZ);
}

void ScrollView_OnVScroll(PSCROLLVIEW pSV, HWND hwndCtl, UINT code, INT pos)
{
    ScrollView_OnHVScroll(pSV, hwndCtl, code, pos, SB_VERT);
}

void ScrollView_OnSize(PSCROLLVIEW pSV, UINT state, INT cx, INT cy)
{
    HWND hwnd = pSV->hwnd;

    if (state != SIZE_RESTORED && state != SIZE_MAXIMIZED)
        return;

    const int bar[] = { SB_HORZ, SB_VERT };
    const int page[] = { cx, cy };
    const int original[] = { pSV->siz.cx, pSV->siz.cy };
    SCROLLINFO si = { sizeof(si) };

    for (size_t i = 0; i < ARRAYSIZE(bar); ++i)
    {
        if (pSV->bars != SB_BOTH && pSV->bars != bar[i])
            continue;

        si.fMask = SIF_PAGE;
        si.nPage = page[i];
        ::SetScrollInfo(hwnd, bar[i], &si, TRUE);
        ::ShowScrollBar(hwnd, bar[i], page[i] < original[i]);

        si.fMask = SIF_POS;
        ::GetScrollInfo(hwnd, bar[i], &si);
        ScrollView_Scroll(pSV, bar[i], si.nPos);
    }
}
