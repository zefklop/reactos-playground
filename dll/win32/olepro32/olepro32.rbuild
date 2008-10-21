<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../tools/rbuild/project.dtd">
<module name="olepro32" type="win32dll" entrypoint="0" baseaddress="${BASEADDRESS_OLEPRO32}" installbase="system32" installname="olepro32.dll" allowwarnings="true">
	<autoregister infsection="OleControlDlls" type="DllRegisterServer" />
	<importlibrary definition="olepro32.spec" />
	<include base="olepro32">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<define name="__WINESRC__" />
	<define name="WINVER">0x600</define>
	<define name="_WIN32_WINNT">0x600</define>
	<library>wine</library>
	<library>oleaut32</library>
	<library>kernel32</library>
	<library>ntdll</library>
	<file>olepro32stubs.c</file>
	<file>version.rc</file>
	<file>olepro32.spec</file>
</module>
