<?xml version="1.0"?>
<!DOCTYPE group SYSTEM "../../../tools/rbuild/project.dtd">
<group>
<module name="mingw_common" type="staticlibrary" isstartuplib="true" underscoresymbols="true" crt="dll">
	<importlibrary definition="moldname-msvcrt.def" dllname="msvcrt.dll" />
	<include base="mingw_common">include</include>
	<file>cpu_features.c</file>
	<file>CRTfmode.c</file>
	<file>CRTglob.c</file>
	<file>CRTinit.c</file>
	<file>gccmain.c</file>
	<file>getopt.c</file>
	<file>isascii.c</file>
	<file>iscsym.c</file>
	<file>iscsymf.c</file>
	<file>toascii.c</file>
	<file>_wgetopt.c</file>
	<if property="ARCH" value="i386">
		<file>pseudo-reloc.c</file>
	</if>
</module>
<module name="mingw_main" type="staticlibrary" isstartuplib="true" allowwarnings="true" crt="dll">
	<include base="mingw_common">include</include>
	<file>binmode.c</file>
	<file>crt1.c</file>
	<file>main.c</file>
</module>
<module name="mingw_wmain" type="staticlibrary" isstartuplib="true" allowwarnings="true" crt="dll">
	<include base="mingw_common">include</include>
	<file>wbinmode.c</file>
	<file>wcrt1.c</file>
	<file>wmain.c</file>
</module>
<module name="mingw_dllmain" type="staticlibrary" isstartuplib="true" crt="dll">
	<include base="mingw_common">include</include>
	<file>dllcrt1.c</file>
</module>
</group>
