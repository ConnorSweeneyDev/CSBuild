#include "../csb.hpp"

#include <cstdlib>
#include <filesystem>

int csb_main()
{
  csb::name = "test";
  csb::output_type = EXECUTABLE;
  csb::cxx_standard = CXX20;
  csb::warning_level = W4;
  csb::linkage_type = STATIC;
  csb::build_configuration = RELEASE;
  csb::subsystem_type = CONSOLE;
  csb::include_directories = {"program/include"};
  for (const auto &entry : std::filesystem::recursive_directory_iterator("program/source"))
    if (entry.is_regular_file() && entry.path().extension() == ".cpp") csb::source_files.insert(entry.path());

  auto vcpkg_outputs = csb::vcpkg_install("2025.08.27", {{"sdl3", "3.2.20"}, {"stb", "2024-07-29#1"}});
  csb::external_include_directories = {vcpkg_outputs.include_directory};
  csb::library_directories = {vcpkg_outputs.library_directory};
  csb::libraries = {"kernel32", "user32",  "shell32", "gdi32", "imm32",    "advapi32", "comdlg32", "ole32",
                    "oleaut32", "version", "winmm",   "uuid",  "setupapi", "dinput8",  "winspool", "SDL3-static"};

  csb::clang_version = "21.1.1";
  csb::clang_compile_commands();
  csb::clang_format();

  csb::build();
  return EXIT_SUCCESS;
}

CSB_MAIN()
