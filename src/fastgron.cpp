#include "simdjson.h"
#include <fast_io.h>
#include <dragonbox/dragonbox_to_chars.h>
#include <fmt/core.h>
#include <functional>
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
        return std::string_view(data.data(), len);
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
    operator string_view() const
    {
        return {data.data(), len};
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

bool force_gprint = false;
string filter;
std::unique_ptr<std::boyer_moore_searcher<string::iterator, hash<char>, equal_to<void>>> searcher;
bool ignore_case = false;
bool sort_output = false;

char fast_tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + ('a' - 'A');
    }
    return c;
}

void gprint(string_view s, growing_string *out_growing_string)
{

    if (!filter.empty())
    {
        if (ignore_case)
        {
            // std::tolower is slow, and doesn't handle UTF-8
            auto it = std::search(
                s.begin(), s.end(),
                filter.begin(), filter.end(),
                [](unsigned char ch1, unsigned char ch2)
                { return fast_tolower(ch1) == (ch2); });
            if (it == s.end())
            {
                return;
            }
        }
        else
        {
            if (s.find(filter) == string_view::npos)
            {
                return;
            }
        }
    }
    if (out_growing_string)
    {
        // fast_io::io::print("out_growing_string.append at gprint: ", string(*out_growing_string), ":)))\n");
        out_growing_string->append(s);
        return;
    }
    fast_io::io::print(s);
}

void recursive_print_gron(ondemand::value element, growing_string &path, growing_string *out_growing_string)
{
    // fast_io::io::print("path: ", string(path), " out_growing_string is nullptr: ", out_growing_string == nullptr, "::)))\n");
    switch (element.type())
    {
    case ondemand::json_type::array:
    {
        size_t orig_base_len = path.size();
        path.append(" = [];\n");
        gprint(path, out_growing_string);
        path.erase(orig_base_len);
        uint64_t index = 0;
        path.append("[");
        size_t base_len = path.size();
        char out[100];

        for (auto child : element.get_array())
        {
            auto end = fast_itoa(out, index++);
            path.append(string_view(out, end - out)).append("]");
            recursive_print_gron(child.value(), path, out_growing_string);
            path.erase(base_len);
        }
        path.erase(orig_base_len);
        break;
    }
    case ondemand::json_type::object:
    {
        size_t base_len = path.size();
        path.append(" = {};\n");
        gprint(path, out_growing_string);
        path.erase(base_len);
        // gprint("OBJECT************\n", out_growing_string);
        if (sort_output)
        {
            // gprint("SORT**********\n", out_growing_string);
            std::vector<std::pair<string, string>> fields;
            growing_string out2;
            for (auto field : element.get_object())
            {
                auto key = field.unescaped_key();
                string key_str(key.value());
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
                recursive_print_gron(field.value(), path, &out2);
                // fast_io::io::print("key0: ", key_str, " value: ", string(out2), ":)\n");
                path.erase(base_len);
                fields.emplace_back(key_str, string(out2));
                out2.erase(0);
            }
            std::sort(fields.begin(), fields.end(), [](auto &a, auto &b)
                      { return a.first < b.first; });
            for (auto &field : fields)
            {
                // fast_io::io::print("key: ", field.first, " value: ", field.second, "::)\n");
                gprint(field.second, out_growing_string);
            }
        }
        else
        {

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
                recursive_print_gron(field.value(), path, out_growing_string);
                path.erase(base_len);
            }
        }
        break;
    }
    case ondemand::json_type::number:
    {
        if (force_gprint)
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
            gprint(path, out_growing_string);
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
        if (force_gprint)
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
            gprint(path, out_growing_string);
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

        size_t base_len = path.size();
        path.reserve_extra(10);
        if (element.get_bool())
        {
            memcpy(&path.data[base_len], " = true;\n", 10);
            path.len = base_len + 10;
        }
        else
        {
            memcpy(&path.data[base_len], " = false;\n", 11);
            path.len = base_len + 11;
        }
        gprint(path, out_growing_string);
        path.erase(base_len);
        break;
    }
    case ondemand::json_type::null:
    {
        if (element.is_null())
        {
            size_t base_len = path.size();
            path.append(" = null;\n");
            gprint(path, out_growing_string);
            path.erase(base_len);
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
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0)
        {
            ignore_case = true;
        }
        else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--fixed-string") == 0)
        {
            if (i + 1 >= argc)
            {
                fast_io::io::perr("Missing argument for -F\n");
                exit(EXIT_FAILURE);
            }
            filter = argv[++i];
            searcher.reset(
                new std::boyer_moore_searcher<string::iterator, hash<char>, equal_to<void>>(
                    filter.begin(), filter.end()));
            force_gprint = true;
        }
        else if (strcmp(argv[i], "--sort") == 0)
        {
            sort_output = true;
            force_gprint = true;
        }
        else if (strcmp(argv[i], "--no-sort") == 0)
        {
            sort_output = false;
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
                      "  -s, --stream   enable stream mode\n"
                      "  -F, --fixed-string  PATTERN filter output by fixed string\n"
                      "  -i, --ignore-case  ignore case distinctions in PATTERN\n"
                      "  --sort sort output by key\n");
}

void print_version()
{
    fast_io::io::perr("fastgron version 0.2.x\n");
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
    if (ignore_case)
    {
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c)
                       { return fast_tolower(c); });
    }

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
            recursive_print_gron(doc, path, nullptr);
        }
    }
    // Execute as single document
    else
    {
        ondemand::document doc = parser.iterate(json);
        ondemand::value val = doc;
        growing_string path = "json";
        recursive_print_gron(val, path, nullptr);
    }

    return EXIT_SUCCESS;
}
