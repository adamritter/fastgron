#include "print_filtered_path.hpp"
#include "simdjson.h"

using std::to_string;

void exit_with_error(string message)
{
    cerr << message << "\n";
    exit(EXIT_FAILURE);
}

int find_path_end(growing_string &path, int processed)
{
    int end = processed + 1;
    while (end < path.size() && path.data[end] != '[' && path.data[end] != '.')
    {
        end++;
    }
    return end;
}

void process_element_as_object(growing_string &path, int processed, int end, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    cerr << "process_element_as_object: " << string(path) << ", processed: " << processed << ", end: " << end << "\n";
    growing_string path2 = path.view().substr(0, processed);
    for (auto field : element.get_object())
    {
        string_view key = field.unescaped_key().value();
        if (is_js_identifier(key))
        {
            path2.append(".");
            path2.append(key);
        }
        else
        {
            path2.append("[\"");
            path2.append(key);
            path2.append("\"]");
        }

        int processed2 = path2.len;
        path2.append(path.view().substr(find_path_end(path, processed)));
        print_filtered_path(path2, processed2, field.value(), flags, filters);
        path2.erase(processed);
    }
}

void process_element_as_array(growing_string &path, int processed, int end, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    int index = 0;
    growing_string path2 = path.view().substr(0, processed);
    for (auto child : element.get_array())
    {
        path2.append("[");
        path2.append(std::to_string(index++));
        path2.append("]");

        int processed2 = path2.len;
        path2.append(path.view().substr(find_path_end(path, processed)));
        print_filtered_path(path2, processed2, child.value(), flags, filters);
        path2.erase(processed);
    }
}

void print_filtered_path(growing_string &path, int processed, simdjson::ondemand::value element, const unsigned flags, vector<string> &filters)
{
    if (path.size() == processed)
    {
        recursive_print_gron(element, path, batched_out, flags, filters);
    }
    else if (path.data[processed] == '.')
    {
        int end = processed + 1;
        while (end < path.size() && path.data[end] != '[' && path.data[end] != '.')
        {
            end++;
        }
        string_view key(&path.data[processed + 1], end - processed - 1);
        if (key == "#")
        {
            if (element.type() == simdjson::ondemand::json_type::array)
            {
                process_element_as_array(path, processed, end, element, flags, filters);
            }
            else if (element.type() == simdjson::ondemand::json_type::object)
            {
                process_element_as_object(path, processed, end, element, flags, filters);
            }
            else
            {
                exit_with_error("Element is not an array or object at path " + string(path).substr(0, processed + 1));
            }
        }
        else if (element.type() == simdjson::ondemand::json_type::object)
        {
            auto field = element.find_field(key);
            if (field.error() == simdjson::SUCCESS)
            {
                print_filtered_path(path, end, field.value(), flags, filters);
            }
            else
            {
                if (field.error() == 19)
                {
                    cerr << "field error: key: " << key << " not found in path " << string(path) << "\n";
                }
                else
                {
                    cerr << "field error: " << (int)field.error() << "\n";
                }
            }
        }
        else if (isdigit(key[0]) && element.type() == simdjson::ondemand::json_type::array)
        {
            int index = stoi(string(key));
            auto child = element.at(index);
            if (child.error() == simdjson::SUCCESS)
            {
                growing_string path2 = path.view().substr(0, processed);
                path2.append("[");
                path2.append(key);
                path2.append("]");
                path2.append(path.view().substr(end));
                print_filtered_path(path2, end + 1, child.value(), flags, filters);
            }
            else
            {
                cerr << "child error: " << (int)child.error() << "\n";
            }
        }
    }
    else if (path.data[processed] == '[')
    {
        int end = processed + 1;
        if (isdigit(path.data[end]))
        {
            while (isdigit(path.data[end]))
            {
                end++;
            }
            if (path.data[end] == ']')
            {
                int index = stoi(string(&path.data[processed + 1], end - processed - 1));
                if (element.type() != simdjson::ondemand::json_type::array)
                {
                    exit_with_error("Element is not an array at path " + string(path).substr(0, processed + 1));
                }
                auto child = element.at(index);
                if (child.error() == simdjson::SUCCESS)
                {
                    print_filtered_path(path, end + 1, child.value(), flags, filters);
                }
                else
                {
                    cerr << "child error: " << (int)child.error() << "\n";
                }
            }
        }
        else if (path.data[end] == '"')
        {
            while (path.data[end] != '"')
            {
                end++;
            }
            end++;
            if (path.data[end] == ']')
            {
                string_view key(&path.data[processed + 2], end - processed - 3);
                auto field = element.find_field(key);
                if (field.error() == simdjson::SUCCESS)
                {
                    print_filtered_path(path, end + 1, field.value(), flags, filters);
                }
            }
        }
        else if (path.data[end] == '#')
        {
            end++;
            if (path.data[end] == ']')
            {
                end++;
                if (element.type() == simdjson::ondemand::json_type::array)
                {
                    process_element_as_array(path, processed, end, element, flags, filters);
                }
                else if (element.type() == simdjson::ondemand::json_type::object)
                {
                    process_element_as_object(path, processed, end, element, flags, filters);
                }
                else
                {
                    exit_with_error("Element is not an array or object at path " + string(path).substr(0, processed + 1));
                }
            }
            else
            {
                exit_with_error("Invalid path: " + string(path) + ", processed: " + std::to_string(processed));
            }
        }
    }
    else
    {
        exit_with_error("Invalid path: " + string(path) + ", processed: " + std::to_string(processed));
    }
}
