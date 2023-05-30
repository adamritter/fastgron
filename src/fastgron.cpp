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
#include <cstring> // for strcmp

// Parse command-line options
struct options
{
    std::string filename;
    bool stream;
};

options parse_options(int argc, char *argv[])
{
    options opts;
    opts.stream = false;

    // Check all arguments
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--stream") == 0 || strcmp(argv[i], "-s") == 0)
        {
            opts.stream = true;
        }
        else
        {
            opts.filename = argv[i]; // Assume this is the filename
        }
    }

    return opts;
}

#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <iostream>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

std::string readFileIntoString(int fd)
{
    std::vector<char> buffer(1000000);
    std::string content;

    ssize_t bytesRead = 0;
    while ((bytesRead = read(fd, buffer.data(), buffer.size())) > 0)
    {
        content.append(buffer.data(), bytesRead);
    }

    if (bytesRead == -1)
    {
        fast_io::io::perr("Failed to read file\n");
        return "";
    }

    close(fd);
    return content;
}

int main(int argc, char *argv[])
{
    ondemand::parser parser;

    options opts = parse_options(argc, argv);
    padded_string json;
    // Check if filename is provided
    if (opts.filename.empty() || opts.filename == "-")
    {
        // Load string from stdin
        json = padded_string(readFileIntoString(0));
    }
    else
    {
        json = padded_string::load(opts.filename);
    }

    // Execute as a stream
    if (opts.stream)
    {
        ondemand::document_stream docs = parser.iterate_many(json);
        int index = 0;
        fast_io::io::print("json = [];\n");
        for (auto doc : docs)
        {
            string path = "json[" + to_string(index++) + "]";
            recursive_print_gron(doc, path);
        }
    }
    // Execute as single document
    else
    {
        ondemand::document doc = parser.iterate(json);
        ondemand::value val = doc;
        string path = "json";
        recursive_print_gron(val, path);
    }

    return EXIT_SUCCESS;
}
