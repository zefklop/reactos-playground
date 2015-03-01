
set_cpp()

list(APPEND SOURCE
    tcpip.cpp)

add_library(tcpip SHARED
    ${SOURCE}
    tcpip.rc)

set_module_type(tcpip kernelmodedriver)
add_importlibs(tcpip ndis ntoskrnl hal)

add_cd_file(TARGET tcpip DESTINATION reactos/system32/drivers FOR all)