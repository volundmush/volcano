add_library(volcano_net STATIC)
add_library(volcano::net ALIAS volcano_net)

target_compile_features(volcano_net PUBLIC cxx_std_23)

target_sources(volcano_net
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/net.cppm
)

# Add deps as needed
target_link_libraries(volcano_net
  PUBLIC
    volcano::log
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)