#include "print_filtered_path.hpp"
#include "simdjson.h"
#include "parse_path.hpp"

using std::to_string;

void exit_with_error(string message)
{
    cerr << message << "\n";
    exit(EXIT_FAILURE);
}

#include <string>
#include <vector>
#include <iostream>
#include <string_view>

void print_value_accessor(
    growing_string &path,
    const ValueAccessor &valueAccessor, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters);

void print_slice(
    growing_string &path,
    const Slice &slice, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    int start = slice.start;
    int end = slice.end;
    if (element.type() == simdjson::ondemand::json_type::array)
    {
        auto array = element.get_array();
        if (slice.start < 0 || slice.end < 0)
        {
            int n = array.count_elements();
            if (slice.start < 0)
            {
                start += n;
            }
            if (slice.end < 0)
            {
                end += n;
            }
        }

        int index = 0;
        int path_size = path.size();
        for (auto child : array)
        {
            if (index >= start && (index < end) && (slice.step == 1 || (index - start) % slice.step == 0))
            {
                path.append("[");
                path.append(std::to_string(index));
                path.append("]");
                print_value_accessor(path, slice.value_accessor, child.value(), flags, filters);
                path.erase(path_size);
            }
            index++;
        }
    }
    else
    {
        exit_with_error("Element is not an array or object at path " + string(path));
    }
}

void print_object_accessors(
    growing_string &path,
    const ObjectAccessors &objectAccessors, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    int path_size = path.size();
    if (element.type() == simdjson::ondemand::json_type::object)
    {
        auto object = element.get_object();
        for (auto field : object)
        {
            string_view key = field.unescaped_key().value();
            // Check if the key matches any objectAccessor
            bool foundMatch = false;
            for (const auto &objectAccessor : objectAccessors.object_accessors)
            {
                if (objectAccessor.key == key)
                {
                    foundMatch = true;
                    string key_to_use = objectAccessor.new_key.value_or(objectAccessor.key);

                    if (is_js_identifier(key_to_use))
                    {
                        path.append(".");
                        path.append(key_to_use);
                    }
                    else
                    {
                        path.append("[\"");
                        path.append(key_to_use);
                        path.append("\"]");
                    }

                    print_value_accessor(path, objectAccessor.value_accessor, field.value(), flags, filters);

                    path.erase(path_size);

                    break;
                }
            }

            if (!foundMatch && objectAccessors.echo_others)
            {
                if (is_js_identifier(key))
                {
                    path.append(".");
                    path.append(key);
                }
                else
                {
                    path.append("[\"");
                    path.append(key);
                    path.append("\"]");
                }

                recursive_print_gron(field.value(), path, batched_out, flags, filters);

                path.erase(path_size);
            }
        }
    }
    else
    {
        exit_with_error("Element is not an object at path " + string(path));
    }
}

void print_all_accessor(
    growing_string &path,
    const AllAccessor &allAccessor, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    if (element.type() == simdjson::ondemand::json_type::array)
    {
        auto array = element.get_array();
        int index = 0;
        int path_size = path.size();
        for (auto child : array)
        {
            path.append("[");
            path.append(std::to_string(index++));
            path.append("]");
            print_value_accessor(path, allAccessor.value_accessor, child.value(), flags, filters);
            path.erase(path_size);
        }
    }
    else if (element.type() == simdjson::ondemand::json_type::object)
    {
        auto object = element.get_object();
        int path_size = path.size();
        for (auto field : object)
        {
            string_view key = field.unescaped_key().value();

            if (is_js_identifier(key))
            {
                path.append(".");
                path.append(key);
            }
            else
            {
                path.append("[\"");
                path.append(key);
                path.append("\"]");
            }

            print_value_accessor(path, allAccessor.value_accessor, field.value(), flags, filters);
            path.erase(path_size);
        }
    }
    else
    {
        exit_with_error("Element is not an array or object at path " + string(path));
    }
}

void print_value_accessor(
    growing_string &path,
    const ValueAccessor &valueAccessor, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    if (std::holds_alternative<std::monostate>(valueAccessor))
    {
        // No value accessor present, print the element
        recursive_print_gron(element, path, batched_out, flags, filters);
    }
    else if (std::holds_alternative<std::unique_ptr<Slice>>(valueAccessor))
    {
        // Value accessor is a slice, handle it
        const auto &slicePtr = std::get<std::unique_ptr<Slice>>(valueAccessor);
        print_slice(path, *slicePtr, element, flags, filters);
    }
    else if (std::holds_alternative<std::unique_ptr<ObjectAccessors>>(valueAccessor))
    {
        // Value accessor is a set of object accessors, handle it
        const auto &objectAccessorsPtr = std::get<std::unique_ptr<ObjectAccessors>>(valueAccessor);
        print_object_accessors(path, *objectAccessorsPtr, element, flags, filters);
    }
    else if (std::holds_alternative<std::unique_ptr<AllAccessor>>(valueAccessor))
    {
        // Value accessor is an all accessor, handle it
        const auto &allAccessorPtr = std::get<std::unique_ptr<AllAccessor>>(valueAccessor);
        print_all_accessor(path, *allAccessorPtr, element, flags, filters);
    }
    else
    {
        exit_with_error("Unknown value accessor type");
    }
}

void print_filtered_path(growing_string &path, int processed, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    string_view input = path.view().substr(processed);
    ValueAccessor valueAccessor = parse_path(input);
    path.erase(processed);
    print_value_accessor(path, valueAccessor, element, flags, filters);
}
