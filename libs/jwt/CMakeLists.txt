add_library(vol_jwt STATIC)
add_library(vol::jwt ALIAS vol_jwt)

target_compile_features(vol_jwt PUBLIC cxx_std_23)

target_include_directories(vol_jwt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# Global up src/*.cpp
file(GLOB_RECURSE SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Also glob up include/*.hpp

file(GLOB_RECURSE INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/jwt/*.hpp
)

target_precompile_headers(vol_jwt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(vol_jwt
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_link_libraries(vol_jwt
  PUBLIC
    nlohmann_json
  PRIVATE
    OpenSSL::Crypto
)