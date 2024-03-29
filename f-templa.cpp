// f-templa.cpp --- 「フォルダでテンプレート」by 片山博文MZ.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <map>
#include <algorithm>
#include <cassert>
#include "CDropTarget.hpp"  // ドロップターゲット。
#include "CDropSource.hpp"  // ドロップソース。
#include "ScrollView.hpp"   // スクロールビュー。
#include "f-templa.hpp"     // メインのヘッダ。
#include "fdt_file.hpp"
#include "InputBox.hpp"     // 入力ボックス。
#include "resource.h"       // リソースIDヘッダ。

#define CLASSNAME TEXT("FolderDeTemplate")  // ウィンドウクラス名。
#define WM_SHELLCHANGE (WM_USER + 100)      // シェル通知を受け取るメッセージ。
#define MAX_REPLACEITEMS 16                 // 置き換え項目の最大個数。
#define IDW_LISTVIEW 1

HWND g_hWnd = NULL; // メインウィンドウ。
HWND g_hListView = NULL; // リストビューコントロール。
HIMAGELIST g_hImageList = NULL; // イメージリスト。
INT g_iDialog = 0; // 現在のダイアログのインデックス。
HWND g_hwndDialogs[2] = { NULL }; // ダイアログ群。
HWND g_hStatusBar = NULL; // ステータスバー。
TCHAR g_root_dir[MAX_PATH] = TEXT(""); // テンプレートのルートディレクトリ。
TCHAR g_temp_dir[MAX_PATH + 1] = TEXT(""); // 一時ファイルの保存先ディレクトリ。
CDropTarget* g_pDropTarget = NULL; // ドロップターゲット。
CDropSource* g_pDropSource = NULL; // ドロップソース。
UINT g_nNotifyID = 0; // シェル通知ID。
SCROLLVIEW g_Dialog1ScrollView, g_Dialog2ScrollView; // ダイアログ1とダイアログ2のスクロールビュー。
string_list_t g_ignore = { L"q", L"*.bin", L".git", L".svn", L".vs" }; // 無視すべきフォルダ。
std::vector<std::pair<string_t, string_t>> g_history; // キーと値の履歴。
POINT g_ptWnd; // メインウィンドウの位置。
SIZE g_sizWnd; // メインウィンドウのサイズ。

static INT s_iItemOld = -1;

// 文字列を読み込む。
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

// 設定をレジストリから読み込む。
BOOL LoadSettings(void)
{
    // 設定の初期化。
    g_ptWnd = { CW_USEDEFAULT, CW_USEDEFAULT };
    g_sizWnd = { 500, 350 };

    // レジストリキーを開く。
    HKEY hKey;
    LONG error = RegOpenKeyEx(HKEY_CURRENT_USER,
                              TEXT("Software\\Katayama Hirofumi MZ\\FolderDeTemplate"),
                              0, KEY_READ, &hKey);
    if (error)
        return FALSE; // 開けなかった。

    DWORD dwValue, cbValue;

    // レジストリから値を読み込む。
    cbValue = sizeof(dwValue);
    error = RegQueryValueEx(hKey, TEXT("g_ptWnd.x"), NULL, NULL, (LPBYTE)&dwValue, &cbValue);
    if (!error && cbValue == sizeof(dwValue))
        g_ptWnd.x = INT(dwValue);

    // レジストリから値を読み込む。
    cbValue = sizeof(dwValue);
    error = RegQueryValueEx(hKey, TEXT("g_ptWnd.y"), NULL, NULL, (LPBYTE)&dwValue, &cbValue);
    if (!error && cbValue == sizeof(dwValue))
        g_ptWnd.y = LONG(dwValue);

    // レジストリから値を読み込む。
    cbValue = sizeof(dwValue);
    error = RegQueryValueEx(hKey, TEXT("g_sizWnd.cx"), NULL, NULL, (LPBYTE)&dwValue, &cbValue);
    if (!error && cbValue == sizeof(dwValue))
        g_sizWnd.cx = LONG(dwValue);

    // レジストリから値を読み込む。
    cbValue = sizeof(dwValue);
    error = RegQueryValueEx(hKey, TEXT("g_sizWnd.cy"), NULL, NULL, (LPBYTE)&dwValue, &cbValue);
    if (!error && cbValue == sizeof(dwValue))
        g_sizWnd.cy = LONG(dwValue);

    // レジストリキーを閉じる。
    RegCloseKey(hKey);
    return TRUE;
}

// 設定をレジストリから保存する。
BOOL SaveSettings(void)
{
    // レジストリに会社キーを作成する。
    HKEY hCompanyKey;
    LONG error = RegCreateKeyEx(HKEY_CURRENT_USER,
                                TEXT("Software\\Katayama Hirofumi MZ"),
                                0, NULL, 0, KEY_WRITE, NULL, &hCompanyKey, NULL);
    if (error)
        return FALSE;

    // レジストリにアプリキーを作成する。
    HKEY hAppKey;
    error = RegCreateKeyEx(hCompanyKey,
                           TEXT("FolderDeTemplate"),
                           0, NULL, 0, KEY_WRITE, NULL, &hAppKey, NULL);
    if (error)
    {
        RegCloseKey(hCompanyKey);
        return FALSE;
    }

    DWORD dwValue, cbValue;

    // レジストリに値を書き込む。
    cbValue = sizeof(dwValue);
    dwValue = g_ptWnd.x;
    RegSetValueEx(hAppKey, TEXT("g_ptWnd.x"), 0, REG_DWORD, (BYTE*)&dwValue, cbValue);

    // レジストリに値を書き込む。
    cbValue = sizeof(dwValue);
    dwValue = g_ptWnd.y;
    RegSetValueEx(hAppKey, TEXT("g_ptWnd.y"), 0, REG_DWORD, (BYTE*)&dwValue, cbValue);

    // レジストリに値を書き込む。
    cbValue = sizeof(dwValue);
    dwValue = g_sizWnd.cx;
    RegSetValueEx(hAppKey, TEXT("g_sizWnd.cx"), 0, REG_DWORD, (BYTE*)&dwValue, cbValue);

    // レジストリに値を書き込む。
    cbValue = sizeof(dwValue);
    dwValue = g_sizWnd.cy;
    RegSetValueEx(hAppKey, TEXT("g_sizWnd.cy"), 0, REG_DWORD, (BYTE*)&dwValue, cbValue);

    // レジストリキーを閉じる。
    RegCloseKey(hAppKey);
    RegCloseKey(hCompanyKey);
    return TRUE;
}

