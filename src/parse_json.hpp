#pragma once

#include "builder.hpp"
#include <vector>
using std::vector;

void parse_json(string_view line, Builder &builder, int offset,
                vector<Builder *> &parse_json_builders,
                vector<int> &parse_json_builder_offsets);
