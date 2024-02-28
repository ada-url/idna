cmake_minimum_required(VERSION 3.16)

project(ada-idna
  DESCRIPTION "IDNA"
  LANGUAGES C CXX
  VERSION 0.1.0
)

include(GNUInstallDirs)

include(CTest)
include(cmake/idna-flags.cmake)

add_subdirectory(src)

include(cmake/clang-format.cmake)

option(ADA_IDNA_BENCHMARKS "Build benchmarks" OFF)

if(ADA_IDNA_BENCHMARKS OR BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/import.cmake)
  import_dependency(simdjson simdjson/simdjson 0a3a00c95665cc1fda760e121ba8d442945ede13)
  add_dependency(simdjson)
endif()

if (ADA_IDNA_BENCHMARKS)
  message(STATUS "Ada benchmarks enabled.")
  add_subdirectory(benchmarks)
else(ADA_IDNA_BENCHMARKS)
  if (is_top_project)
    message(STATUS "Ada IDNA benchmarks disabled. Set ADA_IDNA_BENCHMARKS=ON to enable them.")
  endif()
endif()

if (BUILD_TESTING)
  message(STATUS "The tests are enabled.")
  add_subdirectory(tests)
else()
  if(is_top_project)
    message(STATUS "The tests are disabled.")
  endif()
endif(BUILD_TESTING)

add_subdirectory(singleheader)


install(
    FILES include/idna.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY include/ada
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
