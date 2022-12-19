#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <map>
#include <algorithm>
#include <cassert>
#include "CDropTarget.hpp"
#include "CDropSource.hpp"
#include "ScrollView.hpp"
#include "f-templa.hpp"
#include "fdt_file.hpp"
#include "InputBox.hpp"
#include "resource.h"

#define CLASSNAME TEXT("FolderDeTemple")
#define WM_SHELLCHANGE (WM_USER + 100)
#define MAX_REPLACEITEMS 16

HWND g_hWnd = NULL;
HWND g_hListView = NULL;
HIMAGELIST g_hImageList = NULL;
INT g_iDialog = 0;
HWND g_hwndDialogs[2] = { NULL };
HWND g_hStatusBar = NULL;
TCHAR g_root_dir[MAX_PATH] = TEXT("");
TCHAR g_temp_dir[MAX_PATH + 1] = TEXT("");
CDropTarget* g_pDropTarget = NULL;
CDropSource* g_pDropSource = NULL;
UINT g_nNotifyID = 0;
SCROLLVIEW g_Dialog1ScrollView, g_Dialog2ScrollView;
string_list_t g_ignore = { L"q", L"*.bin", L".git", L".svg", L".vs" };
std::vector<std::pair<string_t, string_t>> g_history;

static INT s_iItemOld = -1;

LPCTSTR doLoadStr(LPCTSTR text)
{
    static TCHAR s_szText[1024] = TEXT("");
    if (HIWORD(text))
        return text;
    s_szText[0] = 0;
    LoadString(NULL, LOWORD(text), s_szText, _countof(s_szText));
    return s_szText;
}

LPCTSTR doLoadStr(UINT text)
{
    return doLoadStr(MAKEINTRESOURCE(text));
}


static void InitListView(HWND hListView, HIMAGELIST hImageList, LPCTSTR pszDir)
{
    TCHAR spec[MAX_PATH];

    GetFullPathName(pszDir, _countof(g_root_dir), g_root_dir, NULL);

    StringCchCopy(spec, _countof(spec), g_root_dir);
    PathAddBackslash(spec);
    StringCchCat(spec, _countof(spec), TEXT("*"));

    INT i = 0;
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(spec, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (lstrcmp(find.cFileName, TEXT(".")) == 0 ||
                lstrcmp(find.cFileName, TEXT("..")) == 0)
            {
                continue;
            }

            if (templa_wildcard(find.cFileName, TEXT("*.fdt")))
                continue;

            SHFILEINFO info;
            SHGetFileInfo(find.cFileName, find.dwFileAttributes, &info, sizeof(info),
                          SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

            LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE };
            item.iItem    = i++;
            item.iSubItem = 0;
            item.pszText  = find.cFileName;
            item.iImage   = info.iIcon;
            ListView_InsertItem(hListView, &item);
        } while (FindNextFile(hFind, &find));

        FindClose(hFind);
    }
}

static BOOL Dialog1_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    ScrollView_Init(&g_Dialog1ScrollView, hwnd, SB_VERT);
    return TRUE;
}

static void Dialog1_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
}

static void Dialog1_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    ScrollView_OnSize(&g_Dialog1ScrollView, state, cx, cy);
}

static void Dialog1_OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
{
    ScrollView_OnVScroll(&g_Dialog1ScrollView, hwndCtl, code, pos);
}

static void Dialog1_OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    if (zDelta > 0)
    {
        for (INT i = 0; i < 3; ++i)
            ::PostMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
    }
    else if (zDelta < 0)
    {
        for (INT i = 0; i < 3; ++i)
            ::PostMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
    }
}

static INT_PTR CALLBACK
Dialog1Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Dialog1_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Dialog1_OnCommand);
        HANDLE_MSG(hwnd, WM_SIZE, Dialog1_OnSize);
        HANDLE_MSG(hwnd, WM_VSCROLL, Dialog1_OnVScroll);
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, Dialog1_OnMouseWheel);
    }
    return 0;
}

static BOOL Dialog2_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    ScrollView_Init(&g_Dialog2ScrollView, hwnd, SB_VERT);
    return TRUE;
}

mapping_t DoGetMapping(void)
{
    HWND hwndDlg = g_hwndDialogs[1];

    mapping_t mapping;
    for (UINT i = 0; i < MAX_REPLACEITEMS; ++i)
    {
        TCHAR szKey[128];
        TCHAR szValue[1024];
        ::GetDlgItemText(hwndDlg, IDC_FROM_00 + i, szKey, _countof(szKey));
        ::GetDlgItemText(hwndDlg, IDC_TO_00 + i, szValue, _countof(szValue));
        StrTrim(szKey, TEXT(" \t\r\n\x3000"));
        if (szKey[0] == 0)
            continue;

        StrTrim(szValue, TEXT(" \t\r\n\x3000"));
        mapping[szKey] = szValue;
    }

    return mapping;
}

static BOOL CALLBACK InputBoxCallback(LPTSTR text)
{
    StrTrim(text, TEXT(" \t\r\n\x3000"));
    return text[0] != 0 && text[0] != L':';
}

static string_list_t g_deleted_sections;

static BOOL Preset_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    for (auto& name : *(string_list_t*)lParam)
    {
        ListBox_AddString(hLst1, name.c_str());
    }
    ListBox_SetCurSel(hLst1, 0);

    g_deleted_sections.clear();

    CenterWindowDx(hwnd);
    return TRUE;
}

static void Preset_OnDelete(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    INT iItem = ListBox_GetCurSel(hLst1);
    if (iItem != LB_ERR)
    {
        TCHAR szText[1024];
        ListBox_GetText(hLst1, iItem, szText);
        ListBox_DeleteString(hLst1, iItem);
        g_deleted_sections.push_back(szText);
        if (iItem >= ListBox_GetCount(hLst1))
            ListBox_SetCurSel(hLst1, iItem - 1);
        else
            ListBox_SetCurSel(hLst1, iItem);
    }
}