// リストビューを初期化する。
static void InitListView(HWND hListView, HIMAGELIST hImageList, LPCTSTR pszDir)
{
    GetFullPathName(pszDir, _countof(g_root_dir), g_root_dir, NULL);

    // FindFirstFile用のスペック文字列を構築する。
    TCHAR spec[MAX_PATH];
    StringCchCopy(spec, _countof(spec), g_root_dir);
    PathAddBackslash(spec);
    StringCchCat(spec, _countof(spec), TEXT("*"));

    INT i = 0;
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(spec, &find);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        // 「.」「..」を無視する。
        if (lstrcmp(find.cFileName, TEXT(".")) == 0 ||
            lstrcmp(find.cFileName, TEXT("..")) == 0)
        {
            continue;
        }

        // 拡張子が「.fdt」のファイルを無視する。
        if (templa_wildcard(find.cFileName, TEXT("*.fdt")))
            continue;

        // パスファイル名を構築する。
        TCHAR szPathName[MAX_PATH];
        StringCchCopy(szPathName, _countof(szPathName), g_root_dir);
        PathAppend(szPathName, find.cFileName);

        // アイコンを取得する。
        SHFILEINFO info;
        UINT uFlags = SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES;
        if (lstrcmpi(PathFindExtension(szPathName), TEXT(".LNK")) == 0)
            uFlags |= SHGFI_LINKOVERLAY;
        SHGetFileInfo(szPathName, find.dwFileAttributes, &info, sizeof(info), uFlags);

        // ショートカットの拡張子「.LNK」を表示しない。
        if (lstrcmpi(PathFindExtension(szPathName), TEXT(".LNK")) == 0)
            PathRemoveExtension(find.cFileName);

        // 項目をリストビューに追加する。
        LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM };
        item.iItem    = i++;
        item.iSubItem = 0;
        item.pszText  = find.cFileName;
        item.iImage   = ImageList_AddIcon(g_hImageList, info.hIcon);
        item.lParam   = (LPARAM)ILCreateFromPath(szPathName); // PIDLデータを保持する。
        ListView_InsertItem(hListView, &item);

        // アイコンを破棄。
        ::DestroyIcon(info.hIcon);
    } while (FindNextFile(hFind, &find));

    // 検索終了。
    ::FindClose(hFind);
}

// WM_INITDIALOG - ダイアログ1の初期化。
static BOOL Dialog1_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // スクロールビューを初期化する。
    ScrollView_Init(&g_Dialog1ScrollView, hwnd, SB_VERT);
    return TRUE;
}

// WM_SIZE - ダイアログ1のサイズ変更。
static void Dialog1_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    // スクロールビューの情報を更新する。
    ScrollView_OnSize(&g_Dialog1ScrollView, state, cx, cy);
}

// WM_VSCROLL - ダイアログ1の縦スクロール。
static void Dialog1_OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
{
    // スクロールビューの縦スクロール。
    ScrollView_OnVScroll(&g_Dialog1ScrollView, hwndCtl, code, pos);
}

// WM_MOUSEWHEEL - ダイアログ1のマウスホイール回転。
static void Dialog1_OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    // WM_VSCROLLメッセージ送信。により、縦スクロール。
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

// ダイアログ1のダイアログプロシージャ。
static INT_PTR CALLBACK
Dialog1Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, Dialog1_OnInitDialog);
        HANDLE_MSG(hwnd, WM_SIZE, Dialog1_OnSize);
        HANDLE_MSG(hwnd, WM_VSCROLL, Dialog1_OnVScroll);
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, Dialog1_OnMouseWheel);
    }
    return 0;
}

// WM_INITDIALOG - ダイアログ2の初期化。
static BOOL Dialog2_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    ScrollView_Init(&g_Dialog2ScrollView, hwnd, SB_VERT);
    return TRUE;
}

// ダイアログから写像を取得する。
mapping_t DoGetMapping(void)
{
    HWND hwndDlg = g_hwndDialogs[1];

    mapping_t mapping;
    for (UINT i = 0; i < MAX_REPLACEITEMS; ++i)
    {
        // テキストボックスからテキストを取得する。
        TCHAR szKey[128], szValue[1024];
        ::GetDlgItemText(hwndDlg, IDC_FROM_00 + i, szKey, _countof(szKey));
        ::GetDlgItemText(hwndDlg, IDC_TO_00 + i, szValue, _countof(szValue));

        // 前後の空白を削除。
        StrTrim(szKey, TEXT(" \t\r\n\x3000"));
        if (szKey[0] == 0) // キーが空の場合は無視。
            continue;
        StrTrim(szValue, TEXT(" \t\r\n\x3000"));

        // キーと値のペアを写像に登録。
        mapping[szKey] = szValue;
    }

    return mapping; // 写像を返す。
}

// 入力ボックスのコールバック関数。
static BOOL CALLBACK InputBoxCallback(LPTSTR text)
{
    StrTrim(text, TEXT(" \t\r\n\x3000"));
    return text[0] != 0 && text[0] != L':';
}

// 削除されたセクションをここに保存する。
static string_list_t g_deleted_sections;

// WM_INITDIALOG - プリセットのダイアログ初期化。
static BOOL Preset_OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // リストにプリセットの値を追加する。
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    for (auto& name : *(string_list_t*)lParam)
    {
        ListBox_AddString(hLst1, name.c_str());
    }
    ListBox_SetCurSel(hLst1, 0);

    // 削除されたセクションをクリアする。
    g_deleted_sections.clear();

    // ダイアログを中央に移動する。
    CenterWindowDx(hwnd);
    return TRUE;
}

// psh1 - プリセットの削除。
static void Preset_OnDelete(HWND hwnd)
{
    // 現在選択中の項目を取得する。
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    INT iItem = ListBox_GetCurSel(hLst1);
    if (iItem == LB_ERR)
        return;

    // 項目のテキストを取得する。
    TCHAR szText[1024];
    ListBox_GetText(hLst1, iItem, szText);

    // 選択項目を削除する。
    ListBox_DeleteString(hLst1, iItem);

    // 削除された項目として覚えておく。
    g_deleted_sections.push_back(szText);

    // 選択を移動する。
    if (iItem >= ListBox_GetCount(hLst1))
        ListBox_SetCurSel(hLst1, iItem - 1);
    else
        ListBox_SetCurSel(hLst1, iItem);
}

// psh2 - プリセットの「すべて削除」。
static void Preset_OnDeleteAll(HWND hwnd)
{
    // 逆順で削除する。
    HWND hLst1 = GetDlgItem(hwnd, lst1);
    INT nCount = ListBox_GetCount(hLst1);
    for (INT iItem = nCount - 1; iItem >= 0; --iItem)
    {
        // テキストを取得する。
        TCHAR szText[1024];
        ListBox_GetText(hLst1, iItem, szText);

        // 削除する。
        ListBox_DeleteString(hLst1, iItem);

        // 削除されたセクションとして覚えておく。
        g_deleted_sections.push_back(szText);
    }
}

// WM_COMMAND - プリセットのコマンド処理。
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

// プリセットのダイアログプロシージャ。
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

