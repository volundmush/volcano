cmake_minimum_required(VERSION 4.1.1)
project(volcano VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VOLCANO_ENABLE_INSTALL "Enable install/export for consumers" OFF)

# ---------------- basics (yours) ----------------
set(CPM_DOWNLOAD_VERSION 0.42.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake")
  file(DOWNLOAD
       https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
       ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Default to C++23 for this workspace; consumers can still raise this.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#set(CPM_SOURCE_CACHE ${PROJECT_SOURCE_DIR}/dependencies)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/compiled)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/compiled)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/compiled)

set(BUILD_TESTING OFF)
# ---------------- 
# ---------------- deps (yours) ----------------
find_package(OpenSSL REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS regex iostreams url)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if(UNIX AND NOT APPLE)
  set(MATH_LIB m)
endif()

set(SPDLOG_FMT_EXTERNAL ON)
if(VOLCANO_ENABLE_INSTALL)
  find_package(fmt CONFIG REQUIRED)
  find_package(spdlog CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  find_package(enchantum CONFIG REQUIRED)
else()
  CPMAddPackage(NAME fmt                   GITHUB_REPOSITORY fmtlib/fmt            GIT_TAG 12.1.0)
  CPMAddPackage(NAME spdlog                GITHUB_REPOSITORY gabime/spdlog         VERSION 1.17.0)
  CPMAddPackage(NAME nlohmann_json         GITHUB_REPOSITORY nlohmann/json         VERSION 3.12.0)
  CPMAddPackage(NAME enchantum            GITHUB_REPOSITORY ZXShady/enchantum    GIT_TAG 0.3.0)
endif()


# ---------------- targets ----------------

add_subdirectory(libs/dotenv)
add_subdirectory(libs/jwt)
add_subdirectory(libs/zlib)
add_subdirectory(libs/util)
add_subdirectory(libs/net)
add_subdirectory(libs/log)
add_subdirectory(libs/web)
add_subdirectory(libs/mud)
add_subdirectory(libs/circle)
add_subdirectory(libs/telnet)
add_subdirectory(libs/config)

# Optional alias for convenience
set(VOLCANO_TARGETS
  volcano_dotenv
  volcano_jwt
  volcano_zlib
  volcano_util
  volcano_net
  volcano_log
  volcano_web
  volcano_mud
  volcano_circle
  volcano_telnet
  volcano_config
)

if(VOLCANO_ENABLE_INSTALL)
  install(
    TARGETS ${VOLCANO_TARGETS}
    EXPORT volcanoTargets
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(
    EXPORT volcanoTargets
    NAMESPACE volcano::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volcano
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/volcanoConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/volcanoConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volcano
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/volcanoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/volcanoConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/volcanoConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/volcano
  )
endif()