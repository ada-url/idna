#
# Amalgamation
#
set(SINGLEHEADER_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ada_idna.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/ada_idna.h
  ${CMAKE_CURRENT_BINARY_DIR}/demo.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/README.md
)
set_source_files_properties(${SINGLEHEADER_FILES} PROPERTIES GENERATED TRUE)

# In theory, this is unneeded, because the tests module does the same test:
find_package (Python3 COMPONENTS Interpreter)

if (Python3_Interpreter_FOUND)
  MESSAGE( STATUS "Python found, we are going to amalgamate.py." )

  add_custom_command(
    OUTPUT ${SINGLEHEADER_FILES}
    COMMAND ${CMAKE_COMMAND} -E env
      AMALGAMATE_SOURCE_PATH=${PROJECT_SOURCE_DIR}/src
      AMALGAMATE_INPUT_PATH=${PROJECT_SOURCE_DIR}/include
      AMALGAMATE_OUTPUT_PATH=${CMAKE_CURRENT_BINARY_DIR}
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/amalgamate.py
      DEPENDS amalgamate.py
  )
  add_custom_target(singleheader-files DEPENDS ${SINGLEHEADER_FILES})

  #
  # Include this if you intend to #include "ada_idna.cpp" in your own .cpp files.
  #
  add_library(ada-idna-singleheader-include-source INTERFACE)
  target_include_directories(ada-idna-singleheader-include-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  add_dependencies(ada-idna-singleheader-include-source singleheader-files)

  add_library(ada-idna-singleheader-source INTERFACE)
  target_sources(ada-idna-singleheader-source INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ada_idna.cpp>)
  target_link_libraries(ada-idna-singleheader-source INTERFACE ada-idna-singleheader-include-source)
  if (BUILD_TESTING)
    add_executable(demo $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/demo.cpp>)
    target_link_libraries(demo ada-idna-singleheader-include-source)

    add_test(demo demo)
  endif()
else()
  MESSAGE( STATUS "Python not found, we are unable to test amalgamate.py." )
endif()