// 値の更新。「{{タグ}}」に対応する値を取得する。
static void UpdateValue(const string_t& first, string_t& second)
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    // 今日。
    if (first == L"{{TODAY}}")
    {
        TCHAR szText[128];
        StringCchPrintf(szText, _countof(szText), TEXT("%04u-%02u-%02u"),
            st.wYear,
            st.wMonth,
            st.wDay);
        second = szText;
        return;
    }
    if (first == doLoadStr(IDS_TODAY))
    {
        second = std::to_wstring(st.wYear);
        second += doLoadStr(IDS_YEAR);
        second += std::to_wstring(st.wMonth);
        second += doLoadStr(IDS_MONTH);
        second += std::to_wstring(st.wDay);
        second += doLoadStr(IDS_DAY);
        return;
    }

    // 現在時刻。
    if (first == L"{{NOW}}")
    {
        TCHAR szText[128];
        StringCchPrintf(szText, _countof(szText), TEXT("%02u:%02u:%02u"),
            st.wHour,
            st.wMinute,
            st.wSecond);
        second = szText;
        return;
    }
    if (first == doLoadStr(IDS_NOW))
    {
        second = std::to_wstring(st.wHour);
        second += doLoadStr(IDS_HOUR);
        second += std::to_wstring(st.wMinute);
        second += doLoadStr(IDS_MINUTE);
        second += std::to_wstring(st.wSecond);
        second += doLoadStr(IDS_SECOND);
        return;
    }

    // タイムスタンプ。
    if (first == L"{{TIMESTAMP}}")
    {
        TCHAR szText[128];
        StringCchPrintf(szText, _countof(szText), TEXT("%04u-%02u-%02u %02u:%02u:%02u"),
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond);
        second = szText;
        return;
    }
    if (first == doLoadStr(IDS_TIMESTAMP))
    {
        second = std::to_wstring(st.wYear);
        second += doLoadStr(IDS_YEAR);
        second += std::to_wstring(st.wMonth);
        second += doLoadStr(IDS_MONTH);
        second += std::to_wstring(st.wDay);
        second += doLoadStr(IDS_DAY);
        second += std::to_wstring(st.wHour);
        second += doLoadStr(IDS_HOUR);
        second += std::to_wstring(st.wMinute);
        second += doLoadStr(IDS_MINUTE);
        second += std::to_wstring(st.wSecond);
        second += doLoadStr(IDS_SECOND);
        return;
    }

    // 年。
    if (first == L"{{YEAR}}" || first == doLoadStr(IDS_THISYEAR))
    {
        second = std::to_wstring(st.wYear);
        return;
    }

    // 月。
    if (first == L"{{MONTH}}" || first == doLoadStr(IDS_THISMONTH))
    {
        second = std::to_wstring(st.wMonth);
        return;
    }

    // 日。
    if (first == L"{{DAY}}" || first == doLoadStr(IDS_THISDAY))
    {
        second = std::to_wstring(st.wDay);
        return;
    }

    // 時。
    if (first == L"{{HOUR}}" || first == doLoadStr(IDS_THISHOUR))
    {
        second = std::to_wstring(st.wHour);
        return;
    }

    // 分。
    if (first == L"{{MINUTE}}" || first == doLoadStr(IDS_THISMINUTE))
    {
        second = std::to_wstring(st.wMinute);
        return;
    }

    // 秒。
    if (first == L"{{SECOND}}" || first == doLoadStr(IDS_THISSECOND))
    {
        second = std::to_wstring(st.wSecond);
        return;
    }

    // ユーザ名。
    if (first == L"{{USER}}" ||
        first == doLoadStr(IDS_USERNAME) ||
        first == doLoadStr(IDS_USERNAME2))
    {
        TCHAR szUser[MAX_PATH];
        DWORD cchUser = _countof(szUser);
        ::GetUserName(szUser, &cchUser);
        second = szUser;
        return;
    }

    // 曜日。
    if (first == L"{{WEEKDAY}}")
    {
        static const LPCWSTR days[] =
        {
            L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat",
        };
        second = days[st.wDayOfWeek];
        return;
    }
    if (first == doLoadStr(IDS_WEEKDAY))
    {
        assert(st.wDayOfWeek < 7);
        second = doLoadStr(IDS_WEEKDAYS)[st.wDayOfWeek];
        return;
    }
}

// 置き換え項目のUIを更新する。
static void Dialog2_RefreshSubst(HWND hwnd, mapping_t& mapping)
{
    // 置き換え項目をいったんクリアする。
    ::SendMessage(hwnd, WM_COMMAND, ID_REFRESH_SUBST, 0);

    // テキストボックスを更新する。
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

static void Dialog2_InitSubst(HWND hwndDlg, INT iItem);

// ダイアログ2において一番上の下矢印ボタンが押された。
// メニューを表示し、処理を行う。
static void Dialog2_OnPreset(HWND hwnd)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1)
        return;

    LV_ITEM item = { LVIF_PARAM, iItem };
    ListView_GetItem(g_hListView, &item);

    TCHAR szPath[MAX_PATH];
    auto pidl = (LPITEMIDLIST)item.lParam;
    SHGetPathFromIDList(pidl, szPath);

    // FDTファイルのパスファイル名を構築する。
    string_t path = szPath;
    path += L".fdt";

    // FDTファイルを読み込む。
    FDT_FILE fdt_file;
    fdt_file.load(path.c_str());

    // ボタンの位置を取得する。
    HWND hwndButton = GetDlgItem(hwnd, IDC_DARROW_PRESET);
    RECT rc;
    ::GetWindowRect(hwndButton, &rc);

    // 新しいメニューを作成する。
    HMENU hMenu = ::CreatePopupMenu();

    // 「値の更新」「名前を付けて保存」メニュー項目を追加する。
    ::AppendMenu(hMenu, MF_STRING, ID_UPDATEVALUE, doLoadStr(IDS_UPDATEVALUE));
    ::AppendMenu(hMenu, MF_STRING, ID_SAVEPRESET, doLoadStr(IDS_SAVEPRESET));
    // メニューに区分線を追加する。
    ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    // 「:」で始まらないセクション名を取得する。
    string_list_t section_names;
    for (auto& pair : fdt_file.name2section)
    {
        if (pair.first.size() && pair.first[0] == L':')
            continue;

        section_names.push_back(pair.first);
    }

    const INT c_section_first = 1000;
    if (section_names.size())
    {
        // セクション名をソートする。
        std::sort(section_names.begin(), section_names.end());

        // セクション名を使ってメニュー項目を追加する。
        INT i = c_section_first;
        for (auto& name : section_names)
        {
            ::AppendMenu(hMenu, MF_STRING, i++, name.c_str());
        }

        // 区分線を追加。
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        // 「編集...」メニュー項目を追加。
        ::AppendMenu(hMenu, MF_STRING, ID_EDITPRESET, doLoadStr(IDS_EDITPRESET));
    }

    // 「リセット」メニュー項目を追加する。
    ::AppendMenu(hMenu, MF_STRING, ID_RESET, doLoadStr(IDS_RESET));

    // 実際にポップアップメニューを表示し、メニュー項目が選択されるのを待つ。
    INT iChoice = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON,
                                   rc.right, rc.top, 0, hwnd, &rc);
    ::DestroyMenu(hMenu); // メニューを破棄する。

    // 選択されたメニュー項目に従って処理を行う。
    switch (iChoice)
    {
    case 0: // 何も選択されなかった。
        return;

    case ID_SAVEPRESET: //「名前を付けて保存」メニュー項目。
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
                // 入力ボックスで入力を待つ。
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
        break;

    case ID_EDITPRESET:
        if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PRESET),
                           hwnd, PresetDialogProc, (LPARAM)&section_names) == IDOK)
        {
            for (auto& name : g_deleted_sections)
            {
                fdt_file.name2section.erase(name);
            }
            fdt_file.save(path.c_str());
        }
        break;

    case ID_RESET:
        // 設定ファイルを削除。
        ::DeleteFile(path.c_str());
        // 置き換え項目の設定を初期化。
        Dialog2_InitSubst(hwnd, iItem);
        break;

    case ID_UPDATEVALUE:
        // 値を更新する。
        {
            mapping_t mapping = DoGetMapping();

            for (auto& pair : mapping)
            {
                UpdateValue(pair.first, pair.second);
            }

            Dialog2_RefreshSubst(hwnd, mapping);
        }
        break;

    default:
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
        }
        break;
    }
}

