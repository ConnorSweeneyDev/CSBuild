# CSBuild
A zero dependency, cross-platform build system for C++ projects.

## Features
- **Zero Dependencies**: All you need is a C++ compiler and git.
- **Cross-Platform**: Works on both Windows and Linux.
- **Simple Configuration**: Uses a straightforward `csb.cpp` file in the root for configuration.
- **Automatic Incremental Builds**: Dependencies are automatically tracked, and only changed files are rebuilt.
- **Multi-Project Support**: Easily use other projects using CSBuild as dependencies.
- **Custom Build Steps**: Define custom build steps for specialized tasks.
- **VCPKG Integration**: Integrates with VCPKG for package management.
- **Clang Format Integration**: Integrates with Clang Format for code formatting.

## Requirements
- Windows or Linux operating system.
- `cl` and `link` on Windows or `g++` on Linux.
- `git` for managing dependencies.

See the [test](test) directory for an example project linking to SDL3.
