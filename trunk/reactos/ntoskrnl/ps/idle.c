/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/ps/idle.c
 * PURPOSE:         Using idle time
 * PROGRAMMER:      David Welch (welch@cwcom.net)
 * UPDATE HISTORY:
 *                  Created 22/05/98
 */

/* INCLUDES *****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <internal/debug.h>

/* GLOBALS *******************************************************************/

/* FUNCTIONS *****************************************************************/

/** System idle thread procedure
 *
 */
VOID STDCALL
PsIdleThreadMain(PVOID Context)
{
   KIRQL oldlvl;

   PKPCR Pcr = KeGetCurrentKPCR();
   
   for(;;)
     {
       if (Pcr->PrcbData.DpcData[0].DpcQueueDepth > 0)
	 {
	   KeRaiseIrql(DISPATCH_LEVEL,&oldlvl);
	   KiDispatchInterrupt();
	   KeLowerIrql(oldlvl);
	 }

       NtYieldExecution();

       Ke386HaltProcessor();
     }
}


/** Initialization of system idle thread
 *
 */ 
VOID INIT_FUNCTION
PsInitIdleThread(VOID)
{
   NTSTATUS Status;
   PETHREAD IdleThread;
   HANDLE IdleThreadHandle;
   
   Status = PsCreateSystemThread(&IdleThreadHandle,
			THREAD_ALL_ACCESS,
			NULL,
			NULL,
			NULL,
			PsIdleThreadMain,
			NULL);
   if(!NT_SUCCESS(Status)) 
   {
	DPRINT("Couldn't create Idle System Thread!");
	KEBUGCHECK(0);
	return;
   }   
   Status = ObReferenceObjectByHandle(IdleThreadHandle,
				      THREAD_ALL_ACCESS,
				      PsThreadType,
				      KernelMode,
				      (PVOID*)&IdleThread,
				      NULL);
   if(!NT_SUCCESS(Status)) 
   {
	DPRINT("Couldn't get pointer to Idle System Thread!");
	KEBUGCHECK(0);
	return;
   }
   NtClose(IdleThreadHandle);
   KeGetCurrentKPCR()->PrcbData.IdleThread = &IdleThread->Tcb;
   KeSetPriorityThread(&IdleThread->Tcb, LOW_PRIORITY);
   KeSetAffinityThread(&IdleThread->Tcb, 1 << 0);

}
