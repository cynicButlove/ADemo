cmake_minimum_required(VERSION 3.20)

project(DemoServer LANGUAGES CXX)

add_executable(DemoServer
    src/DemoServerMain.cpp
)

target_compile_features(DemoServer PRIVATE cxx_std_17)

target_compile_definitions(DemoServer PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(DemoServer PRIVATE /EHsc /W4)
endif()

target_link_libraries(DemoServer PRIVATE ws2_32)
