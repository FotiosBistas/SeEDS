# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

# Find libpcap
pkg_check_modules(PCAP REQUIRED libpcap)
include_directories(${PCAP_INCLUDE_DIRS})
link_directories(${PCAP_LIBRARY_DIRS})

# Find libsystemd
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
include_directories(${SYSTEMD_INCLUDE_DIRS})
link_directories(${SYSTEMD_LIBRARY_DIRS})

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
  SOURCE_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/
)

FetchContent_GetProperties(ndn-cxx)
if(NOT ndn-cxx_POPULATED)
  FetchContent_Populate(ndn-cxx)

  add_custom_target(ndn-cxx-build ALL
    COMMAND ./waf configure --prefix=${CMAKE_BINARY_DIR}/ndn-cxx/install
    COMMAND ./waf
    COMMAND ./waf install
    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
    COMMENT "Building ndn-cxx via waf"
  )
endif()

set(NDN_CXX_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/install/include)
set(NDN_CXX_LIBRARY_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/install/lib)
#####################################################################
# Fetch and build NFD
#####################################################################

FetchContent_Declare(
  nfd
  GIT_REPOSITORY https://github.com/named-data/NFD.git
  GIT_TAG master
  GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_GetProperties(nfd)
if(NOT nfd_POPULATED)
  FetchContent_Populate(nfd)

  message(STATUS "NFD source dir: ${nfd_SOURCE_DIR}")

  add_custom_target(nfd-build ALL
    COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/ndn-cxx/install/lib/pkgconfig
            ./waf configure --prefix=${CMAKE_BINARY_DIR}/NFD/install
    COMMAND ./waf
    COMMAND ./waf install
    DEPENDS ndn-cxx-build
    WORKING_DIRECTORY ${nfd_SOURCE_DIR}
    COMMENT "Building NFD via waf"
  )

endif()

#####################################################################
# Fetch and build ndn-tools
#####################################################################
FetchContent_Declare(
  ndn-tools
  GIT_REPOSITORY https://github.com/named-data/ndn-tools.git
  GIT_TAG master
  SOURCE_DIR ${CMAKE_BINARY_DIR}/ndn-tools/
)

FetchContent_GetProperties(ndn-tools)
if(NOT ndn-tools_POPULATED)
  FetchContent_Populate(ndn-tools)

  add_custom_target(ndn-tools-build ALL
    COMMAND ${CMAKE_COMMAND} -E env
      PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/ndn-cxx/install/lib/pkgconfig
      ./waf configure
    COMMAND ./waf
    COMMAND sudo ./waf install
    DEPENDS ndn-cxx-build
    WORKING_DIRECTORY ${ndn-tools_SOURCE_DIR}
    COMMENT "Building ndn-tools via waf"
  )
endif()

#####################################################################
# Export environment paths for NFD and ndn-cxx
#####################################################################
set(NFD_BIN_DIR "${CMAKE_BINARY_DIR}/NFD/install/bin")
set(NDN_CXX_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/bin")
set(NDN_CXX_LIB_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/lib")
set(NDN_TOOLS_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-tools/install/bin")
set(BASHRC "$ENV{HOME}/.bashrc")

# Only update .bashrc if needed
file(READ "${BASHRC}" BASHRC_CONTENT)

# Append PATH
if(NOT BASHRC_CONTENT MATCHES "${NFD_BIN_DIR}")
  file(APPEND "${BASHRC}" "\n# Added by SeEDS CMake for local NFD build\n")
  file(APPEND "${BASHRC}" "export PATH=\$PATH:${NFD_BIN_DIR}:${NDN_CXX_BIN_DIR}\n")
endif()


if(NOT BASHRC_CONTENT MATCHES "${NDN_TOOLS_BIN_DIR}")
  file(APPEND "${BASHRC}" "export PATH=\$PATH:${NDN_TOOLS_BIN_DIR}\n")
endif()

# Append LD_LIBRARY_PATH
if(NOT BASHRC_CONTENT MATCHES "${NDN_CXX_LIB_DIR}")
  file(APPEND "${BASHRC}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${NDN_CXX_LIB_DIR}\n")
endif()

# Copy nfd.conf.sample → nfd.conf if it doesn’t exist yet
set(NFD_CONF_SRC "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn/nfd.conf.sample")
set(NFD_CONF_DEST "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn/nfd.conf")
if(EXISTS "${NFD_CONF_SRC}" AND NOT EXISTS "${NFD_CONF_DEST}")
  file(COPY "${NFD_CONF_SRC}" DESTINATION "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn")
  file(RENAME "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn/nfd.conf.sample" "${NFD_CONF_DEST}")
endif()

message(STATUS "✅ Exported NFD and ndn-cxx to PATH and LD_LIBRARY_PATH in ~/.bashrc")
message(STATUS "👉 Please run: source ~/.bashrc")

# Ensure nfd.conf exists by copying from nfd.conf.sample
set(NFD_CONF_DIR "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn")
set(NFD_CONF_SRC "${NFD_CONF_DIR}/nfd.conf.sample")
set(NFD_CONF_DEST "${NFD_CONF_DIR}/nfd.conf")

if(EXISTS "${NFD_CONF_SRC}" AND NOT EXISTS "${NFD_CONF_DEST}")
  file(COPY "${NFD_CONF_SRC}" DESTINATION "${NFD_CONF_DIR}")
  file(RENAME "${NFD_CONF_SRC}" "${NFD_CONF_DEST}")
endif()

# Add convenience shell script to launch NFD with correct LD_LIBRARY_PATH
set(NFD_RUNNER_PATH "${CMAKE_BINARY_DIR}/run-nfd.sh")

file(WRITE "${NFD_RUNNER_PATH}" "#!/bin/bash\n")
file(APPEND "${NFD_RUNNER_PATH}" "# Auto-generated by CMake - run NFD with local ndn-cxx lib\n")
file(APPEND "${NFD_RUNNER_PATH}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${NDN_CXX_LIB_DIR}\n")
file(APPEND "${NFD_RUNNER_PATH}" "exec ${CMAKE_BINARY_DIR}/NFD/install/bin/nfd \"\$@\"\n")

# Set executable permissions (755)
file(CHMOD "${NFD_RUNNER_PATH}" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
)

message(STATUS "🛠  Created helper script to run NFD: ${NFD_RUNNER_PATH}")
message(STATUS "👉  You can run it like this: sudo ./run-nfd.sh")


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
target_include_directories(snds_utils PUBLIC
  ${SNDS_UTILS_DIR}
  ${spdlog_SOURCE_DIR}/include
)
target_link_libraries(snds_utils PUBLIC spdlog::spdlog)


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

#####################################################################
# Build standalone consumer executable
#####################################################################
add_executable(consumer_bin ${CMAKE_SOURCE_DIR}/src/consumer.cpp)

target_include_directories(consumer_bin PRIVATE
    ${SNDS_SERVICE_DIR}
    ${SNDS_QUEUE_DIR}
    ${SNDS_MESSAGE_DIR}
    ${NDN_CXX_INCLUDE_DIR}
    ${cpp-httplib_SOURCE_DIR}
    ${SNDS_UTILS_DIR}
)

target_link_libraries(consumer_bin PRIVATE
    snds_service
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)

add_dependencies(consumer_bin ndn-cxx-build)

#####################################################################
# Build standalone producer executable
#####################################################################
add_executable(producer_bin ${CMAKE_SOURCE_DIR}/src/producer.cpp)

target_include_directories(producer_bin PRIVATE
    ${SNDS_SERVICE_DIR}
    ${SNDS_QUEUE_DIR}
    ${SNDS_MESSAGE_DIR}
    ${NDN_CXX_INCLUDE_DIR}
    ${cpp-httplib_SOURCE_DIR}
    ${SNDS_UTILS_DIR}
)

target_link_libraries(producer_bin PRIVATE
    snds_service
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
)

add_dependencies(producer_bin ndn-cxx-build)