# ESP-IDF project file. The two lines below must stay in this order.
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build only the components that main (and its dependencies) actually require.
idf_build_set_property(MINIMAL_BUILD ON)

project(astronomical_watch)
