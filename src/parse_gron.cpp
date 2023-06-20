#include <string_view>
#include "parse_gron.hpp"

// BUG: ["..."] is not handled
void parse_gron(string_view line, Builder &builder, int offset,
                vector<Builder *> &parse_gron_builders,
                vector<int> &parse_gron_builder_offsets)

{
    if (line.empty())
    {
        return;
    }
    if (line[0] == '.')
    {
        if (std::holds_alternative<string_variant>(builder))
        {
            builder.emplace<Map>();
        }
        auto &map_alt = std::get<Map>(builder).map;

        // find end of key
        size_t end = 1;
        while (end < line.size() && line[end] != '[' && line[end] != '=' && line[end] != ' ' && line[end] != '.')
        {
            end++;
        }
        string key(line.substr(1, end - 1));
        auto child = map_alt.find(key);
        if (child == map_alt.end())
        {
            child = map_alt.emplace(key, string_variant()).first;
        }
        parse_gron_builders.push_back(&child->second);
        parse_gron_builder_offsets.emplace_back(offset + end);
        parse_gron(line.substr(end), child->second, offset + end, parse_gron_builders, parse_gron_builder_offsets);
    }
    else if (line[0] == '[' && isdigit(line[1]))
    {
        if (std::holds_alternative<string_variant>(builder))
        {
            builder.emplace<Vector>();
        }
        auto &vector_alt = std::get<Vector>(builder).vector;
        size_t end = 1;
        while (end < line.size() && line[end] != ']')
        {
            end++;
        }
        if (end != line.size())
        {
            end++;
        }
        int index = stoi(string(line.substr(1, end - 1)));
        if (index >= vector_alt.size())
        {
            vector_alt.resize(index + 1);
        }
        parse_gron_builders.push_back(&vector_alt[index]);
        parse_gron_builder_offsets.emplace_back(offset + end);
        parse_gron(line.substr(end), vector_alt[index], offset + end, parse_gron_builders, parse_gron_builder_offsets);
    }
    else if (line[0] == '=' || (line[0] == ' ' && line[1] == '='))
    {
        size_t start = 1;
        while (start < line.size() && (line[start] == ' ' || line[start] == '='))
        {
            start++;
        }
        size_t end = line.end() - line.begin();
        while (end > start && (line[end - 1] == ' ' || line[end - 1] == ';'))
        {
            end--;
        }
        string_view value(line.substr(start, end - start));
        if (value == "[]")
        {
            builder.emplace<Vector>();
        }
        else if (value == "{}")
        {
            builder.emplace<Map>();
        }
        else
        {
            builder.emplace<string_variant>(value);
        }
    }
}