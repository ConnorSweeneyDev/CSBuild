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
#include <iterator>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
  #include <stdio.h>
  #include <stdlib.h>
#endif

enum artifact
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
enum subsystem
{
  CONSOLE,
  WINDOWS
};

namespace csb::utility
{
  struct internal_state
  {
    std::string architecture = {};
  } inline state = {};

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

  inline std::string path_placeholder_replace(const std::filesystem::path &path, const std::string &placeholder)
  {
    std::string result = placeholder;
    size_t pos = 0;

    while ((pos = result.find("[[", pos)) != std::string::npos)
    {
      result.replace(pos, 2, "[");
      pos += 1;
    }
    pos = 0;
    while ((pos = result.find("]]", pos)) != std::string::npos)
    {
      result.replace(pos, 2, "]");
      pos += 1;
    }

    pos = 0;
    while ((pos = result.find("[", pos)) != std::string::npos)
    {
      size_t end_pos = result.find("]", pos);
      if (end_pos == std::string::npos) break;

      std::string placeholder_content = result.substr(pos + 1, end_pos - pos - 1);
      std::filesystem::path current_path = path;

      if (!placeholder_content.empty())
      {
        size_t method_start = 0;
        size_t method_end = 0;
        while (method_end != std::string::npos)
        {
          method_end = placeholder_content.find(".", method_start);
          std::string method = placeholder_content.substr(
            method_start, method_end == std::string::npos ? std::string::npos : method_end - method_start);
          if (!method.empty())
          {
            if (method == "filename")
              current_path = current_path.filename();
            else if (method == "stem")
              current_path = current_path.stem();
            else if (method == "extension")
              current_path = current_path.extension();
            else if (method == "parent_path")
              current_path = current_path.parent_path();
          }
          method_start = method_end + 1;
        }
      }

      std::string replacement = current_path.string();
      result.replace(pos, end_pos - pos + 1, replacement);
      pos += replacement.length();
    }
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
                    std::string item_command = path_placeholder_replace(item, command);
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
          should_stop = true;
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

  inline void touch(const std::filesystem::path &path)
  {
    if (!std::filesystem::exists(path.parent_path())) std::filesystem::create_directories(path.parent_path());
    if (std::filesystem::exists(path)) std::filesystem::remove(path);
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) throw std::runtime_error("Failed to touch file: " + path.string());
    file.close();
  }

  inline std::vector<std::filesystem::path> find_modified_files(
    const std::vector<std::filesystem::path> &target_files, const std::vector<std::string> &check_files,
    const std::function<bool(const std::filesystem::path &, const std::vector<std::filesystem::path> &)>
      &dependency_handler = nullptr)
  {
    std::vector<std::filesystem::path> modified_files = {};
    for (const auto &target_file : target_files)
    {
      std::vector<std::filesystem::path> valid_files;
      bool any_missing = false;
      for (const auto &check_file : check_files)
      {
        std::filesystem::path check_path = path_placeholder_replace(target_file, check_file);
        if (!std::filesystem::exists(check_path))
        {
          any_missing = true;
          break;
        }
        valid_files.push_back(check_path);
      }
      if (any_missing)
      {
        modified_files.push_back(target_file);
        continue;
      }

      std::filesystem::file_time_type csb_header_time;
      if (!std::filesystem::exists("csb.hpp"))
        csb_header_time = std::filesystem::file_time_type::min();
      else
        csb_header_time = std::filesystem::last_write_time("csb.hpp");
      std::filesystem::file_time_type csb_source_time;
      if (!std::filesystem::exists("csb.cpp"))
        csb_source_time = std::filesystem::file_time_type::min();
      else
        csb_source_time = std::filesystem::last_write_time("csb.cpp");
      auto source_time = std::filesystem::last_write_time(target_file);

      bool needs_rebuild = false;
      for (const auto &file : valid_files)
      {
        auto time = std::filesystem::last_write_time(file);
        if (source_time > time || csb_header_time > time || csb_source_time > time)
        {
          needs_rebuild = true;
          break;
        }
      }
      if (needs_rebuild)
      {
        modified_files.push_back(target_file);
        continue;
      }

      if (dependency_handler)
      {
        try
        {
          if (dependency_handler(target_file, valid_files)) modified_files.push_back(target_file);
        }
        catch (const std::exception &)
        {
          modified_files.push_back(target_file);
        }
      }
    }
    return modified_files;
  }

