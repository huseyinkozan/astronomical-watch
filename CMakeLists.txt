# ESP-IDF project file. The two lines below must stay in this order.
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build only the components that main (and its dependencies) actually require.
idf_build_set_property(MINIMAL_BUILD ON)

# Version shown on the About page (PROJECT_VER, read by project() below): the newest release tag
# or else the commit hash, "-dirty" with uncommitted changes. The wom-validator tag marks archived
# code, not a release. Taken when CMake configures the build (idf.py reconfigure refreshes it).
execute_process(COMMAND git describe --always --dirty --exclude wom-validator
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                OUTPUT_VARIABLE PROJECT_VER
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

project(astronomical_watch)
