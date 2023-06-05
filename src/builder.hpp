#pragma once

#include <string_view>
#include <string>
#include <variant>
#include <vector>
#include <map>
using std::string;
using std::string_view;

// string_view is a bit faster, but needs to hold original document in memory.
using string_variant = string_view;

using Builder = std::variant<string_variant, struct Map, struct Vector>;

struct Map
{
    std::map<string, Builder> map;
};

struct Vector
{
    std::vector<Builder> vector;
};
