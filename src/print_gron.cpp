#include "print_gron.hpp"
#include "simdjson.h"

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

void recursive_print_gron(simdjson::ondemand::value element, growing_string &path, growing_string &out_growing_string,
                          const unsigned flags, vector<string> &filters)
{
    switch (element.type())
    {
    case simdjson::ondemand::json_type::array:
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
        gprint(path, out_growing_string, flags, filters);
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
            auto end = simdjson::fast_itoa(out, index++);
            path.append(string_view(out, end - out));
            if (flags & COLOR)
                path.append("\033[1;34m]\033[0m");
            else
                path.append("]");
            recursive_print_gron(child.value(), path, out_growing_string, flags, filters);
            path.erase(base_len);
        }
        path.erase(orig_base_len);
        break;
    }
    case simdjson::ondemand::json_type::object:
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
        gprint(path, out_growing_string, flags, filters);
        path.erase(base_len);
        // fastgron can directly stream results to out_growing_string if we don't need to sort the output
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
                recursive_print_gron(field.value(), path, out2, flags, filters);
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
                recursive_print_gron(field.value(), path, out_growing_string, flags, filters);
                path.erase(base_len);
            }
        }
        break;
    }
    case simdjson::ondemand::json_type::number:
    case simdjson::ondemand::json_type::string:
    case simdjson::ondemand::json_type::boolean:
    case simdjson::ondemand::json_type::null:
    {
        size_t orig_out_len = out_growing_string.size();
        size_t path_size = path.size();
        string_view s = element.raw_json_token();
        out_growing_string.reserve_extra(path_size + orig_out_len + s.length() + 30);
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
            if (element.type() == simdjson::ondemand::json_type::number)
            {
                *ptr++ = '1';
            }
            else if (element.type() == simdjson::ondemand::json_type::string)
            {
                *ptr++ = '2';
            }
            else if (element.type() == simdjson::ondemand::json_type::boolean)
            {
                *ptr++ = '3';
            }
            else if (element.type() == simdjson::ondemand::json_type::null)
            {
                *ptr++ = '0';
            }
            *ptr++ = 'm';
        }
        while (s.size() > 0 && (s[s.size() - 1] == ' ' || s[s.size() - 1] == '\n'))
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
        if (can_show(ss, flags, filters))
        {
            if (flags & COLORIZE_MATCHES)
            {
                ss = colorize_matches(ss, filters);
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
