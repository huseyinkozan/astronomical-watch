idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "")

# ESP-IDF defaults to the newest C++ standard; this project is written in C++17.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
