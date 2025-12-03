#include "../csb.hpp"

void csb::configure()
{
  csb::target_name = "test";
  csb::target_artifact = EXECUTABLE;
  csb::target_linkage = STATIC;
  csb::target_subsystem = CONSOLE;
  csb::target_configuration = RELEASE;
  csb::cxx_standard = CXX20;
  csb::warning_level = W4;
  csb::include_files = csb::files_from({"program/include"});
  csb::source_files = csb::files_from({"program/source"});
  if (csb::host_platform == WINDOWS)
    csb::libraries = {"kernel32"};
  else if (csb::host_platform == LINUX)
    csb::libraries = {"c"};
}

int csb::clean()
{
  csb::clean_build_directory();
  return CSB_SUCCESS;
}

int csb::build()
{
  csb::generate_compile_commands();
  csb::compile();
  csb::link();
  return CSB_SUCCESS;
}

int csb::run()
{
  csb::run_target();
  return CSB_SUCCESS;
}

CSB_MAIN()
