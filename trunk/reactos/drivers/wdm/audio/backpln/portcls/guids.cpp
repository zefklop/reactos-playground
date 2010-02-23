/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Kernel Streaming
 * FILE:            drivers/wdm/audio/backpln/portcls/guids.cpp
 * PURPOSE:         portcls guid mess
 * PROGRAMMER:      Johannes Anderwald
 */

#ifdef _MSC_VER
#define PUT_GUIDS_HERE
#endif
#include "private.hpp"

#ifndef _MSC_VER
const GUID CLSID_PortTopology = {0xb4c90a32L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID CLSID_PortMidi = {0xb4c90a43L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID CLSID_PortWaveCyclic = {0xb4c90a2aL, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID CLSID_PortWavePci = {0xb4c90a54L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID CLSID_PortDMus    = {0xb7902fe9L, 0xfb0a, 0x11d1, {0x81, 0xb0, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};
const GUID CLSID_PortWaveRT  = {0xcc9be57a, 0xeb9e, 0x42b4, {0x94, 0xfc, 0xc, 0xad, 0x3d, 0xbc, 0xe7, 0xfa}};

const GUID IID_IMiniportDMus = {0xc096df9dL, 0xfb09, 0x11d1, {0x81, 0xb0, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};
const GUID IID_IMiniportTopology = {0xb4c90a31L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IMiniportMidi    = {0xb4c90a41L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IMiniportWavePci = {0xb4c90a52L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};

const GUID GUID_NULL ={0x00000000L, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

const GUID IID_IPortTopology = {0xb4c90a30L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};

const GUID CLSID_MiniportDriverDMusUART = {0xd3f0ce1c, 0xfffc, 0x11d1, {0x81, 0xb0, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};
const GUID CLSID_MiniportDriverDMusUARTCapture = {0xd3f0ce1d, 0xfffc, 0x11d1, {0x81, 0xb0, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};
const GUID CLSID_MiniportDriverUart = {0xb4c90ae1L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};

const GUID CLSID_MiniportDriverFmSynth = {0xb4c90ae0L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID CLSID_MiniportDriverFmSynthWithVol = {0xe5a3c139L, 0xf0f2, 0x11d1, {0x81, 0xaf, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};


const GUID IID_IPort =    {0xb4c90a25L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IDrmPort = {0x286D3DF8L, 0xCA22, 0x4E2E, {0xB9, 0xBC, 0x20, 0xB4, 0xF0, 0xE2, 0x01, 0xCE}};
const GUID IID_IDrmPort2 = {0x1ACCE59CL, 0x7311, 0x4B6B, {0x9F, 0xBA, 0xCC, 0x3B, 0xA5, 0x9A, 0xCD, 0xCE}};
const GUID IID_IInterruptSync = {0x22C6AC63L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IPortWavePci = {0xb4c90a50L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IPortWavePciStream = {0xb4c90a51L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IPortMidi    = {0xb4c90a40L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IPortDMus    = {0xc096df9cL, 0xfb09, 0x11d1, {0x81, 0xb0, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};
const GUID IID_IAdapterPowerManagement = {0x793417D0L, 0x35FE, 0x11D1, {0xAD, 0x08, 0x00, 0xA0, 0xC9, 0x0A, 0xB1, 0xB0}};

const GUID IID_IMiniportWaveCyclic = {0xb4c90a27L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IPortWaveCyclic = {0xb4c90a26L, 0x5791, 0x11d0, {0x86, 0xf9, 0x00, 0xa0, 0xc9, 0x11, 0xb5, 0x44}};
const GUID IID_IResourceList = {0x22C6AC60L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IServiceGroup = {0x22C6AC65L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IPinCount = {0x5dadb7dcL, 0xa2cb, 0x4540, {0xa4, 0xa8, 0x42, 0x5e, 0xe4, 0xae, 0x90, 0x51}};
const GUID IID_IPowerNotify = {0x3DD648B8L, 0x969F, 0x11D1, {0x95, 0xA9, 0x00, 0xC0, 0x4F, 0xB9, 0x25, 0xD3}};
const GUID IID_IDmaChannelSlave = {0x22C6AC62L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IDmaChannel = {0x22C6AC61L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IRegistryKey = {0xE8DA4302l, 0xF304, 0x11D0, {0x95, 0x8B, 0x00, 0xC0, 0x4F, 0xB9, 0x25, 0xD3}};
const GUID IID_IServiceSink = {0x22C6AC64L, 0x851B, 0x11D0, {0x9A, 0x7F, 0x00, 0xAA, 0x00, 0x38, 0xAC, 0xFE}};
const GUID IID_IPortClsVersion = {0x7D89A7BBL, 0x869B, 0x4567, {0x8D, 0xBE, 0x1E, 0x16, 0x8C, 0xC8, 0x53, 0xDE}};
const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID IID_IPortEvents = {0xA80F29C4L, 0x5498, 0x11D2, {0x95, 0xD9, 0x00, 0xC0, 0x4F, 0xB9, 0x25, 0xD3}};

const GUID KSNAME_PIN           = {0x146F1A80, 0x4791, 0x11D0, {0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}};
const GUID IID_IDrmAudioStream  = {0x1915c967, 0x3299, 0x48cb, {0xa3, 0xe4, 0x69, 0xfd, 0x1d, 0x1b, 0x30, 0x6e}};

//FIXME
//
const GUID KS_CATEGORY_AUDIO    = {0x6994AD04, 0x93EF, 0x11D0, {0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
const GUID KS_CATEGORY_RENDER   = {0x65E8773E, 0x8F56, 0x11D0, {0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
const GUID KS_CATEGORY_CAPTURE  = {0x65E8773D, 0x8F56, 0x11D0, {0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
const GUID KS_CATEGORY_TOPOLOGY = {0xDDA54A40, 0x1E4C, 0x11D1, {0xA0, 0x50, 0x40, 0x57, 0x05, 0xC1, 0x00, 0x00}};


const GUID KSDATAFORMAT_TYPE_AUDIO =             {0x73647561L, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID KSDATAFORMAT_SUBTYPE_PCM =            {0x00000001L, 0x0000, 0x0010,  {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID KSDATAFORMAT_SPECIFIER_WAVEFORMATEX = {0x05589f81L, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};

const GUID KSPROPSETID_Topology                = {0x720D4AC0L, 0x7533, 0x11D0, {0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}};
const GUID KSPROPSETID_Pin                     = {0x8C134960L, 0x51AD, 0x11CF, {0x87, 0x8A, 0x94, 0xF8, 0x01, 0xC1, 0x00, 0x00}};
const GUID KSPROPSETID_Connection              = {0x1D58C920L, 0xAC9B, 0x11CF, {0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}};
const GUID KSPROPTYPESETID_General             = {0x97E99BA0L, 0xBDEA, 0x11CF, {0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}};

const GUID KSEVENTSETID_LoopedStreaming        = {0x4682B940L, 0xC6EF, 0x11D0, {0x96, 0xD8, 0x00, 0xAA, 0x00, 0x51, 0xE5, 0x1D}};
const GUID KSEVENTSETID_Connection             = {0x7f4bcbe0L, 0x9ea5, 0x11cf, {0xa5, 0xd6, 0x28, 0xdb, 0x04, 0xc1, 0x00, 0x00}};

const GUID IID_IAllocatorMXF                   = {0xa5f0d62cL, 0xb30f, 0x11d2, {0xb7, 0xa3, 0x00, 0x60, 0x08, 0x33, 0x16, 0xc1}};

const GUID KSPROPSETID_Audio = {0x45FFAAA0L, 0x6E1B, 0x11D0, {0xBC, 0xF2, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
///
/// undocumented guids

const GUID IID_ISubdevice = {0xB4C90A61, 0x5791, 0x11D0, {0x86, 0xF9, 0x00, 0xA0, 0xC9, 0x11, 0xB5, 0x44}};
const GUID IID_IIrpTarget = {0xB4C90A60, 0x5791, 0x11D0, {0xF9, 0x86, 0x00, 0xA0, 0xC9, 0x11, 0xB5, 0x44}};
const GUID IID_IIrpTargetFactory =  {0xB4C90A62, 0x5791, 0x11D0, {0xF9, 0x86, 0x00, 0xA0, 0xC9, 0x11, 0xB5, 0x44}};


///
/// >= Vista GUIDs
///
const GUID IID_IPortWaveRT                       = {0x339ff909, 0x68a9, 0x4310, {0xb0, 0x9b, 0x27, 0x4e, 0x96, 0xee, 0x4c, 0xbd}};
const GUID IID_IPortWaveRTStream                 = {0x1809ce5a, 0x64bc, 0x4e62, {0xbd, 0x7d, 0x95, 0xbc, 0xe4, 0x3d, 0xe3, 0x93}};
const GUID IID_IMiniportWaveRT                   = {0x0f9fc4d6, 0x6061, 0x4f3c, {0xb1, 0xfc, 0x7, 0x5e, 0x35, 0xf7, 0x96, 0xa}};
const GUID IID_IMiniportWaveRTStream             = {0x000ac9ab, 0xfaab, 0x4f3d, {0x94, 0x55, 0x6f, 0xf8, 0x30, 0x6a, 0x74, 0xa0}};
const GUID IID_IMiniportWaveRTStreamNotification = {0x23759128, 0x96f1, 0x423b, {0xab, 0x4d, 0x81, 0x63, 0x5b, 0xcf, 0x8c, 0xa1}};
const GUID IID_IUnregisterSubdevice              = {0x16738177, 0xe199, 0x41f9, {0x9a, 0x87, 0xab, 0xb2, 0xa5, 0x43, 0x2f, 0x21}};
const GUID IID_IUnregisterPhysicalConnection     = {0x6c38e231, 0x2a0d, 0x428d, {0x81, 0xf8, 0x07, 0xcc, 0x42, 0x8b, 0xb9, 0xa4}};

#endif
