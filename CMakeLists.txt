# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Fetch ndn-cxx repository
FetchContent_Declare(
  ndn-cxx
  GIT_REPOSITORY https://github.com/named-data/ndn-cxx.git
  GIT_TAG master
  SOURCE_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/src
)

FetchContent_GetProperties(ndn-cxx)
if(NOT ndn-cxx_POPULATED)
  FetchContent_Populate(ndn-cxx)

  # Add custom target to build and install ndn-cxx using the waf build system
  add_custom_target(ndn-cxx-build ALL
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf configure --prefix=${ndn-cxx_BINARY_DIR}/install
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf install
    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
  )
endif()

# Add the include and library directories for ndn-cxx
set(NDN_CXX_INCLUDE_DIR ${ndn-cxx_BINARY_DIR}/install/include)
set(NDN_CXX_LIBRARY_DIR ${ndn-cxx_BINARY_DIR}/install/lib)
include_directories(${NDN_CXX_INCLUDE_DIR})
link_directories(${NDN_CXX_LIBRARY_DIR})

# Include the directory where the custom header file is located
include_directories(src/snds_messages)

# Add source files for the example
set(EXAMPLE_SOURCES
    src/snds_messages/example_snds_messages.cpp
)

# Add the executable for the example
add_executable(example_snds_messages ${EXAMPLE_SOURCES})

# Link ndn-cxx library to the example executable
target_link_libraries(example_snds_messages PRIVATE ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so)

# Ensure ndn-cxx is built before the example executable
add_dependencies(example_snds_messages ndn-cxx-build)