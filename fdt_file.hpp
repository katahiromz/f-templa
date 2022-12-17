#pragma once

struct FDT_FILE
{
    typedef std::pair<string_t, string_t> ITEM;
    struct SECTION
    {
        std::vector<ITEM> items;
        void assign(const ITEM& item);
        void assign(const string_t& key, const string_t& value);
    };
    std::unordered_map<string_t, SECTION> name2section;

    bool load(LPCWSTR filename);
    bool save(LPCWSTR filename);
};
