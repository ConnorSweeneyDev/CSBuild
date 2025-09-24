#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdlib>
#include <exception>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
  #include <stdio.h>
  #include <stdlib.h>
#endif

enum output
{
  EXECUTABLE,
  STATIC_LIBRARY,
  DYNAMIC_LIBRARY
};
enum standard
{
  CXX11 = 11,
  CXX14 = 14,
  CXX17 = 17,
  CXX20 = 20,
  CXX23 = 23
};
enum optimization
{
  O0 = 0,
  O1 = 1,
  O2 = 2,
};
enum warning
{
  W0 = 0,
  W1 = 1,
  W2 = 2,
  W3 = 3,
  W4 = 4
};
enum linkage
{
  STATIC,
  DYNAMIC
};
enum configuration
{
  RELEASE,
  DEBUG
};
enum console
{
  CONSOLE,
  WINDOW
};

namespace csb::utility
{
  inline std::string get_environment_variable(const std::string &name, const std::string &error_message)
  {
    char *value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name.c_str()) != 0 || !value)
      throw std::runtime_error(error_message + "\n" + name + " environment variable not found.");
    std::string result = std::string(value);
    free(value);
    if (result.empty()) throw std::runtime_error(name + " environment variable is empty.");
    return result;
  }

  inline void execute(const std::string &command,
                      std::function<void(const std::string &, const std::string &)> on_success = nullptr,
                      std::function<void(const std::string &, const int, const std::string &)> on_failure = nullptr)
  {
    FILE *pipe = _popen((command).c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to execute command: '" + command + "'.");
    char buffer[512];
    std::string result = {};
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    int return_code = _pclose(pipe);
    if (return_code != 0)
    {
      if (on_failure) on_failure(command, return_code, result);
      return;
    }
    if (on_success) on_success(command, result);
  }

  template <typename container_type>
  concept iterable = requires(container_type &type) {
    std::end(type);
    std::begin(type);
    requires std::same_as<std::remove_cvref_t<decltype(*std::begin(type))>, std::filesystem::path>;
  };
  template <iterable container_type>
  void multi_execute(const std::string &command, const container_type &container, const std::string &task_name,
                     std::function<void(const std::string &, const std::string &)> on_success = nullptr,
                     std::function<void(const std::string &, const int, const std::string &)> on_failure = nullptr)
  {
    std::vector<std::exception_ptr> exceptions = {};
    std::mutex exceptions_mutex = {};
    std::atomic<bool> should_stop = false;
    std::for_each(std::execution::par, container.begin(), container.end(),
                  [&](const auto &item)
                  {
                    std::string item_command = command;
                    auto replace_placeholder = [&](const std::string &placeholder, const std::string &replacement)
                    {
                      size_t pos = 0;
                      while ((pos = item_command.find(placeholder, pos)) != std::string::npos)
                      {
                        item_command.replace(pos, placeholder.length(), replacement);
                        pos += replacement.length();
                      }
                    };
                    replace_placeholder("[.string]", item.string());
                    replace_placeholder("[.stem.string]", item.stem().string());
                    FILE *pipe = _popen((item_command + " 2>&1").c_str(), "r");
                    if (!pipe)
                    {
                      std::lock_guard<std::mutex> lock(exceptions_mutex);
                      exceptions.push_back(std::make_exception_ptr(std::runtime_error(
                        std::format("{}: Failed to execute command: '{}'.", item.string(), item_command))));
                      return;
                    }
                    char buffer[512];
                    std::string result = {};
                    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
                    int return_code = _pclose(pipe);
                    if (return_code != 0)
                    {
                      should_stop = true;
                      if (on_failure) on_failure(item_command, return_code, result);
                    }
                    else if (on_success)
                    {
                      if (on_success) on_success(item_command, result);
                    }
                  });
    if (!exceptions.empty())
    {
      for (const auto &exception : exceptions) try
        {
          if (exception) std::rethrow_exception(exception);
        }
        catch (const std::exception &e)
        {
          std::cerr << e.what() << std::endl;
        }
      return;
    }
    if (should_stop) throw std::runtime_error(task_name + " errors occurred.");
  }

  inline void live_execute(const std::string &command, const std::string &error_message, bool print_command)
  {
    if (print_command) std::cout << command << std::endl;
    if (std::system(command.c_str()) != 0) throw std::runtime_error(error_message);
  }

  inline void bootstrap_vcpkg(const std::filesystem::path &vcpkg_path, const std::string &vcpkg_version)
  {
    if (std::filesystem::exists(vcpkg_path)) return;

    if (!std::filesystem::exists(vcpkg_path.parent_path()))
    {
      std::string command = "git clone https://github.com/microsoft/vcpkg.git " + vcpkg_path.parent_path().string();
      if (std::system(command.c_str()) != 0) throw std::runtime_error("Failed to clone vcpkg.");
      std::cout << "Checking out to vcpkg " + vcpkg_version + "..." << std::endl;
      if (std::system(("cd " + vcpkg_path.parent_path().string() + " && git -c advice.detachedHead=false checkout " +
                       vcpkg_version)
                        .c_str()) != 0)
        throw std::runtime_error("Failed to checkout vcpkg version.");
    }

    std::cout << "Bootstrapping vcpkg... ";
    utility::execute(
      "cd " + vcpkg_path.parent_path().string() + " && bootstrap-vcpkg.bat -disableMetrics",
      [](const std::string &, const std::string &result)
      {
        std::cout << "done." << std::endl;
        size_t start = result.find("https://");
        if (start != std::string::npos)
        {
          size_t end = result.find("...", start);
          if (end != std::string::npos) std::cout << result.substr(start, end - start) << std::endl;
        }
      },
      [](const std::string &, const int return_code, const std::string &result)
      {
        std::cerr << result << std::endl;
        throw std::runtime_error("Failed to bootstrap vcpkg. Return code: " + std::to_string(return_code));
      });
  }

  inline std::filesystem::path bootstrap_clang(const std::string &clang_version)
  {
    std::filesystem::path clang_path = std::format("build\\clang-{}", clang_version);
    if (std::filesystem::exists(clang_path)) return clang_path.string();

    std::string architecture = utility::get_environment_variable(
      "VSCMD_ARG_HOST_ARCH", "Ensure you are running from an environment with access to MSVC tools.");
    if (architecture != "x64" && architecture != "arm64")
      throw std::runtime_error("Clang bootstrap only supports 64 bit architectures.");
    std::string clang_architecture = architecture == "x64" ? "x86_64" : "aarch64";
    std::string url = std::format(
      "https://github.com/llvm/llvm-project/releases/download/llvmorg-{}/clang+llvm-{}-{}-pc-windows-msvc.tar.xz",
      clang_version, clang_version, clang_architecture);
    std::cout << "Downloading archive at '" + url + "'..." << std::endl;
    utility::live_execute(std::format("curl -f -L -o build\\temp.tar.xz {}", url), "Failed to download archive.",
                          false);
    std::cout << "Extracting archive... ";
    utility::live_execute("tar -xf build\\temp.tar.xz -C build", "Failed to extract archive.", false);
    std::filesystem::remove("build\\temp.tar.xz");
    std::filesystem::path extracted_path = std::format("build\\clang+llvm-{}-x86_64-pc-windows-msvc", clang_version);
    if (!std::filesystem::exists(extracted_path))
      throw std::runtime_error("Failed to find " + extracted_path.string() + ".");

    if (!std::filesystem::exists(clang_path)) std::filesystem::create_directories(clang_path);
    for (const auto &entry : std::filesystem::directory_iterator(extracted_path / "bin"))
      if (entry.is_regular_file()) std::filesystem::rename(entry.path(), clang_path / entry.path().filename());
    std::filesystem::remove_all(extracted_path);
    std::cout << "done.\n" << std::endl;

    return clang_path;
  }
}

