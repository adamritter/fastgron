#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using std::string;
using std::string_view;
using std::vector;

// Flags

const unsigned SPACES = 1;
const unsigned SEMICOLON = 2;
const unsigned COLOR = 4;
const unsigned COLORIZE_MATCHES = 8;
const unsigned INVERT_MATCH = 16;
const unsigned IGNORE_CASE = 32;
const unsigned SORT_OUTPUT = 64;
const unsigned INDENT = 128;
const unsigned NEWLINE = 256;

inline bool is_js_identifier(string_view s)
{
    if (s.empty())
    {
        return false;
    }
    if (!isalpha(s[0]) && s[0] != '_')
    {
        return false;
    }
    for (size_t i = 1; i < s.size(); i++)
    {
        if (!isalnum(s[i]) && s[i] != '_')
        {
            return false;
        }
    }
    return true;
}

inline char fast_tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + ('a' - 'A');
    }
    return c;
}

// Returns -1 if end is reached before closing quote
inline int raw_json_string_length(string_view str)
{
    bool in_escape = false;
    const char *s = str.begin();
    while (s < str.end())
    {
        switch (*s)
        {
        case '\\':
            in_escape = !in_escape;
            break;
        case '"':
            if (in_escape)
            {
                in_escape = false;
            }
            else
            {
                return s - str.begin();
            }
            break;
        case '\n':
        case '\r':
        case 0:
            return -1;
        default:
            if (in_escape)
            {
                in_escape = false;
            }
        }
        s++;
    }
    return -1;
}

// Returns -1 if end is reached before closing quote
inline int raw_json_string_length(const char *s)
{
    const char *begin = s;
    bool in_escape = false;
    while (s)
    {
        switch (*s)
        {
        case '\\':
            in_escape = !in_escape;
            break;
        case '"':
            if (in_escape)
            {
                in_escape = false;
            }
            else
            {
                return s - begin;
            }
            break;
        case '\n':
        case '\r':
        case 0:
            return -1;
        default:
            if (in_escape)
            {
                in_escape = false;
            }
        }
        s++;
    }
    return -1;
}

inline bool
can_show(string_view s, const unsigned flags, vector<string> &filters)
{
    if (!filters.empty())
    {
        bool found = false;
        for (string &filter : filters)
        {
            if (flags & IGNORE_CASE)
            {
                // std::tolower is slow, and doesn't handle UTF-8
                auto it = std::search(
                    s.begin(), s.end(), filter.begin(), filter.end(),
                    [](unsigned char ch1, unsigned char ch2)
                    { return fast_tolower(ch1) == (ch2); }
                );
                if (it != s.end())
                {
                    found = true;
                }
            }
            else
            {
                if (s.find(filter) != string_view::npos)
                {
                    found = true;
                }
            }
        }
        if (found == (flags & INVERT_MATCH ? true : false))
        {
            return false;
        }
    }
    return true;
}
