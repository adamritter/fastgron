#pragma once
#include "simdjson.h"
#include "growing_string.hpp"
#include "jsonutils.hpp"
#include "print_gron.hpp"
#include "batched_print.hpp"

void print_filtered_path(growing_string &path, int processed, simdjson::ondemand::value element,
                         const unsigned flags, vector<string> &filters);
