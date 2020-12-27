FILE(GLOB TDeint_Sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "*.c"
  "*.cpp"
  "*.hpp"
  "*.h"
  "../include/*.h"
  "../include/avs/*.h"
  "../common/*.h"
  "../common/*.cpp"
)

message("${TDeint_Sources}")

IF( MSVC OR MINGW )
    # Export definitions in general are not needed on x64 and only cause warnings,
    # unfortunately we still must need a .def file for some COM functions.
    # NO C interface for this plugin
    # if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    #  LIST(APPEND TDeint_Sources "TDeint64.def")
    # else()
    #  LIST(APPEND TDeint_Sources "TDeint.def")
    # endif() 
ENDIF()

IF( MSVC_IDE )
    # Ninja, unfortunately, seems to have some issues with using rc.exe
    LIST(APPEND TDeint_Sources "TDeint.rc")
ENDIF()
