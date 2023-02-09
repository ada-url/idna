add_library(ada-idna-include-source INTERFACE)
target_include_directories(ada-idna-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
add_library(ada-idna-source INTERFACE)
target_sources(ada-idna-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/idna.cpp)
target_link_libraries(ada-idna-source INTERFACE ada-include-source)
add_library(ada-idna STATIC idna.cpp)
target_include_directories(ada-idna PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> )
target_include_directories(ada-idna PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")


if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  message(STATUS "Enabling _GLIBCXX_DEBUG")
  target_compile_definitions(ada-idna PRIVATE _GLIBCXX_DEBUG=1)
endif()

if(ADA_SANITIZE)
  target_compile_options(ada-idna INTERFACE -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_compile_definitions(ada-idna INTERFACE ASAN_OPTIONS=detect_leaks=1)
  target_link_libraries(ada-idna INTERFACE -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
endif()

if(ADA_SANITIZE_UNDEFINED)
  target_compile_options(ada-idna INTERFACE -fsanitize=undefined -fno-sanitize-recover=all)
endif()