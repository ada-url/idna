add_library(ada-idna-include-source INTERFACE)
target_include_directories(ada-idna-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
add_library(ada-idna-source INTERFACE)
target_sources(ada-idna-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/idna.cpp)
target_link_libraries(ada-idna-source INTERFACE ada-include-source)
add_library(ada-idna STATIC idna.cpp)
target_include_directories(ada-idna PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> )
target_include_directories(ada-idna PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

if(MSVC)
  if("${MSVC_TOOLSET_VERSION}" STREQUAL "140")
    target_compile_options(ada-idna INTERFACE /W0 /sdl)
    set(ADA_LEGACY_VISUAL_STUDIO TRUE)
  else()
    target_compile_options(ada-idna INTERFACE /WX /W3 /sdl /w34714) # https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-4-c4714?view=vs-2019
  endif()
else(MSVC)
  if(NOT WIN32)
    target_compile_options(ada-idna INTERFACE -fPIC)
  endif()
  message(STATUS "Assuming GCC-like compiler.")
  target_compile_options(ada-idna PRIVATE -Wall -Wextra -Weffc++)
  target_compile_options(ada-idna PRIVATE -Wfatal-errors -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wno-sign-conversion)
endif(MSVC)

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

install(
    TARGETS ada-idna
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