// ダイアログ2において、下向き矢印ボタンがクリックされた。
static void Dialog2_OnDownArrow(HWND hwnd, INT id)
{
    INT index = id - IDC_DARROW_00;
    UINT nEditFromID = IDC_FROM_00 + index;
    UINT nEditToID = IDC_TO_00 + index;

    // 押されたボタンのウィンドウ領域を取得する。
    RECT rc;
    ::GetWindowRect(GetDlgItem(hwnd, id), &rc);

    // キーのテキストを取得し、前後の空白を削除。
    TCHAR szKey[128];
    ::GetDlgItemText(hwnd, nEditFromID, szKey, _countof(szKey));
    StrTrim(szKey, TEXT(" \t\r\n\x3000"));

    // 値のテキストを取得し、前後の空白を削除。
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

    // 新しいポップアップメニューを作成する。
    HMENU hMenu = CreatePopupMenu();

    // 「値の更新」項目の追加。
    ::AppendMenu(hMenu, MF_STRING, ID_UPDATEVALUE, doLoadStr(IDS_UPDATEVALUE));
    // 「リセット」項目の追加。
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

        // 区分線を追加。
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, L"");
        // 「キーと値の削除」項目を追加。
        ::AppendMenu(hMenu, MF_STRING, ID_DELETEKEYANDVALUE, doLoadStr(IDS_DELETEKEYVALUE));
    }

    // ポップアップメニューを表示し、メニュー項目の選択を待つ。
    INT iChoice = ::TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON,
                                   rc.right, rc.top, 0, hwnd, &rc);
    // メニューを破棄する。
    ::DestroyMenu(hMenu);

    // 選択が無効なら終了。
    if (iChoice == 0)
        return;

    if (iChoice >= c_history_first)
    {
        HWND hwndEdit = ::GetDlgItem(hwnd, nEditToID);
        assert(hwndEdit);
        ::SetWindowText(hwndEdit, strs[iChoice - c_history_first].c_str());
        Edit_SetSel(hwndEdit, 0, -1);
        SetFocus(hwndEdit);
    }
    else if (iChoice == ID_ADDITEM) // 「項目の追加」
    {
        g_history.push_back(std::make_pair(szKey, szValue));
    }
    else if (iChoice == ID_UPDATEVALUE) // 「値の更新」
    {
        std::pair<string_t, string_t> pair;
        pair.first = szKey;
        pair.second = szValue;
        UpdateValue(pair.first, pair.second);

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
            UpdateValue(pair.first, pair.second);

            HWND hwndEdit = ::GetDlgItem(hwnd, nEditToID);
            assert(hwndEdit);
            ::SetWindowText(hwndEdit, pair.second.c_str());
            Edit_SetSel(hwndEdit, 0, -1);
            SetFocus(hwndEdit);
        }

        LV_ITEM item = { LVIF_PARAM, iItem };
        ListView_GetItem(g_hListView, &item);
        TCHAR szPath[MAX_PATH];
        auto pidl = (LPITEMIDLIST)item.lParam;
        SHGetPathFromIDList(pidl, szPath);

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

// WM_COMMAND - ダイアログ2のコマンド処理。
static void Dialog2_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_REFRESH_SUBST:
        // 置き換え項目のテキストボックスをクリアする。
        for (UINT id = IDC_FROM_00; id <= IDC_FROM_15; ++id)
            ::SetDlgItemText(hwnd, id, NULL);
        for (UINT id = IDC_TO_00; id <= IDC_TO_15; ++id)
            ::SetDlgItemText(hwnd, id, NULL);
        break;

    case IDC_DARROW_PRESET:
        Dialog2_OnPreset(hwnd);
        break;

    default:
        if (IDC_RARROW_00 <= id && id < IDC_RARROW_00 + MAX_REPLACEITEMS)
        {
            if (codeNotify == STN_CLICKED)
            {
                UINT nEditID = IDC_TO_00 + (id - IDC_RARROW_00);
                HWND hwndEdit = ::GetDlgItem(hwnd, nEditID);
                assert(hwndEdit);
                Edit_SetSel(hwndEdit, 0, -1);
                ::SetFocus(hwndEdit);
            }
        }
        else if (IDC_DARROW_00 <= id && id < IDC_DARROW_00 + MAX_REPLACEITEMS)
        {
            if (codeNotify == BN_CLICKED)
            {
                Dialog2_OnDownArrow(hwnd, id);
            }
        }
        break;
    }
}

// WM_VSCROLL - ダイアログ2で縦スクロールされた。
static void Dialog2_OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
{
    ScrollView_OnVScroll(&g_Dialog2ScrollView, hwndCtl, code, pos);
}

// WM_MOUSEWHEEL - ダイアログ2でマウスホイールが回転した。
static void Dialog2_OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    // WM_VSCROLLメッセージで縦スクロールする。
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

// WM_SIZE - ダイアログ2のサイズ変更。
void Dialog2_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    // スクロールビューのサイズ変更。
    ScrollView_OnSize(&g_Dialog2ScrollView, state, cx, cy);
}

// ダイアログ2のダイアログプロシージャ。
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

// WM_CREATE - メインウィンドウの作成。
static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    // ウィンドウハンドルを覚えておく。
    g_hWnd = hwnd;

    // 位置を覚えておく。
    RECT rc;
    GetWindowRect(hwnd, &rc);
    g_ptWnd.x = rc.left;
    g_ptWnd.y = rc.top;

    // OLEの初期化。
    ::OleInitialize(NULL);

    // リストビューを作成。
    DWORD style = WS_CHILD | WS_VISIBLE | LVS_ICON | LVS_EDITLABELS |
        LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_SINGLESEL |
        WS_BORDER | WS_VSCROLL | WS_HSCROLL;
    DWORD exstyle = WS_EX_CLIENTEDGE;
    g_hListView = ::CreateWindowEx(exstyle, WC_LISTVIEW, NULL, style,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDW_LISTVIEW, GetModuleHandle(NULL), NULL);
    if (g_hListView == NULL)
        return FALSE;

    // イメージリストを作成する。
    g_hImageList = ImageList_Create(
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
        ILC_COLOR32 | ILC_MASK, 32, 32);
    if (g_hImageList == NULL)
        return FALSE;
    // リストビューにイメージリストをセットする。
    ListView_SetImageList(g_hListView, g_hImageList, LVSIL_NORMAL);

    // テンプレートフォルダのパスファイル名を取得する。
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

    // パスファイル名を使用してリストビューを初期化する。
    InitListView(g_hListView, g_hImageList, szPath);

    // ダイアログ群を作成する。
    static const UINT ids[] = { IDD_TOP, IDD_SUBST };
    DLGPROC procs[] = { Dialog1Proc, Dialog2Proc };
    for (UINT i = 0; i < _countof(g_hwndDialogs); ++i)
    {
        g_hwndDialogs[i] = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(ids[i]), hwnd, procs[i]);
        if (!g_hwndDialogs[i])
            return FALSE;
    }
    ::ShowWindow(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);

    // ステータスバーを作成する。
    style = WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP;
    g_hStatusBar = ::CreateStatusWindow(style, doLoadStr(IDS_SELECTITEM), hwnd, 2);
    if (g_hStatusBar == NULL)
        return FALSE;

    // シェル変更通知を受け取るようにする。
    LPITEMIDLIST pidlDesktop;
    SHGetSpecialFolderLocation(hwnd, CSIDL_DESKTOP, &pidlDesktop);
    SHChangeNotifyEntry  entry;
    entry.pidl = pidlDesktop;
    entry.fRecursive = TRUE;
    LONG nEvents = SHCNE_CREATE | SHCNE_DELETE | SHCNE_MKDIR | SHCNE_RMDIR |
                   SHCNE_RENAMEFOLDER | SHCNE_RENAMEITEM;
    g_nNotifyID = SHChangeNotifyRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_NewDelivery,
                                         nEvents, WM_SHELLCHANGE, 1, &entry);

    // D&Dを登録する。
    g_pDropTarget = new CDropTarget(hwnd);
    ::RegisterDragDrop(hwnd, g_pDropTarget);
    g_pDropSource = new CDropSource();

    // WM_SIZEメッセージの処理により再配置を行う。
    ::PostMessage(hwnd, WM_SIZE, 0, 0);

    return TRUE;
}

