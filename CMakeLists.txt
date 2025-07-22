# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)


# Find libpcap
pkg_check_modules(PCAP REQUIRED libpcap)
include_directories(${PCAP_INCLUDE_DIRS})
link_directories(${PCAP_LIBRARY_DIRS})

# Find libsystemd
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
include_directories(${SYSTEMD_INCLUDE_DIRS})
link_directories(${SYSTEMD_LIBRARY_DIRS})

find_package(Boost REQUIRED)

# Include Boost headers
include_directories(${Boost_INCLUDE_DIRS})

# Find SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

include_directories(${SQLITE3_INCLUDE_DIRS})
link_directories(${SQLITE3_LIBRARY_DIRS})

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Include External Project module
include(ExternalProject)

#####################################################################
# Fetch and build mongo-cxx
#####################################################################
set(MONGO_CXX_VERSION "r4.1.0")
set(MONGO_CXX_INSTALL_DIR "${CMAKE_BINARY_DIR}/mongo-cxx-driver-install")
ExternalProject_Add(mongo-cxx-driver
  URL https://github.com/mongodb/mongo-cxx-driver/releases/download/${MONGO_CXX_VERSION}/mongo-cxx-driver-${MONGO_CXX_VERSION}.tar.gz
  PREFIX ${CMAKE_BINARY_DIR}/mongo-cxx-driver
  SOURCE_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src
  BINARY_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src/build
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_STANDARD=17
    -DBUILD_SHARED_LIBS=ON 
    -DCMAKE_INSTALL_PREFIX=${MONGO_CXX_INSTALL_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} --build .
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
)


# MongoDB C++ Driver include and lib directories
set(MONGOCXX_INCLUDE_DIRS "${MONGO_CXX_INSTALL_DIR}/include/")
set(BSONCXX_INCLUDE_DIRS  "${MONGO_CXX_INSTALL_DIR}/include/")
set(BSONCXX_V_NOABI_INCLUDE_DIR "${MONGO_CXX_INSTALL_DIR}/include/bsoncxx/v_noabi")
set(MONGOCXX_V_NOABI_INCLUDE_DIR "${MONGO_CXX_INSTALL_DIR}/include/mongocxx/v_noabi")
set(MONGO_CXX_LIBRARY_DIR "${MONGO_CXX_INSTALL_DIR}/lib")

# Libraries
set(MONGOCXX_LIBRARIES "${MONGO_CXX_LIBRARY_DIR}/libmongocxx.so")
set(BSONCXX_LIBRARIES  "${MONGO_CXX_LIBRARY_DIR}/libbsoncxx.so")

# Include Fetch Content module 
include(FetchContent)

