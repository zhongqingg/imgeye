cmake_minimum_required(VERSION 3.15)
project(ImgEye LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(imgeye WIN32 main.cpp)

# resvg (SVG) - prebuilt static library
target_include_directories(imgeye PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/resvg/include)
target_link_libraries(imgeye PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/resvg/lib/resvg.lib
)
# Scintilla (editor core) + Lexilla XML lexer - prebuilt static libraries
target_include_directories(imgeye PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/include)
target_link_libraries(imgeye PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/lib/libscintilla.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/lib/liblexilla_xml.lib
)
# resvg (Rust std) static-link dependencies on Windows
target_link_libraries(imgeye PRIVATE Ws2_32 Userenv ntdll imm32)

# Use Unicode
target_compile_definitions(imgeye PRIVATE UNICODE _UNICODE)

# Link Windows libraries
target_link_libraries(imgeye PRIVATE
    windowscodecs
    shlwapi
    comctl32
    shcore
    comdlg32
    ole32
    ws2_32
    userenv
    ntdll
)

# Static CRT for small portable binary
set_target_properties(imgeye PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    # Subsystem Windows (no console)
    WIN32_EXECUTABLE TRUE
)

# Strip release build
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(imgeye PRIVATE /utf-8)
    else()
        target_compile_options(imgeye PRIVATE /O2 /Os /GL /EHs-c- /utf-8)
        target_link_options(imgeye PRIVATE /OPT:REF /OPT:ICF /LTCG)
    endif()
endif()
