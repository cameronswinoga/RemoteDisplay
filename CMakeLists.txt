project(RemoteDisplay-toplevel C CXX)
cmake_minimum_required (VERSION 3.0.2)

set(CMAKE_COLOR_MAKEFILE ON)

# search the FreeRDP Library
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(FREERDP_INCLUDE_DIR /usr/local/include/freerdp2)
set(FREERDP_LIBRARIES /usr/local/lib/freerdp)
set(WINPR_INCLUDE_DIR /usr/local/include/winpr1)
set(WINPR_LIBRARIES /usr/local/lib)

find_package(FreeRDP REQUIRED)
MESSAGE("found FREERDP_INCLUDE_DIR: ${FREERDP_INCLUDE_DIR}")
MESSAGE("found FREERDP_LIBRARIES: ${FREERDP_LIBRARIES}")

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Add LibRemoteDisplay
add_subdirectory(src)

# Add the client
add_subdirectory(client)
