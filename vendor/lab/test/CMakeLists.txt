
cmake_minimum_required(VERSION 2.8)

project(test)

add_executable(lab_test lab_test.cc)
target_link_libraries(lab_test lab)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(lab_test PRIVATE -W3 -EHsc)
else()
    target_compile_options(lab_test PRIVATE -Wall -Wno-missing-braces -std=c++11)
endif()

enable_testing()

add_test(NAME lab_test COMMAND lab_test)
