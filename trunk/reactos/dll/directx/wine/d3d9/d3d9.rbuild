<?xml version="1.0"?>
<!DOCTYPE module SYSTEM "../../../../tools/rbuild/project.dtd">
<module name="d3d9" type="win32dll" installbase="system32" installname="d3d9.dll" allowwarnings ="true" unicode="yes">
	<importlibrary definition="d3d9.spec.def" />
	<include base="d3d9">.</include>
	<include base="ReactOS">include/reactos/wine</include>
	<define name="_WIN32_IE">0x600</define>
	<define name="_WIN32_WINNT">0x501</define>
	<define name="WINVER">0x501</define>
	<define name="__WINESRC__" />
	<define name="USE_WIN32_OPENGL" />

	<library>uuid</library>
	<library>wine</library>
	<library>user32</library>
	<library>opengl32</library>
	<library>gdi32</library>
	<library>advapi32</library>
	<library>kernel32</library>
	<library>wined3d</library>

	<file>basetexture.c</file>
	<file>cubetexture.c</file>
	<file>d3d9_main.c</file>
	<file>device.c</file>
	<file>directx.c</file>
	<file>indexbuffer.c</file>
	<file>pixelshader.c</file>
	<file>query.c</file>
	<file>resource.c</file>
	<file>stateblock.c</file>
	<file>surface.c</file>
	<file>swapchain.c</file>
	<file>texture.c</file>
	<file>vertexbuffer.c</file>
	<file>vertexdeclaration.c</file>
	<file>vertexshader.c</file>
	<file>volume.c</file>
	<file>volumetexture.c</file>
	<file>version.rc</file>
	<file>d3d9.spec</file>
</module>
