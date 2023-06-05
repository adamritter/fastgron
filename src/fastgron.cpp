#include "simdjson.h"
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <string_view>
#include <map>
#include <cstring> // for strcmp
#include <variant>
#include <sys/stat.h>
#include <version.h>

#ifndef FASTGRON_VERSION
#define FASTGRON_VERSION "<MISSING VERSION INFORMATION>"
#endif

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

#ifdef CURL_FOUND
#include <curl/curl.h>
bool curl_found = true;
#else
bool curl_found = false;
#endif

using namespace simdjson;
using namespace std;

string out;

vector<string> filters;
const unsigned SPACES = 1;
const unsigned SEMICOLON = 2;
const unsigned COLOR = 4;
const unsigned COLORIZE_MATCHES = 8;
const unsigned INVERT_MATCH = 16;
const unsigned IGNORE_CASE = 32;
const unsigned SORT_OUTPUT = 64;
unsigned flags = SPACES;

#include "growing_string.hpp"
#include "batched_print.hpp"
#include "jsonutils.hpp"

bool can_show(string_view s)
{
    if (!filters.empty())
    {
        bool found = false;
        for (auto filter : filters)
        {
            if (flags & IGNORE_CASE)
            {
                // std::tolower is slow, and doesn't handle UTF-8
                auto it = std::search(
                    s.begin(), s.end(),
                    filter.begin(), filter.end(),
                    [](unsigned char ch1, unsigned char ch2)
                    { return fast_tolower(ch1) == (ch2); });
                if (it != s.end())
                {
                    found = true;
                }
            }
            else
            {
                if (s.find(filter) != string_view::npos)
                {
                    found = true;
                }
            }
        }
        if (found == (flags & INVERT_MATCH ? true : false))
        {
            return false;
        }
    }
    return true;
}

growing_string &colorize_matches(string_view s)
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

void gprint(string_view s, growing_string &out_growing_string)
{
    if (!can_show(s))
    {
        return;
    }
    if (flags & COLORIZE_MATCHES)
    {
        s = colorize_matches(s);
    }
    out_growing_string.append(s);
}

inline char *print_equals(char *ptr, const unsigned flags)
{
    if (flags & SPACES)
    {
        *ptr++ = ' ';
        *ptr++ = '=';
        *ptr++ = ' ';
    }
    else
    {
        *ptr++ = '=';
    }
    return ptr;
}

