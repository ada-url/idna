# Unicode IDNA
[![VS17-CI](https://github.com/ada-url/idna/actions/workflows/vs.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/vs.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)
[![Alpine Linux](https://github.com/ada-url/idna/actions/workflows/alpine.yml/badge.svg)](https://github.com/ada-url/idna/actions/workflows/alpine.yml)

The ada-url/ada library is a C++ library implementing the `to_array` and `to_unicode` functions from the [Unicode Technical Standard](https://www.unicode.org/reports/tr46/#ToUnicode) supporting a wide range of systems. It is suitable for URL parsing.

## Requirements

- A recent C++ compiler supporting C++17. We test GCC 9 or better, LLVM 10 or better and Microsoft Visual Studio 2022.

## Usage

```C++
    std::string_view input = // non-empty UTF-8 string, must be percent decoded
    std::string idna_ascii = ada::idna::to_ascii(input);
    if(idna_ascii.empty()) {
        // There was an error.
    }
```

## Contributing

### Git hooks

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
