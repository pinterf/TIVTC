# Visual Studio 2019 is supported from CMake 3.14.1
# Tested generators:
# "MinGW Makefiles": MSYS2/Mingw32 GCC 8.3 build
# "Visual Studio 16 2019" optional platform generator Win32 and x64
# "Visual Studio 16 2019" + LLVM 8.0 (clang) optional platform generator Win32 and x64
CMAKE_MINIMUM_REQUIRED(VERSION 3.8)

set(PluginName "TIVTC")

if (NOT WIN32)
  string(TOLOWER "${PluginName}" PluginName)
endif()

set(ProjectName "${PluginName}")
project(${ProjectName} LANGUAGES CXX)

Include("Files.cmake")

add_library(${PluginName} SHARED ${TIVTC_Sources})

set_target_properties(${PluginName} PROPERTIES "OUTPUT_NAME" "${PluginName}")
if (MINGW)
  set_target_properties(${PluginName} PROPERTIES PREFIX "")
  set_target_properties(${PluginName} PROPERTIES IMPORT_PREFIX "")
endif()

#require sse4.1, if there are mixed parts in the source
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINTEL_INTRINSICS -msse4.1")
# no mixed files: sse2 is enough
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINTEL_INTRINSICS -msse2")

# Automatically group source files according to directory structure
foreach(FILE ${TIVTC_Sources}) 
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

if (MSVC_IDE)
  IF(CLANG_IN_VS STREQUAL "1")
      # special AVX option for source files with *_avx.cpp pattern
      file(GLOB_RECURSE SRCS_AVX "*_avx.cpp")
      set_source_files_properties(${SRCS_AVX} PROPERTIES COMPILE_FLAGS " -mavx ")

      # special AVX2 option for source files with *_avx2.cpp pattern
      file(GLOB_RECURSE SRCS_AVX2 "*_avx2.cpp")
      set_source_files_properties(${SRCS_AVX2} PROPERTIES COMPILE_FLAGS " -mavx2 -mfma ")

      # special AVX512 option for source files with *_avx512.cpp pattern
      file(GLOB_RECURSE SRCS_AVX512 "*_avx512.cpp")
      set_source_files_properties(${SRCS_AVX512} PROPERTIES COMPILE_FLAGS " -mavx512f -mavx512bw ")
  ELSE()
      # special AVX option for source files with *_avx.cpp pattern
      file(GLOB_RECURSE SRCS_AVX "*_avx.cpp")
      set_source_files_properties(${SRCS_AVX} PROPERTIES COMPILE_FLAGS " /arch:AVX ")

      # special AVX2 option for source files with *_avx2.cpp pattern
      file(GLOB_RECURSE SRCS_AVX2 "*_avx2.cpp")
      set_source_files_properties(${SRCS_AVX2} PROPERTIES COMPILE_FLAGS " /arch:AVX2 ")

      # special AVX512 option for source files with *_avx512.cpp pattern
      file(GLOB_RECURSE SRCS_AVX512 "*_avx512.cpp")
      set_source_files_properties(${SRCS_AVX512} PROPERTIES COMPILE_FLAGS " /arch:AVX512 ")
  ENDIF()
else()
  # special AVX option for source files with *_avx.cpp pattern
  file(GLOB_RECURSE SRCS_AVX "*_avx.cpp")
  set_source_files_properties(${SRCS_AVX} PROPERTIES COMPILE_FLAGS " -mavx ")

  # special AVX2 option for source files with *_avx2.cpp pattern
  file(GLOB_RECURSE SRCS_AVX2 "*_avx2.cpp")
  set_source_files_properties(${SRCS_AVX2} PROPERTIES COMPILE_FLAGS " -mavx2 -mfma ")

  # special AVX512 option for source files with *_avx512.cpp pattern
  file(GLOB_RECURSE SRCS_AVX512 "*_avx512.cpp")
  set_source_files_properties(${SRCS_AVX512} PROPERTIES COMPILE_FLAGS " -mavx512f -mavx512bw ")
endif()


# Specify include directories
target_link_libraries(${ProjectName})
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#dedicated include dir for avisynth.h
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
#another common include dir
target_include_directories(${ProjectName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common)

# Windows DLL dependencies 
if (MSVC OR MINGW)
  target_link_libraries("TIVTC" "uuid" "winmm" "vfw32" "msacm32" "gdi32" "user32" "advapi32" "ole32" "imagehlp")
endif()

include(GNUInstallDirs)

INSTALL(TARGETS ${ProjectName}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/avisynth")
