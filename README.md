# Unicode IDNA
![OpenSSF Scorecard Badge](https://api.securityscorecards.dev/projects/github.com/ada-url/idna/badge)
[![VS17-CI](https://github.com/ada-url/idna/actions/workflows/vs.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/vs.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)

The ada-url/ada library is a C++ library implementing the `to_ascii` and `to_unicode` functions from the [Unicode Technical Standard](https://www.unicode.org/reports/tr46/#ToUnicode) supporting a wide range of systems. It is suitable for URL parsing.

According to our benchmarks, it can be faster than ICU.

## Requirements

- A recent C++ compiler supporting C++17. We test GCC 9 or better, LLVM 10 or better and Microsoft Visual Studio 2022.

## Usage

```cpp
std::string_view input = u8"meßagefactory.ca";// non-empty UTF-8 string, must be percent decoded
std::string idna_ascii = ada::idna::to_ascii(input);
if(idna_ascii.empty()) {
    // There was an error.
}
std::cout << idna_ascii << std::endl;
// outputs 'xn--meagefactory-m9a.ca' if the input is u8"meßagefactory.ca"
```

## Benchmarks

You may build a benchmarking tool with the library as follows under macOS and Linux:

```bash
cmake -D ADA_IDNA_BENCHMARKS=ON -B build
cmake --build build
./build/benchmarks/to_ascii
```

The commands for users of Visual Studio are slightly different.

Sample result (LLVM 14, Apple M1 Max processor):

```
---------------------------------------------------------------------
Benchmark           Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------
Ada              1504 ns         1504 ns       440984 speed=48.5371M/s time/byte=20.6028ns time/domain=250.667ns url/s=3.98935M/s
Icu              1898 ns         1897 ns       369967 speed=38.4721M/s time/byte=25.9928ns time/url=316.246ns url/s=3.16209M/s
```

## License

This code is made available under the Apache License 2.0 as well as the MIT license.

Our tests include third-party code and data. The benchmarking code includes third-party code: it is provided for research purposes only and not part of the library.