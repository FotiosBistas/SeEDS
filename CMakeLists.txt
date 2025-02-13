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

# Include the directories for custom header files
include_directories(src/snds_message)
include_directories(src/snds_queue)

# Add source files for the `message_queue` library
set(MESSAGE_QUEUE_SOURCES
    src/snds_queue/message_queue.cpp
)

# Add a static library for the message queue
add_library(message_queue STATIC ${MESSAGE_QUEUE_SOURCES})

# Add public include directories for the message_queue library
target_include_directories(message_queue PUBLIC src/snds_message src/snds_queue)

# Link the ndn-cxx library to the message_queue library
target_link_libraries(message_queue PRIVATE ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so)

# Add source files for the example
set(MESSAGE_QUEUE_EXAMPLE_SOURCES
    src/snds_queue/message_queue_example.cpp
)

# Add the executable for the example
add_executable(message_queue_example ${MESSAGE_QUEUE_EXAMPLE_SOURCES})

# Link the message_queue library and ndn-cxx to the example executable
target_link_libraries(message_queue_example PRIVATE message_queue ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so)

# Ensure ndn-cxx is built before the example executable
add_dependencies(message_queue_example ndn-cxx-build)