void recursive_print_gron(ondemand::value element, growing_string &path, growing_string &out_growing_string,
                          const unsigned flags)
{
    switch (element.type())
    {
    case ondemand::json_type::array:
    {
        size_t orig_base_len = path.size();
        if (flags & SPACES)
            if (flags & COLOR)
                path.append(" = \033[1;34m[]\033[0m");
            else
                path.append(" = []");
        else if (flags & COLOR)
            path.append("=\033[1;34m[]\033[0m");
        else
            path.append("=[]");

        if (flags & SEMICOLON)
        {
            path.append(';');
        }
        path.append('\n');
        gprint(path, out_growing_string);
        path.erase(orig_base_len);
        uint64_t index = 0;
        if (flags & COLOR)
            path.append("\033[1;34m[\033[1;32m");
        else
            path.append("[");
        size_t base_len = path.size();
        char out[100];

        for (auto child : element.get_array())
        {
            auto end = fast_itoa(out, index++);
            path.append(string_view(out, end - out));
            if (flags & COLOR)
                path.append("\033[1;34m]\033[0m");
            else
                path.append("]");
            recursive_print_gron(child.value(), path, out_growing_string, flags);
            path.erase(base_len);
        }
        path.erase(orig_base_len);
        break;
    }
    case ondemand::json_type::object:
    {
        size_t base_len = path.size();
        if (flags & SPACES)
            if (flags & COLOR)
                path.append(" = \033[1;34m{}\033[0m");
            else
                path.append(" = {}");
        else if (flags & COLOR)
            path.append("=\033[1;34m{}\033[0m");
        else
            path.append("={}");
        if (flags & SEMICOLON)
        {
            path.append(';');
        }
        path.append('\n');
        gprint(path, out_growing_string);
        path.erase(base_len);
        if (flags & SORT_OUTPUT)
        {
            std::vector<std::pair<string, string>> fields;
            growing_string out2;
            for (auto field : element.get_object())
            {
                auto key = field.unescaped_key();
                string key_str(key.value());
                if (!is_js_identifier(key.value()))
                {
                    if (flags & COLOR)
                        path.append("\033[1;34m[\033[1;35m\"");
                    else
                        path.append("[\"");
                    path.append(key.value());

                    if (flags & COLOR)
                        path.append("\"\033[1;34m]\033[0m");
                    else
                        path.append("\"]");
                }
                else
                {
                    path.append(".");
                    if (flags & COLOR)
                        path.append("\033[1;34m");
                    path.append(key.value());
                    if (flags & COLOR)
                        path.append("\033[0m");
                }
                recursive_print_gron(field.value(), path, out2, flags);
                path.erase(base_len);
                fields.emplace_back(key_str, string(out2));
                out2.erase(0);
            }
            std::sort(fields.begin(), fields.end(), [](auto &a, auto &b)
                      { return a.first < b.first; });
            for (auto &field : fields)
            {
                out_growing_string.append(field.second);
            }
        }
        else
        {

            for (auto field : element.get_object())
            {
                auto key = field.unescaped_key();
                if (!is_js_identifier(key.value()))
                {
                    if (flags & COLOR)
                        path.append("\033[1;34m[\033[1;35m\"");
                    else
                        path.append("[\"");
                    path.append(key.value());
                    if (flags & COLOR)
                        path.append("\"\033[1;34m]\033[0m");
                    else
                        path.append("\"]");
                }
                else
                {
                    path.append(".");
                    if (flags & COLOR)
                        path.append("\033[1;34m");
                    path.append(key.value());
                    if (flags & COLOR)
                        path.append("\033[0m");
                }
                recursive_print_gron(field.value(), path, out_growing_string, flags);
                path.erase(base_len);
            }
        }
        break;
    }
    case ondemand::json_type::number:
    case ondemand::json_type::string:
    case ondemand::json_type::boolean:
    case ondemand::json_type::null:
    {
        size_t orig_out_len = out_growing_string.size();
        size_t path_size = path.size();
        out_growing_string.reserve_extra(path_size + orig_out_len + 30);
        char *ptr = &out_growing_string.data[orig_out_len];
        memcpy(ptr, path.data, path_size);
        ptr += path_size;
        ptr = print_equals(ptr, flags);
        if (flags & COLOR)
        {
            *ptr++ = '\033';
            *ptr++ = '[';
            *ptr++ = '1';
            *ptr++ = ';';
            *ptr++ = '3';
            if (element.type() == ondemand::json_type::number)
            {
                *ptr++ = '1';
            }
            else if (element.type() == ondemand::json_type::string)
            {
                *ptr++ = '2';
            }
            else if (element.type() == ondemand::json_type::boolean)
            {
                *ptr++ = '3';
            }
            else if (element.type() == ondemand::json_type::null)
            {
                *ptr++ = '0';
            }
            *ptr++ = 'm';
        }
        string_view s = element.raw_json_token();
        while (s.size() > 0 && s[s.size() - 1] == ' ')
        {
            s.remove_suffix(1);
        }
        memcpy(ptr, s.data(), s.size());
        ptr += s.size();
        if (flags & COLOR)
        {
            *ptr++ = '\033';
            *ptr++ = '[';
            *ptr++ = '0';
            *ptr++ = 'm';
        }
        if (flags & SEMICOLON)
        {
            *ptr++ = ';';
        }
        *ptr++ = '\n';
        string_view ss = string_view(&out_growing_string.data[orig_out_len], ptr - &out_growing_string.data[orig_out_len]);
        if (can_show(ss))
        {
            if (flags & COLORIZE_MATCHES)
            {
                ss = colorize_matches(ss);
                out_growing_string.append(ss);
            }
            else
            {
                out_growing_string.len = ptr - &out_growing_string.data[0];
            }
        }
        break;
    }
    }
    batched_print_flush_if_needed();
}

