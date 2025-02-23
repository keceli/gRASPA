cmake_minimum_required(VERSION 3.18)
# Use nvc++ as the compiler.
set(CMAKE_CXX_COMPILER "nvc++")

project(nvc_main LANGUAGES CXX)

# Add option for building tests (OFF by default)
option(BUILD_TESTING "Build the testing tree." OFF)


# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define include directories
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
include_directories(${PROJECT_INCLUDE_DIRS})

# Define your custom compile flags as individual list items.
set(NVCFLAG_LIST
    -O3
    -std=c++17
    -target=gpu
    -Minline
    -fopenmp
    -cuda
    -stdpar=multicore
)

# Gather source files (.cpp and .cu) from the src directory.
file(GLOB CPP_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB CUDA_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cu")

# Force .cu files to be compiled as C++.
set_source_files_properties(${CUDA_SOURCES} PROPERTIES LANGUAGE CXX)

# Combine sources.
set(SOURCES ${CPP_SOURCES} ${CUDA_SOURCES})

# Create the executable target.
add_executable(nvc_main.x ${SOURCES})

# Pass the compile flags.
target_compile_options(nvc_main.x PRIVATE ${NVCFLAG_LIST})

# --- Linking the Offload Runtime in a Portable Way ---

# First, try to use the CUDAToolkit package.
find_package(CUDAToolkit)
if(TARGET CUDAToolkit::cudart)
  target_link_libraries(nvc_main.x PRIVATE CUDAToolkit::cudart)
else()
  message(WARNING "CUDAToolkit::cudart target not found. Falling back to manual find_library for cudart.")
  find_library(CUDART_LIB cudart HINTS
    "/opt/nvidia/hpc_sdk/Linux_x86_64/25.1/cuda/lib64"
    "/usr/local/cuda/lib64"
    "/usr/lib64"
    "/opt/local/lib/gcc11"
    "/usr/lib/gcc/x86_64-linux-gnu/11"
    "/opt/nvidia/hpc_sdk/Linux_x86_64/22.7/cuda/lib64"
    "/opt/nvidia/hpc_sdk/Linux_x86_64/23.9/cuda/lib64/"
  )
  if (NOT CUDART_LIB)
    message(FATAL_ERROR "Could not find the cudart library!")
  endif()
  target_link_libraries(nvc_main.x PRIVATE ${CUDART_LIB})
endif()

# Pass the offload flag during linking.
target_link_options(nvc_main.x PRIVATE -cuda)

# --- Link the experimental filesystem library ---
# For GCC 7 and similar compilers that only support std::experimental::filesystem,
# explicitly link the filesystem library.
target_link_libraries(nvc_main.x PRIVATE stdc++fs)

# Only build tests if BUILD_TESTING is ON
if(BUILD_TESTING)
    # Enable testing
    enable_testing()

    # Add the tests subdirectory
    add_subdirectory(tests)
endif()