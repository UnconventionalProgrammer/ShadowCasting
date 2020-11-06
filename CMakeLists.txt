cmake_minimum_required(VERSION 3.17)
project(ShadowCasting)

set(CMAKE_CXX_STANDARD 20)
set(SFML_ROOT $ENV{SFML_ROOT})
set(SFML_INCLUDE_DIR "${SFML_ROOT}/include")
set(SFML_STATIC_LIBRARIES TRUE)

include_directories(.)
include_directories("${SFML_INCLUDE_DIR}/include")

add_executable(ShadowCasting main.cpp)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake_modules")
find_package(SFML 2 REQUIRED graphics  window system)
if (SFML_FOUND)
    include_directories(${SFML_INCLUDE_DIR})
    target_link_libraries(ShadowCasting ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})

endif()