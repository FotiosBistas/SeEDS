cmake_minimum_required(VERSION 3.30)
project(SeEDS LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
include(ExternalProject)
include(FetchContent)

pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

find_path(VALGRIND_INCLUDE_DIR NAMES valgrind/valgrind.h)

if(NOT VALGRIND_INCLUDE_DIR)
    message(FATAL_ERROR "Valgrind development headers are required. ")
endif()


pkg_check_modules(PCAP QUIET IMPORTED_TARGET libpcap)

if(NOT PCAP_FOUND)
    message(FATAL_ERROR "libpcap development files are required.\n")
endif()



#####################################################################
# Fetch and build mongo-c-driver
#####################################################################
set(MONGO_C_DRIVER_VERSION "2.1.0") # or newer stable
set(MONGO_C_INSTALL_DIR "${CMAKE_BINARY_DIR}/mongo-c-driver-install")
ExternalProject_Add(mongo-c-driver
    URL https://github.com/mongodb/mongo-c-driver/releases/download/${MONGO_C_DRIVER_VERSION}/mongo-c-driver-${MONGO_C_DRIVER_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${CMAKE_BINARY_DIR}/mongo-c-driver
    SOURCE_DIR ${CMAKE_BINARY_DIR}/mongo-c-driver-src
    BINARY_DIR ${CMAKE_BINARY_DIR}/mongo-c-driver-src/build
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${MONGO_C_INSTALL_DIR}
        -DENABLE_SRV=ON              # <— required for mongodb+srv
        -DENABLE_SSL=OPENSSL         # <— Atlas requires TLS
        -DCMAKE_INSTALL_LIBDIR=lib
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
)

#####################################################################
# Fetch and build mongo-cxx
#####################################################################
set(MONGO_CXX_VERSION "r4.1.2")
set(MONGO_CXX_INSTALL_DIR "${CMAKE_BINARY_DIR}/mongo-cxx-driver-install")
ExternalProject_Add(mongo-cxx-driver
    URL https://github.com/mongodb/mongo-cxx-driver/releases/download/${MONGO_CXX_VERSION}/mongo-cxx-driver-${MONGO_CXX_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${CMAKE_BINARY_DIR}/mongo-cxx-driver
    SOURCE_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src
    BINARY_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src/build
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DENABLE_SRV=ON
        -DCMAKE_CXX_STANDARD=17
        -DBUILD_SHARED_LIBS=ON
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_INSTALL_PREFIX=${MONGO_CXX_INSTALL_DIR}
        -DCMAKE_PREFIX_PATH=${MONGO_C_INSTALL_DIR}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install
)

add_dependencies(mongo-cxx-driver mongo-c-driver)

# MongoDB C++ Driver include and lib directories
set(MONGOCXX_INCLUDE_DIRS "${MONGO_CXX_INSTALL_DIR}/include/")
set(BSONCXX_INCLUDE_DIRS  "${MONGO_CXX_INSTALL_DIR}/include/")
set(BSONCXX_V_NOABI_INCLUDE_DIR "${MONGO_CXX_INSTALL_DIR}/include/bsoncxx/v_noabi")
set(MONGOCXX_V_NOABI_INCLUDE_DIR "${MONGO_CXX_INSTALL_DIR}/include/mongocxx/v_noabi")
set(MONGO_CXX_LIBRARY_DIR "${MONGO_CXX_INSTALL_DIR}/lib")

# Libraries (full paths)
set(MONGOCXX_LIBRARIES "${MONGO_CXX_LIBRARY_DIR}/libmongocxx.so")
set(BSONCXX_LIBRARIES  "${MONGO_CXX_LIBRARY_DIR}/libbsoncxx.so")
#####################################################################
# Fetch and build SQLite3
#####################################################################
set(SQLITE_VERSION "3.51.2")
set(SQLITE_AUTOCONF_VERSION "3510200")
set(SQLITE_INSTALL_DIR "${CMAKE_BINARY_DIR}/sqlite-install")

ExternalProject_Add(sqlite-local
    URL "https://www.sqlite.org/2026/sqlite-autoconf-${SQLITE_AUTOCONF_VERSION}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${CMAKE_BINARY_DIR}/sqlite"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/sqlite-src"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${SQLITE_INSTALL_DIR}
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
)
#####################################################################
# Fetch and build Boost
#####################################################################
#####################################################################
# Fetch and build Boost locally
#####################################################################

set(BOOST_VERSION "1.85.0")
set(BOOST_VERSION_UNDERSCORE "1_85_0")

set(BOOST_INSTALL_DIR "${CMAKE_BINARY_DIR}/boost-install")

ExternalProject_Add(boost-local
    URL "https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz"
    URL_HASH SHA256=be0d91732d5b0cc6fbb275c7939974457e79b54d6f07ce2e3dfdd68bef883b0b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE

    PREFIX "${CMAKE_BINARY_DIR}/boost"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/boost-src"
    BUILD_IN_SOURCE TRUE

    CONFIGURE_COMMAND
        <SOURCE_DIR>/bootstrap.sh
        --prefix=${BOOST_INSTALL_DIR}
        --with-libraries=atomic,chrono,filesystem,log,program_options,regex,thread,stacktrace

    BUILD_COMMAND
        <SOURCE_DIR>/b2
        variant=release
        threading=multi
        link=shared
        runtime-link=shared
        --layout=system
        "linkflags=-Wl,-rpath,${BOOST_INSTALL_DIR}/lib"

    INSTALL_COMMAND
        <SOURCE_DIR>/b2
        install
        variant=release
        threading=multi
        link=shared
        runtime-link=shared
        --layout=system
        "linkflags=-Wl,-rpath,${BOOST_INSTALL_DIR}/lib"
)
#####################################################################
# Fetch and build ndn-cxx (via waf, incremental)
#####################################################################
FetchContent_Declare(
    ndn-cxx
    GIT_REPOSITORY https://github.com/named-data/ndn-cxx.git
    GIT_TAG ndn-cxx-0.9.0
    GIT_SHALLOW TRUE
    PATCH_COMMAND
        ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
        git apply
        "${CMAKE_SOURCE_DIR}/cmake/patches/ndn-cxx-0.9.0-gcc16.patch"


    SOURCE_SUBDIR __seeds_no_cmake_subdirectory__
)

FetchContent_MakeAvailable(ndn-cxx)

set(NDN_CXX_INSTALL_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/install)
set(NDN_CXX_INCLUDE_DIR ${NDN_CXX_INSTALL_DIR}/include)
set(NDN_CXX_LIBRARY_DIR ${NDN_CXX_INSTALL_DIR}/lib)

file(MAKE_DIRECTORY
    "${SQLITE_INSTALL_DIR}/include"
    "${SQLITE_INSTALL_DIR}/lib"
    "${BOOST_INSTALL_DIR}/include"
    "${BOOST_INSTALL_DIR}/lib"
    "${NDN_CXX_INCLUDE_DIR}"
    "${NDN_CXX_LIBRARY_DIR}"
)

add_custom_command(
    OUTPUT ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so

    COMMAND ${CMAKE_COMMAND} -E env
        PKG_CONFIG_PATH=${SQLITE_INSTALL_DIR}/lib/pkgconfig
        CPPFLAGS=-I${SQLITE_INSTALL_DIR}/include
        "LDFLAGS=-L${BOOST_INSTALL_DIR}/lib -L${SQLITE_INSTALL_DIR}/lib -Wl,-rpath,${BOOST_INSTALL_DIR}/lib -Wl,-rpath,${SQLITE_INSTALL_DIR}/lib"
        ./waf configure
            --prefix=${NDN_CXX_INSTALL_DIR}
            --libdir=${NDN_CXX_LIBRARY_DIR}
            --boost-includes=${BOOST_INSTALL_DIR}/include
            --boost-libs=${BOOST_INSTALL_DIR}/lib

    COMMAND ./waf
    COMMAND ./waf install

    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
    DEPENDS sqlite-local boost-local
    BYPRODUCTS ${NDN_CXX_LIBRARY_DIR}/pkgconfig/libndn-cxx.pc
    COMMENT "Building ndn-cxx via waf (incremental)"
    VERBATIM
)

add_custom_target(ndn-cxx-build
    DEPENDS ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
)

add_custom_target(ndn-cxx-clean
    COMMAND ./waf distclean
    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
    COMMENT "Running waf distclean for ndn-cxx"
)

add_library(ndn-cxx::ndn-cxx SHARED IMPORTED GLOBAL)

set_target_properties(ndn-cxx::ndn-cxx PROPERTIES
    IMPORTED_LOCATION
        "${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so"

    INTERFACE_INCLUDE_DIRECTORIES
        "${NDN_CXX_INCLUDE_DIR};${BOOST_INSTALL_DIR}/include;${SQLITE_INSTALL_DIR}/include"

    INTERFACE_COMPILE_DEFINITIONS
        "BOOST_STACKTRACE_DYN_LINK;BOOST_LOG_DYN_LINK"
)

set_property(TARGET ndn-cxx::ndn-cxx APPEND PROPERTY
    INTERFACE_LINK_OPTIONS
        "-Wl,-rpath-link,${BOOST_INSTALL_DIR}/lib"
        "-Wl,-rpath-link,${SQLITE_INSTALL_DIR}/lib"
        "-Wl,-rpath,${NDN_CXX_LIBRARY_DIR}"
        "-Wl,-rpath,${BOOST_INSTALL_DIR}/lib"
        "-Wl,-rpath,${SQLITE_INSTALL_DIR}/lib"
)

add_dependencies(ndn-cxx::ndn-cxx ndn-cxx-build)

#####################################################################
# Fetch and build NFD
#####################################################################
set(NFD_INSTALL_DIR "${CMAKE_BINARY_DIR}/NFD/install")
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/named-data/NFD.git
    GIT_TAG NFD-24.07
    GIT_SUBMODULES_RECURSE TRUE
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR __seeds_no_cmake_subdirectory__
)