namespace csb
{
  inline std::string name = "a";
  inline output output_type = EXECUTABLE;
  inline standard cxx_standard = CXX20;
  inline optimization optimization_level = O2;
  inline warning warning_level = W4;
  inline linkage linkage_type = STATIC;
  inline configuration build_configuration = RELEASE;
  inline console console_type = CONSOLE;

  inline std::set<std::filesystem::path> source_files = {};
  inline std::set<std::filesystem::path> include_directories = {};
  inline std::set<std::filesystem::path> external_include_directories = {};
  inline std::set<std::filesystem::path> library_directories = {};
  inline std::set<std::string> libraries = {};
  inline std::set<std::string> definitions = {};

  inline std::string clang_version = {};
  inline std::string vcpkg_version = {};
  struct vcpkg_output
  {
    std::filesystem::path include_directory = {};
    std::filesystem::path library_directory = {};
  };

  inline vcpkg_output fetch_vcpkg_dependencies()
  {
    if (vcpkg_version.empty()) throw std::runtime_error("vcpkg_version not set.");
    if (!std::filesystem::exists("vcpkg.json")) throw std::runtime_error("vcpkg.json not found.");

    std::filesystem::path vcpkg_path = "build\\vcpkg\\vcpkg.exe";
    utility::bootstrap_vcpkg(vcpkg_path, vcpkg_version);

    std::string architecture = utility::get_environment_variable(
      "VSCMD_ARG_HOST_ARCH", "Ensure you are running from an environment with access to MSVC tools.");
    std::string vcpkg_triplet = architecture + "-windows" + (linkage_type == STATIC ? "-static" : "") +
                                (build_configuration == RELEASE ? "-release" : "");
    std::filesystem::path vcpkg_installed_directory = "build\\vcpkg_installed";
    std::cout << "Using vcpkg triplet: " << vcpkg_triplet << std::endl;
    utility::live_execute(std::format("{} install --vcpkg-root {} --triplet {} --x-install-root {} | more",
                                      vcpkg_path.string(), vcpkg_path.parent_path().string(), vcpkg_triplet,
                                      vcpkg_installed_directory.string()),
                          "Failed to install vcpkg dependencies.", false);

    vcpkg_output outputs = {vcpkg_installed_directory / vcpkg_triplet / "include",
                            vcpkg_installed_directory / vcpkg_triplet /
                              (build_configuration == RELEASE ? "lib" : "debug/lib")};
    if (!std::filesystem::exists(outputs.include_directory) || !std::filesystem::exists(outputs.library_directory))
      throw std::runtime_error("vcpkg outputs not found.");
    return outputs;
  }

