/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS TCP/IP protocol driver
 * FILE:        include/router.h
 * PURPOSE:     IP routing definitions
 */
#ifndef __ROUTER_H
#define __ROUTER_H

#include <neighbor.h>


/* Forward Information Base Entry */
typedef struct _FIB_ENTRY {
    LIST_ENTRY ListEntry;         /* Entry on list */
    OBJECT_FREE_ROUTINE Free;     /* Routine used to free resources for the object */
    IP_ADDRESS NetworkAddress;    /* Address of network */
    IP_ADDRESS Netmask;           /* Netmask of network */
    PNEIGHBOR_CACHE_ENTRY Router; /* Pointer to NCE of router to use */
    UINT Metric;                  /* Cost of this route */
} FIB_ENTRY, *PFIB_ENTRY;

PFIB_ENTRY RouterAddRoute(
    PIP_ADDRESS NetworkAddress,
    PIP_ADDRESS Netmask,
    PNEIGHBOR_CACHE_ENTRY Router,
    UINT Metric);

PNEIGHBOR_CACHE_ENTRY RouterGetRoute(PIP_ADDRESS Destination);

VOID RouterRemoveRoute(
    PFIB_ENTRY FIBE);

PFIB_ENTRY RouterCreateRoute(
    IP_ADDRESS NetworkAddress,
    IP_ADDRESS Netmask,
    IP_ADDRESS RouterAddress,
    PIP_INTERFACE Interface,
    UINT Metric);

NTSTATUS RouterStartup(
    VOID);

NTSTATUS RouterShutdown(
    VOID);

UINT CountFIBs();

UINT CopyFIBs( PFIB_ENTRY Target );

#endif /* __ROUTER_H */

/* EOF */