// WM_ACTIVATE - メインウィンドウがアクティブになった。
static void OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized)
{
    // リストビューにフォーカスを置く。
    ::SetFocus(g_hListView);
}

// WM_SIZE - メインウィンドウのサイズが変更された。
static void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    // 現在のダイアログの位置とサイズを取得する。
    RECT rc;
    ::GetWindowRect(g_hwndDialogs[g_iDialog], &rc);
    INT cxDialog = rc.right - rc.left;

    // ステータスバーの位置とサイズを取得する。
    ::SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
    ::GetWindowRect(g_hStatusBar, &rc);
    INT cyStatusBar = rc.bottom - rc.top;

    // メインウィンドウのクライアント領域を取得する。
    ::GetClientRect(hwnd, &rc);

    // 領域からステータスバーの高さを除外する。
    rc.bottom -= cyStatusBar;

    // 現在のダイアログの位置とサイズを指定する。
    INT cyDialog = rc.bottom - rc.top;
    ::MoveWindow(g_hwndDialogs[g_iDialog], 0, 0, cxDialog, cyDialog, TRUE);

    // 領域から現在のダイアログの幅を除外する。
    rc.left += cxDialog;

    // リストビューの位置とサイズを指定する。
    ::MoveWindow(g_hListView, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

    // 最小化または最大化されているときは無視する。
    if (IsIconic(hwnd) || IsZoomed(hwnd))
        return;

    // サイズの設定を覚えておく。
    GetWindowRect(hwnd, &rc);
    g_sizWnd.cx = rc.right - rc.left;
    g_sizWnd.cy = rc.bottom - rc.top;
}

// コンテキストメニューの処理を実行する。
HRESULT ExecuteContextCommand(HWND hwnd, IContextMenu *pContextMenu, UINT nCmd)
{
    // 「名前の変更」ならばそうする。
    TCHAR verb[64];
    pContextMenu->GetCommandString(nCmd, GCS_VERB, NULL, (LPSTR)verb, sizeof(verb));
    if (lstrcmpi(verb, TEXT("rename")) == 0)
    {
        INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
        ListView_EditLabel(g_hListView, iItem);
        return S_OK;
    }

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

// コンテキストメニューを表示し、選択した項目に応じてアクションを行う。
BOOL ShowContextMenu(HWND hwnd, INT iItem, INT xPos, INT yPos, UINT uFlags = CMF_NORMAL)
{
    HRESULT hr;
    IShellFolder *pFolder = NULL;
    LPCITEMIDLIST pidlChild;
    IContextMenu *pContextMenu = NULL;
    LPITEMIDLIST pidl = NULL;
    TCHAR szPath[MAX_PATH];
    HMENU hMenu = CreatePopupMenu();
    UINT nID;

    if (iItem == -1) // 選択項目がなければ、テンプレートフォルダを使用する。
    {
        StringCchCopy(szPath, _countof(szPath), g_root_dir);
    }
    else
    {
        ListView_EnsureVisible(g_hListView, iItem, FALSE);

        LV_ITEM item = { LVIF_PARAM, iItem };
        ListView_GetItem(g_hListView, &item);
        auto pidl = (LPITEMIDLIST)item.lParam;
        SHGetPathFromIDList(pidl, szPath);
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

// WM_CONTEXTMENU - コンテキストメニューを要求する。
static void OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1) // 選択項目がなければ背景の右クリック。
    {
        ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_NODEFAULT | CMF_NOVERBS);
        return;
    }

    if (xPos == 0xFFFF && yPos == 0xFFFF) // この場合はキーボードによるコンテキストメニュー。
    {
        // 選択項目の中心の座標を使用する。
        RECT rc;
        ListView_GetItemRect(g_hListView, iItem, &rc, LVIR_ICON);
        POINT pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        ::ClientToScreen(hwnd, &pt);
        xPos = pt.x;
        yPos = pt.y;
    }

    ShowContextMenu(hwnd, iItem, xPos, yPos, CMF_EXPLORE | CMF_NODEFAULT | CMF_CANRENAME);
}

// 一時ディレクトリを削除する。
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

// FDTファイルを保存する。
bool SaveFdtFile(LPCTSTR pszBasePath, mapping_t& mapping)
{
    // FDTファイルのパスファイル名を構築する。
    string_t path = pszBasePath;
    path += L".fdt";

    // 書き込みの前にいったん読み込む。
    FDT_FILE fdt_file;
    fdt_file.load(path.c_str());

    // 最終的設定を保存する。
    auto& latest_section = fdt_file.name2section[L":LATEST"];
    for (auto& pair : mapping)
    {
        latest_section.assign(pair.first, pair.second);
    }

    // 履歴を保存する。
    auto& history_section = fdt_file.name2section[L":HISTORY"];
    for (auto& pair : mapping)
    {
        history_section.assign(pair);
    }
    for (auto& pair : g_history)
    {
        history_section.assign(pair);
    }

    // FDTファイルを書き込む。
    return fdt_file.save(path.c_str());
}

// リストビューの選択が解除された。
static void OnDeSelectItem(HWND hwnd, INT iItem)
{
    LV_ITEM item = { LVIF_PARAM, iItem };
    ListView_GetItem(g_hListView, &item);
    TCHAR szPath[MAX_PATH];
    auto pidl = (LPITEMIDLIST)item.lParam;
    SHGetPathFromIDList(pidl, szPath);

    mapping_t mapping = DoGetMapping();
    SaveFdtFile(szPath, mapping);
}

// WM_DESTROY - メインウィンドウの破棄。
static void OnDestroy(HWND hwnd)
{
    // 選択を解除する処理。
    if (g_iDialog == 1 && s_iItemOld != -1)
        OnDeSelectItem(hwnd, s_iItemOld);

    // 一時ディレクトリを削除。
    DeleteTempDir(hwnd);

    // シェル変更通知の登録を解除する。
    if (g_nNotifyID)
    {
        SHChangeNotifyDeregister(g_nNotifyID);
        g_nNotifyID = 0;
    }

    // D&Dを無効化する。
    ::RevokeDragDrop(hwnd);

    // D&D関連オブジェクトを破棄する。
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

    // イメージリストを破棄する。
    if (g_hImageList)
    {
        ImageList_Destroy(g_hImageList);
        g_hImageList = NULL;
    }

    // リストビューを破棄する。
    if (g_hListView)
    {
        ::DestroyWindow(g_hListView);
        g_hListView = NULL;
    }

    // 子ダイアログを破棄する。
    for (UINT i = 0; i < _countof(g_hwndDialogs); ++i)
    {
        if (g_hwndDialogs[i])
        {
            ::DestroyWindow(g_hwndDialogs[i]);
            g_hwndDialogs[i] = NULL;
        }
    }

    // OLEを破棄する。
    OleUninitialize();

    // メッセージループの終了。
    PostQuitMessage(0);
}

