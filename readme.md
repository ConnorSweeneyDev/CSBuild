# CSBuild
A zero dependency, cross-platform build system for C++ projects.

## Features
- **Zero Dependencies**: All you need is a C++ compiler and Git.
- **Cross-Platform**: Works on both Windows and Linux.
- **Simple Configuration**: Uses a `csb.cpp` file in the root for configuration.
- **Automatic Incremental Builds**: Dependencies are tracked, and only changed files are formatted and built.
- **Multi-Project Support**: Easily use other projects that use CSBuild as a build system.
- **Custom Build Steps**: Define custom build steps for specialized tasks.
- **VCPKG Integration**: Integrates with VCPKG for package management.
- **Clangd Integration**: Generates a `compile_commands.json` file for Clangd support.
- **Clang Format Integration**: Integrates with Clang Format for code formatting.

## Requirements
- Windows or Linux OS.
- An environment with access to `cl` and `link` on Windows or `g++` on Linux.
- Git installed and available.

See the [test](test) directory for an example project linking to SDL3.
