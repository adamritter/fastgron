# fastgron

Make JSON greppable super fast!

fastgron transforms JSON into discrete assignments to make it easier to grep for what you want and see the absolute 'path' to it. It eases the exploration of APIs that return large blobs of JSON but have terrible documentation.

`fastgron` is a high-performance JSON to GRON converter, developed in C++20, utilizing simdjson library.
It's 50x faster than [gron](https://github.com/tomnomnom/gron) on big files (400MB/s input / 1.8GB/s output on M1 Macbook Pro), so it makes big JSON files greppable.

```bash
> fastgron "https://api.github.com/repos/adamritter/fastgron/commits?per_page=1" | fgrep commit.author
json[0].commit.author = {}
json[0].commit.author.name = "adamritter"
json[0].commit.author.email = "58403584+adamritter@users.noreply.github.com"
json[0].commit.author.date = "2023-05-30T18:04:25Z"
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

MacOS:

```bash
brew install fastgron --build-from-source
```

Linux, Windows: Download the latest binary from [releases](https://github.com/adamritter/fastgron/releases)

Warning: On Windows libcurl support is missing from the released binary, so http / https URLs can't yet be read directly

## Usage

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
json = {}
json.name = "Tom"
json.github = "https://github.com/tomnomnom/"
json.likes = []
json.likes[0] = "code"
json.likes[1] = "cheese"
json.likes[2] = "meat"
json.contact = {}
json.contact.email = "mail@tomnomnom.com"
json.contact.twitter = "@TomNomNom"

> fastgron --help
Usage: fastgron [OPTIONS] [FILE | URL] [.path]

positional arguments:
  FILE           file name (or '-' for standard input)

options:
  -h, --help     show this help message and exit
  -V, --version  show version information and exit
  -s, --stream   enable stream mode
  -F, --fixed-string PATTERN  filter output by fixed string.
                     If -F is provided multiple times, multiple patterns are searched.
  -v, --invert-match select non-matching lines for fixed string search
  -i, --ignore-case  ignore case distinctions in PATTERN
  --sort sort output by key
  --user-agent   set user agent
  -u, --ungron   ungron: convert gron output back to JSON
  -p, -path      filter path, for example .#.3.population or cities.#.population
                 -p is optional if path starts with . and file with that name doesn't exist
  --no-indent   don't indent output
  --root        root path, default is json
  --semicolon   add semicolon to the end of each line
```

The file name can be - or missing, in that case the data is read from stdin.

## JSON lines (-s or --stream)

```fastgron testdata/stream.json -s
json = []
json[0] = {}
json[0].one = 1
json[0].two = 2
json[0].three = []
json[0].three[0] = 1
json[0].three[1] = 2
json[0].three[2] = 3
json[1] = {}
json[1].one = 1
json[1].two = 2
json[1].three = []
json[1].three[0] = 1
json[1].three[1] = 2
json[1].three[2] = 3
```

## Speed (50x speedup compared to gron on 190MB file)

While there's a 50x speedup for converting JSON to GRON, gron is not able to convert a 800MB file back to JSON.

It takes 8s for fastgron to convert the 840MB file back to JSON.

citylots.json can be downloaded here: https://github.com/zemirco/sf-city-lots-json/blob/master/citylots.json

```
time fastgron ~/Downloads/citylots.json > /dev/null
fastgron ~/Downloads/citylots.json > /dev/null  0.38s user 0.07s system 99% cpu 0.447 total

time gron --no-sort ~/Downloads/citylots.json  >/dev/null
gron --no-sort ~/Downloads/citylots.json > /dev/null  27.60s user 30.73s system 158% cpu 36.705 total

time fastgron --sort ~/Downloads/citylots.json > /dev/null
fastgron --sort ~/Downloads/citylots.json > /dev/null  1.05s user 0.41s system 99% cpu 1.464 total

time gron ~/Downloads/citylots.json > /dev/null
gron ~/Downloads/citylots.json > /dev/null 52.34s user 48.46s system 117% cpu 1:25.80 total

time fastgron ~/Downloads/citylots.json | rg UTAH
json.features[132396].properties.STREET = "UTAH"
json.features[132434].properties.STREET = "UTAH"
json.features[132438].properties.STREET = "UTAH"
json.features[132480].properties.STREET = "UTAH"
...
json.features[139041].properties.STREET = "UTAH"
json.features[139489].properties.STREET = "UTAH"
fastgron ~/Downloads/citylots.json  0.39s user 0.11s system 80% cpu 0.629 total
rg UTAH  0.07s user 0.05s system 19% cpu 0.629 total

time fastgron -u citylots.gson > c2.json
fastgron -u citylots.gson > c2.json  5.62s user 0.47s system 99% cpu 6.122 total

time gron -u citylots.gson > c3.json
[2]    8270 killed     gron -u citylots.gson > c3.json
gron -u citylots.gson > c3.json  66.99s user 61.06s system 189% cpu 1:07.75 total
```

Path finding is 18x faster than jq and 5x faster than jj:

```bash
> time jq -cM ".features[10000].properties.LOT_NUM" < ~/Downloads/citylots.json
"091"
jq -cM ".features[10000].properties.LOT_NUM" < ~/Downloads/citylots.json  2.91s user 0.28s system 97% cpu 3.252 total
> time jj -r features.10000.properties.LOT_NUM < ~/Downloads/citylots.json
"091"
jj -r features.10000.properties.LOT_NUM < ~/Downloads/citylots.json  0.87s user 0.71s system 161% cpu 0.972 total
> time build/fastgron .features.10000.properties.LOT_NUM < ~/Downloads/citylots.json
json.features[10000].properties.LOT_NUM = "091"
build/fastgron .features.10000.properties.LOT_NUM < ~/Downloads/citylots.json  0.07s user 0.10s system 95% cpu 0.176 total
```

## Installation

To build and run this project, you need:

- A C++20 compatible compiler
- [CMake](https://cmake.org/) (version 3.8 or higher)
- libcurl installed (Optional)

## Building and Installation for Linux

Here are the steps to build, test, and install `fastgron`:

   ```bash
   apt install cmake clang libcurl4-openssl-dev libssl-dev zlib1g-dev
   git clone https://github.com/adamritter/fastgron.git
   cd fastgron
   cmake -B build  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build build
   cmake install build/
   ```

## TODO

- Only color terminal support is missing from GRON
- Paths: Implement path autocompletition, \* / ?, \.
