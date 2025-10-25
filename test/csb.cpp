#include "../csb.hpp" // Would just be "csb.hpp" in a real project

int csb_main()
{
  csb::target_name = "test";
  csb::target_artifact = EXECUTABLE;
  csb::target_linkage = STATIC;
  csb::target_subsystem = CONSOLE;
  csb::target_configuration = RELEASE;
  csb::cxx_standard = CXX20;
  csb::warning_level = W4;
  csb::definitions = {"STB_IMAGE_IMPLEMENTATION"};
  csb::include_files = csb::files_from("program/include", {".hpp", ".inl"});
  csb::source_files = csb::files_from("program/source", {".cpp"});
  csb::libraries = {"kernel32", "user32",  "shell32", "gdi32",    "imm32",    "comdlg32", "ole32",   "oleaut32",
                    "advapi32", "dinput8", "winmm",   "winspool", "setupapi", "uuid",     "version", "SDL3-static"};

  csb::vcpkg_install("2025.08.27");
  csb::subproject_install({{"ConnorSweeneyDev/CSBuildSubproject", "0.0.0", EXECUTABLE}});

  csb::task_run("CSBuildSubproject --hello", "build/csbuildsubproject/hello.marker");
  csb::task_run("CSBuildSubproject --goodbye", "build/csbuildsubproject/goodbye.marker");

  csb::clang_version = "21.1.1";
  csb::clang_compile_commands();
  csb::clang_format();

  csb::compile();
  csb::link();
  return CSB_SUCCESS;
}

CSB_MAIN()
