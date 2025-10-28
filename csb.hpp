// Version 1.1.6

#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <concepts>
#include <cstdio>
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
#include <optional>
#include <set>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

enum platform
{
  UNDEFINED,
  WINDOWS,
  LINUX
};

#if defined(_WIN32)

  #if defined(_M_X64) || defined(__amd64__)
    #define ARCHITECTURE "x64"
  #elif defined(_M_IX86)
    #define ARCHITECTURE "x86"
  #elif defined(_M_ARM64)
    #define ARCHITECTURE "arm64"
  #elif defined(_M_ARM)
    #define ARCHITECTURE "arm"
  #else
    #define ARCHITECTURE "unknown"
  #endif
  #define PLATFORM WINDOWS

inline std::string get_env(const std::string &name, const std::string &error_message)
{
  char *value = nullptr;
  size_t len = 0;
  if (_dupenv_s(&value, &len, name.c_str()) != 0)
    throw std::runtime_error(error_message + "\n" + name + " environment variable not found.");
  std::string result = {};
  if (value) result = std::string(value);
  free(value);
  return result;
}

inline void set_env(const std::string &name, const std::string &value)
{
  if (_putenv_s(name.c_str(), value.c_str()) != 0)
    throw std::runtime_error("Failed to set environment variable: " + name + ".");
}

inline FILE *pipe_open(const std::string &command, const std::string &mode)
{
  return _popen(command.c_str(), mode.c_str());
}

inline int pipe_close(FILE *pipe) { return _pclose(pipe); }

#elif defined(__linux__)

  #if defined(__x86_64__) || defined(__amd64__)
    #define ARCHITECTURE "x64"
  #elif defined(__i386__)
    #define ARCHITECTURE "x86"
  #elif defined(__aarch64__)
    #define ARCHITECTURE "arm64"
  #elif defined(__arm__)
    #define ARCHITECTURE "arm"
  #else
    #define ARCHITECTURE "unknown"
  #endif
  #define PLATFORM LINUX

inline std::string get_env(const std::string &name, const std::string &error_message)
{
  const char *value = std::getenv(name.c_str());
  if (!value) return "";
  return std::string(value);
}

inline void set_env(const std::string &name, const std::string &value)
{
  if (setenv(name.c_str(), value.c_str(), 1) != 0)
    throw std::runtime_error("Failed to set environment variable: " + name + ".");
}

inline FILE *pipe_open(const std::string &command, const std::string &mode)
{
  return popen(command.c_str(), mode.c_str());
}

inline int pipe_close(FILE *pipe) { return pclose(pipe); }

#else
  #define PLATFORM UNDEFINED
  #define ARCHITECTURE "unknown"
#endif

namespace csb
{
  inline platform current_platform = PLATFORM;
  inline std::string current_architecture = ARCHITECTURE;
}

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
  WINDOW
};

namespace csb::utility
{
  struct internal_state
  {
    std::optional<configuration> forced_configuration = {};
    std::filesystem::path build_directory = {};
  } inline state = {};

  inline const std::string big_section_divider = "====================================================================="
                                                 "===================================================";
  inline const std::string small_section_divider = "-------------------------------------------------------------------"
                                                   "-----------------------------------------------------";

  inline void handle_arguments(int argc, char *argv[])
  {
    for (int i = 1; i < argc; ++i)
    {
      std::string argument = argv[i];
      if (argument == "--release")
        state.forced_configuration = RELEASE;
      else if (argument == "--debug")
        state.forced_configuration = DEBUG;
      else
        throw std::runtime_error("Unknown argument: " + argument);
    }
  }

  inline std::string strict_get_env(const std::string &name, const std::string &error_message)
  {
    auto result = get_env(name, "Failed to get environment variable: " + name + ".");
    if (result.empty()) throw std::runtime_error(error_message);
    return result;
  }

