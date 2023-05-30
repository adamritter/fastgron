#include "simdjson.h"
#include <fast_io.h>
#include <dragonbox/dragonbox_to_chars.h>
#include <fmt/core.h>
#include <functional>

#ifdef CURL_FOUND
#include <curl/curl.h>
bool curl_found = true;
#else
bool curl_found = false;
#endif

using namespace simdjson;
using namespace std;

string out;

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

growing_string batched_out;

void batched_print_flush()
{
    fast_io::io::print(batched_out.view());
    batched_out.erase(0);
}

void batched_print(string_view s)
{
    batched_out.append(s);
    if (batched_out.size() > 1000000)
    {
        batched_print_flush();
    }
}

void batched_print(char c)
{
    batched_out.append(c);
    if (batched_out.size() > 1000000)
    {
        batched_print_flush();
    }
}

void batched_print(double d)
{
    batched_out.reserve_extra(100);
    char *ptr2 = fmt::format_to(batched_out.data.data() + batched_out.len, "{}", d);
    batched_out.len = ptr2 - batched_out.data.data();
    if (batched_out.size() > 1000000)
    {
        batched_print_flush();
    }
}

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
        if (element.get_bool())
        {
            path.append(" = true;\n");
        }
        else
        {
            path.append(" = false;\n");
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
    bool ungron;
};

string user_agent = "fastgron/0.3.x";
bool no_indent = false;

options parse_options(int argc, char *argv[])
{
    options opts;
    opts.stream = false;
    opts.help = false;
    opts.version = false;
    opts.ungron = false;

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
        else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--ungron") == 0)
        {
            opts.ungron = true;
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
        else if (strcmp(argv[i], "--user-agent") == 0)
        {
            if (i + 1 >= argc)
            {
                fast_io::io::perr("Missing argument for --user-agent\n");
                exit(EXIT_FAILURE);
            }
            user_agent = argv[++i];
        }
        else if (strcmp(argv[i], "--no-indent") == 0)
        {
            no_indent = true;
        }
        else
        {
            opts.filename = argv[i];
        }
    }

    return opts;
}
#include <variant>
#include <map>

// string_view is a bit faster, but needs to hold original document in memory.
#define string_variant string_view
using Builder = std::variant<string_variant, struct Map, struct Vector>;

struct Map
{
    std::map<string_variant, Builder> map;
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
void parse_json(string_view line, Builder &builder)
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
        parse_json(line.substr(end), child->second);
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
        parse_json(line.substr(end), vector_alt[index]);
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
    fast_io::io::perr(
#ifdef CURL_FOUND
        "Usage: fastgron [OPTIONS] [FILE | URL]\n\n"
#else
        "Usage: fastgron [OPTIONS] [FILE]\n\n"
#endif
        "positional arguments:\n"
        "  FILE           file name (or '-' for standard input)\n\n"
        "options:\n"
        "  -h, --help     show this help message and exit\n"
        "  -V, --version  show version information and exit\n"
        "  -s, --stream   enable stream mode\n"
        "  -F, --fixed-string PATTERN  filter output by fixed string\n"
        "  -i, --ignore-case  ignore case distinctions in PATTERN\n"
        "  --sort sort output by key\n"
        "  --user-agent   set user agent\n"
        "  -u, --ungron   ungron: convert gron output back to JSON\n"
        "  --no-indent   don't indent output\n");
}

void print_version()
{

    fast_io::io::perr("fastgron version 0.3.x\n");
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
        fprintf(stderr, "malloc() failed\n");
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
        fprintf(stderr, "realloc() failed\n");
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
            fast_io::io::perr("Error when downloading data: ", string(curl_err_msg), "\n");
            exit(EXIT_FAILURE);
        }
        free(s.ptr);

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return r;
#else
    fast_io::io::perr("CURL wasn't compiled in fastgron\n");
    exit(EXIT_FAILURE);
#endif
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
        while (data < json.data() + json.size())
        {
            char *end = data;
            while (end < json.data() + json.size() && *end != '\n')
            {
                end++;
            }
            string_view line = string_view(data, end - data);
            if (line.starts_with("json"))
            {
                data = data + 4;
            }
            parse_json(string_view(data, end - data), builder);
            data = end + 1;
        }
        if (std::holds_alternative<string_variant>(builder) && std::get<string_variant>(builder) == "")
        {
            fast_io::io::perr("Builder is not assigned\n");
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
