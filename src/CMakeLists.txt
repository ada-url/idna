message(STATUS "Compiler ID : " ${CMAKE_CXX_COMPILER_ID})
message(STATUS "CMAKE_BUILD_TYPE : " ${CMAKE_BUILD_TYPE})

add_library(ada-idna-include-source INTERFACE)
target_include_directories(ada-idna-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
add_library(ada-idna-source INTERFACE)
target_sources(ada-idna-source INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/idna_hot.cpp
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/idna_cold.cpp)
target_link_libraries(ada-idna-source INTERFACE ada-include-source)

# Object libraries so we can optimize hot vs cold separately, then ld -r into
# a single archive member (one .o in the .a cuts SYMDEF/header overhead).
add_library(ada-idna-hot OBJECT idna_hot.cpp)
add_library(ada-idna-cold OBJECT idna_cold.cpp)
target_compile_features(ada-idna-hot PUBLIC cxx_std_20)
target_compile_features(ada-idna-cold PUBLIC cxx_std_20)
target_include_directories(ada-idna-hot PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(ada-idna-cold PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(ada-idna-hot PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_include_directories(ada-idna-cold PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

find_package(ZLIB)
if(ZLIB_FOUND)
  target_link_libraries(ada-idna-cold PRIVATE ZLIB::ZLIB)
  target_compile_definitions(ada-idna-cold PRIVATE ADA_IDNA_USE_SYSTEM_ZLIB=1)
  # Propagate to consumers of the static lib.
  set(ADA_IDNA_NEEDS_ZLIB TRUE)
else()
  message(STATUS "zlib not found; using in-tree raw DEFLATE inflater")
  set(ADA_IDNA_NEEDS_ZLIB FALSE)
endif()

if(NOT MSVC)
  target_compile_options(ada-idna-hot PRIVATE -O3 -fno-exceptions -fno-rtti)
  target_compile_options(ada-idna-cold PRIVATE -Os -fno-exceptions -fno-rtti)
endif()

# Merge objects into one relocatable .o, strip local symbols, archive.
set(ADA_IDNA_MERGED_OBJ "${CMAKE_CURRENT_BINARY_DIR}/ada_idna_merged.o")
set(ADA_IDNA_HOT_OBJ "$<TARGET_OBJECTS:ada-idna-hot>")
set(ADA_IDNA_COLD_OBJ "$<TARGET_OBJECTS:ada-idna-cold>")

add_custom_command(
  OUTPUT "${ADA_IDNA_MERGED_OBJ}"
  COMMAND ${CMAKE_LINKER} -r -o "${ADA_IDNA_MERGED_OBJ}"
          ${ADA_IDNA_HOT_OBJ} ${ADA_IDNA_COLD_OBJ}
  COMMAND ${CMAKE_STRIP} -x "${ADA_IDNA_MERGED_OBJ}"
  DEPENDS ada-idna-hot ada-idna-cold
  COMMENT "Merging ada-idna hot+cold objects into one archive member"
  VERBATIM
  COMMAND_EXPAND_LISTS
)

add_custom_target(ada-idna-merged DEPENDS "${ADA_IDNA_MERGED_OBJ}")

add_library(ada-idna STATIC "${ADA_IDNA_MERGED_OBJ}")
set_target_properties(ada-idna PROPERTIES LINKER_LANGUAGE CXX)
add_dependencies(ada-idna ada-idna-merged)
target_include_directories(ada-idna PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_compile_features(ada-idna PUBLIC cxx_std_20)
if(ADA_IDNA_NEEDS_ZLIB)
  target_link_libraries(ada-idna PUBLIC ZLIB::ZLIB)
endif()

if(ADA_USE_SIMDUTF)
  target_link_libraries(ada-idna PRIVATE simdutf)
  target_compile_definitions(ada-idna PRIVATE ADA_USE_SIMDUTF)
  target_compile_definitions(ada-idna-hot PRIVATE ADA_USE_SIMDUTF)
  target_compile_definitions(ada-idna-cold PRIVATE ADA_USE_SIMDUTF)
endif()

if(MSVC)
  if("${MSVC_TOOLSET_VERSION}" STREQUAL "140")
    target_compile_options(ada INTERFACE /W0 /sdl)
    set(ADA_LEGACY_VISUAL_STUDIO TRUE)
  else()
    target_compile_options(ada-idna INTERFACE /WX /W3 /sdl /w34714)
  endif()
else(MSVC)
  if(NOT WIN32)
    target_compile_options(ada-idna INTERFACE -fPIC)
    target_compile_options(ada-idna-hot PRIVATE -fPIC)
    target_compile_options(ada-idna-cold PRIVATE -fPIC)
  endif()
  message(STATUS "Assuming GCC-like compiler.")
  foreach(t ada-idna-hot ada-idna-cold)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Weffc++)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${t} PRIVATE -Wsuggest-override -Werror=unused-but-set-variable)
    endif()
    if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
      target_compile_options(${t} PRIVATE -Winconsistent-missing-override)
    endif()
    target_compile_options(${t} PRIVATE -Wfatal-errors -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wconversion -Wno-sign-conversion)
  endforeach()
endif(MSVC)

# workaround for GNU GCC poor AVX load/store code generation
if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$"))
  target_compile_options(ada-idna-hot PRIVATE -mno-avx256-split-unaligned-load -mno-avx256-split-unaligned-store)
  target_compile_options(ada-idna-cold PRIVATE -mno-avx256-split-unaligned-load -mno-avx256-split-unaligned-store)
endif()

if(ADA_SANITIZE)
  target_compile_options(ada-idna PUBLIC -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_compile_definitions(ada-idna PUBLIC ASAN_OPTIONS=detect_leaks=1)
  target_link_libraries(ada-idna PUBLIC -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
endif()
if(ADA_SANITIZE_WITHOUT_LEAKS)
  target_compile_options(ada-idna PUBLIC -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_link_libraries(ada-idna PUBLIC -fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
endif()
if(ADA_SANITIZE_UNDEFINED)
  add_compile_options(ada-idna PUBLIC -fsanitize=undefined -fno-sanitize-recover=all)
  add_link_options(ada-idna PUBLIC -fsanitize=undefined)
endif()

if(BUILD_TESTING)
  target_compile_definitions(ada-idna-hot PRIVATE ADA_TESTING=1)
  target_compile_definitions(ada-idna-cold PRIVATE ADA_TESTING=1)
endif()
