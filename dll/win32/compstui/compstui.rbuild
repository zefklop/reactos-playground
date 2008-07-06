<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../tools/rbuild/project.dtd">
<group>
<module name="compstui" type="win32dll" baseaddress="${BASEADDRESS_COMPSTUI}" installbase="system32" installname="compstui.dll" allowwarnings="true" entrypoint="0">
	<importlibrary definition="compstui.spec.def" />
	<include base="compstui">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<define name="__WINESRC__" />
	<define name="WINVER">0x600</define>
	<define name="_WIN32_WINNT">0x600</define>
	<file>compstui_main.c</file>
	<file>compstui.spec</file>
	<library>wine</library>
	<library>kernel32</library>
	<library>ntdll</library>
</module>
</group>