FetchContent_MakeAvailable(nfd)
message(STATUS "NFD source dir: ${nfd_SOURCE_DIR}")

add_custom_target(nfd-build ALL
    COMMAND ${CMAKE_COMMAND} -E env
        PKG_CONFIG_PATH=${NDN_CXX_LIBRARY_DIR}/pkgconfig:${SQLITE_INSTALL_DIR}/lib/pkgconfig
        ./waf configure
            --prefix=${NFD_INSTALL_DIR}
            --boost-includes=${BOOST_INSTALL_DIR}/include
            --boost-libs=${BOOST_INSTALL_DIR}/lib

    COMMAND ./waf
    COMMAND ./waf install

    COMMAND ${CMAKE_COMMAND}
        -DNFD_CONFIG_SAMPLE=${NFD_INSTALL_DIR}/etc/ndn/nfd.conf.sample
        -DNFD_CONFIG=${NFD_INSTALL_DIR}/etc/ndn/nfd.conf
        -P ${CMAKE_SOURCE_DIR}/cmake/ensure-nfd-config.cmake

    DEPENDS ndn-cxx-build

    WORKING_DIRECTORY ${nfd_SOURCE_DIR}
    COMMENT "Building NFD via waf"
    VERBATIM
)

#####################################################################
# Fetch and build ndn-tools
#####################################################################
FetchContent_Declare(
    ndn-tools
    GIT_REPOSITORY https://github.com/named-data/ndn-tools.git
    GIT_TAG ndn-tools-24.07
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR __seeds_no_cmake_subdirectory__
)

