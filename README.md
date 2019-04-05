libkasofs
==================
[![TravisCI][travis-shield]][travis-link]
[![License][license-shield]][license-link]

[travis-shield]: https://travis-ci.org/abbyssoul/libkasofs.png?branch=master
[travis-link]: https://travis-ci.org/abbyssoul/libkasofs
[license-shield]: https://img.shields.io/badge/License-Apache%202.0-blue.svg
[license-link]: https://opensource.org/licenses/Apache-2.0

Virtual File System

# Using this library
TBD


# Building

### Build tool dependencies
In order to build this project following tools must be present in the system:
* git (to check out project and it’s external modules, see dependencies section)
* doxygen (for documentation)
* cppcheck (static code analysis, latest version from git is used as part of the 'codecheck' step)
* cpplint (for static code analysis in addition to cppcheck)
* valgrind (for runtime code quality verification)

This project is using C++17 features extensively. The minimal tested/required version of gcc is gcc-7.
[CI](https://travis-ci.org/abbyssoul/libkasofs) is using clang-6 and gcc-7.
To install build tools on Debian based Linux distribution:
```shell
sudo apt-get update -qq
sudo apt-get install cmake git doxygen python-pip valgrind ggcov
sudo pip install cpplint
```

The library has single external dependency: [libsolace](https://github.com/abbyssoul/libsolace) managed via [conan package manager](https://conan.io/).
Make sure [conan is installed](https://docs.conan.io/en/latest/installation.html) on your system in order to build the project.


## Building the project
```shell
# In the project check-out directory:
# To build debug version with sanitizer enabled (recommended for development)
./configure --enable-debug --enable-sanitizer

# To build the library it self
make

# To build and run unit tests:
make test

# To run valgrind on test suit:
# Note: `valgrind` doesn’t work with ./configure --enable-sanitize option
make verify

# To build API documentation using doxygen:
make doc
```

To install locally for testing:
```shell
make --prefix=/user/home/<username>/test/lib install
```
To install system wide (as root):
```shell
make install
```
To run code quality check before submission of a patch:
```shell
# Verify code quality before submission
make codecheck
```

## License
Please see LICENSE file for details


## Authors
Please see AUTHORS file for the list of contributors
