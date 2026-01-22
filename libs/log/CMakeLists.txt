add_library(volcano_log STATIC)
add_library(volcano::log ALIAS volcano_log)

target_compile_features(volcano_log PUBLIC cxx_std_23)

target_sources(volcano_log
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/log.cppm
)

target_link_libraries(volcano_log
  PUBLIC
    fmt::fmt
    spdlog::spdlog
)