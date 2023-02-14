# ToASCII Bench
add_executable(to_ascii to_ascii.cpp)
target_link_libraries(to_ascii PRIVATE ada-idna)
target_include_directories(to_ascii PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_include_directories(to_ascii PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/benchmarks>")

include(${PROJECT_SOURCE_DIR}/cmake/import.cmake)

set_off(BENCHMARK_ENABLE_TESTING)
set_off(BENCHMARK_ENABLE_INSTALL)
set_off(BENCHMARK_ENABLE_WERROR)

# Google Benchmarks
import_dependency(google_benchmarks google/benchmark f91b6b4)
add_dependency(google_benchmarks)
target_link_libraries(to_ascii PRIVATE benchmark::benchmark)

message(STATUS "Compiler is " ${CMAKE_CXX_COMPILER_ID})

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
message(STATUS "Compiler version " ${CMAKE_CXX_COMPILER_VERSION})
endif()