static void Preset_OnDeleteAll(HWND hwnd)
{
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    INT nCount = ListBox_GetCount(hLst1);
    for (INT iItem = nCount - 1; iItem >= 0; --iItem)
    {
        TCHAR szText[1024];
        ListBox_GetText(hLst1, iItem, szText);
        ListBox_DeleteString(hLst1, iItem);
        g_deleted_sections.push_back(szText);
    }
}

static void Preset_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case psh1:
        Preset_OnDelete(hwnd);
        break;
    case psh2:
        Preset_OnDeleteAll(hwnd);
        break;
    case IDOK:
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    }
}

static INT_PTR CALLBACK
PresetDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Preset_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Preset_OnCommand);
    }
    return 0;
}

static void ReplacePair(const string_t& first, string_t& second)
{
    if (first == L"{{TODAY}}")
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        TCHAR szText[128];
        StringCchPrintf(szText, _countof(szText), TEXT("%04u-%02u-%02u"),
            st.wYear,
            st.wMonth,
            st.wDay);
        second += szText;
        return;
    }
    if (first == L"{{NOW}}")
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        TCHAR szText[128];
        StringCchPrintf(szText, _countof(szText), TEXT("%02u:%02u:%02u"),
            st.wHour,
            st.wMinute,
            st.wSecond);
        second += szText;
        return;
    }
    if (first == L"{{THISYEAR}}")
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        second = std::to_wstring(st.wYear);
        return;
    }
    if (first == L"{{USER}}")
    {
        TCHAR szUser[MAX_PATH];
        DWORD cchUser = _countof(szUser);
        ::GetUserName(szUser, &cchUser);
        second = szUser;
        return;
    }
    if (first == L"{{WEEKDAY}}")
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        switch (st.wDayOfWeek)
        {
        case 0: second = L"Sun"; break;
        case 1: second = L"Mon"; break;
        case 2: second = L"Tue"; break;
        case 3: second = L"Wed"; break;
        case 4: second = L"Thu"; break;
        case 5: second = L"Fri"; break;
        case 6: second = L"Sat"; break;
        default: second.clear(); break;
        }
        return;
    }
    if (first == doLoadStr(IDS_TODAY))
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        second = std::to_wstring(st.wYear);
        second += doLoadStr(IDS_YEAR);
        second += std::to_wstring(st.wMonth);
        second += doLoadStr(IDS_MONTH);
        second += std::to_wstring(st.wDay);
        second += doLoadStr(IDS_DAY);
        return;
    }
    if (first == doLoadStr(IDS_NOW))
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        second = std::to_wstring(st.wHour);
        second += doLoadStr(IDS_HOUR);
        second += std::to_wstring(st.wMinute);
        second += doLoadStr(IDS_MINUTE);
        second += std::to_wstring(st.wSecond);
        second += doLoadStr(IDS_SECOND);
        return;
    }
    if (first == doLoadStr(IDS_THISYEAR))
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        second = std::to_wstring(st.wYear);
        second += doLoadStr(IDS_YEAR);
        return;
    }
    if (first == doLoadStr(IDS_USERNAME) || first == doLoadStr(IDS_USERNAME2))
    {
        TCHAR szUser[MAX_PATH];
        DWORD cchUser = _countof(szUser);
        ::GetUserName(szUser, &cchUser);
        second = szUser;
        return;
    }
    if (first == doLoadStr(IDS_WEEKDAY))
    {
        SYSTEMTIME st;
        ::GetLocalTime(&st);
        assert(st.wDayOfWeek < 7);
        second.clear();
        second += doLoadStr(IDS_WEEKDAYS)[st.wDayOfWeek];
        return;
    }
}

static void Dialog2_RefreshSubst(HWND hwnd, mapping_t& mapping)
{
    ::SendMessage(hwnd, WM_COMMAND, ID_REFRESH_SUBST, 0);

    UINT i = 0;
    for (auto& pair : mapping)
    {
        ::SetDlgItemText(hwnd, IDC_FROM_00 + i, pair.first.c_str());
        ::SetDlgItemText(hwnd, IDC_TO_00 + i, pair.second.c_str());
        ++i;
        if (i >= MAX_REPLACEITEMS)
            break;
    }
}

