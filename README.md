# fastgron

`fastgron` is a high-performance JSON to GRON converter, developed in C++20, utilizing simdjson and fast_io libraries.

## Prerequisites

To build and run this project, you need:

- A C++20 compatible compiler
- [CMake](https://cmake.org/) (version 3.8 or higher)

## Building and Installation

Here are the steps to build, test, and install `fastgron`:

1. Clone this repository:
   ```bash
   git clone https://github.com/adamritter/fastgron.git
   ```
2. Move into the project directory:
   ```bash
   cd fastgron
   ```
3. Make a new directory for building:
   ```bash
   mkdir build && cd build
   ```
4. Generate Makefile using CMake:
   ```bash
   cmake ..
   ```
5. Build the project:
   ```bash
   make
   ```
6. To install, use the following command:
   ```bash
   sudo make install
   ```

## Usage

Here is a basic usage example:

```bash
./fastgron citylots.json
```

## Speed (80x speedup compared to gron on 190MB file)

citylots.json can be downloaded here: https://github.com/zemirco/sf-city-lots-json/blob/master/citylots.json

time fastgron ~/Downloads/citylots.json > /dev/null
fastgron ~/Downloads/citylots.json > /dev/null 0.95s user 0.08s system 99% cpu 1.029 total

time gron ~/Downloads/citylots.json > /dev/null
gron ~/Downloads/citylots.json > /dev/null 52.34s user 48.46s system 117% cpu 1:25.80 total

time fastgron ~/Downloads/citylots.json | rg UTAH
json.features[132396].properties.STREET = "UTAH";
json.features[132434].properties.STREET = "UTAH";
json.features[132438].properties.STREET = "UTAH";
json.features[132480].properties.STREET = "UTAH";
...
json.features[139041].properties.STREET = "UTAH";
json.features[139489].properties.STREET = "UTAH";
fastgron ~/Downloads/citylots.json 1.00s user 0.11s system 98% cpu 1.128 total
rg UTAH 0.09s user 0.07s system 14% cpu 1.127 total

## Usage