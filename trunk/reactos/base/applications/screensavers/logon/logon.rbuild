<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../../tools/rbuild/project.dtd">
<module name="logon" type="win32scr" installbase="system32" installname="logon.scr">
	<include base="logon">.</include>
	<define name="__USE_W32API" />
	<define name="UNICODE" />
	<define name="_UNICODE" />

	<library>kernel32</library>
	<library>user32</library>
	<library>gdi32</library>

	<metadata description = "Default ReactOS Logo screensaver" />

	<file>logon.c</file>
	<file>logon.rc</file>
</module>
