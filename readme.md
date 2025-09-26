# CSBuild
A zero dependency, cross-platform build system for C++ projects.

## Features
- [x] **Zero Dependencies**: All you need is a C++20 compiler and Git.
- [ ] **Cross-Platform**: Works on both Windows and Linux.
- [x] **Simple Configuration**: Uses a `csb.cpp` file in the root for configuration.
- [x] **Automatic Incremental Builds**: Dependencies are tracked, and only affected steps are re-run.
- [ ] **Multi-Project Support**: Easily use other projects that use CSBuild as a build system.
- [ ] **Custom Build Steps**: Define custom build steps for specialized tasks.
- [x] **VCPKG Integration**: Integrates with VCPKG for package management.
- [x] **Clangd Integration**: Generates a `compile_commands.json` file for Clangd support.
- [x] **Clang Format Integration**: Integrates with Clang Format for code formatting.

## Requirements
- Windows or Linux OS.
- An environment with access to `cl` and `link` on Windows or `g++` on Linux.
- Git installed and available.

See the [test](test) directory for an example project linking to SDL3.
