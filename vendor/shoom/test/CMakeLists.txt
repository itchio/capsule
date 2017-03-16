
cmake_minimum_required(VERSION 2.8)

project(test)

add_executable(shoom_test shoom_test.cc)
target_link_libraries(shoom_test shoom)

if (${CMAKE_GENERATOR} MATCHES "Visual")
    add_compile_options(-W3 -EHsc)
else()
    add_compile_options(-Wall -Wno-missing-braces)

    target_compile_options(shoom_test PUBLIC -std=c++11)
endif()

enable_testing()

add_test(NAME shoom_test COMMAND shoom_test)
