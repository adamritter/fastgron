#pragma once

#include "batched_print.hpp"
#include "growing_string.hpp"
#include "jsonutils.hpp"
#include "simdjson.h"
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
using simdjson::ondemand::value;
using std::string;
using std::string_view;
using std::vector;
void recursive_print_gron(
    simdjson::ondemand::value element,
    growing_string &path,
    growing_string &out_growing_string,
    const unsigned flags,
    vector<string> &filters
);

inline growing_string &colorize_matches(string_view s, vector<string> &filters)
{
    static growing_string out;
    out.clear_mem();
    out.reserve_extra(s.size() + 100);
    while (s.size() > 0)
    {
        size_t pos = 0;
        size_t len = 0;
        for (auto match : filters)
        {
            size_t pos2 = s.find(match, 0);
            if (pos2 != string_view::npos)
            {
                pos = pos2;
                len = match.size();
            }
        }
        if (pos == 0)
        {
            out.append(s);
            break;
        }
        out.append(s.substr(0, pos));
        out.append("\033[1;31m");
        out.append(s.substr(pos, len));
        out.append("\033[0m");
        s = s.substr(pos + len);
    }
    return out;
}

inline void gprint(
    string_view s,
    growing_string &out_growing_string,
    const unsigned flags,
    vector<string> &filters
)
{
    if (!can_show(s, flags, filters))
    {
        return;
    }
    if (flags & COLORIZE_MATCHES)
    {
        s = colorize_matches(s, filters);
    }
    out_growing_string.append(s);
}
