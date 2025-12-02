# CSBuild
A zero dependency, cross-platform build system for C++ projects.

## Features
- **Zero Dependencies**: All you need is a C++20 compiler and Git.
- **Cross-Platform**: Works on both Windows and Linux.
- **Automatic Incremental Builds**: Dependencies are tracked, and only affected steps are re-run.
- **Simple Configuration**: Uses a `csb.cpp` file in the root for configuration.
- **Custom Build Steps**: Define custom build steps for specialized tasks.
- **Multi-Project Support**: Easily use other projects that use CSBuild as a build system.
- **Archive Support**: Easily download and extract archives as part of the build process.
- **Embed Support**: Easily embed resources into compilation units.
- **Json Support**: Easily read from and write to JSON files.
- **Pixel Data Support**: Easily read pixel data from PNG files.
- **VCPKG Integration**: Integrates with VCPKG for package management.
- **Clangd Integration**: Generates a `compile_commands.json` file for clangd support.
- **Clang Format Integration**: Integrates with Clang Format for code formatting.

## Requirements
- Windows or Linux OS.
- An environment with access to `cl`, `link` and `lib` on Windows or `gcc`, `g++` and `ar` on Linux.
- An environment with access to `git`.

## Usage
You can clone `https://github.com/ConnorSweeneyDev/CSTemplate` for a barebones template project.\
For advanced usage, refer to [CSEngine](https://github.com/ConnorSweeneyDev/CSEngine) as an example.
