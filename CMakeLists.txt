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

  # Configure, build, and install ndn-cxx using add_custom_target
  add_custom_target(ndn-cxx-build ALL
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf configure --prefix=${ndn-cxx_BINARY_DIR}/install
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf
    COMMAND python3 ${ndn-cxx_SOURCE_DIR}/waf install
    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
  )

  # Add the include and library directories of ndn-cxx
  set(NDN_CXX_INCLUDE_DIR ${ndn-cxx_BINARY_DIR}/install/include)
  set(NDN_CXX_LIBRARY_DIR ${ndn-cxx_BINARY_DIR}/install/lib)

  # Include ndn-cxx in the build
  include_directories(${NDN_CXX_INCLUDE_DIR})
  link_directories(${NDN_CXX_LIBRARY_DIR})
endif()

# Add source files for your project
set(SOURCES
    src/snds_service.cpp
)

# Add executable
add_executable(SeEDS ${SOURCES})

# Link ndn-cxx library to your target
target_include_directories(SeEDS PRIVATE ${NDN_CXX_INCLUDE_DIR})
target_link_libraries(SeEDS PRIVATE ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so)
add_dependencies(SeEDS ndn-cxx-build)