// Parse command-line options
struct options
{
    std::string filename;
    bool stream;
    bool help;
    bool version;
    bool ungron;
    std::string filtered_path;
};
string root = "json";

string user_agent = "fastgron";
bool no_indent = false;

bool is_url(string_view url)
{
    if (url.starts_with("http://") || url.starts_with("https://"))
    {
        return true;
    }
    return false;
}

void print_simdjson_version()
{
    cerr << "simdjson v" << SIMDJSON_VERSION << endl;
    cerr << "  Detected the best implementation for your machine: " << simdjson::get_active_implementation()->name();
    cerr << "(" << simdjson::get_active_implementation()->description() << ")" << endl;
}

options parse_options(int argc, char *argv[])
{
    options opts;
    opts.stream = false;
    opts.help = false;
    opts.version = false;
    opts.ungron = false;

    if (argc == 1 && isatty(0))
    {
        opts.help = true;
    }

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
        else if (strcmp(argv[i], "--simdjson-version") == 0)
        {
            print_simdjson_version();
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0)
        {
            flags |= IGNORE_CASE;
        }
        else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--ungron") == 0)
        {
            opts.ungron = true;
        }
        else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--fixed-string") == 0)
        {
            if (i + 1 >= argc)
            {
                cerr << "Missing argument for -F\n";
                exit(EXIT_FAILURE);
            }
            filters.push_back(argv[++i]);
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--invert-match") == 0)
        {
            flags |= INVERT_MATCH;
        }
        else if (strcmp(argv[i], "--sort") == 0)
        {
            flags |= SORT_OUTPUT;
        }
        else if (strcmp(argv[i], "--no-sort") == 0)
        {
            flags &= ~SORT_OUTPUT;
        }
        else if (strcmp(argv[i], "--user-agent") == 0)
        {
            if (i + 1 >= argc)
            {
                cerr << "Missing argument for --user-agent\n";
                exit(EXIT_FAILURE);
            }
            user_agent = argv[++i];
        }
        else if (strcmp(argv[i], "--no-indent") == 0)
        {
            no_indent = true;
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--path") == 0)
        {
            if (i + 1 >= argc)
            {
                cerr << "Missing argument for -p\n";
                exit(EXIT_FAILURE);
            }
            opts.filtered_path = argv[++i];
        }
        else if (strcmp(argv[i], "--root") == 0)
        {
            if (i + 1 >= argc)
            {
                cerr << "Missing argument for --root\n";
                exit(EXIT_FAILURE);
            }
            root = argv[++i];
        }
        else if (strcmp(argv[i], "--semicolon") == 0)
        {
            flags |= SEMICOLON;
        }
        else if (strcmp(argv[i], "--no-spaces") == 0)
        {
            flags &= ~SPACES;
        }
        else if (strcmp(argv[i], "--color") == 0 || strcmp(argv[i], "-c") == 0)
        {
            flags |= COLOR;
        }
        else if (strcmp(argv[i], "--no-color") == 0)
        {
            flags &= ~COLOR;
        }
        else if (argv[i][0] == '-')
        {
            cerr << "Unknown option: " << argv[i] << "\n";
            exit(EXIT_FAILURE);
        }
        else
        {
            if (!is_url(argv[i]) && access(argv[i], F_OK) == -1 && argv[i] != string("-"))
            {
                // Treat strings starting with . as paths
                if (argv[i][0] == '.')
                {
                    opts.filtered_path = argv[i];
                    continue;
                }
                else
                {
                    cerr << "File not found: " << argv[i] << "\n";
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                opts.filename = argv[i];
            }
        }
    }

    return opts;
}

// string_view is a bit faster, but needs to hold original document in memory.
#define string_variant string_view
using Builder = std::variant<string_variant, struct Map, struct Vector>;

struct Map
{
    std::map<string, Builder> map;
};

struct Vector
{
    std::vector<Builder> vector;
};

growing_string indent = "";
void print_json(Builder builder);
void print_vector(Vector &vector_holder)
{
    batched_print("[\n");
    if (!no_indent)
    {
        indent.append("  ");
    }
    bool first = true;
    for (auto &item : vector_holder.vector)
    {
        if (!first)
        {
            batched_print(",\n");
        }
        first = false;
        batched_print(indent.view());
        print_json(item);
    }
    if (!no_indent)
    {
        indent.erase(indent.size() - 2);
    }
    batched_print("\n");
    batched_print(indent.view());
    batched_print("]");
}

void print_map(Map &map_holder)
{
    batched_print("{\n");
    if (!no_indent)
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
        print_json(item.second);
    }
    if (!no_indent)
    {
        indent.erase(indent.size() - 2);
    }
    batched_print("\n");
    batched_print(indent.view());
    batched_print("}");
}

void print_json(Builder builder)
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
        print_vector(std::get<Vector>(builder));
    }
    else if (std::holds_alternative<Map>(builder))
    {
        print_map(std::get<Map>(builder));
    }
}
// Parse .hello[3]["asdf"] = 3.14; into builder
vector<Builder *> parse_json_builders;
vector<int> parse_json_builder_offsets;