FetchContent_MakeAvailable(ndn-tools)

add_custom_target(ndn-tools-build ALL
    COMMAND ${CMAKE_COMMAND} -E env
      PKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/ndn-cxx/install/lib/pkgconfig
      ./waf configure 
        --prefix="${CMAKE_BINARY_DIR}/ndn-tools/install"
        --boost-includes=${BOOST_INSTALL_DIR}/include
        --boost-libs=${BOOST_INSTALL_DIR}/lib
    COMMAND ./waf
    COMMAND ./waf install
    DEPENDS ndn-cxx-build
    WORKING_DIRECTORY ${ndn-tools_SOURCE_DIR}
    COMMENT "Building ndn-tools via waf"
)

#####################################################################
# Export environment paths
#####################################################################
set(NFD_BIN_DIR "${CMAKE_BINARY_DIR}/NFD/install/bin")
set(NDN_CXX_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/bin")
set(NDN_CXX_LIB_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/lib")
set(NDN_TOOLS_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-tools/install/bin")

#####################################################################
# Sample NFD Config
#####################################################################
set(NFD_CONF_DIR "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn")
set(NFD_CONF_SRC "${NFD_CONF_DIR}/nfd.conf.sample")
set(NFD_CONF_DEST "${NFD_CONF_DIR}/nfd.conf")
if(EXISTS "${NFD_CONF_SRC}" AND NOT EXISTS "${NFD_CONF_DEST}")
    file(COPY "${NFD_CONF_SRC}" DESTINATION "${NFD_CONF_DIR}")
    file(RENAME "${NFD_CONF_SRC}" "${NFD_CONF_DEST}")
endif()

#####################################################################
# Run script
#####################################################################

set(NFD_RUNNER_PATH "${CMAKE_BINARY_DIR}/run-nfd.sh")

file(WRITE "${NFD_RUNNER_PATH}"
    "#!/usr/bin/env bash
    set -e

    export LD_LIBRARY_PATH=\"${NDN_CXX_LIB_DIR}:${BOOST_INSTALL_DIR}/lib:${SQLITE_INSTALL_DIR}/lib:\$LD_LIBRARY_PATH\"

    exec \"${CMAKE_BINARY_DIR}/NFD/install/bin/nfd\" \"\$@\"
    "
)

file(CHMOD "${NFD_RUNNER_PATH}" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)

message(STATUS "🛠  Created helper script to run NFD: ${NFD_RUNNER_PATH}")
message(STATUS "👉  You can run it like this: sudo ${NFD_RUNNER_PATH}")

#####################################################################
# Fetch and add nlohmann/json
#####################################################################
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
#####################################################################
# Fetch and add spdlog
#####################################################################
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)
#####################################################################
# Fetch and add cxxopts (CLI flags)
#####################################################################
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.2.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cxxopts)
#####################################################################
# Fetch and add cpp-httplib
#####################################################################
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.60.1
)
FetchContent_MakeAvailable(cpp-httplib)
#####################################################################
# Source dirs
#####################################################################
set(SNDS_MESSAGE_DIR        ${CMAKE_SOURCE_DIR}/src/snds_message)
set(SNDS_QUERY_PARAMS_DIR   ${CMAKE_SOURCE_DIR}/src/snds_query_params)
set(SNDS_QUEUE_DIR          ${CMAKE_SOURCE_DIR}/src/snds_queue)
set(SNDS_SERVICE_DIR        ${CMAKE_SOURCE_DIR}/src/snds_service)
set(SNDS_UTILS_DIR          ${CMAKE_SOURCE_DIR}/src/snds_utils)
set(SNDS_BROKER_DIR         ${CMAKE_SOURCE_DIR}/src/snds_broker)
set(SNDS_INTEREST_DIR       ${CMAKE_SOURCE_DIR}/src/snds_interest)
set(SNDS_TEMPORAL_QUERY_DIR ${CMAKE_SOURCE_DIR}/src/snds_temporal_query)
set(SNDS_RESILIENCE_DIR     ${CMAKE_SOURCE_DIR}/src/snds_resilience)
set(SNDS_IPC_DIR            ${CMAKE_SOURCE_DIR}/src/snds_inter_process_communication/)
#####################################################################
# snds_query_params
#####################################################################
add_library(snds_query_params INTERFACE)
target_include_directories(snds_query_params INTERFACE
  ${SNDS_QUERY_PARAMS_DIR}
  ${cpp-httplib_SOURCE_DIR}
)

