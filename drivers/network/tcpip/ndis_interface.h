
#pragma once

#include <ndis.h>

class NdisInterface
{
public:
    /* NDIS bindings initialization */
    static NTSTATUS Init();

private:
    /* The handle we get from NdisRegisterProtocol */
    static NDIS_HANDLE g_NdisHandle;
    /* NDIS callbacks */
    static VOID NTAPI OpenAdapterComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ NDIS_STATUS Status,
        _In_ NDIS_STATUS OpenErrorStatus);
    static VOID NTAPI CloseAdapterComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ NDIS_STATUS Status);
    static VOID NTAPI SendComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ PNDIS_PACKET Packet,
        _In_ NDIS_STATUS Status);
    static VOID NTAPI TransferDataComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ PNDIS_PACKET Packet,
        _In_ NDIS_STATUS Status,
        _In_ UINT BytesTransferred);
    static VOID NTAPI ResetComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ NDIS_STATUS Status);
    static VOID NTAPI RequestComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ PNDIS_REQUEST NdisRequest,
        _In_ NDIS_STATUS Status);
    static NDIS_STATUS NTAPI Receive(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ NDIS_HANDLE MacReceiveContext,
        _In_ PVOID HeaderBuffer,
        _In_ UINT HeaderBufferSize,
        _In_ PVOID LookAheadBuffer,
        _In_ UINT LookaheadBufferSize,
        _In_ UINT PacketSize);
    static VOID NTAPI ReceiveComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext);
    static VOID NTAPI StatusComplete(
        _In_ NDIS_HANDLE ProtocolBindingContext);
    static INT NTAPI ReceivePacket(
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ PNDIS_PACKET Packet);
    static VOID NTAPI BindAdapter(
        _Out_ PNDIS_STATUS Status,
        _In_ NDIS_HANDLE BindContext,
        _In_ PNDIS_STRING DeviceName,
        _In_ PVOID SystemSpecific1,
        _In_ PVOID SystemSpecific2);
    static VOID NTAPI UnbindAdapter(
        _Out_ PNDIS_STATUS Status,
        _In_ NDIS_HANDLE ProtocolBindingContext,
        _In_ NDIS_HANDLE UnbindContext);
};