  inline std::filesystem::path bootstrap_vcpkg(const std::string &vcpkg_version)
  {
    bool needs_bootstrap = false;

    std::filesystem::path vcpkg_path = std::format("build\\vcpkg-{}\\vcpkg.exe", vcpkg_version);
    if (!std::filesystem::exists(vcpkg_path.parent_path()))
    {
      needs_bootstrap = true;
      live_execute("git clone https://github.com/microsoft/vcpkg.git " + vcpkg_path.parent_path().string(),
                   "Failed to clone vcpkg.", false);
    }

    std::string current_hash = {};
    execute(
      std::format("cd {} && git rev-parse HEAD", vcpkg_path.parent_path().string()),
      [&](const std::string &, const std::string &result)
      {
        current_hash = result;
        current_hash.erase(std::remove(current_hash.begin(), current_hash.end(), '\n'), current_hash.end());
      },
      [](const std::string &, const int return_code, const std::string &result)
      {
        std::cerr << result << std::endl;
        throw std::runtime_error("Failed to get vcpkg current version. Return code: " + std::to_string(return_code));
      });
    std::string target_hash = {};
    execute(
      std::format("cd {} && git rev-parse {}", vcpkg_path.parent_path().string(), vcpkg_version),
      [&](const std::string &, const std::string &result)
      {
        target_hash = result;
        target_hash.erase(std::remove(target_hash.begin(), target_hash.end(), '\n'), target_hash.end());
      },
      [](const std::string &, const int return_code, const std::string &result)
      {
        std::cerr << result << std::endl;
        throw std::runtime_error("Failed to get vcpkg target version. Return code: " + std::to_string(return_code));
      });
    if (current_hash != target_hash)
    {
      needs_bootstrap = true;
      std::cout << "Checking out to vcpkg " + vcpkg_version + "..." << std::endl;
      live_execute("cd " + vcpkg_path.parent_path().string() + " && git -c advice.detachedHead=false checkout " +
                     vcpkg_version,
                   "Failed to checkout vcpkg version.", false);
    }

    if (!needs_bootstrap)
    {
      std::cout << "Using vcpkg version: " + vcpkg_version << std::endl;
      return vcpkg_path;
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

    if (!std::filesystem::exists(vcpkg_path)) throw std::runtime_error("Failed to find " + vcpkg_path.string() + ".");
    return vcpkg_path;
  }

  inline std::filesystem::path bootstrap_clang(const std::string &clang_version)
  {
    std::filesystem::path clang_path = std::format("build\\clang-{}", clang_version);
    if (std::filesystem::exists(clang_path)) return clang_path;
    std::cout << std::endl;

    if (state.architecture != "x64" && state.architecture != "arm64")
      throw std::runtime_error("Clang bootstrap only supports 64 bit architectures.");
    std::string clang_architecture = state.architecture == "x64" ? "x86_64" : "aarch64";
    std::string url = std::format(
      "https://github.com/llvm/llvm-project/releases/download/llvmorg-{}/clang+llvm-{}-{}-pc-windows-msvc.tar.xz",
      clang_version, clang_version, clang_architecture);
    std::cout << "Downloading archive at '" + url + "'..." << std::endl;
    utility::live_execute(std::format("curl -f -L -C - -o build\\temp.tar.xz {}", url), "Failed to download archive.",
                          false);
    std::cout << "Extracting archive... ";
    utility::live_execute("tar -xf build\\temp.tar.xz -C build", "Failed to extract archive.", false);
    std::filesystem::remove("build\\temp.tar.xz");
    std::filesystem::path extracted_path =
      std::format("build\\clang+llvm-{}-{}-pc-windows-msvc", clang_version, clang_architecture);
    if (!std::filesystem::exists(extracted_path))
      throw std::runtime_error("Failed to find " + extracted_path.string() + ".");

    if (!std::filesystem::exists(clang_path)) std::filesystem::create_directories(clang_path);
    for (const auto &entry : std::filesystem::directory_iterator(extracted_path / "bin"))
      if (entry.is_regular_file()) std::filesystem::rename(entry.path(), clang_path / entry.path().filename());
    std::filesystem::remove_all(extracted_path);
    std::cout << "done." << std::endl;

    if (!std::filesystem::exists(clang_path)) throw std::runtime_error("Failed to find " + clang_path.string() + ".");
    return clang_path;
  }
}

namespace csb
{
  inline std::string target_name = "a";
  inline artifact target_artifact = EXECUTABLE;
  inline linkage target_linkage = STATIC;
  inline subsystem target_subsystem = CONSOLE;
  inline configuration target_configuration = RELEASE;
  inline standard cxx_standard = CXX20;
  inline warning warning_level = W4;