#####################################################################
# snds_ipc (header-only)
#####################################################################
add_library(snds_ipc INTERFACE)
target_include_directories(snds_ipc INTERFACE ${SNDS_IPC_DIR})

# Asio is header-only here, but on POSIX we still want pthreads for safety.
find_package(Threads REQUIRED)
target_link_libraries(snds_ipc INTERFACE Threads::Threads)

#####################################################################
# snds_message
#####################################################################
add_library(snds_message INTERFACE)
target_include_directories(snds_message INTERFACE ${SNDS_MESSAGE_DIR})
target_link_libraries(snds_message INTERFACE snds_query_params)

#####################################################################
# snds_utils
#####################################################################
set(SNDS_UTILS_SOURCES ${SNDS_UTILS_DIR}/snds_utils.cpp)
add_library(snds_utils STATIC ${SNDS_UTILS_SOURCES})
target_include_directories(snds_utils PUBLIC ${SNDS_UTILS_DIR})
target_link_libraries(snds_utils PUBLIC
  spdlog::spdlog
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
)
target_link_libraries(snds_query_params INTERFACE snds_utils)

#####################################################################
# temporal_query
#####################################################################
set(SNDS_TEMPORAL_QUERY_SOURCES ${SNDS_TEMPORAL_QUERY_DIR}/temporal_query.cpp)
add_library(temporal_query STATIC ${SNDS_TEMPORAL_QUERY_SOURCES})
target_include_directories(temporal_query PUBLIC ${SNDS_TEMPORAL_QUERY_DIR})
target_link_libraries(temporal_query PRIVATE
  snds_utils
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)

