#include "print_json.hpp"
#include "batched_print.hpp"
#include "jsonutils.hpp"

void print_json_inner(Builder& builder, const unsigned flags, growing_string &indent);

void print_vector(Vector &vector_holder, const unsigned flags, growing_string &indent)
{
    batched_out.append('[');
    if (flags & NEWLINE)
    {
        batched_out.append('\n');
    }
    if (flags & INDENT)
    {
        indent.append("  ");
    }
    bool first = true;
    for (auto &item : vector_holder.vector)
    {
        if (!first)
        {
            batched_out.append(',');
            if (flags & NEWLINE)
            {
                batched_out.append('\n');
            }
        }
        first = false;
        batched_print(indent.view());
        print_json_inner(item, flags, indent);
    }
    if (flags & INDENT)
    {
        indent.erase(indent.size() - 2);
    }
    // batched_print("\n");
    if (flags & NEWLINE)
    {
        batched_out.append('\n');
    }
    batched_print(indent.view());
    batched_print("]");
}

void print_map(Map &map_holder, const unsigned flags, growing_string &indent)
{
    batched_print('{');
    if (flags & NEWLINE)
    {
        batched_print('\n');
    }
    if (flags & INDENT)
    {
        indent.append("  ");
    }
    bool first = true;
    for (auto &item : map_holder.map)
    {
        if (!first)
        {
            batched_print(",\n");
        }
        first = false;
        batched_print(indent.view());
        batched_print('"');
        batched_print(item.first);
        batched_print("\": ");
        print_json_inner(item.second, flags, indent);
    }
    if (flags & INDENT)
    {
        indent.erase(indent.size() - 2);
    }
    if (flags & NEWLINE)
    {
        batched_print("\n");
    }
    batched_print(indent.view());
    batched_print("}");
}

void print_json_inner(Builder& builder, const unsigned flags, growing_string &indent)
{
    if (std::holds_alternative<string_variant>(builder))
    {
        string_variant &s = std::get<string_variant>(builder);
        if (s.empty())
        {
            batched_print("null");
            return;
        }
        batched_print(std::get<string_variant>(builder));
    }
    else if (std::holds_alternative<Vector>(builder))
    {
        print_vector(std::get<Vector>(builder), flags, indent);
    }
    else if (std::holds_alternative<Map>(builder))
    {
        print_map(std::get<Map>(builder), flags, indent);
    }
}

void print_json(Builder& builder, const unsigned flags)
{
    growing_string indent;
    batched_out.reserve_extra(1000000);
    print_json_inner(builder, flags, indent);
    batched_print("\n"); // Final newline is always printed
    batched_print_flush();
}