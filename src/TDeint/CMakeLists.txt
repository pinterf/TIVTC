# Visual Studio 2019 is supported from CMake 3.14.1
# Tested generators:
# "MinGW Makefiles": MSYS2/Mingw32 GCC 8.3 build
# "Visual Studio 16 2019" optional platform generator Win32 and x64
# "Visual Studio 16 2019" + LLVM 8.0 (clang) optional platform generator Win32 and x64
CMAKE_MINIMUM_REQUIRED(VERSION 3.8)

set(PluginName "TDeint")

if (NOT WIN32)
  string(TOLOWER "${PluginName}" PluginName)
endif()

set(ProjectName "${PluginName}")
project(${ProjectName} LANGUAGES CXX)

Include("Files.cmake")

# Helper function to apply SIMD flags based on filename suffixes
function(apply_simd_flags_to_target TARGET_NAME SOURCES_LIST)
    foreach(FILE ${SOURCES_LIST})
        if(FILE MATCHES ".*_avx2\\.cpp$")
            if(MSVC)
                set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "/arch:AVX2")
            else()
                set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
            endif()
        elseif(FILE MATCHES ".*_avx\\.cpp$")
            if(MSVC)
                set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "/arch:AVX")
            else()
                set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "-mavx")
            endif()
        elseif(FILE MATCHES ".*_sse41\\.cpp$")
            if(NOT MSVC)
                set_source_files_properties(${FILE} PROPERTIES COMPILE_FLAGS "-msse4.1")
            endif()
            # MSVC doesn't have a specific /arch:SSE4.1; /arch:SSE2 is usually the default
        endif()
    endforeach()
endfunction()


add_library(${PluginName} SHARED ${TDeint_Sources})
apply_simd_flags_to_target(${PluginName} "${TDeint_Sources}")

if (GIT_FOUND)
    execute_process (COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        OUTPUT_VARIABLE ver
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (ver STREQUAL "")
      set_target_properties(${PluginName} PROPERTIES OUTPUT_NAME "${PluginName}")
    else ()
      set_target_properties(${PluginName} PROPERTIES OUTPUT_NAME "${PluginName}.${ver}")
    endif()
else ()
    set_target_properties(${PluginName} PROPERTIES OUTPUT_NAME "${PluginName}")
endif ()

if (MINGW)
  set_target_properties(${PluginName} PROPERTIES PREFIX "")
  set_target_properties(${PluginName} PROPERTIES IMPORT_PREFIX "")
endif()

#require sse4.1, if there are mixed parts in the source
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINTEL_INTRINSICS -msse4.1")
# no mixed files: sse2 is enough
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINTEL_INTRINSICS -msse2")

# Automatically group source files according to directory structure
foreach(FILE ${TDeint_Sources}) 
  get_filename_component(PARENT_DIR "${FILE}" PATH)

  string(REGEX REPLACE "(\\./)" "" GROUP "${PARENT_DIR}")
  string(REPLACE "/" "\\" GROUP "${GROUP}")

  # group into "Source Files" and "Header Files"
  if ("${FILE}" MATCHES ".*\\.cpp")
    set(GROUP "Source Files\\${GROUP}")
  elseif("${FILE}" MATCHES ".*\\.h")
    set(GROUP "Header Files\\${GROUP}")
  endif()

  source_group("${GROUP}" FILES "${FILE}")
endforeach()

# Specify include directories
target_link_libraries(${ProjectName})
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#dedicated include dir for avisynth.h
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
#another common include dir
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common)

# Windows DLL dependencies 
if (MSVC OR MINGW)
  target_link_libraries("TDeint" "uuid" "winmm" "vfw32" "msacm32" "gdi32" "user32" "advapi32" "ole32" "imagehlp")
endif()

include(GNUInstallDirs)

INSTALL(TARGETS ${ProjectName}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/avisynth")
