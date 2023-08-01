#include <windows.h>
#include <strsafe.h>
#include <algorithm>
#include <cassert>
#include "f-templa.hpp"
#include "fdt_file.hpp"

// 項目を追加する。すでに存在する場合は削除してから追加する。
void FDT_FILE::SECTION::assign(const ITEM& item)
{
retry:
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i] == item)
        {
            items.erase(items.begin() + i);
            goto retry;
        }
    }
    items.push_back(item);
}

// 項目を削除する。
void FDT_FILE::SECTION::erase(const string_t& key)
{
retry:
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].first == key)
        {
            items.erase(items.begin() + i);
            goto retry;
        }
    }
}

// 項目を追加する。すでに存在する場合は削除してから追加する。
void FDT_FILE::SECTION::assign(const string_t& key, const string_t& value)
{
    erase(key);
    items.push_back(std::make_pair(key, value));
}

// FDTファイルを読み込む。
bool FDT_FILE::load(LPCWSTR filename)
{
    TEMPLA_FILE file;
    if (!file.load(filename))
        return false;

    string_list_t lines;
    str_split(lines, file.m_string, string_t(L"\n"));

    string_t section_name;
    for (auto& line : lines)
    {
        str_trim(line, L" \t\r\n");

        if (templa_wildcard(line, L"[*]", false))
        {
            section_name = line.substr(1, line.size() - 2);
            continue;
        }

        size_t ich = line.find(L'=');
        if (ich != line.npos)
        {
            auto key = line.substr(0, ich);
            auto value = line.substr(ich + 1);
            str_trim(key, L" \t\r\n");
            str_trim(value, L" \t\r\n");
            ITEM item = { key, value };
            name2section[section_name].items.push_back(item);
            continue;
        }
    }

    return true;
}

// FDTファイルを保存する。
bool FDT_FILE::save(LPCWSTR filename)
{
    TEMPLA_FILE file;
    file.m_encoding = TE_UTF8;
    file.m_newline = TNL_CRLF;
    file.m_bom = true;

    for (auto& pair : name2section)
    {
        auto& name = pair.first;
        auto& section = pair.second;

        file.m_string += L'[';
        file.m_string += name;
        file.m_string += L"]\r\n";
        for (auto& pair : section.items)
        {
            file.m_string += pair.first;
            file.m_string += L" = ";
            file.m_string += pair.second;
            file.m_string += L"\r\n";
        }
        file.m_string += L"\r\n";
    }

    return file.save(filename);
}
