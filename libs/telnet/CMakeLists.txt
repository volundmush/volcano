add_library(volcano_telnet STATIC)
add_library(volcano::telnet ALIAS volcano_telnet)

target_compile_features(volcano_telnet PUBLIC cxx_std_23)

target_sources(volcano_telnet
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/src/telnet.cppm
      ${CMAKE_CURRENT_SOURCE_DIR}/src/telnet.base.cppm
      ${CMAKE_CURRENT_SOURCE_DIR}/src/telnet.options.cppm
      ${CMAKE_CURRENT_SOURCE_DIR}/src/telnet.connection.cppm
)

# Add deps as needed

target_link_libraries(volcano_telnet
  PUBLIC
    volcano::log
    volcano::net
    volcano::zlib
    volcano::client
    nlohmann_json_modules
)