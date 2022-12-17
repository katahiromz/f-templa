#pragma once

struct FDT_FILE
{
    struct ITEM
    {
        string_t key;
        string_t value;
    };
    struct SECTION
    {
        std::vector<ITEM> items;
        void simplify();
    };
    std::unordered_map<string_t, SECTION> name2section;

    bool load(LPCWSTR filename);
    bool save(LPCWSTR filename);
};
