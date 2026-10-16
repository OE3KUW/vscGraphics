cmake_minimum_required(VERSION 3.16)

project(vscGraphics LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(vscGraphicsLib STATIC graphics.c)
target_include_directories(vscGraphicsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(vscGraphicsLib PUBLIC user32 gdi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(vscGraphicsLib PUBLIC X11::X11 m)
else()
    message(FATAL_ERROR "vscGraphics supports Linux/X11 and Windows/Win32.")
endif()

add_executable(vscGraphics main.c)
target_link_libraries(vscGraphics PRIVATE vscGraphicsLib)

# Der TERRA25-FAT-Datentraeger ist unter Linux mit showexec eingebunden.
# Dort werden Dateien mit .exe-Endung ausfuehrbar markiert. Unter Windows setzt
# CMake die Endung ohnehin automatisch.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(vscGraphics PROPERTIES SUFFIX ".exe")
endif()

if(MSVC)
    target_compile_options(vscGraphicsLib PRIVATE /W4)
    target_compile_options(vscGraphics PRIVATE /W4)
else()
    target_compile_options(vscGraphicsLib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(vscGraphics PRIVATE -Wall -Wextra -Wpedantic)
endif()
