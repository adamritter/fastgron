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

#include "growing_string.hpp"
#include "batched_print.hpp"
#include "jsonutils.hpp"
#include "print_gron.hpp"
#include "print_filtered_path.hpp"
#include "parse_gron.hpp"
#include "print_json.hpp"

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

growing_string indent = "";
#include "builder.hpp"

// Parse .hello[3]["asdf"] = 3.14; into builder
vector<Builder *> parse_gron_builders;
vector<int> parse_gron_builder_offsets;

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
        "                 More complex path expressions: .{id,users[1:-3:2].{name,address}}\n"
        "                 [[3]] is an index accessor without outputting on the path.\n"
        "  --no-indent   don't indent output\n"
        "  --no-newline  no newline inside JSON output\n"
        "  --root        root path, default is json\n"
        "  --semicolon   add semicolon to the end of each line\n"
        "  --no-spaces   don't add spaces around =\n"
        "  -c, --color   colorize output\n"
        "  --no-color    don't colorize output\n"
        "\nHome page with more information: https://github.com/adamritter/fastgron\n"
        "\nIf you have a feature that would help you, open an issue here:\nhttps://github.com/adamritter/fastgron/issues\n";
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
unsigned flags = SPACES | INDENT | NEWLINE;
vector<string> filters;

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
            flags &= ~INDENT;
        }
        else if (strcmp(argv[i], "--no-newline") == 0)
        {
            flags &= ~NEWLINE;
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
            if (!can_show(line_orig, flags, filters))
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
            while (index < parse_gron_builder_offsets.size() && parse_gron_builder_offsets[index] <= common)
            {
                index++;
            }
            parse_gron_builders.erase(parse_gron_builders.begin() + index, parse_gron_builders.end());
            parse_gron_builder_offsets.erase(parse_gron_builder_offsets.begin() + index, parse_gron_builder_offsets.end());
            Builder &passed_builder = parse_gron_builders.empty() ? builder : *parse_gron_builders.back();
            int offset = parse_gron_builders.empty() ? 0 : parse_gron_builder_offsets.back();
            parse_gron(line.substr(offset), passed_builder, offset, parse_gron_builders, parse_gron_builder_offsets);

            // parse_gron(line, builder, 0);
            last_line = line;
            data = end + 1;
        }
        if (std::holds_alternative<string_variant>(builder) && std::get<string_variant>(builder) == "")
        {
            cerr << "Builder is not assigned\n";
            return EXIT_FAILURE;
        }
        if (opts.stream)
        {
            flags &= ~NEWLINE;
            flags &= ~INDENT;
            if (std::holds_alternative<Vector>(builder))
            {
                for (auto &item : std::get<Vector>(builder).vector)
                {
                    print_json(item, flags);
                }
            }
            else
            {
                // Error: not a stream
                cerr << "Error: input gron file must be an array to be able to output a stream\n";
            }
        }
        else
        {
            print_json(builder, flags);
        }
        return EXIT_SUCCESS;
    }

    // Execute as a stream
    if (opts.stream)
    {
        ondemand::document_stream docs = parser.iterate_many(json);
        int index = 0;
        gprint(root + " = [];\n", batched_out, flags, filters);
        for (auto doc : docs)
        {
            growing_string path = growing_string(root);
            path.append("[").append(to_string(index++)).append("]");
            recursive_print_gron(doc, path, batched_out, flags, filters);
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
            int processed = 0;
            if (processed == 0 && path.starts_with(root))
            {
                processed = root.size();
            }
            print_filtered_path(path, processed, val, flags, filters);
        }
        else
        {
            recursive_print_gron(val, path, batched_out, flags, filters);
        }
    }
    batched_print_flush();

    return EXIT_SUCCESS;
}