#####################################################################
# Fetch and build ndn-cxx
#####################################################################
FetchContent_Declare(
  ndn-cxx
  GIT_REPOSITORY https://github.com/named-data/ndn-cxx.git
  GIT_TAG master
  GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(ndn-cxx)
if(NOT ndn-cxx_POPULATED)
  FetchContent_Populate(ndn-cxx)

  add_custom_target(ndn-cxx-build ALL
    COMMAND ./waf configure --prefix=${CMAKE_BINARY_DIR}/ndn-cxx/install
    COMMAND ./waf configure
        --prefix=${CMAKE_BINARY_DIR}/ndn-cxx/install
        --libdir=${CMAKE_BINARY_DIR}/ndn-cxx/install/lib
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
  GIT_SHALLOW          TRUE
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
  GIT_SHALLOW    TRUE
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

if(NOT BASHRC_CONTENT MATCHES "${MONGO_CXX_LIBRARY_DIR}")
  file(APPEND "${BASHRC}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${MONGO_CXX_LIBRARY_DIR}\n")
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

###################################################################### 
# Fetch and add nlohmann/json (single-header JSON library)
###################################################################### 
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
  FetchContent_Populate(nlohmann_json)
  add_subdirectory(
    ${nlohmann_json_SOURCE_DIR}
    ${nlohmann_json_BINARY_DIR}
    EXCLUDE_FROM_ALL
  )
endif()


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
set(SNDS_BROKER_DIR  ${CMAKE_SOURCE_DIR}/src/snds_broker)
set(SNDS_INTEREST_DIR ${CMAKE_SOURCE_DIR}/src/snds_interest)

#####################################################################
# Build the parsed interest static library
#####################################################################

set(SNDS_INTEREST_SOURCES
  ${SNDS_INTEREST_DIR}/parsed_interest_name.cpp
)

add_library(snds_interest STATIC ${SNDS_INTEREST_SOURCES})

target_include_directories(snds_interest
  PUBLIC
    ${SNDS_INTEREST_DIR}
    ${NDN_CXX_INCLUDE_DIR}
)

target_link_libraries(snds_interest
  PUBLIC
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
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
target_link_libraries(snds_utils PUBLIC 
  spdlog::spdlog 
  OpenSSL::Crypto
)

#####################################################################
# Build the snds_broker static library (MongoDB driver required)
#####################################################################

# Broker sources
set(SNDS_BROKER_SOURCES
    ${SNDS_BROKER_DIR}/broker.cpp
    ${SNDS_BROKER_DIR}/broker_utils.cpp
)

# Broker main executable source
set(SNDS_BROKER_MAIN
    ${SNDS_BROKER_DIR}/broker_main.cpp
)

# Temporal query module sources
set(SNDS_BROKER_TIMEFRAMES_SOURCES
    ${SNDS_BROKER_DIR}/temporal_query.cpp
)

# Build the 'temporal_query' static library
add_library(temporal_query STATIC ${SNDS_BROKER_TIMEFRAMES_SOURCES})

target_include_directories(temporal_query PUBLIC
  ${SNDS_UTILS_DIR}
  ${SNDS_BROKER_DIR}
)

target_link_libraries(temporal_query PUBLIC
  snds_utils
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)


# Build the 'snds_broker' static library and link temporal_query
add_library(snds_broker STATIC ${SNDS_BROKER_SOURCES})

# Include directories for Mongo, httplib, and broker headers
target_include_directories(snds_broker PUBLIC
    ${SNDS_BROKER_DIR}
    ${cpp-httplib_SOURCE_DIR}
    ${MONGO_CXX_INSTALL_DIR}/include
    ${MONGO_CXX_INSTALL_DIR}/include/bsoncxx/v_noabi
    ${MONGO_CXX_INSTALL_DIR}/include/mongocxx/v_noabi
    ${OPENSSL_INCLUDE_DIR}
)

# Link required libraries
target_link_libraries(snds_broker PUBLIC
    ${MONGO_CXX_INSTALL_DIR}/lib/libbsoncxx.so
    ${MONGO_CXX_INSTALL_DIR}/lib/libmongocxx.so
    temporal_query
    spdlog::spdlog
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    snds_utils
)

# Create the broker executable
add_executable(snds_broker_bin ${SNDS_BROKER_MAIN})

# Include broker headers and httplib in the executable
target_include_directories(snds_broker_bin PRIVATE
    ${SNDS_BROKER_DIR}
    ${cpp-httplib_SOURCE_DIR}
)

# Link broker static library to the executable
target_link_libraries(snds_broker_bin PRIVATE
    snds_broker
)

# Add dependency on Mongo driver build
add_dependencies(snds_broker_bin mongo-cxx-driver)

set(BROKER_RUNNER_PATH "${CMAKE_BINARY_DIR}/run-broker.sh")

file(WRITE "${BROKER_RUNNER_PATH}" "#!/bin/bash\n")
file(APPEND "${BROKER_RUNNER_PATH}" "# Auto-generated by CMake - run broker with correct MongoDB lib path\n")
file(APPEND "${BROKER_RUNNER_PATH}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${MONGO_CXX_LIBRARY_DIR}\n")
file(APPEND "${BROKER_RUNNER_PATH}" "exec ${CMAKE_BINARY_DIR}/snds_broker_bin \"\$@\"\n")

file(CHMOD "${BROKER_RUNNER_PATH}" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
)

message(STATUS "🛠  Created helper script to run broker: ${BROKER_RUNNER_PATH}")
message(STATUS "👉  You can run it like this: ./run-broker.sh")

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
# Build the snds_service static library (excluding main.cpp)
#####################################################################
set(SNDS_SERVICE_SOURCES
    ${SNDS_SERVICE_DIR}/snds_service_base.cpp
    ${SNDS_SERVICE_DIR}/snds_http_server.cpp
    ${SNDS_SERVICE_DIR}/snds_worker.cpp
    ${SNDS_SERVICE_DIR}/snds_producer.cpp
    ${SNDS_SERVICE_DIR}/snds_consumer.cpp
    ${SNDS_SERVICE_DIR}/snds_digital_twin.cpp
)

add_library(snds_service STATIC 
    ${SNDS_SERVICE_SOURCES}
)

target_include_directories(snds_service PUBLIC
    ${SNDS_SERVICE_DIR}
    ${NDN_CXX_INCLUDE_DIR}        # for ndn-cxx headers
    ${cpp-httplib_SOURCE_DIR}       # for cpp-httplib header
    ${SNDS_UTILS_DIR}
    ${SNDS_BROKER_DIR}
    ${SNDS_INTEREST_DIR}
)

target_link_libraries(snds_service PRIVATE
    snds_utils
    snds_interest
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    snds_broker
)

add_dependencies(snds_service ndn-cxx-build snds_broker)

#####################################################################
# Build the main executable that runs the daemon
#####################################################################
# Compile main.cpp only in the executable.
add_executable(snds_daemon_bin ${SNDS_SERVICE_DIR}/main.cpp)

target_include_directories(snds_daemon_bin PRIVATE
    ${SNDS_SERVICE_DIR}
    ${NDN_CXX_INCLUDE_DIR}
    ${SNDS_UTILS_DIR}
    ${SNDS_BROKER_DIR} 
)

target_link_libraries(snds_daemon_bin PRIVATE
    snds_service
    message_queue
    ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    spdlog::spdlog
    snds_broker
    nlohmann_json::nlohmann_json
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