  inline std::string remove_trailing_and_leading_newlines(const std::string &input)
  {
    size_t start = input.find_first_not_of('\n');
    size_t end = input.find_last_not_of('\n');
    if (start == std::string::npos)
      return "";
    else
      return input.substr(start, end - start + 1);
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
    FILE *pipe = pipe_open((command + " 2>&1").c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to execute command: '" + command + "'.");
    char buffer[512];
    std::string result = {};
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    int return_code = pipe_close(pipe);
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
    requires std::same_as<std::remove_cvref_t<decltype(*std::begin(type))>, std::filesystem::path> ||
               std::same_as<std::remove_cvref_t<decltype(*std::begin(type))>,
                            std::pair<const std::filesystem::path, std::vector<std::filesystem::path>>>;
  };
  template <iterable container_type>
  void multi_execute(const std::string &command, const container_type &container,
                     std::function<void(const std::filesystem::path &, const std::vector<std::filesystem::path> &,
                                        const std::string &, const std::string &)>
                       on_success = nullptr,
                     std::function<void(const std::filesystem::path &, const std::vector<std::filesystem::path> &,
                                        const std::string &, const int, const std::string &)>
                       on_failure = nullptr)
  {
    std::vector<std::exception_ptr> exceptions = {};
    std::mutex exceptions_mutex = {};
    std::atomic<bool> should_stop = false;
    std::for_each(std::execution::par, container.begin(), container.end(),
                  [&](const auto &item)
                  {
                    std::filesystem::path item_path = {};
                    std::vector<std::filesystem::path> item_dependencies = {};
                    if constexpr (std::same_as<std::remove_cvref_t<decltype(item)>, std::filesystem::path>)
                      item_path = item;
                    else
                    {
                      item_path = item.first;
                      item_dependencies = item.second;
                    }

                    std::string item_command = path_placeholder_replace(item_path, command);
                    FILE *pipe = pipe_open((item_command + " 2>&1").c_str(), "r");
                    if (!pipe)
                    {
                      std::lock_guard<std::mutex> lock(exceptions_mutex);
                      exceptions.push_back(std::make_exception_ptr(std::runtime_error(
                        std::format("{}: Failed to execute command: '{}'.", item_path.string(), item_command))));
                      return;
                    }
                    char buffer[512];
                    std::string result = {};
                    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
                    int return_code = pipe_close(pipe);
                    if (return_code != 0)
                    {
                      should_stop = true;
                      if (on_failure) on_failure(item_path, item_dependencies, item_command, return_code, result);
                    }
                    else if (on_success)
                    {
                      if (on_success) on_success(item_path, item_dependencies, item_command, result);
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
    if (should_stop) throw std::runtime_error("Errors occurred.");
  }

  inline void live_execute(const std::string &command, const std::string &error_message, bool print_command)
  {
    if (print_command) std::cout << command << std::endl;

    FILE *pipe = pipe_open((command + " 2>&1").c_str(), "r");
    if (!pipe) throw std::runtime_error("Failed to execute command: '" + command + "'.");

    int character;
    while ((character = fgetc(pipe)) != EOF)
    {
      std::cout << static_cast<char>(character);
      std::cout.flush();
    }

    int return_code = pipe_close(pipe);
    if (return_code != 0) throw std::runtime_error(error_message);
  }

  inline void touch(const std::filesystem::path &path)
  {
    if (!std::filesystem::exists(path.parent_path())) std::filesystem::create_directories(path.parent_path());
    if (std::filesystem::exists(path))
      std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
    else
    {
      std::ofstream file(path, std::ios::app);
      if (!file.is_open()) throw std::runtime_error("Failed to touch file: " + path.string());
      file.close();
    }
  }

  inline std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>> find_modified_files(
    const std::vector<std::filesystem::path> &target_files, const std::vector<std::filesystem::path> &check_files,
    const std::function<bool(const std::filesystem::path &, const std::vector<std::filesystem::path> &)>
      &dependency_handler = nullptr)
  {
    std::unordered_map<std::filesystem::path, std::vector<std::filesystem::path>> modified_files = {};
    for (const auto &target_file : target_files)
    {
      std::vector<std::filesystem::path> target_check_files = {};
      for (const auto &check_file : check_files)
        target_check_files.push_back(std::filesystem::path(path_placeholder_replace(target_file, check_file.string())));

      std::vector<std::filesystem::path> valid_files;
      bool any_missing = false;
      for (const auto &check_file : check_files)
      {
        std::filesystem::path check_path = path_placeholder_replace(target_file, check_file.string());
        if (!std::filesystem::exists(check_path))
        {
          any_missing = true;
          break;
        }
        valid_files.push_back(check_path);
      }
      if (any_missing)
      {
        modified_files.insert({target_file, target_check_files});
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
        modified_files.insert({target_file, target_check_files});
        continue;
      }

      if (dependency_handler)
      {
        try
        {
          if (dependency_handler(target_file, valid_files)) modified_files.insert({target_file, target_check_files});
        }
        catch (const std::exception &)
        {
          modified_files.insert({target_file, target_check_files});
        }
      }
    }
    return modified_files;
  }

  inline std::filesystem::path bootstrap_vcpkg(const std::string &vcpkg_version)
  {
    bool needs_bootstrap = false;

    auto vcpkg_path = std::filesystem::path("build") / std::format("vcpkg-{}", vcpkg_version) /
                      (current_platform == WINDOWS ? "vcpkg.exe" : "vcpkg");
    if (!std::filesystem::exists(vcpkg_path.parent_path()))
    {
      needs_bootstrap = true;
      live_execute("git clone --progress https://github.com/microsoft/vcpkg.git " + vcpkg_path.parent_path().string(),
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
      live_execute("cd " + vcpkg_path.parent_path().string() +
                     " && git -c advice.detachedHead=false checkout --progress " + vcpkg_version,
                   "Failed to checkout vcpkg version.", false);
    }

    if (!needs_bootstrap)
    {
      std::cout << "Using vcpkg version: " + vcpkg_version << std::endl;
      return vcpkg_path;
    }
    std::cout << "Bootstrapping vcpkg... ";
    std::cout.flush();
    utility::execute(
      std::format("cd {} && {}bootstrap-vcpkg.{} -disableMetrics", vcpkg_path.parent_path().string(),
                  current_platform == WINDOWS ? "" : "./", current_platform == WINDOWS ? "bat" : "sh"),
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
    auto clang_path = std::filesystem::path("build") / std::format("clang-{}", clang_version);
    if (std::filesystem::exists(clang_path)) return clang_path;
    std::cout << std::endl << small_section_divider << std::endl;

    auto to_upper = [](std::string string)
    {
      std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::toupper(c); });
      return string;
    };

    if (current_architecture != "x64" && current_architecture != "arm64")
      throw std::runtime_error("Clang bootstrap only supports 64 bit architectures.");
    std::string clang_architecture = {};
    if (current_platform == WINDOWS)
      clang_architecture = current_architecture == "x64" ? "x86_64" : "aarch64";
    else if (current_platform == LINUX)
      clang_architecture = to_upper(current_architecture);
    std::filesystem::path archive = std::format(
      "{}-{}-{}.tar.xz", current_platform == WINDOWS ? "clang+llvm" : "LLVM", clang_version,
      current_platform == WINDOWS ? clang_architecture + "-pc-windows-msvc" : "Linux-" + clang_architecture);
    std::string url = std::format("https://github.com/llvm/llvm-project/releases/download/llvmorg-{}/{}", clang_version,
                                  archive.string());
    std::cout << "Downloading archive at '" + url + "'..." << std::endl;
    utility::live_execute(
      std::format("curl -f -L -C - -o {} {}", (std::filesystem::path("build") / "temp.tar.xz").string(), url),
      "Failed to download archive.", false);
    std::cout << "Extracting archive... ";
    std::cout.flush();
    utility::live_execute(std::format("tar -xf {} -C build", (std::filesystem::path("build") / "temp.tar.xz").string()),
                          "Failed to extract archive.", false);
    std::filesystem::remove(std::filesystem::path("build") / "temp.tar.xz");
    auto extracted_path = "build" / archive.stem().stem();
    if (!std::filesystem::exists(extracted_path))
      throw std::runtime_error("Failed to find " + extracted_path.string() + ".");

    if (!std::filesystem::exists(clang_path)) std::filesystem::create_directories(clang_path);
    for (const auto &entry : std::filesystem::directory_iterator(extracted_path / "bin"))
      if (entry.is_regular_file()) std::filesystem::rename(entry.path(), clang_path / entry.path().filename());
    std::filesystem::remove_all(extracted_path);
    std::cout << "done." << std::endl << small_section_divider << std::endl;

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

  inline std::string get_environment_variable(const std::string &name)
  {
    return get_env(name, "Failed to get environment variable: " + name + ".");
  }

  inline void set_environment_variable(const std::string &name, const std::string &value) { set_env(name, value); }

  inline void append_environment_variable(const std::string &name, const std::string &value)
  {
    std::string current_value = get_env(name, "Failed to get environment variable: " + name + ".");
    if (!current_value.empty())
      current_value += (current_platform == WINDOWS ? ";" : ":") + value;
    else
      current_value = value;
    set_env(name, current_value);
  }

  inline void prepend_environment_variable(const std::string &name, const std::string &value)
  {
    std::string current_value = get_env(name, "Failed to get environment variable: " + name + ".");
    if (!current_value.empty())
      current_value = value + (current_platform == WINDOWS ? ";" : ":") + current_value;
    else
      current_value = value;
    set_env(name, current_value);
  }

  inline std::vector<std::filesystem::path> files_from(const std::set<std::filesystem::path> &directories,
                                                       const std::set<std::string> &extensions,
                                                       const std::set<std::filesystem::path> &overrides = {},
                                                       bool recursive = true)
  {
    std::set<std::filesystem::path> files = {};
    for (const auto &directory : directories)
    {
      if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
        throw std::runtime_error("Directory does not exist: " + directory.string());
      if (recursive)
      {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
          if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
            files.insert(entry.path().string());
      }
      else
      {
        for (const auto &entry : std::filesystem::directory_iterator(directory))
          if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
            files.insert(entry.path().string());
      }
    }
    for (const auto &override_file : overrides) files.insert(override_file);
    std::vector<std::filesystem::path> result(files.begin(), files.end());
    return result;
  }

  inline void task_run(const std::variant<std::string, std::function<bool()>> &task)
  {
    std::cout << std::endl << utility::small_section_divider << std::endl;

    if (std::holds_alternative<std::string>(task))
    {
      auto command = std::get<std::string>(task);
      utility::execute(
        command,
        [&](const std::string &real_command, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cout << real_command + "\n" + (result.empty() ? "" : result + "\n");
        },
        [&](const std::string &real_command, const int return_code, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cerr << real_command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        });
    }
    else if (std::holds_alternative<std::function<bool()>>(task))
    {
      auto function = std::get<std::function<bool()>>(task);
      if (!function()) throw std::runtime_error("Task function reported failure.");
    }
    else
      throw std::runtime_error("Invalid task variant.");

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void task_run(const std::variant<std::string, std::function<bool()>> &task,
                       const std::filesystem::path &check_file)
  {
    if (std::filesystem::exists(check_file)) return;
    std::cout << std::endl << utility::small_section_divider << std::endl;

    if (std::holds_alternative<std::string>(task))
    {
      auto command = std::get<std::string>(task);
      utility::execute(
        command,
        [&](const std::string &real_command, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cout << real_command + "\n" + (result.empty() ? "" : result + "\n");
          utility::touch(check_file);
        },
        [&](const std::string &real_command, const int return_code, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cerr << real_command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        });
    }
    else if (std::holds_alternative<std::function<bool()>>(task))
    {
      auto function = std::get<std::function<bool()>>(task);
      if (!function()) throw std::runtime_error("Task function reported failure.");
      utility::touch(check_file);
    }
    else
      throw std::runtime_error("Invalid task variant.");

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void task_run(const std::variant<std::string, std::function<bool()>> &task,
                       const std::vector<std::filesystem::path> &target_files,
                       const std::vector<std::filesystem::path> &check_files,
                       std::function<bool(const std::filesystem::path &, const std::vector<std::filesystem::path> &)>
                         dependency_handler = nullptr)
  {
    auto modified_files = utility::find_modified_files(target_files, check_files, dependency_handler);
    if (modified_files.empty()) return;
    std::cout << std::endl << utility::small_section_divider << std::endl;

    if (std::holds_alternative<std::string>(task))
    {
      auto command = std::get<std::string>(task);
      utility::execute(
        command,
        [&](const std::string &real_command, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cout << real_command + "\n" + (result.empty() ? "" : result + "\n");
          for (const auto &file : modified_files)
            for (const auto &dependency : file.second) utility::touch(dependency);
        },
        [&](const std::string &real_command, const int return_code, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cerr << real_command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        });
    }
    else if (std::holds_alternative<std::function<bool()>>(task))
    {
      auto function = std::get<std::function<bool()>>(task);
      if (!function()) throw std::runtime_error("Task function reported failure.");
      for (const auto &file : modified_files)
        for (const auto &dependency : file.second) utility::touch(dependency);
    }
    else
      throw std::runtime_error("Invalid task variant.");

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void multi_task_run(const std::variant<std::string, std::function<bool(const std::filesystem::path &)>> &task,
                             const std::vector<std::filesystem::path> &check_files)
  {
    std::vector<std::filesystem::path> target_files = {};
    for (const auto &file : check_files)
      if (!std::filesystem::exists(file)) target_files.push_back(file);
    if (target_files.empty()) return;
    std::cout << std::endl << utility::small_section_divider;
    std::cout.flush();

    if (std::holds_alternative<std::string>(task))
    {
      auto command = std::get<std::string>(task);
      utility::multi_execute(
        command, target_files,
        [](const std::filesystem::path &item, const std::vector<std::filesystem::path> &,
           const std::string &item_command, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cout << "\n" + item_command + "\n" + (result.empty() ? "" : result + "\n");
          utility::touch(item);
        },
        [](const std::filesystem::path &, const std::vector<std::filesystem::path> &, const std::string &item_command,
           const int return_code, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cerr << "\n" + item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        });
    }
    else if (std::holds_alternative<std::function<bool(const std::filesystem::path &)>>(task))
    {
      auto function = std::get<std::function<bool(const std::filesystem::path &)>>(task);
      std::vector<std::exception_ptr> exceptions = {};
      std::mutex exceptions_mutex = {};
      std::atomic<bool> should_stop = false;
      std::for_each(std::execution::par, target_files.begin(), target_files.end(),
                    [&](const std::filesystem::path &file)
                    {
                      try
                      {
                        if (!function(file)) should_stop = true;
                      }
                      catch (const std::exception &error)
                      {
                        should_stop = true;
                        std::lock_guard<std::mutex> lock(exceptions_mutex);
                        exceptions.push_back(std::make_exception_ptr(
                          std::runtime_error(std::format("{}: {}", file.string(), error.what()))));
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
      if (should_stop) throw std::runtime_error("Task function errors occurred.");
      for (const auto &file : target_files) utility::touch(file);
    }
    else
      throw std::runtime_error("Invalid task variant.");

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void multi_task_run(
    const std::variant<std::string, std::function<bool(const std::filesystem::path &)>> &task,
    const std::vector<std::filesystem::path> &target_files, const std::vector<std::filesystem::path> &check_files,
    std::function<bool(const std::filesystem::path &, const std::vector<std::filesystem::path> &)> dependency_handler =
      nullptr)
  {
    auto modified_files = utility::find_modified_files(target_files, check_files, dependency_handler);
    if (modified_files.empty()) return;
    std::cout << std::endl << utility::small_section_divider;
    std::cout.flush();

    if (std::holds_alternative<std::string>(task))
    {
      auto command = std::get<std::string>(task);
      utility::multi_execute(
        command, modified_files,
        [](const std::filesystem::path &, const std::vector<std::filesystem::path> &dependencies,
           const std::string &item_command, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cout << "\n" + item_command + "\n" + (result.empty() ? "" : result + "\n");
          for (const auto &dependency : dependencies) utility::touch(dependency);
        },
        [](const std::filesystem::path &, const std::vector<std::filesystem::path> &, const std::string &item_command,
           const int return_code, std::string result)
        {
          result = utility::remove_trailing_and_leading_newlines(result);
          std::cerr << "\n" + item_command + " -> " + std::to_string(return_code) + "\n" + result + "\n";
        });
    }
    else if (std::holds_alternative<std::function<bool(const std::filesystem::path &)>>(task))
    {
      auto function = std::get<std::function<bool(const std::filesystem::path &)>>(task);
      std::vector<std::exception_ptr> exceptions = {};
      std::mutex exceptions_mutex = {};
      std::atomic<bool> should_stop = false;
      std::for_each(std::execution::par, modified_files.begin(), modified_files.end(),
                    [&](const auto &file)
                    {
                      try
                      {
                        if (!function(file.first)) should_stop = true;
                        for (const auto &dependency : file.second) utility::touch(dependency);
                      }
                      catch (const std::exception &error)
                      {
                        should_stop = true;
                        std::lock_guard<std::mutex> lock(exceptions_mutex);
                        exceptions.push_back(std::make_exception_ptr(
                          std::runtime_error(std::format("{}: {}", file.first.string(), error.what()))));
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
      if (should_stop) throw std::runtime_error("Task function errors occurred.");
    }
    else
      throw std::runtime_error("Invalid task variant.");

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void live_task_run(const std::string &command)
  {
    std::cout << std::endl << utility::small_section_divider << std::endl;

    utility::live_execute(command, "Failed to run task: " + command, true);

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void live_task_run(const std::string &command, const std::filesystem::path &check_file)
  {
    if (std::filesystem::exists(check_file)) return;
    std::cout << std::endl << utility::small_section_divider << std::endl;

    utility::live_execute(command, "Failed to run task: " + command, true);
    utility::touch(check_file);

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void live_task_run(
    const std::string &command, const std::vector<std::filesystem::path> &target_files,
    const std::vector<std::filesystem::path> &check_files,
    std::function<bool(const std::filesystem::path &, const std::vector<std::filesystem::path> &)> dependency_handler =
      nullptr)
  {
    auto modified_files = utility::find_modified_files(target_files, check_files, dependency_handler);
    if (modified_files.empty()) return;
    std::cout << std::endl << utility::small_section_divider << std::endl;

    utility::live_execute(command, "Failed to run task: " + command, true);
    for (const auto &file : modified_files)
      for (const auto &dependency : file.second) utility::touch(dependency);

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void archive_install(
    const std::vector<std::tuple<std::string, std::filesystem::path, std::vector<std::filesystem::path>>> &archives)
  {
    if (archives.empty()) throw std::runtime_error("No archives to install.");
    bool all_exist = true;
    for (const auto &archive : archives)
    {
      auto [url, extract_path, target_paths] = archive;
      if (url.empty()) throw std::runtime_error("Archive URL not set.");
      if (extract_path.empty()) throw std::runtime_error("Archive extract path not set.");
      if (!std::filesystem::exists(extract_path))
      {
        all_exist = false;
        break;
      }
    }
    if (all_exist) return;
    std::cout << std::endl << utility::small_section_divider;

    for (const auto &archive : archives)
    {
      auto [url, extract_path, target_paths] = archive;
      if (url.empty()) throw std::runtime_error("Archive URL not set.");
      if (extract_path.empty()) throw std::runtime_error("Archive extract path not set.");
      if (std::filesystem::exists("build" / extract_path)) continue;

      std::string archive_name = url.substr(url.find_last_of('/') + 1);
      auto archive_path = std::filesystem::path("build") / archive_name;
      if (!std::filesystem::exists(archive_path))
      {
        std::cout << "\nDownloading archive at '" + url + "'..." << std::endl;
        utility::live_execute(std::format("curl -f -L -C - -o {} {}", archive_path.string(), url),
                              "Failed to download archive: " + url, false);
      }

      std::cout << "Extracting archive to '" + extract_path.string() + "'... ";
      std::cout.flush();
      std::filesystem::path temp_extract_path = (extract_path.string() + "_temp");
      if (!std::filesystem::exists(temp_extract_path)) std::filesystem::create_directories(temp_extract_path);
      utility::live_execute(std::format("tar -xf {} -C {}", archive_path.string(), temp_extract_path.string()),
                            "Failed to extract archive: " + archive_name, false);
      std::filesystem::remove(archive_path);

      if (target_paths.empty())
        std::filesystem::rename(temp_extract_path, extract_path);
      else
      {
        if (!std::filesystem::exists(extract_path)) std::filesystem::create_directories(extract_path);
        for (const auto &target_path : target_paths)
        {
          auto full_target_path = temp_extract_path / target_path;
          if (!std::filesystem::exists(full_target_path))
          {
            std::filesystem::remove_all(temp_extract_path);
            throw std::runtime_error("Failed to find target path in archive: " + full_target_path.string());
          }

          if (std::filesystem::is_directory(full_target_path))
            for (const auto &entry : std::filesystem::directory_iterator(full_target_path))
              std::filesystem::rename(entry.path(), extract_path / entry.path().filename());
          else
            std::filesystem::rename(full_target_path, extract_path / target_path.filename());
        }
        std::filesystem::remove_all(temp_extract_path);
      }

      std::cout << "done." << std::endl;
    }

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void subproject_install(const std::vector<std::tuple<std::string, std::string, artifact>> &subprojects)
  {
    if (subprojects.empty()) throw std::runtime_error("No subprojects to install.");
    if (utility::state.forced_configuration.has_value())
      target_configuration = utility::state.forced_configuration.value();

    auto subproject_directory = std::filesystem::path("build") / "subproject";
    if (!std::filesystem::exists(subproject_directory)) std::filesystem::create_directories(subproject_directory);

    for (const auto &subproject : subprojects)
    {
      std::cout << std::endl << utility::big_section_divider << std::endl;

      auto [name, version, artifact_type] = subproject;
      if (name.empty()) throw std::runtime_error("Subproject name not set.");
      if (version.empty()) throw std::runtime_error("Subproject version not set.");

      bool ran_git = false;
      std::string repo_name = name.substr(name.find('/') + 1);
      std::filesystem::path subproject_path = subproject_directory / repo_name;
      if (!std::filesystem::exists(subproject_path))
      {
        utility::live_execute("git clone --progress https://github.com/" + name + ".git " + subproject_path.string(),
                              "Failed to clone subproject: " + name, false);
        ran_git = true;
      }
      std::string current_hash = {};
      utility::execute(
        std::format("cd {} && git rev-parse HEAD", subproject_path.string()),
        [&](const std::string &, const std::string &result)
        {
          current_hash = result;
          current_hash.erase(std::remove(current_hash.begin(), current_hash.end(), '\n'), current_hash.end());
        },
        [](const std::string &, const int return_code, const std::string &result)
        {
          std::cerr << result << std::endl;
          throw std::runtime_error("Failed to get subproject current version. Return code: " +
                                   std::to_string(return_code));
        });
      std::string target_hash = {};
      utility::execute(
        std::format("cd {} && git rev-parse {}", subproject_path.string(), version),
        [&](const std::string &, const std::string &result)
        {
          target_hash = result;
          target_hash.erase(std::remove(target_hash.begin(), target_hash.end(), '\n'), target_hash.end());
        },
        [](const std::string &, const int return_code, const std::string &result)
        {
          std::cerr << result << std::endl;
          throw std::runtime_error("Failed to get subproject target version. Return code: " +
                                   std::to_string(return_code));
        });
      if (current_hash != target_hash)
      {
        std::cout << "Checking out to subproject " + name + " " + version + "..." << std::endl;
        utility::live_execute("cd " + subproject_path.string() +
                                " && git -c advice.detachedHead=false checkout --progress " + version,
                              "Failed to checkout subproject version: " + version, false);
        ran_git = true;
      }

      std::cout << (ran_git ? "\n" : "") << "Building subproject " + repo_name + " (" + version + ")..." << std::endl;
      auto build_path = subproject_path / "build" / (target_configuration == RELEASE ? "release" : "debug");
      if (!std::filesystem::exists(build_path)) std::filesystem::create_directories(build_path);

      std::string upper_architecture = current_architecture;
      std::transform(upper_architecture.begin(), upper_architecture.end(), upper_architecture.begin(),
                     [](unsigned char c) { return std::toupper(c); });
      std::string build_command = {};
      if (current_platform == WINDOWS)
        build_command = std::format("cl /nologo /EHsc /std:c++20 /O2 /Fobuild\\ /c csb.cpp && link /NOLOGO /MACHINE:{} "
                                    "/OUT:build\\csb.exe build\\csb.obj && build\\csb.exe --{}",
                                    upper_architecture, target_configuration == RELEASE ? "release" : "debug");
      else if (current_platform == LINUX)
        build_command = std::format("g++ -std=c++20 -O2 -o build/csb csb.cpp && build/csb --{}",
                                    target_configuration == RELEASE ? "release" : "debug");
      utility::live_execute(std::format("cd {} && {}", subproject_path.string(), build_command),
                            "Failed to install subproject: " + name, false);

      if (artifact_type == EXECUTABLE)
      {
        std::string new_path = get_env("PATH", "Could not get PATH environment variable.") +
                               (current_platform == WINDOWS ? ";" : ":") +
                               std::filesystem::absolute(build_path).string();
        set_env("PATH", new_path);
      }
      else if (artifact_type == STATIC_LIBRARY || artifact_type == DYNAMIC_LIBRARY)
      {
        std::filesystem::path include_path = subproject_path / "include";
        if (std::filesystem::exists(include_path) && std::filesystem::is_directory(include_path))
          external_include_directories.push_back(include_path);
        library_directories.push_back(build_path);
      }

      std::cout << utility::big_section_divider << std::endl;
    }
  }

  inline void vcpkg_install(const std::string &vcpkg_version)
  {
    if (vcpkg_version.empty()) throw std::runtime_error("vcpkg_version not set.");
    if (utility::state.forced_configuration.has_value())
      target_configuration = utility::state.forced_configuration.value();
    std::cout << std::endl << utility::small_section_divider << std::endl;

    auto vcpkg_path = utility::bootstrap_vcpkg(vcpkg_version);

    std::string vcpkg_triplet = {};
    if (current_platform == WINDOWS)
      vcpkg_triplet = std::format("{}-windows{}{}", current_architecture, (target_linkage == STATIC ? "-static" : ""),
                                  (target_configuration == RELEASE ? "-release" : ""));
    else if (current_platform == LINUX)
      vcpkg_triplet = std::format("{}-linux", current_architecture);
    auto vcpkg_installed_directory = std::filesystem::path("build") / "vcpkg_installed";
    std::cout << "Using vcpkg triplet: " << vcpkg_triplet << std::endl;
    utility::live_execute(std::format("{} install --vcpkg-root {} --triplet {} --x-install-root {}",
                                      vcpkg_path.string(), vcpkg_path.parent_path().string(), vcpkg_triplet,
                                      vcpkg_installed_directory.string()),
                          "Failed to install vcpkg dependencies.", false);

    std::pair<std::filesystem::path, std::filesystem::path> outputs = {
      vcpkg_installed_directory / vcpkg_triplet / "include",
      vcpkg_installed_directory / vcpkg_triplet /
        (target_configuration == RELEASE ? "lib" : std::filesystem::path("debug") / "lib")};
    if (std::filesystem::exists(outputs.first)) external_include_directories.push_back(outputs.first);
    if (std::filesystem::exists(outputs.second)) library_directories.push_back(outputs.second);

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void generate_compile_commands()
  {
    if (source_files.empty()) throw std::runtime_error("No source files to generate compile commands for.");
    std::vector<std::filesystem::path> target_files = {};
    target_files.reserve(source_files.size() + include_files.size());
    target_files.insert(target_files.end(), source_files.begin(), source_files.end());
    target_files.insert(target_files.end(), include_files.begin(), include_files.end());
    if (utility::find_modified_files(target_files, {"compile_commands.json"}).empty()) return;
    if (utility::state.forced_configuration.has_value())
      target_configuration = utility::state.forced_configuration.value();
    std::cout << std::endl << utility::small_section_divider;
    std::cout.flush();

    std::cout << std::endl << "Generating compile_commands.json... ";
    std::cout.flush();

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
    auto build_directory = std::filesystem::path("build") / (target_configuration == RELEASE ? "release" : "debug");
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
                             "-Wunreachable-code-aggressive -Wformat=2 ",
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
      content += std::format("-c \\\"{}\\\" -o \\\"{}\\\"\"\n", escape_backslashes((*iterator).string()),
                             escape_backslashes((build_directory / (iterator->stem().string() + ".o")).string()));
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

    std::cout << utility::small_section_divider << std::endl;
  }

  inline void clang_format(const std::string &clang_version, std::vector<std::filesystem::path> exclude_files = {})
  {
    if (clang_version.empty()) throw std::runtime_error("clang_version not set.");
    if (source_files.empty() && include_files.empty()) throw std::runtime_error("No files to format.");

    auto format_directory = std::filesystem::path("build") / "format";
    if (!std::filesystem::exists(format_directory)) std::filesystem::create_directories(format_directory);
    std::vector<std::filesystem::path> format_files = {};
    format_files.reserve(source_files.size() + include_files.size());
    format_files.insert(format_files.end(), source_files.begin(), source_files.end());
    format_files.insert(format_files.end(), include_files.begin(), include_files.end());
    if (!exclude_files.empty())
      format_files.erase(
        std::remove_if(format_files.begin(), format_files.end(), [&](const std::filesystem::path &path)
                       { return std::find(exclude_files.begin(), exclude_files.end(), path) != exclude_files.end(); }),
        format_files.end());
    auto clang_path = utility::bootstrap_clang(clang_version);
    auto clang_format_path = clang_path / (current_platform == WINDOWS ? "clang-format.exe" : "clang-format");

    csb::multi_task_run(
      std::format("{} -i \"[]\"", (current_platform == WINDOWS ? "" : "./") + clang_format_path.string()), format_files,
      {format_directory / "[.filename].formatted"});
  }

  inline void compile()
  {
    if (target_name.empty()) throw std::runtime_error("Executable name not set.");
    if (source_files.empty()) throw std::runtime_error("No source files to compile.");
    if (utility::state.forced_configuration.has_value())
      target_configuration = utility::state.forced_configuration.value();

    utility::state.build_directory =
      std::filesystem::path("build") / (target_configuration == RELEASE ? "release" : "debug");
    if (!std::filesystem::exists(utility::state.build_directory))
      std::filesystem::create_directories(utility::state.build_directory);

    if (current_platform == WINDOWS)
    {
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
      std::vector<std::filesystem::path> check_files = {utility::state.build_directory / "[.filename.stem].obj",
                                                        utility::state.build_directory / "[.filename.stem].d"};
      if (target_configuration == DEBUG) check_files.push_back(utility::state.build_directory / "[.filename.stem].pdb");

      multi_task_run(
        std::format(
          "cl /nologo /std:c++{} /W{} /external:W0 {}/EHsc /MP /{} /DWIN32 /D_WINDOWS {}/ifcOutput{}\\ /Fo{}\\ "
          "/Fd{} /sourceDependencies{} {}{}/c \"[]\"",
          std::to_string(cxx_standard), std::to_string(warning_level), compile_debug_flags, runtime_library,
          compile_definitions, utility::state.build_directory.string(), utility::state.build_directory.string(),
          (utility::state.build_directory / "[.stem].pdb").string(),
          (utility::state.build_directory / "[.stem].d").string(), compile_include_directories,
          compile_external_include_directories),
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
    }
    else if (current_platform == LINUX)
    {
      std::string compile_debug_flags = target_configuration == RELEASE ? "-O2 " : "-O0 -g ";
      std::string compile_pic_flag = target_artifact == DYNAMIC_LIBRARY ? "-fPIC " : "";
      std::string compile_definitions = {};
      for (const auto &definition : definitions) compile_definitions += std::format("-D{} ", definition);
      std::vector<std::filesystem::path> include_directories = {};
      for (const auto &include_file : include_files)
      {
        if (include_file.has_parent_path() && std::find(include_directories.begin(), include_directories.end(),
                                                        include_file.parent_path()) == include_directories.end())
          include_directories.push_back(include_file.parent_path());
      }
      std::string compile_include_directories = {};
      for (const auto &directory : include_directories)
        compile_include_directories += std::format("-I\"{}\" ", directory.string());
      std::string compile_external_include_directories = {};
      for (const auto &directory : external_include_directories)
        compile_external_include_directories += std::format("-isystem\"{}\" ", directory.string());
      std::vector<std::filesystem::path> check_files = {utility::state.build_directory / "[.filename.stem].o",
                                                        utility::state.build_directory / "[.filename.stem].d"};
      std::string warning_flags = {};
      if (warning_level >= W1) warning_flags += "-Wall ";
      if (warning_level >= W2) warning_flags += "-Wextra ";
      if (warning_level >= W3) warning_flags += "-Wpedantic ";
      if (warning_level >= W4)
        warning_flags +=
          "-Wconversion -Wshadow -Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal -Wformat=2 ";

      multi_task_run(std::format("g++ -std=c++{} {}{}{}-MMD -MP {}{}{}-c \"[]\" -o {}/[.stem].o",
                                 std::to_string(cxx_standard), warning_flags, compile_debug_flags, compile_pic_flag,
                                 compile_definitions, compile_include_directories, compile_external_include_directories,
                                 utility::state.build_directory.string()),
                     source_files, check_files,
                     [](const std::filesystem::path &, const std::vector<std::filesystem::path> &checked_files) -> bool
                     {
                       auto object_time = std::filesystem::last_write_time(checked_files[0]);
                       std::filesystem::path dependency_path = checked_files[1];

                       std::ifstream dependency_file(dependency_path);
                       if (!dependency_file.is_open()) return true;
                       std::string line;
                       std::string full_content;
                       while (std::getline(dependency_file, line))
                       {
                         if (line.empty()) continue;
                         if (line.back() == '\\') line.pop_back();
                         full_content += line + " ";
                       }
                       dependency_file.close();

                       size_t colon_pos = full_content.find(':');
                       if (colon_pos == std::string::npos) return true;
                       std::string dependencies = full_content.substr(colon_pos + 1);

                       size_t pos = 0;
                       while (pos < dependencies.length())
                       {
                         while (pos < dependencies.length() && std::isspace(dependencies[pos])) pos++;
                         if (pos >= dependencies.length()) break;

                         size_t end = pos;
                         while (end < dependencies.length() && !std::isspace(dependencies[end])) end++;

                         std::filesystem::path include_path = dependencies.substr(pos, end - pos);
                         if (std::filesystem::exists(include_path))
                         {
                           auto include_time = std::filesystem::last_write_time(include_path);
                           if (include_time > object_time) return true;
                         }
                         pos = end;
                       }
                       return false;
                     });
    }
  }

  inline void link()
  {
    if (!std::filesystem::exists(utility::state.build_directory))
      throw std::runtime_error("Link called before compile.");

    if (current_platform == WINDOWS)
    {
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
          ? (target_configuration == RELEASE
               ? std::format("/IMPLIB:{}.lib ", (utility::state.build_directory / target_name).string())
               : std::format("/PDB:{}.pdb /IMPLIB:{}.lib ", (utility::state.build_directory / target_name).string(),
                             (utility::state.build_directory / target_name).string()))
        : target_artifact == EXECUTABLE
          ? (target_configuration == RELEASE
               ? ""
               : std::format("/PDB:{}.pdb ", (utility::state.build_directory / target_name).string()))
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
        link_objects += std::format("{}.obj ", (utility::state.build_directory / source_file.stem()).string());

      std::vector<std::filesystem::path> target_files = {};
      target_files.reserve(source_files.size() + include_files.size());
      target_files.insert(target_files.end(), source_files.begin(), source_files.end());
      target_files.insert(target_files.end(), include_files.begin(), include_files.end());
      std::vector<std::filesystem::path> check_files = {utility::state.build_directory /
                                                        (target_name + "." + extension)};
      if (target_configuration == DEBUG) check_files.push_back(utility::state.build_directory / (target_name + ".pdb"));

      task_run(std::format("{} /NOLOGO /MACHINE:{} {}/SUBSYSTEM:{} {}{}{}{}{}/OUT:{}", executable_option,
                           current_architecture, dynamic_flags, console_option, link_debug_flags,
                           link_library_directories, link_libraries, link_objects, output_flags,
                           (utility::state.build_directory / (target_name + "." + extension)).string()),
               target_files, check_files);
    }
    else if (current_platform == LINUX)
    {
      std::string extension = target_artifact == STATIC_LIBRARY ? "a" : target_artifact == DYNAMIC_LIBRARY ? "so" : "";
      std::string output_name = (target_artifact == STATIC_LIBRARY || target_artifact == DYNAMIC_LIBRARY)
                                  ? "lib" + target_name + "." + extension
                                  : target_name;

      std::string runtime_linkage = target_linkage == STATIC ? "-static-libstdc++ -static-libgcc " : "";
      std::string link_library_directories = {};
      for (const auto &directory : library_directories)
        link_library_directories += std::format("-L\"{}\" ", directory.string());
      std::string link_libraries = {};
      for (const auto &library : libraries) link_libraries += std::format("-l{} ", library);
      std::string link_objects = {};
      for (const auto &source_file : source_files)
        link_objects += std::format("{}.o ", (utility::state.build_directory / source_file.stem()).string());

      std::vector<std::filesystem::path> target_files = {};
      target_files.reserve(source_files.size() + include_files.size());
      target_files.insert(target_files.end(), source_files.begin(), source_files.end());
      target_files.insert(target_files.end(), include_files.begin(), include_files.end());
      std::vector<std::filesystem::path> check_files = {utility::state.build_directory / output_name};

      std::string command;
      if (target_artifact == STATIC_LIBRARY)
        command = std::format("ar rcs {} {}", (utility::state.build_directory / output_name).string(), link_objects);
      else if (target_artifact == DYNAMIC_LIBRARY)
        command = std::format("g++ -shared {}-o {} {}{}{}", runtime_linkage,
                              (utility::state.build_directory / output_name).string(), link_objects,
                              link_library_directories, link_libraries);
      else
        command =
          std::format("g++ {}-o {} {}{}{}", runtime_linkage, (utility::state.build_directory / output_name).string(),
                      link_objects, link_library_directories, link_libraries);

      task_run(command, target_files, check_files);
    }
  }
}

#define CSB_SUCCESS EXIT_SUCCESS
#define CSB_FAILURE EXIT_FAILURE
#define CSB_MAIN()                                                                                                     \
  int main(int argc, char *argv[])                                                                                     \
  {                                                                                                                    \
    if (csb::current_platform == WINDOWS)                                                                              \
    {                                                                                                                  \
      const std::string error_message = "Ensure you are running from an environment with access to MSVC tools.";       \
      const std::string vs_path = csb::utility::strict_get_env("VSINSTALLDIR", error_message);                         \
      const std::string toolset_version = csb::utility::strict_get_env("VCToolsVersion", error_message);               \
      const std::string sdk_version = csb::utility::strict_get_env("WindowsSDKVersion", error_message);                \
      std::cout << std::format("Visual Studio: {}\nToolset: {}\nWindows SDK: {}\nArchitecture: {}", vs_path,           \
                               toolset_version, sdk_version, csb::current_architecture)                                \
                << std::endl;                                                                                          \
    }                                                                                                                  \
    else if (csb::current_platform == LINUX)                                                                           \
      std::cout << std::format("Architecture: {}", csb::current_architecture) << std::endl;                            \
    else                                                                                                               \
      throw std::runtime_error("Unsupported platform.");                                                               \
                                                                                                                       \
    try                                                                                                                \
    {                                                                                                                  \
      csb::utility::handle_arguments(argc, argv);                                                                      \
      return csb_main();                                                                                               \
    }                                                                                                                  \
    catch (const std::exception &exception)                                                                            \
    {                                                                                                                  \
      std::cerr << "Error: " << exception.what() << std::endl;                                                         \
      return CSB_FAILURE;                                                                                              \
    }                                                                                                                  \
  }
