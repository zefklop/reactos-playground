<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../tools/rbuild/project.dtd">
<module name="installfreeldr" type="win32cui">
	<include base="installfreeldr">.</include>
	<define name="__USE_W32API" />
	<library>kernel32</library>
	<file>install.c</file>
	<file>volume.c</file>
</module>
