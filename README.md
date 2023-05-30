# fastgron

Make JSON greppable super fast!

fastgron transforms JSON into discrete assignments to make it easier to grep for what you want and see the absolute 'path' to it. It eases the exploration of APIs that return large blobs of JSON but have terrible documentation.

`fastgron` is a high-performance JSON to GRON converter, developed in C++20, utilizing simdjson and fast_io libraries.
It's 40x faster than [gron](https://github.com/tomnomnom/gron) on big files, so it makes big JSON files greppable.

```bash
> fastgron "https://api.github.com/repos/adamritter/fastgron/commits?per_page=1" | fgrep commit.author
json[0].commit.author = {};
json[0].commit.author.name = "adamritter";
json[0].commit.author.email = "58403584+adamritter@users.noreply.github.com";
json[0].commit.author.date = "2023-05-30T18:04:25Z";
```

fastgron can work backwards too, enabling you to turn your filtered data back into JSON:

```bash
> fastgron "https://api.github.com/repos/adamritter/fastgron/commits?per_page=1" | fgrep commit.author | fastgron --ungron
[
  {
    "commit": {
      "author": {
        "date": "2023-05-30T18:11:03Z",
        "email": "58403584+adamritter@users.noreply.github.com",
        "name": "adamritter"
      }
    }
  }
]
```

## Quick Install

MacOS, Linux:

```bash
brew install adamritter/homebrew-fastgron/fastgron
```

Windows: Download from [release](https://github.com/adamritter/fastgron/releases/tag/v0.1.8)
libcurl support is missing from the released binary, so http / https URLs can't yet be read directly on Windows


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

> fastgron --help
Usage: fastgron [OPTIONS] [FILE | URL]

positional arguments:
  FILE           file name (or '-' for standard input)

options:
  -h, --help     show this help message and exit
  -V, --version  show version information and exit
  -s, --stream   enable stream mode
  -F, --fixed-string  PATTERN filter output by fixed string
  -i, --ignore-case  ignore case distinctions in PATTERN
  --sort sort output by key
  --user-agent   set user agent
  -u, --ungron   ungron: convert gron output back to JSON
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

While there's a 40x speedup for converting JSON to GRON, gron is not able to convert a 800MB file back to JSON.

It takes 8s for fastgron to convert the 840MB file back to JSON.

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

time fastgron -u citylots.gson > c2.json  
fastgron -u citylots.gson > c2.json  7.32s user 0.95s system 97% cpu 8.502 total

time gron -u citylots.gson > c3.json
[2]    8270 killed     gron -u citylots.gson > c3.json
gron -u citylots.gson > c3.json  66.99s user 61.06s system 189% cpu 1:07.75 total
```



## Installation

To build and run this project, you need:

- A C++20 compatible compiler
- [CMake](https://cmake.org/) (version 3.8 or higher)
- libcurl installed (Optional)

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

- Only color terminal support is missing from GRON
- Also adding filter paths is easy and can give a great user experience.
- Make ungron even faster by remembering keys in the last path
