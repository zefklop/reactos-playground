/*
 * PROJECT:         ReactOS tcpip.sys
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            drivers/network/tcpip/ip.cpp
 * PURPOSE:         loopback interface (127.0.0.1) implementation
 */

#include "tcpip.h"

NTSTATUS
LoopbackInterface::Init(void)
{
    /* Allocate our interface */
    LoopbackInterface* Interface = new LoopbackInterface();
    if (!Interface)
        return STATUS_NO_MEMORY;

    /* Set the address, gateway and mask */
    Interface->m_Address = new Ipv4Address(0xf0000001);
    if (!Interface->m_Address)
        return STATUS_NO_MEMORY;
    Interface->m_Gateway = NULL;

    Interface->m_Mask = new Ipv4Address(0xff000000);
    if (!Interface->m_Mask)
        return STATUS_NO_MEMORY;

    /* Insert it into the Ip device interface list */
    TcpIpDriver::GetIpDevice()->AddInterface(Interface);

    return STATUS_SUCCESS;
}
