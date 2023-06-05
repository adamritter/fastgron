#pragma once

#include <string_view>
#include <algorithm>
#include <cstring>
using std::max;
using std::string_view;

struct growing_string
{
    char *data;
    size_t len = 0;
    size_t capacity;

    growing_string() : capacity(1000)
    {
        data = new char[capacity];
    }

    bool starts_with(string_view s) const
    {
        if (s.size() > len)
        {
            return false;
        }
        return memcmp(data, s.data(), s.size()) == 0;
    }

    growing_string(string_view s) : capacity(s.size() + 100)
    {
        data = new char[capacity];
        memcpy(data, s.data(), s.size());
        len = s.size();
    }

    growing_string(const char *s) : capacity(strlen(s)), len(strlen(s))
    {
        data = new char[capacity];
        memcpy(data, s, len);
    }

    ~growing_string()
    {
        delete[] data;
    }
    void reserve_extra(size_t extra)
    {
        if (len + extra > capacity)
        {
            capacity = max(capacity * 2, len + extra);
            char *new_data = new char[capacity];
            memcpy(new_data, data, len);
            delete[] data;
            data = new_data;
        }
    }

    string_view view() const
    {
        return std::string_view(data, len);
    }

    growing_string &append(string_view s)
    {
        reserve_extra(s.size());
        memcpy(data + len, s.data(), s.size());
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
        if (newlen <= len)
        {
            len = newlen;
        }
        return *this;
    }

    inline size_t size() const
    {
        return len;
    }

    operator string_view() const
    {
        return {data, len};
    }

    void clear_mem()
    {
        len = 0;
        delete[] data;
        capacity = 0;
        data = nullptr;
    }
};
