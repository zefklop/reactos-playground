<module name="ntoskrnl" type="kernel" installbase="system32" installname="ntoskrnl.exe">
    <bootstrap base="$(CDOUTPUT)" />
    <importlibrary definition="ntoskrnl.def" />
    <define name="_SEH_NO_NATIVE_NLG" />
    <define name="_DISABLE_TIDENTS" />
    <define name="__NTOSKRNL__" />
    <define name="_NTOSKRNL_" />
    <define name="_NTSYSTEM_" />
    <define name="__NO_CTYPE_INLINES" />
    <define name="__USE_W32API" />
    <define name="WIN9X_COMPAT_SPINLOCK" />
    <define name="_IN_KERNEL_" />
    <if property="_WINKD_" value="1">
        <define name="_WINKD_" />
    </if>
    <include base="cmlib">.</include>
    <include base="ntoskrnl">include</include>
    <include base="ReactOS">include/reactos/drivers</include>
    <library>csq</library>
    <library>hal</library>
    <library>pseh</library>
    <library>cmlib</library>
    <library>rtl</library>
    <library>rossym</library>
    <library>libcntpr</library>
    <library>kdcom</library>
    <library>bootvid</library>
    <library>wdmguid</library>
    <directory name="include">
        <pch>ntoskrnl.h</pch>
    </directory>
    <directory name="ke">
        <if property="ARCH" value="i386">
            <directory name="i386">
                <file first="true">boot.S</file>
                <file>abios.c</file>
                <file>cpu.c</file>
                <file>ctxswitch.S</file>
                <file>exp.c</file>
                <file>irqobj.c</file>
                <file>kiinit.c</file>
                <file>ldt.c</file>
                <file>mtrr.c</file>
                <file>patpge.c</file>
                <file>systimer.S</file>
                <file>thrdini.c</file>
                <file>trap.s</file>
                <file>usercall_asm.S</file>
                <file>usercall.c</file>
                <file>v86vdm.c</file>
                <file>v86m_sup.S</file>
            </directory>
        </if>
            <file>apc.c</file>
            <file>balmgr.c</file>
            <file>bug.c</file>
            <file>clock.c</file>
            <file>config.c</file>
            <file>devqueue.c</file>
            <file>dpc.c</file>
            <file>eventobj.c</file>
            <file>except.c</file>
            <file>freeldr.c</file>
            <file>gate.c</file>
            <file>gmutex.c</file>
            <file>ipi.c</file>
            <file>krnlinit.c</file>
            <file>mutex.c</file>
            <file>procobj.c</file>
            <file>profobj.c</file>
            <file>queue.c</file>
            <file>semphobj.c</file>
            <file>spinlock.c</file>
            <file>thrdschd.c</file>
            <file>thrdobj.c</file>
            <file>timerobj.c</file>
            <file>wait.c</file>
    </directory>
    <directory name="cc">
            <file>cacheman.c</file>
            <file>copy.c</file>
            <file>fs.c</file>
            <file>mdl.c</file>
            <file>pin.c</file>
            <file>view.c</file>
    </directory>
    <directory name="cm">
            <file>import.c</file>
            <file>ntfunc.c</file>
            <file>regfile.c</file>
            <file>registry.c</file>
            <file>regobj.c</file>
            <file>newcm.c</file>
            <file>cmdata.c</file>
    </directory>
    <directory name="dbgk">
        <file>dbgkutil.c</file>
        <file>dbgkobj.c</file>
    </directory>
    <directory name="ex">
        <if property="ARCH" value="i386">
            <directory name="i386">
                <file>interlck_asm.S</file>
                <file>fastinterlck_asm.S</file>
                <file>ioport.S</file>
                <file>zw.S</file>
            </directory>
        </if>
        <if property="ARCH" value="powerpc">
            <directory name="powerpc">
                <file>zw.S</file>
            </directory>
        </if>
        <if property="ARCH" value="mips">
            <directory name="mips">
                <file>zw.S</file>
            </directory>
        </if>
            <file>atom.c</file>
            <file>callback.c</file>
            <file>dbgctrl.c</file>
            <file>efi.c</file>
            <file>event.c</file>
            <file>evtpair.c</file>
            <file>fmutex.c</file>
            <file>handle.c</file>
            <file>harderr.c</file>
            <file>init.c</file>
            <file>keyedevt.c</file>
            <file>locale.c</file>
            <file>lookas.c</file>
            <file>mutant.c</file>
            <file>pushlock.c</file>
            <file>profile.c</file>
            <file>resource.c</file>
            <file>rundown.c</file>
            <file>sem.c</file>
            <file>shutdown.c</file>
            <file>sysinfo.c</file>
            <file>time.c</file>
            <file>timer.c</file>
            <file>uuid.c</file>
            <file>win32k.c</file>
            <file>work.c</file>
            <file>zone.c</file>
    </directory>
    <directory name="fsrtl">
            <file>dbcsname.c</file>
            <file>fastio.c</file>
            <file>faulttol.c</file>
            <file>filelock.c</file>
            <file>filter.c</file>
            <file>filtrctx.c</file>
            <file>fsfilter.c</file>
            <file>fsrtlpc.c</file>
            <file>largemcb.c</file>
            <file>name.c</file>
            <file>notify.c</file>
            <file>oplock.c</file>
            <file>pnp.c</file>
            <file>stackovf.c</file>
            <file>tunnel.c</file>
            <file>unc.c</file>
    </directory>
    <directory name="fstub">
        <file>disksup.c</file>
        <file>fstubex.c</file>
        <file>halstub.c</file>
    </directory>
    <directory name="inbv">
        <file>inbv.c</file>
    </directory>
    <directory name="io">
        <directory name="iomgr">
            <file>adapter.c</file>
            <file>arcname.c</file>
            <file>bootlog.c</file>
            <file>controller.c</file>
            <file>device.c</file>
            <file>deviface.c</file>
            <file>driver.c</file>
            <file>drvrlist.c</file>
            <file>error.c</file>
            <file>file.c</file>
            <file>iocomp.c</file>
            <file>ioevent.c</file>
            <file>iofunc.c</file>
            <file>iomdl.c</file>
            <file>iomgr.c</file>
            <file>iorsrce.c</file>
            <file>iotimer.c</file>
            <file>iowork.c</file>
            <file>irp.c</file>
            <file>irq.c</file>
            <file>rawfs.c</file>
            <file>remlock.c</file>
            <file>util.c</file>
            <file>symlink.c</file>
            <file>volume.c</file>
        </directory>
        <directory name="pnpmgr">
            <file>plugplay.c</file>
            <file>pnpdma.c</file>
            <file>pnpmgr.c</file>
            <file>pnpnotify.c</file>
            <file>pnpreport.c</file>
            <file>pnproot.c</file>
        </directory>
    </directory>
    <if property="_WINKD_" value="0">
        <directory name="kdbg">
            <if property="ARCH" value="i386">
                <directory name="i386">
                    <if property="KDBG" value="1">
                        <group>
                            <file>i386-dis.c</file>
                            <file>kdb_help.S</file>
                            <file>longjmp.S</file>
                            <file>setjmp.S</file>
                        </group>
                    </if>
                </directory>
            </if>
            <if property="KDBG" value="1">
                <file>kdb.c</file>
                <file>kdb_cli.c</file>
                <file>kdb_expr.c</file>
                <file>kdb_keyboard.c</file>
                <file>kdb_serial.c</file>
                <file>kdb_string.c</file>
            </if>
            <if property="DBG_OR_KDBG" value="true">
                <file>kdb_symbols.c</file>
            </if>
        </directory>
        <directory name="kd">
            <directory name="wrappers">
                <file>bochs.c</file>
                <file>gdbstub.c</file>
            </directory>
            <file>kdinit.c</file>
            <file>kdio.c</file>
            <file>kdmain.c</file>
        </directory>
    </if>
    <if property="_WINKD_" value ="1">
        <directory name="kd64">
            <file>kdapi.c</file>
            <file>kdbreak.c</file>
            <file>kddata.c</file>
            <file>kdinit.c</file>
            <file>kdlock.c</file>
            <file>kdprint.c</file>
            <file>kdtrap.c</file>
        </directory>
    </if>
    <directory name="lpc">
            <file>close.c</file>
            <file>complete.c</file>
            <file>connect.c</file>
            <file>create.c</file>
            <file>listen.c</file>
            <file>port.c</file>
            <file>reply.c</file>
            <file>send.c</file>
    </directory>
    <directory name="mm">
        <if property="ARCH" value="i386">
            <directory name="i386">
                <file>page.c</file>
            </directory>
        </if>
            <file>anonmem.c</file>
            <file>aspace.c</file>
            <file>balance.c</file>
            <file>cont.c</file>
            <file>drvlck.c</file>
            <file>freelist.c</file>
            <file>iospace.c</file>
            <file>kmap.c</file>
            <file>marea.c</file>
            <file>mdlsup.c</file>
            <file>mm.c</file>
            <file>mminit.c</file>
            <file>mpw.c</file>
            <file>ncache.c</file>
            <file>npool.c</file>
            <file>pagefile.c</file>
            <file>pageop.c</file>
            <file>pager.c</file>
            <file>pagfault.c</file>
            <file>paging.c</file>
            <file>pe.c</file>
            <file>physical.c</file>
            <file>pool.c</file>
            <file>ppool.c</file>
            <file>procsup.c</file>
            <file>region.c</file>
            <file>rmap.c</file>
            <file>section.c</file>
            <file>sysldr.c</file>
            <file>verifier.c</file>
            <file>virtual.c</file>
            <file>wset.c</file>
        <file>elf32.c</file>
        <file>elf64.c</file>
    </directory>
    <directory name="ob">
            <file>obdir.c</file>
            <file>obinit.c</file>
            <file>obhandle.c</file>
            <file>obname.c</file>
            <file>oblife.c</file>
            <file>obref.c</file>
            <file>sdcache.c</file>
            <file>obsecure.c</file>
            <file>oblink.c</file>
            <file>obwait.c</file>
    </directory>
    <directory name="po">
        <file>power.c</file>
        <file>events.c</file>
    </directory>
    <directory name="ps">
            <file>debug.c</file>
            <file>job.c</file>
            <file>kill.c</file>
            <file>psnotify.c</file>
            <file>process.c</file>
            <file>psmgr.c</file>
            <file>query.c</file>
            <file>quota.c</file>
            <file>security.c</file>
            <file>state.c</file>
            <file>thread.c</file>
            <file>win32.c</file>
    </directory>
    <directory name="rtl">
            <file>libsupp.c</file>
            <file>misc.c</file>
            <file>strtok.c</file>
    </directory>
    <directory name="se">
            <file>access.c</file>
            <file>acl.c</file>
            <file>audit.c</file>
            <file>lsa.c</file>
            <file>luid.c</file>
            <file>priv.c</file>
            <file>sd.c</file>
            <file>semgr.c</file>
            <file>sid.c</file>
            <file>token.c</file>
    </directory>
    <directory name="vdm">
        <if property="ARCH" value="i386">
            <file>vdmmain.c</file>
            <file>vdmexec.c</file>
        </if>
    </directory>
    <directory name="wmi">
        <file>wmi.c</file>
    </directory>
    <file>ntoskrnl.rc</file>
    <linkerflag>-nostartfiles</linkerflag>
    <linkerflag>-nostdlib</linkerflag>
    <linkerflag>-lgcc</linkerflag>
</module>