#####################################################################
# snds_resilience
#####################################################################
set(SNDS_RESILIENCE_SOURCES
  ${SNDS_RESILIENCE_DIR}/snds_resilience.cpp
)
add_library(snds_resilience STATIC ${SNDS_RESILIENCE_SOURCES})
target_include_directories(snds_resilience PUBLIC ${SNDS_RESILIENCE_DIR})
target_link_libraries(snds_resilience PUBLIC
  snds_utils
  spdlog::spdlog
  cxxopts::cxxopts
  nlohmann_json::nlohmann_json
)

#####################################################################
# snds_interest
#####################################################################
set(SNDS_INTEREST_SOURCES ${SNDS_INTEREST_DIR}/parsed_interest_name.cpp)
add_library(snds_interest STATIC ${SNDS_INTEREST_SOURCES})
target_include_directories(snds_interest PUBLIC ${SNDS_INTEREST_DIR})
target_link_libraries(snds_interest PUBLIC ndn-cxx::ndn-cxx)

#####################################################################
# message_queue
#####################################################################
set(MESSAGE_QUEUE_SOURCES ${SNDS_QUEUE_DIR}/message_queue.cpp)
add_library(message_queue STATIC ${MESSAGE_QUEUE_SOURCES})
target_include_directories(message_queue PUBLIC
  ${SNDS_MESSAGE_DIR}
  ${SNDS_QUEUE_DIR}
)
target_link_libraries(message_queue
  PRIVATE
    ndn-cxx::ndn-cxx
    spdlog::spdlog
  PUBLIC
    snds_message
)

#####################################################################
# snds_broker
#####################################################################
set(SNDS_BROKER_SOURCES
  ${SNDS_BROKER_DIR}/broker.cpp
  ${SNDS_BROKER_DIR}/broker_utils.cpp
)
add_library(snds_broker STATIC ${SNDS_BROKER_SOURCES})
add_dependencies(snds_broker mongo-cxx-driver)
target_include_directories(snds_broker PRIVATE
  ${SNDS_BROKER_DIR}
  ${MONGO_CXX_INSTALL_DIR}/include
  ${MONGO_CXX_INSTALL_DIR}/include/bsoncxx/v_noabi
  ${MONGO_CXX_INSTALL_DIR}/include/mongocxx/v_noabi
  ${cpp-httplib_SOURCE_DIR}
)
target_link_libraries(snds_broker PUBLIC
  ${BSONCXX_LIBRARIES}
  ${MONGOCXX_LIBRARIES}
  nlohmann_json::nlohmann_json
  temporal_query
  OpenSSL::Crypto
  snds_utils
)

add_executable(snds_broker_bin ${SNDS_BROKER_DIR}/broker_main.cpp)
add_dependencies(snds_broker_bin mongo-cxx-driver)
target_include_directories(snds_broker_bin PRIVATE
  ${cpp-httplib_SOURCE_DIR}
  ${SNDS_BROKER_DIR}
  ${MONGO_CXX_INSTALL_DIR}/include
  ${MONGO_CXX_INSTALL_DIR}/include/bsoncxx/v_noabi
  ${MONGO_CXX_INSTALL_DIR}/include/mongocxx/v_noabi
)
target_link_libraries(snds_broker_bin PRIVATE
  snds_broker
  spdlog::spdlog
  cxxopts::cxxopts
)
set_target_properties(snds_broker_bin PROPERTIES
  BUILD_RPATH   "${MONGO_CXX_LIBRARY_DIR}"
  INSTALL_RPATH "${MONGO_CXX_LIBRARY_DIR}"
)

