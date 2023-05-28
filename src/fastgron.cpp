#include <iostream>
#include "simdjson.h"
#include <fast_io.h>
using namespace simdjson;
using namespace std;

string out;
const string strue = "true";
const string sfalse = "false";

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

inline int raw_json_string_length(const ondemand::raw_json_string &str)
{
    bool in_escape = false;
    const char *s = str.raw();
    while (true)
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
                return s - str.raw();
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
}

void recursive_print_gron(ondemand::value element, string &path)
{
    switch (element.type())
    {
    case ondemand::json_type::array:
    {
        fast_io::io::print(path, " = [];\n");
        uint64_t index = 0;
        size_t orig_base_len = path.size();
        path.append("[");
        size_t base_len = path.size();
        char out[100];

        for (auto child : element.get_array())
        {
            auto end = fast_itoa(out, index++);
            path.append(string_view(out, end - out)).append("]");
            recursive_print_gron(child.value(), path);
            path.erase(base_len);
        }
        path.erase(orig_base_len);
        break;
    }
    case ondemand::json_type::object:
    {
        fast_io::io::print(path, " = {};\n");

        size_t base_len = path.size();
        for (auto field : element.get_object())
        {
            auto key = field.unescaped_key();
            if (!is_js_identifier(key.value()))
            {
                path.append("[\"");
                path.append(key.value());
                path.append("\"]");
            }
            else
            {
                path.append(".");
                path.append(key.value());
            }
            recursive_print_gron(field.value(), path);
            path.erase(base_len);
        }
        break;
    }
    case ondemand::json_type::number:
    {
        fast_io::io::print(path, " = ", element.get_double().value(), ";\n");
        break;
    }
    case ondemand::json_type::string:
    {
        fast_io::io::print(path, " = \"",
                           string_view(element.get_raw_json_string().raw(),
                                       raw_json_string_length(element.get_raw_json_string())),
                           "\";\n");
        break;
    }
    case ondemand::json_type::boolean:
    {
        fast_io::io::print(
            path, " = ",
            element.get_bool() ? strue : sfalse, ";\n");
        break;
    }
    case ondemand::json_type::null:
    {
        if (element.is_null())
        {
            fast_io::io::print(path, " = null;\n");
        }
        break;
    }
    }
}

int main(int argc, char *argv[])
{
    ondemand::parser parser;
    // get string name from command line
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <json>" << endl;
        return EXIT_FAILURE;
    }
    string fn = argv[1];
    for (int i = 0; i < 1; i++)
    {

        padded_string json = padded_string::load(fn);
        ondemand::document doc = parser.iterate(json);
        ondemand::value val = doc;
        string path = "json";
        recursive_print_gron(val, path);
    }
}
