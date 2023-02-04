if(MSVC)
  add_compile_options("/Zi" "/EHsc" "/GR")
endif()

include(${PROJECT_SOURCE_DIR}/cmake/add-cpp-test.cmake)
link_libraries(ada-idna)

add_cpp_test(punycode_tests)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
target_link_libraries(punycode_tests PUBLIC stdc++fs)
endif()
endif()



file(GLOB_RECURSE test_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.txt)
foreach(test_files ${test_files})
  configure_file(${test_files} ${test_files} COPYONLY)
endforeach(test_files)