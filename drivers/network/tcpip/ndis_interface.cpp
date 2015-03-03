/*
 * PROJECT:         ReactOS tcpip.sys
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            drivers/network/tcpip/ndis_interface.cpp
 * PURPOSE:         NDIS bindings for tcpip.sys
 */

#include "tcpip.h"

#define NDEBUG
#include <debug.h>

// Global initializers
NDIS_HANDLE NdisInterface::g_NdisHandle = NULL;

NTSTATUS
NdisInterface::Init(void)
{
    NDIS_STATUS Status;
    NDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics;

    /* Simply fill in the structure and pass it to the NDIS driver. */
    RtlZeroMemory(&ProtocolCharacteristics, sizeof(ProtocolCharacteristics));
    ProtocolCharacteristics.Ndis40Chars.MajorNdisVersion = NDIS_PROTOCOL_MAJOR_VERSION;
    ProtocolCharacteristics.Ndis40Chars.MinorNdisVersion = NDIS_PROTOCOL_MINOR_VERSION;
    ProtocolCharacteristics.Ndis40Chars.Reserved = 0;
    ProtocolCharacteristics.Ndis40Chars.OpenAdapterCompleteHandler = OpenAdapterComplete;
    ProtocolCharacteristics.Ndis40Chars.CloseAdapterCompleteHandler = CloseAdapterComplete;
    ProtocolCharacteristics.Ndis40Chars.SendCompleteHandler = SendComplete;
    ProtocolCharacteristics.Ndis40Chars.TransferDataCompleteHandler = TransferDataComplete;
    ProtocolCharacteristics.Ndis40Chars.ResetCompleteHandler = ResetComplete;
    ProtocolCharacteristics.Ndis40Chars.RequestCompleteHandler = RequestComplete;
    ProtocolCharacteristics.Ndis40Chars.ReceiveHandler = Receive;
    ProtocolCharacteristics.Ndis40Chars.ReceiveCompleteHandler = ReceiveComplete;
    ProtocolCharacteristics.Ndis40Chars.StatusCompleteHandler = StatusComplete;
    ProtocolCharacteristics.Ndis40Chars.ReceivePacketHandler = ReceivePacket;
    ProtocolCharacteristics.Ndis40Chars.BindAdapterHandler = BindAdapter;
    ProtocolCharacteristics.Ndis40Chars.UnbindAdapterHandler = UnbindAdapter;
    RtlInitUnicodeString(&ProtocolCharacteristics.Ndis40Chars.Name, L"TcpIp");

    NdisRegisterProtocol(&Status, &g_NdisHandle, &ProtocolCharacteristics, sizeof(ProtocolCharacteristics));

    return Status;
}

VOID
NTAPI
NdisInterface::OpenAdapterComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ NDIS_STATUS Status,
    _In_ NDIS_STATUS OpenErrorStatus)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::CloseAdapterComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ NDIS_STATUS Status)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::SendComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ PNDIS_PACKET Packet,
    _In_ NDIS_STATUS Status)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::TransferDataComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ PNDIS_PACKET Packet,
    _In_ NDIS_STATUS Status,
    _In_ UINT BytesTransferred)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::ResetComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ NDIS_STATUS Status)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::RequestComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ PNDIS_REQUEST NdisRequest,
    _In_ NDIS_STATUS Status)
{
    UNIMPLEMENTED;
}

NDIS_STATUS
NTAPI
NdisInterface::Receive(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ NDIS_HANDLE MacReceiveContext,
    _In_ PVOID HeaderBuffer,
    _In_ UINT HeaderBufferSize,
    _In_ PVOID LookAheadBuffer,
    _In_ UINT LookaheadBufferSize,
    _In_ UINT PacketSize)
{
    UNIMPLEMENTED;
    return NDIS_STATUS_NOT_ACCEPTED;
}

VOID
NTAPI
NdisInterface::ReceiveComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext)
{
    UNIMPLEMENTED;
}

VOID
NTAPI
NdisInterface::StatusComplete(
    _In_ NDIS_HANDLE ProtocolBindingContext)
{
    UNIMPLEMENTED;
}

INT
NTAPI
NdisInterface::ReceivePacket(
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ PNDIS_PACKET Packet)
{
    UNIMPLEMENTED;
    return 0;
}

VOID
NTAPI
NdisInterface::BindAdapter(
    _Out_ PNDIS_STATUS Status,
    _In_ NDIS_HANDLE BindContext,
    _In_ PNDIS_STRING DeviceName,
    _In_ PVOID SystemSpecific1,
    _In_ PVOID SystemSpecific2)
{
    UNIMPLEMENTED;
    *Status = NDIS_STATUS_UNSUPPORTED_MEDIA;
}

VOID
NTAPI
NdisInterface::UnbindAdapter(
    _Out_ PNDIS_STATUS Status,
    _In_ NDIS_HANDLE ProtocolBindingContext,
    _In_ NDIS_HANDLE UnbindContext)
{
    UNIMPLEMENTED;
    *Status = NDIS_STATUS_SUCCESS;
}