// BUG: ["..."] is not handled
void parse_json(string_view line, Builder &builder, int offset)
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
        parse_json_builders.push_back(&child->second);
        parse_json_builder_offsets.emplace_back(offset + end);
        parse_json(line.substr(end), child->second, offset + end);
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
        parse_json_builders.push_back(&vector_alt[index]);
        parse_json_builder_offsets.emplace_back(offset + end);
        parse_json(line.substr(end), vector_alt[index], offset + end);
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

void print_help()
{
    cerr <<
#ifdef CURL_FOUND
        "Usage: fastgron [OPTIONS] [FILE | URL] [.path]\n\n"
#else
        "Usage: fastgron [OPTIONS] [FILE] [.path]\n\n"
#endif
        "positional arguments:\n"
        "  FILE           file name (or '-' for standard input)\n\n"
        "options:\n"
        "  -h, --help     show this help message and exit\n"
        "  -V, --version  show version information and exit\n"
        "  -s, --stream   enable stream mode\n"
        "  -F, --fixed-string PATTERN  filter output by fixed string.\n"
        "                     If -F is provided multiple times, multiple patterns are searched.\n"
        "  -v, --invert-match select non-matching lines for fixed string search\n"
        "  -i, --ignore-case  ignore case distinctions in PATTERN\n"
        "  --sort sort output by key\n"
        "  --user-agent   set user agent\n"
        "  -u, --ungron   ungron: convert gron output back to JSON\n"
        "  -p, -path      filter path, for example .#.3.population or cities.#.population\n"
        "                 -p is optional if path starts with . and file with that name doesn't exist\n"
        "  --no-indent   don't indent output\n"
        "  --root        root path, default is json\n"
        "  --semicolon   add semicolon to the end of each line\n"
        "  --no-spaces   don't add spaces around =\n"
        "  -c, --color   colorize output\n"
        "  --no-color    don't colorize output\n"
        "\nHome page with more information: https://github.com/adamritter/fastgron\n";
}

void print_version()
{
    cerr << "fastgron version " << FASTGRON_VERSION << "\n";
}

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
        cerr << "Failed to read file\n";
        return "";
    }

    close(fd);
    return content;
}

// Set up CURL

struct string2
{
    char *ptr;
    size_t len;
};

