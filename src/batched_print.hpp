#pragma once
#include "growing_string.hpp"
#include <iostream>
using std::cerr;
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

extern growing_string batched_out;

inline void write_all(string_view s)
{
    int written = 0;
    while (written < s.size())
    {
        int w = write(1, s.data() + written, s.size() - written);
        if (w == -1)
        {
            cerr << "write failed\n";
            exit(EXIT_FAILURE);
        }
        written += w;
    }
}

inline void batched_print_flush()
{
    write_all(batched_out.view());
    batched_out.erase(0);
}

inline void batched_print_flush_if_needed()
{
    if (batched_out.size() > 1000000)
    {
        batched_print_flush();
    }
}

inline void batched_print(string_view s)
{
    batched_out.append(s);
    batched_print_flush_if_needed();
}

inline void batched_print_no_flush(string_view s) { batched_out.append(s); }

inline void batched_print(char c)
{
    batched_out.append(c);
    batched_print_flush_if_needed();
}