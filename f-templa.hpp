#pragma once

#include "templa/templa.hpp"

extern TCHAR g_root_dir[MAX_PATH];
extern TCHAR g_temp_dir[MAX_PATH + 1];

void CenterWindowDx(HWND hwnd);
BOOL Dialog2_InitSubstFile(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping);
BOOL Dialog2_InitSubstDir(HWND hwndDlg, LPCTSTR pszPath, mapping_t& mapping);
bool SaveFdtFile(LPCTSTR pszBasePath, mapping_t& mapping);