static void Dialog2_OnPreset(HWND hwnd)
{
    HWND hwndButton = GetDlgItem(hwnd, IDC_DARROW_PRESET);
    assert(hwndButton);

    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1)
        return;

    TCHAR szItem[MAX_PATH], szPath[MAX_PATH];
    ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
    StringCchCopy(szPath, _countof(szPath), g_root_dir);
    PathAppend(szPath, szItem);

    string_t path = szPath;
    path += L".fdt";

    RECT rc;
    ::GetWindowRect(hwndButton, &rc);

    HMENU hMenu = ::CreatePopupMenu();
    ::AppendMenu(hMenu, MF_STRING, ID_UPDATEVALUE, doLoadStr(IDS_UPDATEVALUE));
    ::AppendMenu(hMenu, MF_STRING, ID_SAVEPRESET, doLoadStr(IDS_SAVEPRESET));

    FDT_FILE fdt_file;
    fdt_file.load(path.c_str());

    string_list_t section_names;
    for (auto& pair : fdt_file.name2section)
    {
        if (pair.first.size() && pair.first[0] == L':')
            continue;

        section_names.push_back(pair.first);
    }

    ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    const INT c_section_first = 1000;
    if (section_names.size())
    {
        std::sort(section_names.begin(), section_names.end());

        INT i = c_section_first;
        for (auto& name : section_names)
        {
            ::AppendMenu(hMenu, MF_STRING, i++, name.c_str());
        }

        ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        ::AppendMenu(hMenu, MF_STRING, ID_EDITPRESET, doLoadStr(IDS_EDITPRESET));
    }

    ::AppendMenu(hMenu, MF_STRING, ID_RESET, doLoadStr(IDS_RESET));

    INT iChoice = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON,
                                   rc.right, rc.top, 0, hwnd, &rc);
    ::DestroyMenu(hMenu);
    if (iChoice == 0)
        return;

    if (iChoice >= c_section_first)
    {
        auto& name = section_names[iChoice - c_section_first];
        auto& section = fdt_file.name2section[name];
        for (auto& item : section.items)
        {
            for (INT id = IDC_FROM_00; id < IDC_FROM_00 + MAX_REPLACEITEMS; ++id)
            {
                TCHAR szFrom[128];
                GetDlgItemText(hwnd, id, szFrom, _countof(szFrom));
                StrTrim(szFrom, TEXT(" \t\r\n\x3000"));

                if (item.first == szFrom)
                {
                    SetDlgItemText(hwnd, IDC_TO_00 + (id - IDC_FROM_00), item.second.c_str());
                    break;
                }
            }
        }
        return;
    }

    if (iChoice == ID_SAVEPRESET)
    {
        TCHAR name[128];
        const INT c_max = 100;
        INT iSection = 0;
        for (; iSection < c_max; ++iSection)
        {
            bool found = false;
            StringCchPrintf(name, _countof(name), doLoadStr(IDS_SECTIONAME), iSection + 1);
            for (auto& pair : fdt_file.name2section)
            {
                if (pair.first != name)
                    continue;

                found = true;
                break;
            }

            if (!found)
                break;
        }

        if (iSection != c_max)
        {
            string_t section_name = name;
            if (InputBox(hwnd, section_name, MAKEINTRESOURCE(IDS_DONAME), NULL, InputBoxCallback))
            {
                auto& section = fdt_file.name2section[section_name];
                mapping_t mapping = DoGetMapping();
                for (auto& pair : mapping)
                {
                    section.assign(pair.first, pair.second);
                }

                fdt_file.save(path.c_str());
            }
        }
    }

    if (iChoice == ID_EDITPRESET)
    {
        if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PRESET),
                           hwnd, PresetDialogProc, (LPARAM)&section_names) == IDOK)
        {
            for (auto& name : g_deleted_sections)
            {
                fdt_file.name2section.erase(name);
            }
            fdt_file.save(path.c_str());
        }
    }

    if (iChoice == ID_RESET)
    {
        ::DeleteFile(path.c_str());

        mapping_t mapping;
        if (PathIsDirectory(szPath))
            Dialog2_InitSubstDir(hwnd, szPath, mapping);
        else
            Dialog2_InitSubstFile(hwnd, szPath, mapping);

        for (auto& pair : mapping)
        {
            ReplacePair(pair.first, pair.second);
        }

        Dialog2_RefreshSubst(hwnd, mapping);

        g_history.clear();
    }

    if (iChoice == ID_UPDATEVALUE)
    {
        mapping_t mapping = DoGetMapping();

        for (auto& pair : mapping)
        {
            ReplacePair(pair.first, pair.second);
        }

        Dialog2_RefreshSubst(hwnd, mapping);
    }
}

