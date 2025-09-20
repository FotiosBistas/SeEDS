# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and language
project(SeEDS LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)

# Find libpcap
pkg_check_modules(PCAP REQUIRED libpcap)
include_directories(${PCAP_INCLUDE_DIRS})
link_directories(${PCAP_LIBRARY_DIRS})

# Find libsystemd
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
include_directories(${SYSTEMD_INCLUDE_DIRS})
link_directories(${SYSTEMD_LIBRARY_DIRS})

# Find SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
include_directories(${SQLITE3_INCLUDE_DIRS})
link_directories(${SQLITE3_LIBRARY_DIRS})

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include External Project / FetchContent modules
include(ExternalProject)
include(FetchContent)

set(MONGO_C_DRIVER_VERSION "2.1.0") # or newer stable
set(MONGO_C_INSTALL_DIR "${CMAKE_BINARY_DIR}/mongo-c-driver-install")

#####################################################################
# Fetch and build mongo-c-driver
#####################################################################
ExternalProject_Add(mongo-c-driver
  URL https://github.com/mongodb/mongo-c-driver/releases/download/${MONGO_C_DRIVER_VERSION}/mongo-c-driver-${MONGO_C_DRIVER_VERSION}.tar.gz
  PREFIX ${CMAKE_BINARY_DIR}/mongo-c-driver
  SOURCE_DIR ${CMAKE_BINARY_DIR}/mongo-c-driver-src
  BINARY_DIR ${CMAKE_BINARY_DIR}/mongo-c-driver-src/build
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${MONGO_C_INSTALL_DIR}
    -DENABLE_SRV=ON              # <— required for mongodb+srv
    -DENABLE_SSL=OPENSSL         # <— Atlas requires TLS
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
  PREFIX ${CMAKE_BINARY_DIR}/mongo-cxx-driver
  SOURCE_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src
  BINARY_DIR ${CMAKE_BINARY_DIR}/mongo-cxx-driver-src/build
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=Release
    -DENABLE_SRV=ON
    -DCMAKE_CXX_STANDARD=17
    -DBUILD_SHARED_LIBS=ON
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
# Fetch and build ndn-cxx (via waf, incremental)
#####################################################################
FetchContent_Declare(
  ndn-cxx
  GIT_REPOSITORY https://github.com/named-data/ndn-cxx.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(ndn-cxx)
if(NOT ndn-cxx_POPULATED)
  FetchContent_Populate(ndn-cxx)

  set(NDN_CXX_INSTALL_DIR ${CMAKE_BINARY_DIR}/ndn-cxx/install)
  set(NDN_CXX_INCLUDE_DIR ${NDN_CXX_INSTALL_DIR}/include)
  set(NDN_CXX_LIBRARY_DIR ${NDN_CXX_INSTALL_DIR}/lib)

  file(MAKE_DIRECTORY "${NDN_CXX_INCLUDE_DIR}" "${NDN_CXX_LIBRARY_DIR}")

  add_custom_command(
    OUTPUT ${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so
    COMMAND ./waf configure
            --prefix=${NDN_CXX_INSTALL_DIR}
            --libdir=${NDN_CXX_LIBRARY_DIR}
    COMMAND ./waf
    COMMAND ./waf install
    WORKING_DIRECTORY ${ndn-cxx_SOURCE_DIR}
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
    IMPORTED_LOCATION             "${NDN_CXX_LIBRARY_DIR}/libndn-cxx.so"
    INTERFACE_INCLUDE_DIRECTORIES "${NDN_CXX_INCLUDE_DIR}"
  )
  add_dependencies(ndn-cxx::ndn-cxx ndn-cxx-build)
endif()

#####################################################################
# Fetch and build NFD
#####################################################################
FetchContent_Declare(
  nfd
  GIT_REPOSITORY https://github.com/named-data/NFD.git
  GIT_TAG master
  GIT_SUBMODULES_RECURSE TRUE
  GIT_SHALLOW TRUE
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
  GIT_SHALLOW TRUE
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
# Export environment paths
#####################################################################
set(NFD_BIN_DIR "${CMAKE_BINARY_DIR}/NFD/install/bin")
set(NDN_CXX_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/bin")
set(NDN_CXX_LIB_DIR "${CMAKE_BINARY_DIR}/ndn-cxx/install/lib")
set(NDN_TOOLS_BIN_DIR "${CMAKE_BINARY_DIR}/ndn-tools/install/bin")
set(BASHRC "$ENV{HOME}/.bashrc")

file(READ "${BASHRC}" BASHRC_CONTENT)

if(NOT BASHRC_CONTENT MATCHES "${NFD_BIN_DIR}")
  file(APPEND "${BASHRC}" "\n# Added by SeEDS CMake for local NFD build\n")
  file(APPEND "${BASHRC}" "export PATH=\$PATH:${NFD_BIN_DIR}:${NDN_CXX_BIN_DIR}\n")
endif()
if(NOT BASHRC_CONTENT MATCHES "${NDN_TOOLS_BIN_DIR}")
  file(APPEND "${BASHRC}" "export PATH=\$PATH:${NDN_TOOLS_BIN_DIR}\n")
endif()
if(NOT BASHRC_CONTENT MATCHES "${NDN_CXX_LIB_DIR}")
  file(APPEND "${BASHRC}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${NDN_CXX_LIB_DIR}\n")
endif()
if(NOT BASHRC_CONTENT MATCHES "${MONGO_CXX_LIBRARY_DIR}")
  file(APPEND "${BASHRC}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${MONGO_CXX_LIBRARY_DIR}\n")
endif()

set(NFD_CONF_DIR "${CMAKE_BINARY_DIR}/NFD/install/etc/ndn")
set(NFD_CONF_SRC "${NFD_CONF_DIR}/nfd.conf.sample")
set(NFD_CONF_DEST "${NFD_CONF_DIR}/nfd.conf")
if(EXISTS "${NFD_CONF_SRC}" AND NOT EXISTS "${NFD_CONF_DEST}")
  file(COPY "${NFD_CONF_SRC}" DESTINATION "${NFD_CONF_DIR}")
  file(RENAME "${NFD_CONF_SRC}" "${NFD_CONF_DEST}")
endif()

set(NFD_RUNNER_PATH "${CMAKE_BINARY_DIR}/run-nfd.sh")
file(WRITE "${NFD_RUNNER_PATH}" "#!/bin/bash\n")
file(APPEND "${NFD_RUNNER_PATH}" "# Auto-generated by CMake - run NFD with local ndn-cxx lib\n")
file(APPEND "${NFD_RUNNER_PATH}" "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:${NDN_CXX_LIB_DIR}\n")
file(APPEND "${NFD_RUNNER_PATH}" "exec ${CMAKE_BINARY_DIR}/NFD/install/bin/nfd \"\$@\"\n")
file(CHMOD "${NFD_RUNNER_PATH}" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
)
message(STATUS "🛠  Created helper script to run NFD: ${NFD_RUNNER_PATH}")
message(STATUS "👉  You can run it like this: sudo ./run-nfd.sh")

#####################################################################
# Fetch and add nlohmann/json
#####################################################################
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
# Fetch and add spdlog
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
# Fetch and add cxxopts (CLI flags)
#####################################################################
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.2.0
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(cxxopts)
if(NOT cxxopts_POPULATED)
  FetchContent_Populate(cxxopts)
  # cxxopts provides a proper CMake target; add it
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#####################################################################
# Fetch and add cpp-httplib
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
  nlohmann_json::nlohmann_json
  snds_interest
  snds_utils
  snds_ipc
)

add_dependencies(resilience_poller_bin ndn-cxx-build)