<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../tools/rbuild/project.dtd">
<module name="kbdlv" type="keyboardlayout" entrypoint="0" installbase="system32" installname="kbdlv.dll">
	<importlibrary definition="kbdlv.spec" />
	<include base="ntoskrnl">include</include>
	<file>kbdlv.c</file>
	<file>kbdlv.rc</file>
</module>
