
set_cpp()

list(APPEND SOURCE
    ip.cpp
    tcpip.cpp)

add_library(tcpip SHARED
    ${SOURCE}
    tcpip.rc)

set_module_type(tcpip kernelmodedriver)
add_importlibs(tcpip ndis ntoskrnl hal)

if(NOT MSVC)
    add_target_compile_flags(tcpip "-Wno-write-strings")
endif()

add_cd_file(TARGET tcpip DESTINATION reactos/system32/drivers FOR all)