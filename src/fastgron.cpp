#include "simdjson.h"
#include <fast_io.h>
#include <dragonbox/dragonbox_to_chars.h>
#include <fmt/core.h>
using namespace simdjson;
using namespace std;

string out;
const string strue = "true";
const string sfalse = "false";

struct growing_string
{
    std::vector<char> data;
    size_t len = 0;
    growing_string() : data(1000) {}
    growing_string(const char *s) : data(s, s + strlen(s)), len(strlen(s)) {}
    void reserve_extra(size_t extra)
    {
        if (len + extra > data.size())
        {
            data.resize(len + extra);
        }
    }
    string_view view() const
    {
        return string_view(data.data(), len);
    }

    growing_string &append(string_view s)
    {
        reserve_extra(s.size());
        memcpy(data.data() + len, s.data(), s.size());
        len += s.size();
        return *this;
    }
    growing_string &append(char c)
    {
        reserve_extra(1);
        data[len++] = c;
        return *this;
    }
    growing_string &erase(size_t newlen)
    {
        len = newlen;
        return *this;
    }
    size_t size() const
    {
        return len;
    }
};

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

void recursive_print_gron(ondemand::value element, growing_string &path)
{
    switch (element.type())
    {
    case ondemand::json_type::array:
    {
        fast_io::io::print(path.view(), " = [];\n");
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
        fast_io::io::print(path.view(), " = {};\n");

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
        bool use_dragonbox = false;
        if (use_dragonbox)
        {

            int base_len = path.size();
            path.reserve_extra(jkj::dragonbox::max_output_string_length<jkj::dragonbox::ieee754_binary64> + 10);
            char *ptr = &path.data[base_len];
            *ptr++ = ' ';
            *ptr++ = '=';
            *ptr++ = ' ';
            double value = element.get_double().value();
            if (value == 0.0 || value == -0.0)
            {
                *ptr++ = '0';
            }
            else
            {
                // ptr = jkj::dragonbox::to_chars_n(element.get_double().value(), ptr);
                ptr = fmt::format_to(ptr, "{}", element.get_double().value());
                // fast_io::io::print(&path.data[base_len], element.get_double().value());
            }
            *ptr++ = ';';
            *ptr++ = '\n';
            path.len = ptr - &path.data[0];
            fast_io::io::print(path.view());
            path.erase(base_len);
        }
        else
        {
            fast_io::io::print(path.view(), " = ", element.get_double().value(), ";\n");
        }
        break;
    }
    case ondemand::json_type::string:
    {
        bool create_string_first = false;
        if (create_string_first)
        {
            size_t base_len = path.size();
            int raw_json_string_len = raw_json_string_length(element.get_raw_json_string());
            path.reserve_extra(raw_json_string_len + 20);
            char *ptr = &path.data[base_len];
            *ptr++ = ' ';
            *ptr++ = '=';
            *ptr++ = ' ';
            *ptr++ = '"';
            memcpy(ptr, element.get_raw_json_string().raw(), raw_json_string_len);
            ptr += raw_json_string_len;
            *ptr++ = '"';
            *ptr++ = ';';
            *ptr++ = '\n';
            path.len = ptr - &path.data[0];
            fast_io::io::print(path.view());
            path.erase(base_len);
        }
        else
        {
            fast_io::io::print(path.view(), " = \"",
                               string_view(element.get_raw_json_string().raw(),
                                           raw_json_string_length(element.get_raw_json_string())),
                               "\";\n");
        }
        break;
    }
    case ondemand::json_type::boolean:
    {
        fast_io::io::print(
            path.view(), " = ",
            element.get_bool() ? strue : sfalse, ";\n");
        break;
    }
    case ondemand::json_type::null:
    {
        if (element.is_null())
        {
            fast_io::io::print(path.view(), " = null;\n");
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
    bool help;
    bool version;
};

options parse_options(int argc, char *argv[])
{
    options opts;
    opts.stream = false;
    opts.help = false;
    opts.version = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--stream") == 0 || strcmp(argv[i], "-s") == 0)
        {
            opts.stream = true;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            opts.help = true;
            break; // No need to process further arguments
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0)
        {
            opts.version = true;
            break; // No need to process further arguments
        }
        else
        {
            opts.filename = argv[i];
        }
    }

    return opts;
}

void print_help()
{
    fast_io::io::perr("Usage: fastgron [OPTIONS] [FILE]\n\n"
                      "positional arguments:\n"
                      "  FILE           file name (or '-' for standard input)\n\n"
                      "options:\n"
                      "  -h, --help     show this help message and exit\n"
                      "  -V, --version  show version information and exit\n"
                      "  -s, --stream   enable stream mode\n");
}

void print_version()
{
    fast_io::io::perr("fastgron version 0.2\n");
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

    if (opts.help)
    {
        print_help();
        return 0;
    }

    if (opts.version)
    {
        print_version();
        return 0;
    }

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
            growing_string path = growing_string("json[").append(to_string(index++)).append("]");
            recursive_print_gron(doc, path);
        }
    }
    // Execute as single document
    else
    {
        ondemand::document doc = parser.iterate(json);
        ondemand::value val = doc;
        growing_string path = "json";
        recursive_print_gron(val, path);
    }

    return EXIT_SUCCESS;
}
