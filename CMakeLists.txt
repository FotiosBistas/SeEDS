# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

enable_testing()

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

#####################################################################
# Fetch and build ndn-cxx
#####################################################################
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

# Set ndn-cxx include and library directories.
set(NDN_CXX_INCLUDE_DIR ${ndn-cxx_BINARY_DIR}/install/include)
set(NDN_CXX_LIBRARY_DIR ${ndn-cxx_BINARY_DIR}/install/lib)
include_directories(${NDN_CXX_INCLUDE_DIR})
link_directories(${NDN_CXX_LIBRARY_DIR})

#####################################################################
# Fetch and add spdlog (thread-safe logging library)
#####################################################################
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.11.0
)

FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  FetchContent_Populate(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

#####################################################################
# Fetch and add cpp-httplib (header-only HTTP server library)
#####################################################################
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
)
FetchContent_GetProperties(cpp-httplib)
if(NOT cpp-httplib_POPULATED)
  FetchContent_Populate(cpp-httplib)
endif()
# Include cpp-httplib header directory.
include_directories(${cpp-httplib_SOURCE_DIR})

#####################################################################
# Include directories for custom header files
#####################################################################
# Adjust these paths to match your folder structure.
include_directories(src/snds_message)
include_directories(src/snds_queue)
include_directories(src/snds_service)
include_directories(src/snds_utils)   # Include snds_utils headers

#####################################################################
# Build the message_queue static library
#####################################################################
set(MESSAGE_QUEUE_SOURCES
    src/snds_queue/message_queue.cpp
)

add_library(message_queue STATIC ${MESSAGE_QUEUE_SOURCES})

# Public include directories for message_queue
target_include_directories(message_queue PUBLIC
    src/snds_message
    src/snds_queue
)

# Link ndn-cxx and spdlog to the message_queue library
target_link_libraries(message_queue
    PRIVATE
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)

#####################################################################
# Build the snds_utils static library
#####################################################################
set(SNDS_UTILS_SOURCES
    src/snds_utils/snds_utils.cpp
)

add_library(snds_utils STATIC ${SNDS_UTILS_SOURCES})
target_include_directories(snds_utils PUBLIC src/snds_utils)


#####################################################################
# Build the snds_service static library (including producer & consumer)
#####################################################################
set(SNDS_SERVICE_SOURCES
    src/snds_service/snds_service_base.cpp
    src/snds_service/snds_http_service.cpp
    src/snds_service/snds_http_worker.cpp
    src/snds_service/snds_producer.cpp
    src/snds_service/snds_consumer.cpp
    src/snds_service/snds_digital_twin.cpp
    src/snds_service/main.cpp
)

add_library(snds_service STATIC ${SNDS_SERVICE_SOURCES})
target_include_directories(snds_service PUBLIC src/snds_service)
# Link dependencies including message_queue and snds_utils
target_link_libraries(snds_service
    PRIVATE
    message_queue
    snds_utils
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)
add_dependencies(snds_service ndn-cxx-build)

#####################################################################
# Build the main executable that runs the daemon
#####################################################################
set(SNDS_DAEMON_SOURCES
    src/snds_service/main.cpp
)

add_executable(snds_daemon_bin ${SNDS_DAEMON_SOURCES})
target_link_libraries(snds_daemon_bin
    PRIVATE
    snds_service
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)
add_dependencies(snds_daemon_bin ndn-cxx-build)

#####################################################################
# Build the snds_utils test executable
#####################################################################
set(SNDS_UTILS_TEST_SOURCES
    src/snds_utils/test_snds_utils.cpp
    src/snds_utils/snds_utils.cpp
)

add_executable(test_snds_utils ${SNDS_UTILS_TEST_SOURCES})
target_include_directories(test_snds_utils PRIVATE src/snds_utils)
target_link_libraries(test_snds_utils
    PRIVATE
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)
add_dependencies(test_snds_utils ndn-cxx-build)

# Locate Google Test (requires GTest installed system-wide)
find_package(GTest REQUIRED)

# Add a test executable for producer tests
add_executable(test_snds_producer tests/test_snds_producer.cpp src/snds_service/snds_producer.cpp src/snds_service/snds_service_base.cpp)

# Include directories for the test
target_include_directories(test_snds_producer PRIVATE
    src/snds_service
    src/snds_message
    src/snds_queue
)

# Link libraries
target_link_libraries(test_snds_producer PRIVATE
    gtest
    gtest_main
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    pthread
)

# Ensure ndn-cxx is built before test
add_dependencies(test_snds_producer ndn-cxx-build)

# Register the test with CTest so "ctest" can run it
add_test(NAME ProducerTests COMMAND test_snds_producer)

find_package(fmt REQUIRED)
target_link_libraries(test_snds_producer PRIVATE fmt::fmt)

# Add a test executable for consumer tests
add_executable(test_snds_consumer tests/test_snds_consumer.cpp src/snds_service/snds_consumer.cpp src/snds_service/snds_service_base.cpp)

# Include directories for the test
target_include_directories(test_snds_consumer PRIVATE
    src/snds_service
    src/snds_message
    src/snds_queue
)

# Link libraries
target_link_libraries(test_snds_consumer PRIVATE
    gtest
    gtest_main
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    pthread
)

# Ensure ndn-cxx is built before test
add_dependencies(test_snds_consumer ndn-cxx-build)

# Register the test with CTest so "ctest" can run it
add_test(NAME ConsumerTests COMMAND test_snds_consumer)

find_package(fmt REQUIRED)
target_link_libraries(test_snds_consumer PRIVATE fmt::fmt)