static void Dialog2_OnDownArrow(HWND hwnd, INT id)
{
    INT index = id - IDC_DARROW_00;
    UINT nEditFromID = IDC_FROM_00 + index;
    UINT nEditToID = IDC_TO_00 + index;
    RECT rc;
    ::GetWindowRect(GetDlgItem(hwnd, id), &rc);

    TCHAR szKey[128];
    ::GetDlgItemText(hwnd, nEditFromID, szKey, _countof(szKey));
    StrTrim(szKey, TEXT(" \t\r\n\x3000"));

    TCHAR szValue[1024];
    ::GetDlgItemText(hwnd, nEditToID, szValue, _countof(szValue));
    StrTrim(szValue, TEXT(" \t\r\n\x3000"));

    string_list_t strs;
    for (auto& pair : g_history)
    {
        if (pair.first == szKey)
        {
            if (std::find(strs.begin(), strs.end(), pair.second) == strs.end())
                strs.push_back(pair.second);
        }
    }

    HMENU hMenu = CreatePopupMenu();

    ::AppendMenu(hMenu, MF_STRING, ID_UPDATEVALUE, doLoadStr(IDS_UPDATEVALUE));
    ::AppendMenu(hMenu, MF_STRING, ID_RESET, doLoadStr(IDS_RESET));

    const INT c_history_first = 1000;
    if (strs.size() || szValue[0])
    {
        BOOL bFound = FALSE;
        if (strs.size())
        {
            ::AppendMenu(hMenu, MF_SEPARATOR, 0, L"");

            INT i = c_history_first;
            for (auto& str : strs)
            {
                ::AppendMenu(hMenu, MF_STRING, i++, str.c_str());
                if (str == szValue)
                    bFound = TRUE;
            }
        }


        if (!bFound && szValue[0])
        {
            ::AppendMenu(hMenu, MF_SEPARATOR, 0, L"");

            TCHAR szItem[1024];
            StringCchPrintf(szItem, _countof(szItem), doLoadStr(IDS_ADDITEM), szValue);
            ::AppendMenu(hMenu, MF_STRING, ID_ADDITEM, szItem);
        }

        ::AppendMenu(hMenu, MF_SEPARATOR, 0, L"");
        ::AppendMenu(hMenu, MF_STRING, ID_DELETEKEYANDVALUE, doLoadStr(IDS_DELETEKEYVALUE));
    }

    INT iChoice = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON,
                                   rc.right, rc.top, 0, hwnd, &rc);
    ::DestroyMenu(hMenu);
    if (iChoice != 0)
    {
        if (iChoice >= c_history_first)
        {
            HWND hwndEdit = ::GetDlgItem(hwnd, nEditToID);
            assert(hwndEdit);
            ::SetWindowText(hwndEdit, strs[iChoice - c_history_first].c_str());
            Edit_SetSel(hwndEdit, 0, -1);
            SetFocus(hwndEdit);
        }
        else if (iChoice == ID_ADDITEM)
        {
            g_history.push_back(std::make_pair(szKey, szValue));
        }
        else if (iChoice == ID_UPDATEVALUE)
        {
            std::pair<string_t, string_t> pair;
            pair.first = szKey;
            ReplacePair(pair.first, pair.second);

            HWND hwndEdit = ::GetDlgItem(hwnd, nEditToID);
            assert(hwndEdit);
            ::SetWindowText(hwndEdit, pair.second.c_str());
            Edit_SetSel(hwndEdit, 0, -1);
            SetFocus(hwndEdit);
        }
        else if (iChoice == ID_RESET || iChoice == ID_DELETEKEYANDVALUE)
        {
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (iItem == -1)
                return;

            if (iChoice == ID_DELETEKEYANDVALUE)
            {
                ::SetDlgItemText(hwnd, nEditFromID, NULL);
                ::SetDlgItemText(hwnd, nEditToID, NULL);
            }
            else if (iChoice == ID_RESET)
            {
                std::pair<string_t, string_t> pair;
                pair.first = szKey;
                ReplacePair(pair.first, pair.second);

                HWND hwndEdit = ::GetDlgItem(hwnd, nEditToID);
                assert(hwndEdit);
                ::SetWindowText(hwndEdit, pair.second.c_str());
                Edit_SetSel(hwndEdit, 0, -1);
                SetFocus(hwndEdit);
            }

            TCHAR szItem[MAX_PATH], szPath[MAX_PATH];
            ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
            StringCchCopy(szPath, _countof(szPath), g_root_dir);
            PathAppend(szPath, szItem);

            string_t path = szPath;
            path += L".fdt";

            FDT_FILE fdt_file;
            if (fdt_file.load(path.c_str()))
            {
                for (auto& pair : fdt_file.name2section)
                {
                    auto& section = pair.second;
                    section.erase(szKey);

                    if (iChoice == ID_RESET)
                    {
                        section.assign(szKey, szValue);
                    }
                }
                fdt_file.save(path.c_str());
            }

            for (size_t i = g_history.size() - 1; i < g_history.size(); --i)
            {
                if (g_history[i].first == szKey)
                {
                    g_history.erase(g_history.begin() + i);
                }
            }
        }
    }
}

static void Dialog2_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_REFRESH_SUBST:
        for (UINT id = IDC_FROM_00; id <= IDC_FROM_15; ++id)
            ::SetDlgItemText(hwnd, id, NULL);
        for (UINT id = IDC_TO_00; id <= IDC_TO_15; ++id)
            ::SetDlgItemText(hwnd, id, NULL);
        break;
    case IDC_RARROW_00:
    case IDC_RARROW_01:
    case IDC_RARROW_02:
    case IDC_RARROW_03:
    case IDC_RARROW_04:
    case IDC_RARROW_05:
    case IDC_RARROW_06:
    case IDC_RARROW_07:
    case IDC_RARROW_08:
    case IDC_RARROW_09:
    case IDC_RARROW_10:
    case IDC_RARROW_11:
    case IDC_RARROW_12:
    case IDC_RARROW_13:
    case IDC_RARROW_14:
    case IDC_RARROW_15:
        if (codeNotify == STN_CLICKED)
        {
            UINT nEditID = IDC_TO_00 + (id - IDC_RARROW_00);
            HWND hwndEdit = ::GetDlgItem(hwnd, nEditID);
            assert(hwndEdit);
            Edit_SetSel(hwndEdit, 0, -1);
            ::SetFocus(hwndEdit);
        }
        break;
    case IDC_DARROW_PRESET:
        Dialog2_OnPreset(hwnd);
        break;
    case IDC_DARROW_00:
    case IDC_DARROW_01:
    case IDC_DARROW_02:
    case IDC_DARROW_03:
    case IDC_DARROW_04:
    case IDC_DARROW_05:
    case IDC_DARROW_06:
    case IDC_DARROW_07:
    case IDC_DARROW_08:
    case IDC_DARROW_09:
    case IDC_DARROW_10:
    case IDC_DARROW_11:
    case IDC_DARROW_12:
    case IDC_DARROW_13:
    case IDC_DARROW_14:
    case IDC_DARROW_15:
        if (codeNotify == BN_CLICKED)
        {
            Dialog2_OnDownArrow(hwnd, id);
        }
        break;
    }
}

static void Dialog2_OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
{
    ScrollView_OnVScroll(&g_Dialog2ScrollView, hwndCtl, code, pos);
}

static void Dialog2_OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    if (zDelta > 0)
    {
        for (INT i = 0; i < 3; ++i)
            ::PostMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
    }
    else if (zDelta < 0)
    {
        for (INT i = 0; i < 3; ++i)
            ::PostMessage(hwnd, WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
    }
}

void Dialog2_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    ScrollView_OnSize(&g_Dialog2ScrollView, state, cx, cy);
}

static INT_PTR CALLBACK
Dialog2Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Dialog2_OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, Dialog2_OnCommand);
        HANDLE_MSG(hwnd, WM_VSCROLL, Dialog2_OnVScroll);
        HANDLE_MSG(hwnd, WM_SIZE, Dialog2_OnSize);
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, Dialog2_OnMouseWheel);
    }
    return 0;
}

