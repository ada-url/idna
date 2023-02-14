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



if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  message(STATUS "Apple system detected.")
  # People who run macOS often use brew.
  if(EXISTS /opt/homebrew/opt/icu4c)
    message(STATUS "icu is provided by homebrew at /opt/homebrew/opt/icu4c.")
    ## This is a bit awkward, but it is a lot better than asking the
    ## user to figure that out.
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/icu4c/include")
    list(APPEND CMAKE_LIBRARY_PATH "/opt/homebrew/opt/icu4c/lib")
  elseif(EXISTS /usr/local/opt/icu4c)
    message(STATUS "icu is provided by homebrew at /usr/local/opt/icu4c.")
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/icu4c/include")
    list(APPEND CMAKE_LIBRARY_PATH "/usr/local/opt/icu4c/lib")
  endif()
endif()

find_package(ICU COMPONENTS uc i18n)
### If the user does not have ICU, let us help them with instructions:
if(ICU_FOUND)
  target_link_libraries(to_ascii PRIVATE ICU::uc ICU::i18n)
  target_compile_definitions(to_ascii PRIVATE ICU_AVAILABLE=1)
else(ICU_FOUND)
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      if(EXISTS /opt/homebrew)
        message(STATUS "Under macOS, you may install ICU with brew, using 'brew install icu4c'.")
      else()
        message(STATUS "Under macOS, you should install brew (see https://brew.sh) and then icu4c ('brew install icu4c').")
      endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      message(STATUS "Under Linux, you may be able to install ICU with a command such as 'apt-get install libicu-dev'." )
  endif()
endif(ICU_FOUND)