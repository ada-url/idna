# Unicode IDNA
[![VS17-CI](https://github.com/ada-url/idna/actions/workflows/vs.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/vs.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)

The ada-url/ada library is a C++ library implementing the `to_array` and `to_unicode` functions from the [Unicode Technical Standard](https://www.unicode.org/reports/tr46/#ToUnicode) supporting a wide range of systems. It is suitable for URL parsing.

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

Sample result (LLVM 14, Apple M2 processor):

```
---------------------------------------------------------------------
Benchmark           Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------
Ada              1411 ns         1411 ns       491877 speed=50.3364M/s time/byte=19.8664ns time/domain=235.085ns url/s=4.25378M/s
Icu              2405 ns         2405 ns       299564 speed=29.5213M/s time/byte=33.8738ns time/url=400.84ns url/s=2.49476M/s
```

## Contributing

### Git hooks

Git hooks are recommended for following our `.clang-format` style.

1. Install [pre-commit](https://pre-commit.com/)

    Using [pip](https://www.w3schools.com/python/python_pip.asp):
    ```bash
    pip install pre-commit
    ```
    Using [homebrew](https://brew.sh/index_pt-br)
    ```bash
    brew install pre-commit
    ```

2. Use `pre-commit` to install local hooks

    Run the following command in the root of this repo
    ```bash
    pre-commit install
    ```

You are all set for the hooks!