  inline void clang_format()
  {
    if (clang_version.empty()) throw std::runtime_error("clang_version not set.");
    if (source_files.empty() && include_directories.empty()) throw std::runtime_error("No files to format.");

    std::filesystem::path clang_path = utility::bootstrap_clang(clang_version);
    std::filesystem::path clang_format_path = clang_path / "clang-format.exe";

    std::set<std::filesystem::path> format_files = source_files;
    for (auto iterator = include_directories.begin(); iterator != include_directories.end(); ++iterator)
      for (const auto &entry : std::filesystem::recursive_directory_iterator(*iterator))
        if (entry.is_regular_file() && (entry.path().extension() == ".hpp" || entry.path().extension() == ".inl"))
          format_files.insert(entry.path());

    utility::multi_execute(
      std::format("{} -i \"[.string]\"", clang_format_path.string()), format_files, "Formatting",
      [](const std::string &item_command, const std::string result)
      { std::cout << item_command + "\n" + result + "\n"; },
      [](const std::string item_command, const int return_code, const std::string &result)
      { std::cerr << item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n"; });
  }

  inline void generate_compile_commands()
  {
    if (source_files.empty()) throw std::runtime_error("No source files to compile.");

    auto escape_backslashes = [](const std::string &string) -> std::string
    {
      std::string result;
      for (char character : string)
      {
        if (character == '\\')
          result += "\\\\";
        else
          result += character;
      }
      return result;
    };

    std::filesystem::path compile_commands_path = "compile_commands.json";
    std::string build_directory = build_configuration == RELEASE ? "build\\release\\" : "build\\debug\\";

    std::string content = std::format(
      "[\n  {{\n    \"directory\": \"{}\",\n    \"file\": \"{}\",\n    \"command\": \"clang++ -std=c++{} "
      "-Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align "
      "-Wfloat-equal -Wparentheses -Wunreachable-code-aggressive -Wformat=2\"\n  }},\n",
      escape_backslashes(std::filesystem::current_path().string()),
      escape_backslashes((std::filesystem::current_path() / std::filesystem::path("csb.cpp")).string()),
      cxx_standard <= CXX20 ? "20" : std::to_string(cxx_standard));
    for (auto iterator = source_files.begin(); iterator != source_files.end();)
    {
      content += "  {\n";
      content +=
        std::format("    \"directory\": \"{}\",\n", escape_backslashes(std::filesystem::current_path().string()));
      content +=
        std::format("    \"file\": \"{}\",\n", escape_backslashes(std::filesystem::absolute(*iterator).string()));
      content += std::format(
        "    \"command\": \"clang++ -std=c++{} -Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated "
        "-Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal -Wparentheses -Wunreachable-code-aggressive -Wformat=2 "
        "-DWIN32 -D_WINDOWS ",
        std::to_string(cxx_standard));
      for (const auto &definition : definitions) content += std::format("-D{} ", definition);
      for (const auto &directory : include_directories)
        content += std::format("-I\\\"{}\\\" ", escape_backslashes(directory.string()));
      for (const auto &directory : external_include_directories)
        content += std::format("-isystem\\\"{}\\\" ", escape_backslashes(directory.string()));
      content += std::format(
        "-c \\\"{}\\\" -o \\\"{}\\\"\"\n", escape_backslashes(std::filesystem::absolute(*iterator).string()),
        escape_backslashes((std::filesystem::absolute(build_directory) / (iterator->stem().string() + ".o")).string()));
      content += "  }";
      if (++iterator != source_files.end())
        content += ",\n";
      else
        content += "\n";
    }
    content += "]\n";

    std::ofstream compile_commands_file(compile_commands_path);
    if (!compile_commands_file.is_open())
      throw std::runtime_error("Failed to open compile_commands.json file for writing.");
    compile_commands_file << content;
    compile_commands_file.close();
    std::cout << "Generated " << compile_commands_path.string() + "\n" << std::endl;
  }

  inline void build()
  {
    if (name.empty()) throw std::runtime_error("Executable name not set.");
    if (source_files.empty()) throw std::runtime_error("No source files to compile.");

    std::string build_directory = build_configuration == RELEASE ? "build\\release\\" : "build\\debug\\";
    if (!std::filesystem::exists(build_directory)) std::filesystem::create_directories(build_directory);

    std::string compile_debug_flags = build_configuration == RELEASE ? "" : "/Zi /RTC1 ";
    std::string runtime_library = linkage_type == STATIC ? (build_configuration == RELEASE ? "MT" : "MTd")
                                                         : (build_configuration == RELEASE ? "MD" : "MDd");
    std::string optimization_option = optimization_level == O0 ? "d" : std::to_string(optimization_level);
    std::string compile_definitions = {};
    for (const auto &definition : definitions) compile_definitions += std::format("/D{} ", definition);
    std::string compile_include_directories = {};
    for (const auto &directory : include_directories)
      compile_include_directories += std::format("/I\"{}\" ", directory.string());
    std::string compile_external_include_directories = {};
    for (const auto &directory : external_include_directories)
      compile_external_include_directories += std::format("/external:I\"{}\" ", directory.string());
    utility::multi_execute(
      std::format(
        "cl /nologo /std:c++{} /O{} /W{} /external:W0 /EHsc /MP /{} /ifcOutput{} /Fo{} /Fd{}[.stem.string].pdb "
        "/sourceDependencies{}[.stem.string].d.json {}/DWIN32 /D_WINDOWS {}{}{}/c \"[.string]\"",
        std::to_string(cxx_standard), optimization_option, std::to_string(warning_level), runtime_library,
        build_directory, build_directory, build_directory, build_directory, compile_debug_flags, compile_definitions,
        compile_include_directories, compile_external_include_directories),
      source_files, "Compilation", [](const std::string &item_command, const std::string result)
      { std::cout << item_command + "\n" + result + "\n"; },
      [](const std::string item_command, const int return_code, const std::string &result)
      { std::cerr << item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n"; });

    std::string architecture = utility::get_environment_variable(
      "VSCMD_ARG_TGT_ARCH", "Ensure you are running from an environment with access to MSVC tools.");
    std::string executable_option = output_type == STATIC_LIBRARY ? "lib" : "link";
    std::string console_option = console_type == CONSOLE ? "CONSOLE" : "WINDOWS";
    std::string link_debug_flags = build_configuration == RELEASE  ? ""
                                   : output_type == STATIC_LIBRARY ? ""
                                                                   : "/DEBUG:FULL ";
    std::string dynamic_flags = output_type == DYNAMIC_LIBRARY ? "/DLL /MANIFEST:EMBED /INCREMENTAL:NO "
                                : output_type == EXECUTABLE    ? "/MANIFEST:EMBED /INCREMENTAL:NO "
                                                               : "";
    std::string extra_flags = output_type == DYNAMIC_LIBRARY ? std::format("/PDB:{}{}.pdb /IMPLIB:{}{}.lib ",
                                                                           build_directory, name, build_directory, name)
                              : output_type == EXECUTABLE    ? std::format("/PDB:{}{}.pdb ", build_directory, name)
                                                             : "";
    std::string extension = output_type == STATIC_LIBRARY ? "lib" : output_type == DYNAMIC_LIBRARY ? "dll" : "exe";
    std::string link_library_directories = {};
    for (const auto &directory : library_directories)
      link_library_directories += std::format("/LIBPATH:\"{}\" ", directory.string());
    std::string link_libraries = {};
    for (const auto &library : libraries) link_libraries += std::format("{}.lib ", library);
    std::string link_objects = {};
    for (const auto &source_file : source_files)
      link_objects += std::format("{}{}.obj ", build_directory, source_file.stem().string());
    utility::execute(
      std::format("{} /NOLOGO /MACHINE:{} {}/SUBSYSTEM:{} {}{}{}{}{}/OUT:{}{}.{}", executable_option, architecture,
                  dynamic_flags, console_option, link_debug_flags, link_library_directories, link_libraries,
                  link_objects, extra_flags, build_directory, name, extension),
      [&](const std::string &command, const std::string &result) { std::cout << command + "\n" + result; },
      [&](const std::string &command, const int return_code, const std::string &result)
      {
        std::cerr << command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        throw std::runtime_error("Linking errors occurred.");
      });
  }
}

#define CSB_MAIN()                                                                                                     \
  int main()                                                                                                           \
  {                                                                                                                    \
    const std::string error_message = "Ensure you are running from an environment with access to MSVC tools.";         \
    const std::string architecture = csb::utility::get_environment_variable("VSCMD_ARG_HOST_ARCH", error_message);     \
    const std::string vs_path = csb::utility::get_environment_variable("VSINSTALLDIR", error_message);                 \
    const std::string toolset_version = csb::utility::get_environment_variable("VCToolsVersion", error_message);       \
    const std::string sdk_version = csb::utility::get_environment_variable("WindowsSDKVersion", error_message);        \
    std::cout << std::format("Architecture: {}\nVisual Studio: {}\nToolset: {}\nWindows SDK: {}\n", architecture,      \
                             vs_path, toolset_version, sdk_version)                                                    \
              << std::endl;                                                                                            \
    try                                                                                                                \
    {                                                                                                                  \
      return csb_main();                                                                                               \
    }                                                                                                                  \
    catch (const std::exception &e)                                                                                    \
    {                                                                                                                  \
      std::cerr << "Error: " << e.what() << std::endl;                                                                 \
      return EXIT_FAILURE;                                                                                             \
    }                                                                                                                  \
  }
