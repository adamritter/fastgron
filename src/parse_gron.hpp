#pragma once

#include "builder.hpp"
#include <vector>
using std::vector;

void parse_gron(string_view line, Builder &builder, int offset,
                vector<Builder *> &parse_gron_builders,
                vector<int> &parse_gron_builder_offsets);