static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    g_hWnd = hwnd;

    ::OleInitialize(NULL);

    DWORD style = WS_CHILD | WS_VISIBLE | LVS_ICON |
        LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_SINGLESEL |
        WS_BORDER | WS_VSCROLL | WS_HSCROLL;
    DWORD exstyle = WS_EX_CLIENTEDGE;
    g_hListView = ::CreateWindowEx(exstyle, WC_LISTVIEW, NULL, style,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)1, GetModuleHandle(NULL), NULL);
    if (g_hListView == NULL)
        return FALSE;

    SHFILEINFO info;
    g_hImageList = (HIMAGELIST)SHGetFileInfo(TEXT("C:\\"), 0, &info, sizeof(info), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
    if (g_hImageList == NULL)
        return FALSE;
    ListView_SetImageList(g_hListView, g_hImageList, LVSIL_NORMAL);

    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("Templates"));
    if (!PathIsDirectory(szPath))
    {
        PathRemoveFileSpec(szPath);
        PathAppend(szPath, TEXT("..\\Templates"));
        if (!PathIsDirectory(szPath))
        {
            PathRemoveFileSpec(szPath);
            PathAppend(szPath, TEXT("..\\Templates"));
        }
    }

    InitListView(g_hListView, g_hImageList, szPath);

    static const UINT ids[] = { IDD_TOP, IDD_SUBST };
    DLGPROC procs[] = { Dialog1Proc, Dialog2Proc };
    for (UINT i = 0; i < _countof(g_hwndDialogs); ++i)
    {
        g_hwndDialogs[i] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(ids[i]), hwnd, procs[i]);
        if (!g_hwndDialogs[i])
            return FALSE;
    }
    ::ShowWindow(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);

    style = WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP;
    g_hStatusBar = ::CreateStatusWindow(style, doLoadStr(IDS_SELECTITEM), hwnd, 2);
    if (g_hStatusBar == NULL)
        return FALSE;

    LPITEMIDLIST pidlDesktop;
    SHGetSpecialFolderLocation(hwnd, CSIDL_DESKTOP, &pidlDesktop);
    SHChangeNotifyEntry  entry;
    entry.pidl = pidlDesktop;
    entry.fRecursive = TRUE;
    LONG nEvents = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR |
                   SHCNE_RENAMEFOLDER | SHCNE_RENAMEITEM;
    g_nNotifyID = SHChangeNotifyRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_NewDelivery,
                                         nEvents, WM_SHELLCHANGE, 1, &entry);

    g_pDropTarget = new CDropTarget(hwnd);
    ::RegisterDragDrop(hwnd, g_pDropTarget);
    g_pDropSource = new CDropSource();

    ::PostMessage(hwnd, WM_SIZE, 0, 0);
    return TRUE;
}

static void OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized)
{
    ::SetFocus(g_hListView);
}

static void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;
    ::GetWindowRect(g_hwndDialogs[g_iDialog], &rc);
    INT cxDialog = rc.right - rc.left;
    ::SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    ::GetWindowRect(g_hStatusBar, &rc);
    INT cyStatusBar = rc.bottom - rc.top;

    ::GetClientRect(hwnd, &rc);

    rc.bottom -= cyStatusBar;
    INT cyDialog = rc.bottom - rc.top;

    ::MoveWindow(g_hwndDialogs[g_iDialog], 0, 0, cxDialog, cyDialog, TRUE);
    rc.left += cxDialog;
    ::MoveWindow(g_hListView, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

HRESULT ExecuteContextCommand(HWND hwnd, IContextMenu *pContextMenu, UINT nCmd)
{
    CMINVOKECOMMANDINFO info = { sizeof(info) };
    info.lpVerb = MAKEINTRESOURCEA(nCmd);
    info.hwnd = hwnd;
    info.fMask = CMIC_MASK_UNICODE;
    info.nShow = SW_SHOWNORMAL;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        info.fMask |= CMIC_MASK_SHIFT_DOWN;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        info.fMask |= CMIC_MASK_CONTROL_DOWN;
    return pContextMenu->InvokeCommand(&info);
}

BOOL ShowContextMenu(HWND hwnd, INT iItem, INT xPos, INT yPos, UINT uFlags = CMF_NORMAL)
{
    HRESULT hr;
    IShellFolder *pFolder = NULL;
    LPCITEMIDLIST pidlChild;
    IContextMenu *pContextMenu = NULL;
    LPITEMIDLIST pidl = NULL;
    TCHAR szPath[MAX_PATH];
    TCHAR szItem[MAX_PATH];
    HMENU hMenu = CreatePopupMenu();
    UINT nID;

    if (iItem == -1)
    {
        StringCchCopy(szPath, _countof(szPath), g_root_dir);
    }
    else
    {
        ListView_EnsureVisible(g_hListView, iItem, FALSE);
        ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
        StringCchCopy(szPath, _countof(szPath), g_root_dir);
        PathAppend(szPath, szItem);
    }

    hr = SHParseDisplayName(szPath, 0, &pidl, 0, 0);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = SHBindToParent(pidl, IID_IShellFolder, (void **)&pFolder, &pidlChild);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = pFolder->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, NULL, (void **)&pContextMenu);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    hr = pContextMenu->QueryContextMenu(hMenu, 0, FCIDM_SHVIEWFIRST, FCIDM_SHVIEWLAST, uFlags);
    if (FAILED(hr))
    {
        assert(0);
        goto Finish;
    }

    if (iItem == -1)
    {
        // properties only
        while (GetMenuItemCount(hMenu) > 1)
        {
            DeleteMenu(hMenu, 0, MF_BYPOSITION);
        }
    }

    if (uFlags & CMF_DEFAULTONLY)
    {
        nID = GetMenuItemID(hMenu, 0);
        ExecuteContextCommand(hwnd, pContextMenu, nID);
    }
    else
    {
        SetForegroundWindow(hwnd);
        nID = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             xPos, yPos, 0, hwnd, NULL);
        if (nID != 0 && nID != -1)
        {
            ExecuteContextCommand(hwnd, pContextMenu, nID);
        }
    }

