#pragma once

typedef struct tagSCROLLVIEW
{
    HWND hwnd;
    INT bars;
    SIZE siz;
} SCROLLVIEW, *PSCROLLVIEW;

BOOL ScrollView_Init(PSCROLLVIEW pSV, HWND hwnd, INT bars = SB_BOTH);
INT ScrollView_GetScrollPos(PSCROLLVIEW pSV, INT bar, UINT code);
void ScrollView_Scroll(PSCROLLVIEW pSV, INT bar, INT pos);
void ScrollView_OnHScroll(PSCROLLVIEW pSV, HWND hwndCtl, UINT code, INT pos);
void ScrollView_OnVScroll(PSCROLLVIEW pSV, HWND hwndCtl, UINT code, INT pos);
void ScrollView_OnSize(PSCROLLVIEW pSV, UINT state, INT cx, INT cy);