  inline std::vector<std::filesystem::path> include_files = {};
  inline std::vector<std::filesystem::path> source_files = {};
  inline std::vector<std::filesystem::path> external_include_directories = {};
  inline std::vector<std::filesystem::path> library_directories = {};
  inline std::vector<std::string> libraries = {};
  inline std::vector<std::string> definitions = {};

  inline std::string clang_version = {};

  inline std::vector<std::filesystem::path> files_from(const std::filesystem::path &directory,
                                                       const std::set<std::string> &extensions, bool recursive = true)
  {
    std::vector<std::filesystem::path> files = {};
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
      throw std::runtime_error("Directory does not exist: " + directory.string());
    if (recursive)
    {
      for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
        if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
          files.push_back(entry.path().string());
      return files;
    }
    for (const auto &entry : std::filesystem::directory_iterator(directory))
      if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
        files.push_back(entry.path().string());
    return files;
  }

  inline void vcpkg_install(const std::string &vcpkg_version)
  {
    if (vcpkg_version.empty()) throw std::runtime_error("vcpkg_version not set.");
    std::cout << std::endl;

    std::filesystem::path vcpkg_path = utility::bootstrap_vcpkg(vcpkg_version);

    std::string vcpkg_triplet = utility::state.architecture + "-windows" + (target_linkage == STATIC ? "-static" : "") +
                                (target_configuration == RELEASE ? "-release" : "");
    std::filesystem::path vcpkg_installed_directory = "build\\vcpkg_installed";
    std::cout << "Using vcpkg triplet: " << vcpkg_triplet << std::endl;
    utility::live_execute(std::format("{} install --vcpkg-root {} --triplet {} --x-install-root {}",
                                      vcpkg_path.string(), vcpkg_path.parent_path().string(), vcpkg_triplet,
                                      vcpkg_installed_directory.string()),
                          "Failed to install vcpkg dependencies.", false);

    std::pair<std::filesystem::path, std::filesystem::path> outputs = {
      vcpkg_installed_directory / vcpkg_triplet / "include",
      vcpkg_installed_directory / vcpkg_triplet / (target_configuration == RELEASE ? "lib" : "debug\\lib")};
    if (!std::filesystem::exists(outputs.first) || !std::filesystem::exists(outputs.second))
      throw std::runtime_error("vcpkg outputs not found.");
    external_include_directories.push_back(outputs.first);
    library_directories.push_back(outputs.second);
  }

