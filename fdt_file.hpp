#pragma once

struct FDT_FILE
{
    typedef std::pair<string_t, string_t> ITEM;
    struct SECTION
    {
        std::vector<ITEM> items;
        void simplify();
    };
    std::unordered_map<string_t, SECTION> name2section;

    bool load(LPCWSTR filename);
    bool save(LPCWSTR filename);
};
