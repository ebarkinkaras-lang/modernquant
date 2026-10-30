cmake_minimum_required(VERSION 3.20)
project(ModernQuantSuite
    VERSION 2.0.0
    DESCRIPTION "High-Performance Quantitative & Algorithmic Trading Terminal in C++20"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings & Flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wno-deprecated-declarations -Wno-deprecated-builtins)
endif()

# Find Dependencies
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

# Eigen3 Discovery
find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT Eigen3_FOUND)
    # Check common fallback locations
    find_path(EIGEN3_INCLUDE_DIR NAMES signature_of_eigen3_matrix_library
        PATHS
        /usr/include/eigen3
        /usr/local/include/eigen3
        /opt/homebrew/include/eigen3
        $ENV{CONDA_PREFIX}/include/eigen3
        $ENV{HOME}/anaconda3/include/eigen3
    )
    if(EIGEN3_INCLUDE_DIR)
        add_library(Eigen3::Eigen INTERFACE IMPORTED)
        set_target_properties(Eigen3::Eigen PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "Eigen3 not found! Please install libeigen3-dev or brew install eigen.")
    endif()
endif()

# Boost Discovery (Header-only components: Accumulators, MultiArray, Math)
find_package(Boost QUIET)
if(NOT Boost_FOUND)
    find_path(BOOST_INCLUDE_DIR NAMES boost/version.hpp
        PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        $ENV{CONDA_PREFIX}/include
        $ENV{HOME}/anaconda3/include
    )
    if(BOOST_INCLUDE_DIR)
        add_library(Boost::boost INTERFACE IMPORTED)
        set_target_properties(Boost::boost PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${BOOST_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "Boost headers not found! Please install libboost-all-dev or brew install boost.")
    endif()
endif()

# Project Targets
add_executable(quant_suite src/main.cpp)

target_include_directories(quant_suite PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(quant_suite PRIVATE
    Eigen3::Eigen
    Boost::boost
    CURL::libcurl
    Threads::Threads
)

set_target_properties(quant_suite PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
