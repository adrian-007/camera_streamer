cmake_minimum_required(VERSION 3.1)
project(camera_streamer)

set(CMAKE_CXX_STANDARD 14)
set(TARGET camera_streamer)

find_package(CURL REQUIRED)
find_package(Boost 1.60.0 COMPONENTS system filesystem program_options REQUIRED)

add_executable(${TARGET}
    src/main.cpp
    src/Configuration.cpp
    src/Configuration.h
    src/CURLSession.cpp
    src/CURLSession.h
)

target_include_directories(${TARGET}
    PUBLIC
    src
    PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${TARGET}
    PRIVATE
    ${CURL_LIBRARIES}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

install(TARGETS ${TARGET}
    RUNTIME DESTINATION bin
)

install(FILES assets/camera.config.env
    DESTINATION etc/default
    RENAME camera
)

install(PROGRAMS assets/camera_capturer.sh
    DESTINATION bin
    RENAME camera_capturer
)