Finish:
    if (hMenu)
        DestroyMenu(hMenu);
    if (pidl)
        CoTaskMemFree(pidl);
    if (pFolder)
        pFolder->Release();
    if (pContextMenu)
        pContextMenu->Release();
    return TRUE;
}

static void OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1)
    {
        ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_NODEFAULT | CMF_NOVERBS);
        return;
    }

    if (xPos == 0xFFFF && yPos == 0xFFFF)
    {
        RECT rc;
        ListView_GetItemRect(g_hListView, iItem, &rc, LVIR_ICON);
        POINT pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        ::ClientToScreen(hwnd, &pt);
        xPos = pt.x;
        yPos = pt.y;
    }

    ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_EXPLORE | CMF_NODEFAULT);
}

static void DeleteTempDir(HWND hwnd)
{
    if (g_temp_dir[0])
    {
        SHFILEOPSTRUCT op = { hwnd, FO_DELETE, g_temp_dir };
        op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;
        ::SHFileOperation(&op);
        ZeroMemory(g_temp_dir, sizeof(g_temp_dir));
    }
}

bool SaveFdtFile(LPCTSTR pszBasePath, mapping_t& mapping)
{
    string_t path = pszBasePath;
    path += L".fdt";

    FDT_FILE fdt_file;
    fdt_file.load(path.c_str());

    auto& latest_section = fdt_file.name2section[L":LATEST"];
    for (auto& pair : mapping)
    {
        latest_section.assign(pair.first, pair.second);
    }

    auto& history_section = fdt_file.name2section[L":HISTORY"];
    for (auto& pair : mapping)
    {
        history_section.assign(pair);
    }
    for (auto& pair : g_history)
    {
        history_section.assign(pair);
    }

    return fdt_file.save(path.c_str());
}

static void OnDeSelectItem(HWND hwnd, INT iItem)
{
    TCHAR szItem[MAX_PATH], szPath[MAX_PATH];
    ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
    StringCchCopy(szPath, _countof(szPath), g_root_dir);
    PathAppend(szPath, szItem);

    mapping_t mapping = DoGetMapping();
    SaveFdtFile(szPath, mapping);
}

static void OnDestroy(HWND hwnd)
{
    if (g_iDialog == 1 && s_iItemOld != -1)
        OnDeSelectItem(hwnd, s_iItemOld);

    DeleteTempDir(hwnd);

    if (g_nNotifyID)
    {
        SHChangeNotifyDeregister(g_nNotifyID);
        g_nNotifyID = 0;
    }
    ::RevokeDragDrop(hwnd);
    if (g_pDropTarget)
    {
        g_pDropTarget->Release();
        g_pDropTarget = NULL;
    }
    if (g_pDropSource)
    {
        g_pDropSource->Release();
        g_pDropSource = NULL;
    }
    if (g_hImageList)
    {
        ImageList_Destroy(g_hImageList);
        g_hImageList = NULL;
    }
    if (g_hListView)
    {
        ::DestroyWindow(g_hListView);
        g_hListView = NULL;
    }
    for (UINT i = 0; i < _countof(g_hwndDialogs); ++i)
    {
        if (g_hwndDialogs[i])
        {
            ::DestroyWindow(g_hwndDialogs[i]);
            g_hwndDialogs[i] = NULL;
        }
    }
    OleUninitialize();
    PostQuitMessage(0);
}

BOOL Dialog2_FindSubst(HWND hwndDlg, const string_t& str, mapping_t& mapping)
{
    size_t ich0 = 0;
    for (;;)
    {
        size_t ich = str.find(L"{{", ich0);
        if (ich == str.npos)
            return TRUE;
        ich += 2;
        ich0 = str.find(L"}}", ich);
        if (ich0 == str.npos)
            return TRUE;
        auto tag = str.substr(ich, ich0 - ich);
        tag = L"{{" + tag + L"}}";
        mapping[tag] = L"";
    }

    return FALSE;
}

BOOL Dialog2_InitSubstFile(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping)
{
    TEMPLA_FILE file;
    if (!file.load(pszPath))
        return TRUE;

    return Dialog2_FindSubst(hwndDlg, file.m_string, mapping);
}

BOOL Dialog2_InitSubstDir(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping)
{
    string_t str = PathFindFileName(pszPath);

    Dialog2_FindSubst(hwndDlg, str, mapping);

    TCHAR szSpec[MAX_PATH];
    StringCchCopy(szSpec, _countof(szSpec), pszPath);
    PathAppend(szSpec, L"*");
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(szSpec, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            auto filename = find.cFileName;
            if (filename[0] == L'.')
            {
                if (filename[1] == 0)
                    continue;
                if (filename[1] == L'.' && filename[2] == 0)
                    continue;
            }

            Dialog2_FindSubst(hwndDlg, filename, mapping);

            string_t path = pszPath;
            path += L'\\';
            path += filename;

            if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                Dialog2_InitSubstDir(hwndDlg, path.c_str(), mapping);
            else
                Dialog2_InitSubstFile(hwndDlg, path.c_str(), mapping);
        } while (FindNextFile(hFind, &find));
        FindClose(hFind);
    }

    return TRUE;
}

