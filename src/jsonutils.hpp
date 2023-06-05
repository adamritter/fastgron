#pragma once
#include <string_view>

using std::string_view;

bool is_js_identifier(string_view s)
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

char fast_tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + ('a' - 'A');
    }
    return c;
}

// Returns -1 if end is reached before closing quote
int raw_json_string_length(string_view str)
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