// テキストの中から{{タグ}}を探して、写像に追加する。
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
        if (tag.size() < 128)
        {
            tag = L"{{" + tag + L"}}";
            mapping[tag] = L"";
        }
    }

    return FALSE;
}

// ダイアログ2において置き換え項目を初期化する（通常ファイルの場合）。
BOOL Dialog2_InitSubstFile(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping)
{
    Dialog2_FindSubst(hwndDlg, PathFindFileNameW(pszPath), mapping);

    TEMPLA_FILE file;
    if (!file.load(pszPath))
        return TRUE;

    if (file.m_encoding == TE_BINARY)
        return TRUE;
    return Dialog2_FindSubst(hwndDlg, file.m_string, mapping);
}

// ダイアログ2において置き換え項目を初期化する（ディレクトリの場合）。
BOOL Dialog2_InitSubstDir(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping)
{
    Dialog2_FindSubst(hwndDlg, PathFindFileName(pszPath), mapping);

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

// FDTファイルを読み込む。
static bool LoadFdtFile(LPCTSTR pszBasePath, mapping_t& mapping)
{
    // 履歴を削除する。
    g_history.clear();

    // FDTファイルのパスファイル名を構築する。
    string_t path = pszBasePath;
    path += L".fdt";

    // FDTファイルを読み込む。
    FDT_FILE fdt_file;
    if (!fdt_file.load(path.c_str()))
        return false;

    // 最終更新を読み込む。
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

    // 履歴を読み込む。
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

// 置き換え項目を初期化する。
static void Dialog2_InitSubst(HWND hwndDlg, INT iItem)
{
    // 項目のパスファイル名を構築する。
    LV_ITEM item = { LVIF_PARAM, iItem };
    ListView_GetItem(g_hListView, &item);
    TCHAR szPath[MAX_PATH];
    auto pidl = (LPITEMIDLIST)item.lParam;
    SHGetPathFromIDList(pidl, szPath);

    // 写像を初期化する。
    mapping_t mapping;
    if (PathIsDirectory(szPath))
        Dialog2_InitSubstDir(hwndDlg, szPath, mapping);
    else
        Dialog2_InitSubstFile(hwndDlg, szPath, mapping);

    if (LoadFdtFile(szPath, mapping)) // FDTファイルの読み込みに成功したら
    {
        // 空の値を更新する。
        for (auto& pair : mapping)
        {
            if (pair.second.empty())
                UpdateValue(pair.first, pair.second);
        }
    }
    else
    {
        // 値を更新する。
        for (auto& pair : mapping)
        {
            UpdateValue(pair.first, pair.second);
        }
    }

    // ダイアログ2のUIを更新する。
    Dialog2_RefreshSubst(hwndDlg, mapping);

    // 写像をFDTファイルに保存する。
    SaveFdtFile(szPath, mapping);
}

// テンプレートからの生成。
BOOL DoTempla(HWND hwnd, LPTSTR pszPath, INT iItem)
{
    // 一時フォルダを削除する。
    DeleteTempDir(hwnd);

    // ファイルタイトルを取得する。
    string_t filename = PathFindFileName(pszPath);

    // 一時フォルダを取得する。
    TCHAR temp_path[MAX_PATH];
    GetTempPath(_countof(temp_path), temp_path);

    // 一時ファイル名を取得する。
    TCHAR temp_dir[MAX_PATH];
    GetTempFileName(temp_path, L"FDT", 0, temp_dir);

    // 一時フォルダを作成する。
    DeleteFile(temp_dir);
    if (!CreateDirectory(temp_dir, NULL))
        return FALSE;

    // 一時フォルダのパスファイル名を保存する。
    StringCchCopy(g_temp_dir, _countof(g_temp_dir), temp_dir);

    // 写像を取得し、FDTファイルとして保存する。
    mapping_t mapping = DoGetMapping();
    SaveFdtFile(pszPath, mapping);

    // 主処理を行う。
    templa(pszPath, temp_dir, mapping, g_ignore);

    // ファイル名を置き換えする。
    for (auto& pair : mapping)
    {
        str_replace(filename, pair.first, pair.second);
    }

    // 無効なファイル名を置き換える。
    templa_validate_filename(filename);

    // パスファイル名を再構築する。
    StringCchCopy(pszPath, MAX_PATH, temp_dir);
    PathAppend(pszPath, filename.c_str());

    return TRUE;
}

// LVN_BEGINDRAG / LVN_BEGINRDRAG - ドラッグが開始された。
static void OnBeginDrag(HWND hwnd, NM_LISTVIEW* pListView)
{
    // 選択がなければ無視。
    INT cSelected = ListView_GetSelectedCount(g_hListView);
    if (cSelected == 0)
        return;

    // 絶対PIDLを作成。
    PIDLIST_ABSOLUTE* ppidlAbsolute;
    ppidlAbsolute = (PIDLIST_ABSOLUTE*)CoTaskMemAlloc(sizeof(PIDLIST_ABSOLUTE) * cSelected);

    // 子PIDLを作成。
    PITEMID_CHILD* ppidlChild;
    ppidlChild = (PITEMID_CHILD*)CoTaskMemAlloc(sizeof(PITEMID_CHILD) * cSelected);

    // いずれかが失敗していれば破棄して戻る。
    if (!ppidlAbsolute || !ppidlChild)
    {
        CoTaskMemFree(ppidlAbsolute);
        CoTaskMemFree(ppidlChild);
        return;
    }

    // 各選択項目について一時ファイルを作成し、ドロップ元とする。
    INT cItems = ListView_GetItemCount(g_hListView);
    for (INT i = 0, j = 0; i < cItems; ++i)
    {
        if (ListView_GetItemState(g_hListView, i, LVIS_SELECTED))
        {
            LV_ITEM item = { LVIF_PARAM, i };
            ListView_GetItem(g_hListView, &item);
            TCHAR szPath[MAX_PATH];
            auto pidl = (LPITEMIDLIST)item.lParam;
            SHGetPathFromIDList(pidl, szPath);

            DoTempla(hwnd, szPath, i);
            ppidlAbsolute[j++] = ILCreateFromPath(szPath);
        }
    }

    // IShellFolderを取得し、バインドする。
    IShellFolder* pShellFolder = NULL;
    SHBindToParent(ppidlAbsolute[0], IID_PPV_ARGS(&pShellFolder), NULL);

    // 末端PIDLを取得する。
    for (INT i = 0; i < cSelected; i++)
        ppidlChild[i] = ILFindLastID(ppidlAbsolute[i]);

    // ドラッグの開始。ただし自分自身へのドロップは禁止する。
    g_pDropSource->BeginDrag();

    // データオブジェクトを取得する。
    IDataObject *pDataObject = NULL;
    pShellFolder->GetUIObjectOf(NULL, cSelected, (LPCITEMIDLIST*)ppidlChild, IID_IDataObject, NULL,
                                (void **)&pDataObject);
    if (pDataObject)
    {
        // ドラッグを処理する。
        DWORD dwEffect = DROPEFFECT_NONE;
        ::DoDragDrop(pDataObject, g_pDropSource, DROPEFFECT_COPY, &dwEffect);
    }

    // ドラッグの終了。
    g_pDropSource->EndDrag();

    // PIDLの解放。
    for (INT i = 0; i < cSelected; ++i)
        CoTaskMemFree(ppidlAbsolute[i]);

    CoTaskMemFree(ppidlAbsolute);
    CoTaskMemFree(ppidlChild);
}

// VK_DELETE - リストビューでDelキーが押された。
static void OnListViewDeleteKey(HWND hwnd, INT iItem)
{
    TCHAR szPath[MAX_PATH + 1];
    ZeroMemory(szPath, sizeof(szPath)); // SHFileOperationのために、二重ヌル終端にする。

    // 項目のパスファイル名を取得。
    LV_ITEM item = { LVIF_PARAM, iItem };
    ListView_GetItem(g_hListView, &item);
    auto pidl = (LPITEMIDLIST)item.lParam;
    SHGetPathFromIDList(pidl, szPath);

    if (::GetKeyState(VK_SHIFT) < 0) // Shiftキーが押されている
    {
        // 普通に削除する。
        if (!::DeleteFile(szPath))
        {
            MessageBox(hwnd, doLoadStr(IDS_CANTDELETEFILE), NULL, MB_ICONERROR);
        }
    }
    else
    {
        // シェルを使って削除する。
        SHFILEOPSTRUCT op = { hwnd, FO_DELETE, szPath };
        op.fFlags = FOF_ALLOWUNDO;
        ::SHFileOperation(&op);
    }
}

// リストビューの項目を削除する処理。
static void OnDeleteItem(HWND hwnd, NM_LISTVIEW *pListView)
{
    ILFree((LPITEMIDLIST)pListView->lParam);
}

// LVN_KEYDOWN - リストビューでキーが押された。
static LRESULT OnListViewKeyDown(HWND hwnd, LV_KEYDOWN *pKeyDown)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);

    switch (pKeyDown->wVKey)
    {
    case VK_RETURN: // Enterキーが押された。
        ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY); // 開く処理。
        break;

    case VK_DELETE: // Delキー（削除）が押された。
        if (iItem != -1)
            OnListViewDeleteKey(hwnd, iItem);
        break;

    case VK_F2: // F2キーが押された。
        {
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            ListView_EditLabel(g_hListView, iItem);
        }
        break;
    }

    return 0;
}