static bool LoadFdtFile(LPCTSTR pszBasePath, mapping_t& mapping)
{
    g_history.clear();

    string_t path = pszBasePath;
    path += L".fdt";

    FDT_FILE fdt_file;
    if (!fdt_file.load(path.c_str()))
        return false;

    auto it = fdt_file.name2section.find(L":LATEST");
    if (it != fdt_file.name2section.end())
    {
        auto& latest_section = it->second;
        for (auto& item : latest_section.items)
        {
            mapping[item.first] = item.second;
            if (item.second.size())
                g_history.push_back(item);
        }
    }

    it = fdt_file.name2section.find(L":HISTORY");
    if (it != fdt_file.name2section.end())
    {
        auto& history_section = it->second;
        for (auto& item : history_section.items)
        {
            if (item.second.size())
                g_history.push_back(item);
        }
    }

    return true;
}

static void Dialog2_InitSubst(HWND hwndDlg, INT iItem)
{
    TCHAR szItem[MAX_PATH], szPath[MAX_PATH];
    ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
    StringCchCopy(szPath, _countof(szPath), g_root_dir);
    PathAppend(szPath, szItem);

    mapping_t mapping;
    if (PathIsDirectory(szPath))
        Dialog2_InitSubstDir(hwndDlg, szPath, mapping);
    else
        Dialog2_InitSubstFile(hwndDlg, szPath, mapping);

    if (!LoadFdtFile(szPath, mapping))
    {
        for (auto& pair : mapping)
        {
            ReplacePair(pair.first, pair.second);
        }
    }
    else
    {
        for (auto& pair : mapping)
        {
            if (pair.second.empty())
                ReplacePair(pair.first, pair.second);
        }
    }

    Dialog2_RefreshSubst(hwndDlg, mapping);

    SaveFdtFile(szPath, mapping);
}

BOOL DoTempla(HWND hwnd, LPTSTR pszPath, INT iItem)
{
    DeleteTempDir(hwnd);

    string_t filename = PathFindFileName(pszPath);

    TCHAR temp_path[MAX_PATH];
    GetTempPath(_countof(temp_path), temp_path);

    TCHAR temp_dir[MAX_PATH];
    GetTempFileName(temp_path, L"FDT", 0, temp_dir);

    DeleteFile(temp_dir);
    if (!CreateDirectory(temp_dir, NULL))
        return FALSE;

    StringCchCopy(g_temp_dir, _countof(g_temp_dir), temp_dir);

    mapping_t mapping = DoGetMapping();
    SaveFdtFile(pszPath, mapping);

    templa(pszPath, temp_dir, mapping, g_ignore);

    for (auto& pair : mapping)
    {
        str_replace(filename, pair.first, pair.second);
    }

    StringCchCopy(pszPath, MAX_PATH, temp_dir);
    PathAppend(pszPath, filename.c_str());
    return TRUE;
}

static void OnBeginDrag(HWND hwnd, NM_LISTVIEW* pListView)
{
    INT cSelected = ListView_GetSelectedCount(g_hListView);
    if (cSelected == 0)
        return;

    PIDLIST_ABSOLUTE* ppidlAbsolute;
    ppidlAbsolute = (PIDLIST_ABSOLUTE*)CoTaskMemAlloc(sizeof(PIDLIST_ABSOLUTE) * cSelected);

    PITEMID_CHILD* ppidlChild;
    ppidlChild = (PITEMID_CHILD*)CoTaskMemAlloc(sizeof(PITEMID_CHILD) * cSelected);

    if (!ppidlAbsolute || !ppidlChild)
    {
        CoTaskMemFree(ppidlAbsolute);
        CoTaskMemFree(ppidlChild);
        return;
    }

    INT cItems = ListView_GetItemCount(g_hListView);
    for (INT i = 0, j = 0; i < cItems; ++i)
    {
        if (ListView_GetItemState(g_hListView, i, LVIS_SELECTED))
        {
            TCHAR szPath[MAX_PATH], szItem[MAX_PATH];
            ListView_GetItemText(g_hListView, i, 0, szItem, _countof(szItem));
            StringCchCopy(szPath, _countof(szPath), g_root_dir);
            PathAppend(szPath, szItem);
            DoTempla(hwnd, szPath, i);
            ppidlAbsolute[j++] = ILCreateFromPath(szPath);
        }
    }

    IShellFolder* pShellFolder = NULL;
    SHBindToParent(ppidlAbsolute[0], IID_PPV_ARGS(&pShellFolder), NULL);

    for (INT i = 0; i < cSelected; i++)
        ppidlChild[i] = ILFindLastID(ppidlAbsolute[i]);

    IDataObject *pDataObject = NULL;
    pShellFolder->GetUIObjectOf(NULL, cSelected, (LPCITEMIDLIST*)ppidlChild, IID_IDataObject, NULL,
                                (void **)&pDataObject);
    if (pDataObject)
    {
        DWORD dwEffect = DROPEFFECT_COPY;
        DoDragDrop(pDataObject, g_pDropSource, DROPEFFECT_COPY, &dwEffect);
    }

    for (INT i = 0; i < cSelected; ++i)
        CoTaskMemFree(ppidlAbsolute[i]);

    CoTaskMemFree(ppidlAbsolute);
    CoTaskMemFree(ppidlChild);
}

static void OnListViewDeleteKey(HWND hwnd, INT iItem)
{
    TCHAR szPath[MAX_PATH], szItem[MAX_PATH];
    ZeroMemory(szPath, sizeof(szPath));

    StringCchCopy(szPath, _countof(szPath), g_root_dir);
    ListView_GetItemText(g_hListView, iItem, 0, szItem, _countof(szItem));
    PathAppend(szPath, szItem);

    if (::GetKeyState(VK_SHIFT) < 0)
    {
        if (!::DeleteFile(szPath))
        {
            MessageBox(hwnd, doLoadStr(IDS_CANTDELETEFILE), NULL, MB_ICONERROR);
        }
    }
    else
    {
        SHFILEOPSTRUCT op = { hwnd, FO_DELETE, szPath };
        op.fFlags = FOF_ALLOWUNDO;
        ::SHFileOperation(&op);
    }
}

