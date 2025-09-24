cmake_minimum_required(VERSION 3.16)

project(ada-idna
  DESCRIPTION "IDNA"
  LANGUAGES C CXX
  VERSION 0.3.4
)
set(ADA_IDNA_LIB_VERSION "0.3.4" CACHE STRING "ada-idna library version")
set(ADA_IDNA_LIB_SOVERSION "0" CACHE STRING "ada-idna library soversion")

include(GNUInstallDirs)
include(CTest)
include(cmake/idna-flags.cmake)

add_subdirectory(src)

option(ADA_USE_SIMDUTF "Whether to use SIMDUTF for unicode transcoding" OFF)

if(ADA_IDNA_BENCHMARKS OR BUILD_TESTING)
  include(cmake/CPM.cmake)
endif()

if(ADA_USE_SIMDUTF)
  include(cmake/CPM.cmake)
  CPMAddPackage(
    NAME simdutf
    GITHUB_REPOSITORY simdutf/simdutf
    VERSION 7.0.0
    OPTIONS "SIMDUTF_TESTS OFF"
  )
endif()

if (ADA_IDNA_BENCHMARKS)
  message(STATUS "Ada benchmarks enabled.")
  CPMAddPackage(
    NAME benchmark
    GITHUB_REPOSITORY google/benchmark
    VERSION 1.9.0
    OPTIONS "BENCHMARK_ENABLE_TESTING OFF"
            "BENCHMARK_ENABLE_INSTALL OFF"
            "BENCHMARK_ENABLE_WERROR OFF"
  )
  add_subdirectory(benchmarks)
else(ADA_IDNA_BENCHMARKS)
  if (is_top_project)
    message(STATUS "Ada IDNA benchmarks disabled. Set ADA_IDNA_BENCHMARKS=ON to enable them.")
  endif()
endif()

if (BUILD_TESTING)
  message(STATUS "The tests are enabled.")
  CPMAddPackage(
    NAME GTest
    GITHUB_REPOSITORY google/googletest
    VERSION 1.17.0
    OPTIONS  "BUILD_GMOCK OFF" "INSTALL_GTEST OFF"
  )
  CPMAddPackage("gh:simdjson/simdjson@4.0.6")
  add_subdirectory(tests)
else()
  if(is_top_project)
    message(STATUS "The tests are disabled.")
  endif()
endif(BUILD_TESTING)

add_subdirectory(singleheader)

add_library(ada-idna::ada-idna ALIAS ada-idna)

set_target_properties(
  ada-idna PROPERTIES
  VERSION "${ADA_IDNA_LIB_VERSION}"
  SOVERSION "${ADA_IDNA_LIB_SOVERSION}"
  WINDOWS_EXPORT_ALL_SYMBOLS YES
)

install(
  FILES include/idna.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT ada-idna_development
)

install(
  DIRECTORY include/ada
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT ada-idna_development
)

install(
  TARGETS ada-idna
  EXPORT ada-idna_targets
  RUNTIME COMPONENT ada-idna_runtime
  LIBRARY COMPONENT ada-idna_runtime
  NAMELINK_COMPONENT ada-idna_development
  ARCHIVE COMPONENT ada-idna_development
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