// LVN_ITEMCHANGED - リストビューの項目が変更された。
static LRESULT OnListViewItemChanged(HWND hwnd, NM_LISTVIEW* pListView)
{
    INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem != -1) // リストビューで何かが選択された。
    {
        // ダイアログ2に変更。
        g_iDialog = 1;

        // ステータスバーを更新。
        ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_TYPESUBST));

        // 置き換え項目を初期化。
        Dialog2_InitSubst(g_hwndDialogs[1], iItem);
    }
    else
    {
        // 選択を解除する前に処理を行う。
        if (g_iDialog == 1 && s_iItemOld != -1)
            OnDeSelectItem(hwnd, s_iItemOld);

        // ダイアログ1に変更。
        g_iDialog = 0;

        // ステータスバーを更新。
        ::SendMessage(g_hStatusBar, SB_SETTEXT, 0 | 0, (LPARAM)doLoadStr(IDS_SELECTITEM));
    }

    // 古い項目のインデックスを保存する。
    s_iItemOld = iItem;

    // g_iDialogが表すダイアログ以外非表示。
    for (INT i = 0; i < _countof(g_hwndDialogs); ++i)
    {
        if (i != g_iDialog)
            ::ShowWindowAsync(g_hwndDialogs[i], SW_HIDE);
    }
    ::ShowWindowAsync(g_hwndDialogs[g_iDialog], SW_SHOWNOACTIVATE);

    // WM_SIZEのハンドラによるコントロールの再配置。
    ::PostMessage(hwnd, WM_SIZE, 0, 0);

    return 0;
}

// WM_NOTIFY - メインウィンドウへの通知メッセージ。
static LRESULT OnNotify(HWND hwnd, int idFrom, LPNMHDR pnmhdr)
{
    // リストビュー以外は無視。
    if (idFrom != IDW_LISTVIEW)
        return 0;

    // 通知コードに応じて処理。
    switch (pnmhdr->code)
    {
    case NM_DBLCLK: // ダブルクリックされた。
        {
            // 開く処理。
            INT iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (iItem != -1)
                ShowContextMenu(hwnd, iItem, 0, 0, CMF_DEFAULTONLY);
        }
        break;

    case LVN_KEYDOWN: // キーが押された。
        return OnListViewKeyDown(hwnd, (LV_KEYDOWN*)pnmhdr);

    case LVN_ITEMCHANGED: // 項目が変更された。
        return OnListViewItemChanged(hwnd, (NM_LISTVIEW*)pnmhdr);

    case LVN_BEGINDRAG:
    case LVN_BEGINRDRAG:
        // ドラッグが開始された。
        OnBeginDrag(hwnd, (NM_LISTVIEW*)pnmhdr);
        break;

    case LVN_DELETEITEM:
        // 項目が削除された。
        OnDeleteItem(hwnd, (NM_LISTVIEW*)pnmhdr);
        break;

    case LVN_BEGINLABELEDIT:
        // 名前の変更。
        return FALSE;

    case LVN_ENDLABELEDIT:
        // 名前の変更の終了。
        {
            LV_DISPINFO *pDispInfo = (LV_DISPINFO *)pnmhdr;
            if (pDispInfo->item.pszText == NULL)
                break; // キャンセルされた。

            PathCleanupSpec(NULL, pDispInfo->item.pszText);
            if (pDispInfo->item.pszText[0] == 0)
                return FALSE; // 無効なファイル名だった。

            // パス名を取得する。
            LV_ITEM item = { LVIF_PARAM, pDispInfo->item.iItem };
            ListView_GetItem(g_hListView, &item);
            auto pidl = (LPITEMIDLIST)item.lParam;
            TCHAR szPath1[MAX_PATH], szPath2[MAX_PATH];
            SHGetPathFromIDList(pidl, szPath1);
            SHGetPathFromIDList(pidl, szPath2);

            // 拡張子が「.LNK」なら特別扱い。
            if (lstrcmpi(PathFindExtension(szPath1), TEXT(".LNK")) == 0 &&
                !PathIsDirectory(szPath1))
            {
                // 新しいパス名を構築する。
                PathRemoveFileSpec(szPath2);
                PathAppend(szPath2, pDispInfo->item.pszText);
                PathAddExtension(szPath2, TEXT(".LNK"));

                // 変更前に変更通知を行う。
                SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH, szPath1, szPath2);

                // 名前を変更する。
                MoveFileEx(szPath1, szPath2, MOVEFILE_COPY_ALLOWED);
            }
            else
            {
                // 新しいパス名を構築する。
                PathRemoveFileSpec(szPath2);
                PathAppend(szPath2, pDispInfo->item.pszText);

                // 変更前に変更通知を行う。
                if (PathIsDirectory(szPath1))
                    SHChangeNotify(SHCNE_RENAMEFOLDER, SHCNF_PATH, szPath1, szPath2);
                else
                    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATH, szPath1, szPath2);

                // 名前を変更する。
                MoveFileEx(szPath1, szPath2, MOVEFILE_COPY_ALLOWED);
            }

            // 古いPIDLを破棄する。
            ILFree(pidl);

            // アイコンを取得する。
            SHFILEINFO info;
            DWORD dwAttrs = (PathIsDirectory(szPath2) ? FILE_ATTRIBUTE_DIRECTORY : 0);
            UINT uFlags = SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES;
            if (lstrcmpi(PathFindExtension(szPath2), TEXT(".LNK")) == 0)
                uFlags |= SHGFI_LINKOVERLAY;
            SHGetFileInfo(szPath2, dwAttrs, &info, sizeof(info), uFlags);

            // 新しいPIDLをセット。アイコンをセット。
            item.mask = LVIF_IMAGE | LVIF_PARAM;
            item.iImage = ImageList_AddIcon(g_hImageList, info.hIcon);
            item.lParam = (LPARAM)ILCreateFromPath(szPath2);
            ListView_SetItem(g_hListView, &item);

            // アイコンを破棄する。
            ::DestroyIcon(info.hIcon);

            // 名前の変更を完了する。
            return TRUE;
        }
        break;
    }

    return 0;
}