  inline void clang_compile_commands()
  {
    if (clang_version.empty()) throw std::runtime_error("clang_version not set.");
    if (source_files.empty()) throw std::runtime_error("No source files to generate compile commands for.");

    utility::bootstrap_clang(clang_version);

    std::cout << std::endl;
    std::cout << "Generating compile_commands.json... ";

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
    std::string build_directory = target_configuration == RELEASE ? "build\\release\\" : "build\\debug\\";
    std::string content =
      std::format("[\n  {{\n    \"directory\": \"{}\",\n    \"file\": \"{}\",\n    \"command\": \"clang++ -std=c++{} "
                  "-Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated -Wtype-limits -Wcast-qual "
                  "-Wcast-align -Wfloat-equal -Wunreachable-code-aggressive -Wformat=2\"\n  }},\n",
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
      content += std::format("    \"command\": \"clang++ -std=c++{} -Wall -Wextra -Wpedantic -Wconversion -Wshadow-all "
                             "-Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal "
                             "-Wunreachable-code-aggressive -Wformat=2 -DWIN32 -D_WINDOWS ",
                             std::to_string(cxx_standard));
      for (const auto &definition : definitions) content += std::format("-D{} ", definition);
      std::vector<std::filesystem::path> include_directories = {};
      for (const auto &include_file : include_files)
      {
        if (include_file.has_parent_path() && std::find(include_directories.begin(), include_directories.end(),
                                                        include_file.parent_path()) == include_directories.end())
          include_directories.push_back(include_file.parent_path());
      }
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
    std::cout << "done." << std::endl;
  }

  inline void clang_format()
  {
    if (clang_version.empty()) throw std::runtime_error("clang_version not set.");
    if (source_files.empty() && include_files.empty()) throw std::runtime_error("No files to format.");

    std::filesystem::path format_directory = "build\\format\\";
    if (!std::filesystem::exists(format_directory)) std::filesystem::create_directories(format_directory);

    std::filesystem::path clang_path = utility::bootstrap_clang(clang_version);
    std::filesystem::path clang_format_path = clang_path / "clang-format.exe";

    std::vector<std::filesystem::path> format_files = {};
    format_files.reserve(source_files.size() + include_files.size());
    format_files.insert(format_files.end(), source_files.begin(), source_files.end());
    format_files.insert(format_files.end(), include_files.begin(), include_files.end());

    auto modified_files =
      utility::find_modified_files(format_files, {format_directory.string() + "[.filename].formatted"});
    utility::multi_execute(
      std::format("{} -i \"[]\"", clang_format_path.string()), modified_files, "Formatting",
      [&](const std::string &item_command, const std::string &result)
      {
        std::cout << "\n" + item_command + "\n" + result;
        std::filesystem::path item_path = item_command.substr(item_command.find("\"") + 1);
        item_path = item_path.string().substr(0, item_path.string().rfind("\""));
        utility::touch(format_directory / item_path.filename().string().append(".formatted"));
      },
      [](const std::string &item_command, const int return_code, const std::string &result)
      { std::cerr << item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n"; });
  }

  inline void build()
  {
    if (target_name.empty()) throw std::runtime_error("Executable name not set.");
    if (source_files.empty()) throw std::runtime_error("No source files to compile.");

    std::filesystem::path build_directory = target_configuration == RELEASE ? "build\\release\\" : "build\\debug\\";
    if (!std::filesystem::exists(build_directory)) std::filesystem::create_directories(build_directory);

    std::string compile_debug_flags = target_configuration == RELEASE ? "/O2 " : "/Od /Zi /RTC1 ";
    std::string runtime_library = target_linkage == STATIC ? (target_configuration == RELEASE ? "MT" : "MTd")
                                                           : (target_configuration == RELEASE ? "MD" : "MDd");
    std::string compile_definitions = {};
    for (const auto &definition : definitions) compile_definitions += std::format("/D{} ", definition);
    std::vector<std::filesystem::path> include_directories = {};
    for (const auto &include_file : include_files)
    {
      if (include_file.has_parent_path() && std::find(include_directories.begin(), include_directories.end(),
                                                      include_file.parent_path()) == include_directories.end())
        include_directories.push_back(include_file.parent_path());
    }
    std::string compile_include_directories = {};
    for (const auto &directory : include_directories)
      compile_include_directories += std::format("/I\"{}\" ", directory.string());
    std::string compile_external_include_directories = {};
    for (const auto &directory : external_include_directories)
      compile_external_include_directories += std::format("/external:I\"{}\" ", directory.string());

    std::vector<std::string> check_files = {build_directory.string() + "[.filename.stem].obj",
                                            build_directory.string() + "[.filename.stem].d"};
    if (target_configuration == DEBUG) check_files.push_back(build_directory.string() + "[.filename.stem].pdb");
    auto modified_files = utility::find_modified_files(
      source_files, check_files,
      [](const std::filesystem::path &, const std::vector<std::filesystem::path> &checked_files) -> bool
      {
        auto object_time = std::filesystem::last_write_time(checked_files[0]);
        std::filesystem::path dependency_path = checked_files[1];

        std::ifstream dependency_file(dependency_path);
        if (!dependency_file.is_open()) return true;
        std::string json_content((std::istreambuf_iterator<char>(dependency_file)), std::istreambuf_iterator<char>());
        dependency_file.close();

        size_t includes_start = json_content.find("\"Includes\": [");
        if (includes_start == std::string::npos) return true;
        size_t includes_end = json_content.find("]", includes_start);
        if (includes_end == std::string::npos) return true;

        std::string includes_section = json_content.substr(includes_start + 13, includes_end - includes_start - 13);
        size_t pos = 0;
        while ((pos = includes_section.find("\"", pos)) != std::string::npos)
        {
          size_t start = pos + 1;
          size_t end = includes_section.find("\"", start);
          if (end == std::string::npos) break;
          std::filesystem::path include_path = includes_section.substr(start, end - start);
          if (std::filesystem::exists(include_path))
          {
            auto include_time = std::filesystem::last_write_time(include_path);
            if (include_time > object_time) return true;
          }
          pos = end + 1;
        }
        return false;
      });
    utility::multi_execute(
      std::format("cl /nologo /std:c++{} /W{} /external:W0 {}/EHsc /MP /{} /DWIN32 /D_WINDOWS {}/ifcOutput{} /Fo{} "
                  "/Fd{}[.stem].pdb /sourceDependencies{}[.stem].d {}{}/c \"[]\"",
                  std::to_string(cxx_standard), std::to_string(warning_level), compile_debug_flags, runtime_library,
                  compile_definitions, build_directory.string(), build_directory.string(), build_directory.string(),
                  build_directory.string(), compile_include_directories, compile_external_include_directories),
      modified_files, "Compilation", [](const std::string &item_command, const std::string &result)
      { std::cout << "\n" + item_command + "\n" + result; },
      [](const std::string &item_command, const int return_code, const std::string &result)
      { std::cerr << item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n"; });

    std::string executable_option = target_artifact == STATIC_LIBRARY ? "lib" : "link";
    std::string console_option = target_subsystem == CONSOLE ? "CONSOLE" : "WINDOWS";
    std::string link_debug_flags = target_configuration == RELEASE     ? ""
                                   : target_artifact == STATIC_LIBRARY ? ""
                                                                       : "/DEBUG:FULL ";
    std::string dynamic_flags = target_artifact == DYNAMIC_LIBRARY ? "/DLL /MANIFEST:EMBED /INCREMENTAL:NO "
                                : target_artifact == EXECUTABLE    ? "/MANIFEST:EMBED /INCREMENTAL:NO "
                                                                   : "";
    std::string output_flags =
      target_artifact == DYNAMIC_LIBRARY
        ? (target_configuration == RELEASE ? std::format("/IMPLIB:{}{}.lib ", build_directory.string(), target_name)
                                           : std::format("/PDB:{}{}.pdb /IMPLIB:{}{}.lib ", build_directory.string(),
                                                         target_name, build_directory.string(), target_name))
      : target_artifact == EXECUTABLE
        ? (target_configuration == RELEASE ? "" : std::format("/PDB:{}{}.pdb ", build_directory.string(), target_name))
        : "";
    std::string extension = target_artifact == STATIC_LIBRARY    ? "lib"
                            : target_artifact == DYNAMIC_LIBRARY ? "dll"
                                                                 : "exe";
    std::string link_library_directories = {};
    for (const auto &directory : library_directories)
      link_library_directories += std::format("/LIBPATH:\"{}\" ", directory.string());
    std::string link_libraries = {};
    for (const auto &library : libraries) link_libraries += std::format("{}.lib ", library);
    std::string link_objects = {};
    for (const auto &source_file : source_files)
      link_objects += std::format("{}{}.obj ", build_directory.string(), source_file.stem().string());

    if (target_artifact == STATIC_LIBRARY &&
        std::filesystem::exists(build_directory.string() + target_name + "." + extension) && modified_files.empty())
      return;
    if ((target_artifact == DYNAMIC_LIBRARY || target_artifact == EXECUTABLE) && target_configuration == DEBUG &&
        (std::filesystem::exists(build_directory.string() + target_name + "." + extension) &&
         std::filesystem::exists(build_directory.string() + target_name + ".pdb")) &&
        modified_files.empty())
      return;
    if ((target_artifact == DYNAMIC_LIBRARY || target_artifact == EXECUTABLE) && target_configuration == RELEASE &&
        (std::filesystem::exists(build_directory.string() + target_name + "." + extension)) && modified_files.empty())
      return;
    utility::execute(
      std::format("{} /NOLOGO /MACHINE:{} {}/SUBSYSTEM:{} {}{}{}{}{}/OUT:{}{}.{}", executable_option,
                  utility::state.architecture, dynamic_flags, console_option, link_debug_flags,
                  link_library_directories, link_libraries, link_objects, output_flags, build_directory.string(),
                  target_name, extension),
      [&](const std::string &command, const std::string &result) { std::cout << "\n" + command + "\n" + result; },
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
    const std::string vs_path = csb::utility::get_environment_variable("VSINSTALLDIR", error_message);                 \
    const std::string toolset_version = csb::utility::get_environment_variable("VCToolsVersion", error_message);       \
    const std::string sdk_version = csb::utility::get_environment_variable("WindowsSDKVersion", error_message);        \
    csb::utility::state.architecture = csb::utility::get_environment_variable("VSCMD_ARG_HOST_ARCH", error_message);   \
    std::cout << std::format("Visual Studio: {}\nToolset: {}\nWindows SDK: {}\nArchitecture: {}", vs_path,             \
                             toolset_version, sdk_version, csb::utility::state.architecture)                           \
              << std::endl;                                                                                            \
    try                                                                                                                \
    {                                                                                                                  \
      return csb_main();                                                                                               \
    }                                                                                                                  \
    catch (const std::exception &exception)                                                                            \
    {                                                                                                                  \
      std::cerr << "Error: " << exception.what() << std::endl;                                                         \
      return EXIT_FAILURE;                                                                                             \
    }                                                                                                                  \
  }
