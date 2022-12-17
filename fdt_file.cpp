#include <windows.h>
#include <strsafe.h>
#include <algorithm>
#include <cassert>
#include "f-templa.hpp"
#include "fdt_file.hpp"

void FDT_FILE::SECTION::simplify()
{
    std::stable_sort(items.begin(), items.end(), [&](const ITEM& a, const ITEM& b){
        return a.key == b.key;
    });
    auto it = std::unique(items.begin(), items.end(), [&](const ITEM& a, const ITEM& b){
        return a.key == b.key;
    });
    items.erase(items.end(), it);
}

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
            file.m_string += pair.key;
            file.m_string += L" = ";
            file.m_string += pair.value;
            file.m_string += L"\r\n";
        }
        file.m_string += L"\r\n";
    }

    return file.save(filename);
}
