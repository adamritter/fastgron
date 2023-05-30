# fastgron

`fastgron` is a high-performance JSON to GRON converter, developed in C++20, utilizing simdjson and fast_io libraries.
It's 40x faster than [gron](https://github.com/tomnomnom/gron) on big files, so it makes big JSON files greppable.

## Usage

Here is a basic usage example:

```bash
> cat testdata/two.json
{
    "name": "Tom",
    "github": "https://github.com/tomnomnom/",
    "likes": ["code", "cheese", "meat"],
    "contact": {
        "email": "mail@tomnomnom.com",
        "twitter": "@TomNomNom"
    }
}
> fastgron testdata/two.json
json = {};
json.name = "Tom";
json.github = "https://github.com/tomnomnom/";
json.likes = [];
json.likes[0] = "code";
json.likes[1] = "cheese";
json.likes[2] = "meat";
json.contact = {};
json.contact.email = "mail@tomnomnom.com";
json.contact.twitter = "@TomNomNom";
```

The file name can be - or missing, in that case the data is read from stdin.

## JSON lines (-s or --stream)

```fastgron testdata/stream.json -s
json = []
json[0] = {};
json[0].one = 1;
json[0].two = 2;
json[0].three = [];
json[0].three[0] = 1;
json[0].three[1] = 2;
json[0].three[2] = 3;
json[1] = {};
json[1].one = 1;
json[1].two = 2;
json[1].three = [];
json[1].three[0] = 1;
json[1].three[1] = 2;
json[1].three[2] = 3;
```

## Speed (40x speedup compared to gron on 190MB file)

citylots.json can be downloaded here: https://github.com/zemirco/sf-city-lots-json/blob/master/citylots.json

```
time fastgron ~/Downloads/citylots.json > /dev/null
./fastgron ~/Downloads/citylots.json --sort > /dev/null  0.97s user 0.07s system 99% cpu 1.041 total

time gron --no-sort ~/Downloads/citylots.json  >/dev/null
gron --no-sort ~/Downloads/citylots.json > /dev/null  30.12s user 36.74s system 161% cpu 41.501 total

time fastgron --sort ~/Downloads/citylots.json > /dev/null
fastgron --sort ~/Downloads/citylots.json > /dev/null  1.55s user 0.48s system 87% cpu 2.322 total

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
```

## Quick Install

MacOS, Linux:

```bash
brew install adamritter/homebrew-fastgron/fastgron
```

Windows: Download from [release](https://github.com/adamritter/fastgron/releases/tag/v0.1.8)

## Installation

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

## TODO

These features need to be implemented to be mostly compatible with gron (and add a few more features):

- Filter paths
- Ungron
- Color terminal support
- Grep by regexp (Use RE2? It compiles very slowly, but probably good library)