#####################################################################
# snds_service
#####################################################################
set(SNDS_SERVICE_SOURCES
  ${SNDS_SERVICE_DIR}/snds_service_base.cpp
  ${SNDS_SERVICE_DIR}/snds_http_server.cpp
  ${SNDS_SERVICE_DIR}/snds_worker.cpp
  ${SNDS_SERVICE_DIR}/snds_producer.cpp
  ${SNDS_SERVICE_DIR}/snds_consumer.cpp
  ${SNDS_SERVICE_DIR}/snds_digital_twin.cpp
)
add_library(snds_service STATIC ${SNDS_SERVICE_SOURCES})
target_include_directories(snds_service PUBLIC
  ${SNDS_SERVICE_DIR}
  ${NDN_CXX_INCLUDE_DIR}
  ${cpp-httplib_SOURCE_DIR}
)

target_link_libraries(snds_service PUBLIC
  snds_utils
  snds_interest
  temporal_query
  message_queue
  snds_query_params
  ndn-cxx::ndn-cxx
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  snds_resilience
  snds_ipc
)
add_dependencies(snds_service ndn-cxx-build)

#####################################################################
# Daemon executable
#####################################################################
add_executable(snds_daemon_bin ${SNDS_SERVICE_DIR}/main.cpp)
target_include_directories(snds_daemon_bin PRIVATE
  ${SNDS_SERVICE_DIR}
  ${NDN_CXX_INCLUDE_DIR}
  ${SNDS_UTILS_DIR}
)
target_link_libraries(snds_daemon_bin PRIVATE
  snds_service
  message_queue
  snds_query_params
  ndn-cxx::ndn-cxx
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  cxxopts::cxxopts
)
add_dependencies(snds_daemon_bin ndn-cxx-build)

#####################################################################
# snds_utils test executable
#####################################################################
set(SNDS_UTILS_TEST_SOURCES ${SNDS_UTILS_DIR}/test_snds_utils.cpp)
add_executable(test_snds_utils ${SNDS_UTILS_TEST_SOURCES})
target_include_directories(test_snds_utils PRIVATE
  ${SNDS_UTILS_DIR}
  ${NDN_CXX_INCLUDE_DIR}
)
target_link_libraries(test_snds_utils PRIVATE
  snds_utils
  ndn-cxx::ndn-cxx
  spdlog::spdlog
)
add_dependencies(test_snds_utils ndn-cxx-build)

#####################################################################
# Demo executable for snds_resilience
#####################################################################
add_executable(resilience_demo
  ${SNDS_RESILIENCE_DIR}/test_resilience.cpp
)
target_link_libraries(resilience_demo PRIVATE
  snds_resilience
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  cxxopts::cxxopts
)

#####################################################################
# Resilience poller 
#####################################################################
add_executable(resilience_poller_bin
  ${SNDS_RESILIENCE_DIR}/main.cpp
  ${SNDS_RESILIENCE_DIR}/snds_resilience.cpp
  ${SNDS_RESILIENCE_DIR}/snds_resilience_poller.cpp
  ${SNDS_UTILS_DIR}/snds_utils.cpp
)

target_include_directories(resilience_poller_bin PRIVATE
  ${SNDS_RESILIENCE_DIR}
  ${SNDS_INTEREST_DIR}
  ${SNDS_UTILS_DIR}
  ${SNDS_IPC_DIR}
  ${NDN_CXX_INCLUDE_DIR}
)

target_link_libraries(resilience_poller_bin PRIVATE
  ndn-cxx::ndn-cxx
  spdlog::spdlog
  cxxopts::cxxopts
  nlohmann_json::nlohmann_json
  snds_interest
  snds_utils
  snds_ipc
)

add_dependencies(resilience_poller_bin ndn-cxx-build)
