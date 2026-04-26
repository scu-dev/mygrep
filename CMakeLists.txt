cmake_minimum_required(VERSION "4.0")

project("MyGrep")

#------------Basic-----------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
set(CMAKE_SUPPRESS_REGENERATION ON CACHE BOOL "" FORCE)
#----------------------------------

#--------------Tweaks--------------
add_library(ScopedFlags INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
    message("Using MSVC / clang-cl")
    target_compile_options(ScopedFlags INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CXX>:/bigobj>
        $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>
    )
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # I got a linker error about "__setusermatherr is replaced" when I link using lld and I have no idea why. This is a black magic workaround that seems to work: use MSVC.
    set(CMAKE_LINKER_TYPE "MSVC" CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message("Using clang / clang-cl")
    add_compile_options(
        # I hate clang
        # —Yes I know about `-Wno-error` but I don't want to disable all warnings as errors.
        # If you run into some warnings that does not exist in this list, please add it here via PR.
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-command-line-argument>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unqualified-std-cast-call>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-variable>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-macro-redefined>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-missing-field-initializers>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-invalid-offsetof>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-function>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-private-field>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-unused-but-set-variable>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-missing-braces>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-macro-redefined>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-deprecated-declarations>
        $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-microsoft-void-pseudo-dtor>
    )
endif()

if(WIN32)
    # Override everything to Windows 10 SDK
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:/U_WIN32_WINNT>
        $<$<COMPILE_LANGUAGE:C,CXX>:/UWINVER>
        $<$<COMPILE_LANGUAGE:C,CXX>:/UNTDDI_VERSION>
    )
    add_compile_definitions(
        $<$<COMPILE_LANGUAGE:C,CXX>:WIN32_LEAN_AND_MEAN>
        $<$<COMPILE_LANGUAGE:C,CXX>:NOMINMAX>
        $<$<COMPILE_LANGUAGE:C,CXX>:_WIN32_WINNT=0x0A00>
        $<$<COMPILE_LANGUAGE:C,CXX>:WINVER=0x0A00>
        $<$<COMPILE_LANGUAGE:C,CXX>:NTDDI_VERSION=0x0A000000>
    )
endif()
#----------------------------------

#------------Clangd----------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
#----------------------------------

#------------Files-----------------
file(GLOB_RECURSE MYGREP_SOURCES CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
#----------------------------------

#--------------MyGrep--------------
add_executable(${PROJECT_NAME} ${MYGREP_SOURCES})
#----------------------------------

#------------Build:Macro-----------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Common usage
    $<IF:$<CONFIG:Debug>,DEBUG,NDEBUG>
)
#----------------------------------

#------------Inclusions------------
target_include_directories(${PROJECT_NAME} PRIVATE
    "libs/cli11/include"
)
#----------------------------------

#---------------Link---------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    ScopedFlags
)
#----------------------------------