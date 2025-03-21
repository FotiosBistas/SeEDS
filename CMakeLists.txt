# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

enable_testing()

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
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

#####################################################################
# Set up common include directories for custom headers
#####################################################################
set(SNDS_MESSAGE_DIR ${CMAKE_SOURCE_DIR}/src/snds_message)
set(SNDS_QUEUE_DIR   ${CMAKE_SOURCE_DIR}/src/snds_queue)
set(SNDS_SERVICE_DIR ${CMAKE_SOURCE_DIR}/src/snds_service)
set(SNDS_UTILS_DIR   ${CMAKE_SOURCE_DIR}/src/snds_utils)

#####################################################################
# Build the message_queue static library
#####################################################################
set(MESSAGE_QUEUE_SOURCES
    ${SNDS_QUEUE_DIR}/message_queue.cpp
)

add_library(message_queue STATIC ${MESSAGE_QUEUE_SOURCES})

# Public include directories for message_queue
target_include_directories(message_queue PUBLIC
    ${SNDS_MESSAGE_DIR}
    ${SNDS_QUEUE_DIR}
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
    ${SNDS_UTILS_DIR}/snds_utils.cpp
)

add_library(snds_utils STATIC ${SNDS_UTILS_SOURCES})
target_include_directories(snds_utils PUBLIC ${SNDS_UTILS_DIR})

#####################################################################
# Build the snds_service static library (excluding main.cpp)
#####################################################################
set(SNDS_SERVICE_SOURCES
    ${SNDS_SERVICE_DIR}/snds_service_base.cpp
    ${SNDS_SERVICE_DIR}/snds_http_service.cpp
    ${SNDS_SERVICE_DIR}/snds_http_worker.cpp
    ${SNDS_SERVICE_DIR}/snds_producer.cpp
    ${SNDS_SERVICE_DIR}/snds_consumer.cpp
    ${SNDS_SERVICE_DIR}/snds_digital_twin.cpp
    ${SNDS_SERVICE_DIR}/old_snds_service.cpp
)

add_library(snds_service STATIC ${SNDS_SERVICE_SOURCES})
target_include_directories(snds_service PUBLIC
    ${SNDS_SERVICE_DIR}
    ${NDN_CXX_INCLUDE_DIR}        # for ndn-cxx headers
    ${cpp-httplib_SOURCE_DIR}       # for cpp-httplib header
    ${SNDS_UTILS_DIR}
)
target_link_libraries(snds_service PRIVATE
    message_queue
    snds_utils
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)
add_dependencies(snds_service ndn-cxx-build)

#####################################################################
# Build the main executable that runs the daemon
#####################################################################
# Compile main.cpp only in the executable.
add_executable(snds_daemon_bin ${SNDS_SERVICE_DIR}/main.cpp)
target_include_directories(snds_daemon_bin PRIVATE
    ${SNDS_SERVICE_DIR}
    ${NDN_CXX_INCLUDE_DIR}
    ${SNDS_UTILS_DIR}
)
target_link_libraries(snds_daemon_bin PRIVATE
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
    ${SNDS_UTILS_DIR}/test_snds_utils.cpp
)
add_executable(test_snds_utils ${SNDS_UTILS_TEST_SOURCES})
target_include_directories(test_snds_utils PRIVATE
    ${SNDS_UTILS_DIR}
    ${NDN_CXX_INCLUDE_DIR}
)
target_link_libraries(test_snds_utils PRIVATE
    snds_utils
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)
add_dependencies(test_snds_utils ndn-cxx-build)

#####################################################################
# Find and configure GoogleTest and fmt
#####################################################################
#find_package(GTest REQUIRED)
#find_package(fmt REQUIRED)
#
######################################################################
## Build the test executable for producer tests
######################################################################
#add_executable(test_snds_producer tests/test_snds_producer.cpp)
#target_include_directories(test_snds_producer PRIVATE
#    ${SNDS_SERVICE_DIR}
#    ${SNDS_MESSAGE_DIR}
#    ${SNDS_QUEUE_DIR}
#    ${NDN_CXX_INCLUDE_DIR}
#    ${SNDS_UTILS_DIR}
#)
#target_link_libraries(test_snds_producer PRIVATE
#    gtest
#    gtest_main
#    snds_service
#    message_queue
#    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
#    pthread
#    fmt::fmt
#)
#add_dependencies(test_snds_producer ndn-cxx-build)
#add_test(NAME ProducerTests COMMAND test_snds_producer)
#
######################################################################
## Build the test executable for consumer tests
######################################################################
#add_executable(test_snds_consumer tests/test_snds_consumer.cpp)
#target_include_directories(test_snds_consumer PRIVATE
#    ${SNDS_SERVICE_DIR}
#    ${SNDS_MESSAGE_DIR}
#    ${SNDS_QUEUE_DIR}
#    ${NDN_CXX_INCLUDE_DIR}
#    ${SNDS_UTILS_DIR}
#)
#target_link_libraries(test_snds_consumer PRIVATE
#    gtest
#    gtest_main
#    snds_service
#    message_queue
#    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
#    pthread
#    fmt::fmt
#)
#add_dependencies(test_snds_consumer ndn-cxx-build)
#add_test(NAME ConsumerTests COMMAND test_snds_consumer)

#####################################################################
# Build the snds_service standalone executable
#####################################################################
add_executable(old_snds_service_bin ${SNDS_SERVICE_DIR}/old_snds_service.cpp)

target_include_directories(old_snds_service_bin PRIVATE
    ${SNDS_SERVICE_DIR}
    ${NDN_CXX_INCLUDE_DIR}
    ${cpp-httplib_SOURCE_DIR}
    ${SNDS_UTILS_DIR}
)

target_link_libraries(old_snds_service_bin PRIVATE
    snds_service
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)

add_dependencies(old_snds_service_bin ndn-cxx-build)