add_library(volcano_config STATIC)
add_library(volcano::config ALIAS volcano_config)

target_sources(volcano_config
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cppm
)

# Add deps as needed
target_link_libraries(volcano_config
  PUBLIC
    volcano::net
    volcano::log
    volcano::jwt
    volcano::dotenv
)