void init_string(struct string2 *s)
{
    s->len = 0;
    s->ptr = (char *)malloc(s->len + 1);
    if (s->ptr == NULL)
    {
        cerr << "malloc() failed\n";
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string2 *s)
{
    size_t new_len = s->len + size * nmemb;
    s->ptr = (char *)realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL)
    {
        cerr << "realloc() failed\n";
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size * nmemb;
}

std::string download(std::string url)
{
#ifdef CURL_FOUND
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    string r;
    if (curl)
    {
        struct string2 s;
        init_string(&s);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
        res = curl_easy_perform(curl);

        r = string_view(s.ptr, s.len);
        if (res != CURLE_OK)
        {
            const char *curl_err_msg = curl_easy_strerror(res);
            cerr << "Error when downloading data: " << string(curl_err_msg) << "\n";
            exit(EXIT_FAILURE);
        }
        free(s.ptr);

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return r;
#else
    cerr << "CURL wasn't compiled in fastgron\n";
    exit(EXIT_FAILURE);
#endif
}
void print_filtered_path(growing_string &path, int processed, ondemand::value element)
{
    if (processed == 0 && path.starts_with(root))
    {
        processed = root.size();
    }
    if (path.size() == processed)
    {
        recursive_print_gron(element, path, batched_out, flags);
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
            if (element.type() == ondemand::json_type::array)
            {
                int index = 0;
                growing_string path2 = path.view().substr(0, processed);
                for (auto child : element.get_array())
                {
                    path2.append("[");
                    path2.append(std::to_string(index++));
                    path2.append("]");
                    int processed2 = path2.len;
                    path2.append(path.view().substr(end));
                    print_filtered_path(path2, processed2, child.value());
                    path2.erase(processed);
                }
            }
            else if (element.type() == ondemand::json_type::object)
            {
                growing_string path2 = path.view().substr(0, processed);
                for (auto field : element.get_object())
                {
                    path2.append(".");
                    path2.append(field.unescaped_key().value());
                    int processed2 = path2.len;
                    path2.append(path.view().substr(end));
                    print_filtered_path(path2, processed2, field.value());
                    path2.erase(processed);
                }
            }
            else
            {
                cerr << "Element is not an array or object at path " << string(path).substr(0, processed + 1) << "\n";
                exit(EXIT_FAILURE);
            }
        }
        else if (element.type() == ondemand::json_type::object)
        {

            auto field = element.find_field(key);
            if (field.error() == SUCCESS)
            {
                print_filtered_path(path, end, field.value());
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
        else if (isdigit(key[0]) && element.type() == ondemand::json_type::array)
        {
            int index = stoi(string(key));
            auto child = element.at(index);
            if (child.error() == SUCCESS)
            {
                growing_string path2 = path.view().substr(0, processed);
                path2.append("[");
                path2.append(key);
                path2.append("]");
                path2.append(path.view().substr(end));
                print_filtered_path(path2, end + 1, child.value());
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
                if (element.type() != ondemand::json_type::array)
                {
                    cerr << "Element is not an array at path " << string(path).substr(0, processed + 1) << "\n";
                    exit(EXIT_FAILURE);
                }
                auto child = element.at(index);
                if (child.error() == SUCCESS)
                {
                    print_filtered_path(path, end + 1, child.value());
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
                if (field.error() == SUCCESS)
                {
                    print_filtered_path(path, end + 1, field.value());
                }
            }
        }
        else if (path.data[end] == '#')
        {
            end++;
            if (path.data[end] == ']')
            {
                end++;
                if (element.type() == ondemand::json_type::array)
                {
                    growing_string path2 = path.view().substr(0, processed);
                    int index = 0;
                    for (auto child : element.get_array())
                    {
                        path2.append("[");
                        path2.append(std::to_string(index++));
                        path2.append("]");
                        int processed2 = path2.len;
                        path2.append(path.view().substr(end));
                        print_filtered_path(path2, processed2, child.value());
                        path2.erase(processed);
                    }
                }
                else if (element.type() == ondemand::json_type::object)
                {
                    growing_string path2 = path.view().substr(0, processed);
                    for (auto field : element.get_object())
                    {
                        path2.append("[\"");
                        path2.append(field.unescaped_key().value());
                        path2.append("\"]");
                        int processed2 = path2.len;
                        path2.append(path.view().substr(end));
                        print_filtered_path(path2, processed2, field.value());
                        path2.erase(processed);
                    }
                }
                else
                {
                    cerr << "Element is not an array or object at path " << string(path).substr(0, processed + 1) << "\n";
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                cerr << "Invalid path: " << string(path) << ", processed: " << processed << "\n";
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        cerr << "Invalid path: " << string(path) << ", processed: " << processed << "\n";
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (isatty(1))
    {
        flags |= COLOR;
    }
    ondemand::parser parser;

    options opts = parse_options(argc, argv);
    if (flags & IGNORE_CASE)
    {
        for (auto &filter : filters)
        {
            std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c)
                           { return fast_tolower(c); });
        }
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
    if ((flags & COLOR) && filters.size() > 0)
    {
        flags &= ~COLOR; // Can't search in colorized text
        flags |= COLORIZE_MATCHES;
    }

    padded_string json;
    // Check if filename is provided
    if (opts.filename.empty() || opts.filename == "-")
    {
        // Load string from stdin
        json = padded_string(readFileIntoString(0));
    }
    else if (curl_found && opts.filename.compare(0, 7, "http://") == 0 || opts.filename.compare(0, 8, "https://") == 0)
    {
        json = padded_string(download(opts.filename));
    }
    else
    {
        json = padded_string::load(opts.filename);
    }

    if (opts.ungron)
    {
        Builder builder;
        char *data = json.data();
        string_view last_line = "";
        while (data < json.data() + json.size())
        {
            char *end = data;
            while (end < json.data() + json.size() && *end != '\n')
            {
                end++;
            }
            string_view line_orig = string_view(data, end - data);
            if (!can_show(line_orig))
            {
                data = end + 1;
                continue;
            }
            if (line_orig.starts_with(root))
            {
                data = data + root.size();
            }
            string_view line = string_view(data, end - data);
            // find commonality with last line
            int common = 0;
            while (common < line.size() && common < last_line.size() && line[common] == last_line[common])
            {
                common++;
            }
            int index = 0;
            while (index < parse_json_builder_offsets.size() && parse_json_builder_offsets[index] <= common)
            {
                index++;
            }
            parse_json_builders.erase(parse_json_builders.begin() + index, parse_json_builders.end());
            parse_json_builder_offsets.erase(parse_json_builder_offsets.begin() + index, parse_json_builder_offsets.end());
            Builder &passed_builder = parse_json_builders.empty() ? builder : *parse_json_builders.back();
            int offset = parse_json_builders.empty() ? 0 : parse_json_builder_offsets.back();
            parse_json(line.substr(offset), passed_builder, offset);
            // parse_json(line, builder, 0);
            last_line = line;
            data = end + 1;
        }
        if (std::holds_alternative<string_variant>(builder) && std::get<string_variant>(builder) == "")
        {
            cerr << "Builder is not assigned\n";
            return EXIT_FAILURE;
        }
        batched_out.reserve_extra(1000000);
        print_json(builder);
        batched_print("\n");
        batched_print_flush();

        return EXIT_SUCCESS;
    }

    // Execute as a stream
    if (opts.stream)
    {
        ondemand::document_stream docs = parser.iterate_many(json);
        int index = 0;
        gprint(root + " = [];\n", batched_out);
        for (auto doc : docs)
        {
            growing_string path = growing_string(root).append("[").append(to_string(index++)).append("]");
            recursive_print_gron(doc, path, batched_out, flags);
        }
    }
    // Execute as single document
    else
    {
        ondemand::document doc = parser.iterate(json);
        ondemand::value val = doc;
        growing_string path(root);
        if (!opts.filtered_path.empty())
        {
            if (opts.filtered_path.starts_with(root))
            {
                path.erase(0);
            }
            else if (!(opts.filtered_path[0] == '.' ||
                       opts.filtered_path[0] == '['))
            {
                path.append(".");
            }
            path.append(opts.filtered_path);
            print_filtered_path(path, 0, val);
        }
        else
        {
            recursive_print_gron(val, path, batched_out, flags);
        }
    }
    batched_print_flush();

    return EXIT_SUCCESS;
}