static LRESULT OnListViewKeyDown(HWND hwnd, LV_KEYDOWN *pKeyDown)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);

    switch (pKeyDown->wVKey)
    {
    case VK_RETURN:
        ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY);
        break;

    case VK_DELETE:
        if (iItem != -1)
            OnListViewDeleteKey(hwnd, iItem);
        break;
    }

    return 0;
}

static LRESULT OnListViewItemChanged(HWND hwnd, NM_LISTVIEW* pListView)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem != -1)
    {
        g_iDialog = 1;
        ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_TYPESUBST));
        Dialog2_InitSubst(g_hwndDialogs[1], iItem);
        s_iItemOld = iItem;
    }
    else
    {
        if (g_iDialog == 1 && s_iItemOld != -1)
            OnDeSelectItem(hwnd, s_iItemOld);

        g_iDialog = 0;
        ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_SELECTITEM));
        s_iItemOld = iItem;
    }

    for (INT i = 0; i < _countof(g_hwndDialogs); ++i)
        ::ShowWindow(g_hwndDialogs[i], SW_HIDE);

    ::ShowWindow(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);

    ::PostMessage(hwnd, WM_SIZE, 0, 0);
    return 0;
}

static LRESULT OnNotify(HWND hwnd, int idFrom, LPNMHDR pnmhdr)
{
    if (idFrom != 1)
        return 0;

    switch (pnmhdr->code)
    {
    case NM_DBLCLK:
        {
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (iItem != -1)
                ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY);
        }
        break;

    case LVN_KEYDOWN:
        return OnListViewKeyDown(hwnd, (LV_KEYDOWN*)pnmhdr);

    case LVN_ITEMCHANGED:
        return OnListViewItemChanged(hwnd, (NM_LISTVIEW*)pnmhdr);

    case LVN_BEGINDRAG:
    case LVN_BEGINRDRAG:
        OnBeginDrag(hwnd, (NM_LISTVIEW*)pnmhdr);
        break;
    }

    return 0;
}

static LRESULT OnShellChange(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    LPITEMIDLIST *ppidlAbsolute;
    LONG lEvent;
    HANDLE hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &ppidlAbsolute, &lEvent);
    if (!hLock)
    {
        assert(0);
        return 0;
    }

    TCHAR szPath[MAX_PATH];
    SHGetPathFromIDList(ppidlAbsolute[0], szPath);
    PathRemoveFileSpec(szPath);

    if (lstrcmpi(szPath, g_root_dir) == 0)
    {
        SHGetPathFromIDList(ppidlAbsolute[0], szPath);
        LPTSTR pszFileName = PathFindFileName(szPath);
        LV_FINDINFO Find = { LVFI_STRING, pszFileName };
        INT iItem = ListView_FindItem(g_hListView, -1, &Find);
        switch (lEvent)
        {
        case SHCNE_CREATE:
        case SHCNE_MKDIR:
            if (iItem == -1 && !templa_wildcard(pszFileName, L"*.fdt"))
            {
                DWORD dwAttrs = ((lEvent == SHCNE_MKDIR) ? FILE_ATTRIBUTE_DIRECTORY : 0);
                SHFILEINFO info;
                SHGetFileInfo(pszFileName, dwAttrs, &info, sizeof(info),
                              SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
                LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE };
                item.iItem    = ListView_GetItemCount(g_hListView);
                item.iSubItem = 0;
                item.pszText  = pszFileName;
                item.iImage   = info.iIcon;
                ListView_InsertItem(g_hListView, &item);
            }
            break;
        case SHCNE_DELETE:
        case SHCNE_RMDIR:
            ListView_DeleteItem(g_hListView, iItem);
            break;
        case SHCNE_RENAMEFOLDER:
        case SHCNE_RENAMEITEM:
            {
                SHGetPathFromIDList(ppidlAbsolute[1], szPath);
                string_t filename = PathFindFileName(szPath);
                if (templa_wildcard(filename, L"*.fdt"))
                    ListView_DeleteItem(g_hListView, iItem);
                else
                    ListView_SetItemText(g_hListView, iItem, 0, &filename[0]);
            }
            break;
        }
    }

    SHChangeNotification_Unlock(hLock);
    return 0;
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_ACTIVATE, OnActivate);
        HANDLE_MSG(hwnd, WM_SIZE, OnSize);
        HANDLE_MSG(hwnd, WM_CONTEXTMENU, OnContextMenu);
        HANDLE_MSG(hwnd, WM_NOTIFY, OnNotify);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    case WM_SHELLCHANGE:
        return OnShellChange(hwnd, wParam, lParam);
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    InitCommonControls();

    WNDCLASSEX wcx = { sizeof(wcx) };
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = WindowProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcx.lpszMenuName = NULL;
    wcx.lpszClassName = CLASSNAME;
    wcx.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!::RegisterClassEx(&wcx))
    {
        MessageBoxA(NULL, "RegisterClassEx failed", NULL, MB_ICONERROR);
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = 0;
    HWND hwnd = ::CreateWindowEx(exstyle, CLASSNAME, doLoadStr(IDS_APPVERSION), style,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 500, 350,
                                 NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBoxA(NULL, "CreateWindowEx failed", NULL, MB_ICONERROR);
        return 2;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        BOOL bContinue = FALSE;
        for (INT i = 0; i < _countof(g_hwndDialogs); ++i)
        {
            if (IsDialogMessage(g_hwndDialogs[i], &msg))
            {
                bContinue = TRUE;
                break;
            }
        }
        if (bContinue)
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return INT(msg.wParam);
}