// シェル変更通知が届いた。
static LRESULT OnShellChange(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    // 変更通知を処理するための前処理。
    LPITEMIDLIST *ppidlAbsolute;
    LONG lEvent;
    HANDLE hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &ppidlAbsolute, &lEvent);
    if (!hLock)
    {
        assert(0);
        return 0;
    }

    // 変更されたパスの親ディレクトリを取得する。
    TCHAR szPath[MAX_PATH];
    SHGetPathFromIDList(ppidlAbsolute[0], szPath);
    PathRemoveFileSpec(szPath);

    if (lstrcmpi(szPath, g_root_dir) == 0) // 親ディレクトリがテンプレートフォルダなら
    {
        // パス名を取得する。
        SHGetPathFromIDList(ppidlAbsolute[0], szPath);
        LPTSTR pszFileName = PathFindFileName(szPath);

        // ショートカットの拡張子「.LNK」を表示しない。
        TCHAR szFileTitle[MAX_PATH];
        GetFileTitle(szPath, szFileTitle, _countof(szFileTitle));
        if (lstrcmpi(PathFindExtension(szFileTitle), TEXT(".LNK")) == 0)
            PathRemoveExtension(szFileTitle);

        // ファイルタイトルで項目を検索する。
        LV_FINDINFO Find = { LVFI_STRING, szFileTitle };
        INT iItem = ListView_FindItem(g_hListView, -1, &Find);

        // イベントの種類に応じて処理を行う。
        switch (lEvent)
        {
        case SHCNE_CREATE:
        case SHCNE_MKDIR:
            // ファイルまたはフォルダの追加。FDTファイルは無視する。
            if (iItem == -1 && !templa_wildcard(pszFileName, L"*.fdt"))
            {
                // アイコンを取得する。
                DWORD dwAttrs = ((lEvent == SHCNE_MKDIR) ? FILE_ATTRIBUTE_DIRECTORY : 0);
                SHFILEINFO info;
                UINT uFlags = SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES;
                if (lstrcmpi(PathFindExtension(szPath), TEXT(".LNK")) == 0)
                    uFlags |= SHGFI_LINKOVERLAY;
                SHGetFileInfo(szPath, dwAttrs, &info, sizeof(info), uFlags);

                // リストビューに項目を追加する。
                LV_ITEM item = { LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM };
                item.iItem     = ListView_GetItemCount(g_hListView);
                item.iSubItem  = 0;
                item.pszText   = szFileTitle;
                item.iImage    = ImageList_AddIcon(g_hImageList, info.hIcon);
                item.lParam    = (LPARAM)ILCreateFromPath(szPath); // PIDLデータを保持する。
                iItem = ListView_InsertItem(g_hListView, &item);

                // アイコンを破棄。
                ::DestroyIcon(info.hIcon);

                // 追加された項目が見えるように表示位置を移動する。
                ListView_EnsureVisible(g_hListView, iItem, FALSE);
            }
            break;

        case SHCNE_DELETE:
        case SHCNE_RMDIR:
            // ファイルまたはフォルダの削除。リストビューから削除する。
            ListView_DeleteItem(g_hListView, iItem);
            break;

        case SHCNE_RENAMEFOLDER:
        case SHCNE_RENAMEITEM:
            // ファイルまたはフォルダの名前変更。
            {
                // パス名を取得する。
                SHGetPathFromIDList(ppidlAbsolute[1], szPath);

                // FDTファイルは無視する。
                if (templa_wildcard(PathFindFileName(szPath), L"*.fdt"))
                {
                    ListView_DeleteItem(g_hListView, iItem);
                }
                else
                {
                    // 拡張子が「.LNK」なら拡張子を表示しない。
                    LPTSTR pszFileTitle = PathFindFileName(szPath);
                    if (lstrcmpi(PathFindExtension(pszFileTitle), TEXT(".LNK")) == 0)
                        PathRemoveExtension(pszFileTitle);

                    // 項目名を更新する。
                    ListView_SetItemText(g_hListView, iItem, 0, pszFileTitle);

                    // 古いPIDLを破棄する。
                    LV_ITEM item = { LVIF_PARAM, iItem };
                    ListView_GetItem(g_hListView, &item);
                    ILFree((LPITEMIDLIST)item.lParam);

                    // PIDLを更新する。
                    item.lParam = (LPARAM)ILClone(ppidlAbsolute[1]);
                    ListView_SetItem(g_hListView, &item);
                }
            }
            break;
        }
    }

    // 変更通知の後処理。
    SHChangeNotification_Unlock(hLock);
    return 0;
}

// WM_MOVE - メインウィンドウの移動。
void OnMove(HWND hwnd, int x, int y)
{
    // 最小化または最大化されているときは無視する。
    if (IsIconic(hwnd) || IsZoomed(hwnd))
        return;

    // 位置を覚えておく。
    RECT rc;
    GetWindowRect(hwnd, &rc);
    g_ptWnd.x = rc.left;
    g_ptWnd.y = rc.top;
}

// ウィンドウプロシージャ。
LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_ACTIVATE, OnActivate);
        HANDLE_MSG(hwnd, WM_MOVE, OnMove);
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

// Windowsアプリのメイン関数。
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    HRESULT hrCoInit = CoInitialize(NULL);

    // コモンコントロールを初期化。
    InitCommonControls();

    // 設定の読み込み。
    LoadSettings();

    // ウィンドウクラスの登録。
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
        // 失敗。
        MessageBoxA(NULL, "RegisterClassEx failed", NULL, MB_ICONERROR);
        return 1;
    }

    // メインウィンドウの作成。
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = 0;
    HWND hwnd = ::CreateWindowEx(exstyle, CLASSNAME, doLoadStr(IDS_APPVERSION), style,
                                 g_ptWnd.x, g_ptWnd.y, g_sizWnd.cx, g_sizWnd.cy,
                                 NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        // 失敗。
        MessageBoxA(NULL, "CreateWindowEx failed", NULL, MB_ICONERROR);
        return 2;
    }

    // メインウィンドウの表示。
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // メッセージループ。
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

    // 設定の保存。
    SaveSettings();

    if (SUCCEEDED(hrCoInit))
        CoUninitialize();

    return INT(msg.wParam);
}
