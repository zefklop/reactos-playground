#if defined(__cplusplus)
extern "C" {
#endif

/* Helper macro to enable gcc's extension. */
#ifndef __GNU_EXTENSION
#ifdef __GNUC__
#define __GNU_EXTENSION __extension__
#else
#define __GNU_EXTENSION
#endif
#endif

#define STDMETHODCALLTYPE __stdcall

#ifndef _WDMDDK_
typedef GUID *PGUID;
#endif

/* Types */

typedef struct _BDA_PIN_PAIRING
{
  ULONG ulInputPin;
  ULONG ulOutputPin;
  ULONG ulcMaxInputsPerOutput;
  ULONG ulcMinInputsPerOutput;
  ULONG ulcMaxOutputsPerInput;
  ULONG ulcMinOutputsPerInput;
  ULONG ulcTopologyJoints;
  const ULONG *pTopologyJoints;
} BDA_PIN_PAIRING, *PBDA_PIN_PAIRING;

typedef struct _BDA_FILTER_TEMPLATE
{
  const KSFILTER_DESCRIPTOR *pFilterDescriptor;
  ULONG ulcPinPairs;
  const BDA_PIN_PAIRING *pPinPairs;
} BDA_FILTER_TEMPLATE, *PBDA_FILTER_TEMPLATE;


typedef struct _KSM_PIN
{
  KSMETHOD Method;
    __GNU_EXTENSION union
    {
    ULONG PinId;
    ULONG PinType;
  };
  ULONG Reserved;
} KSM_PIN, * PKSM_PIN;

/* Functions */

STDMETHODIMP_(NTSTATUS) BdaCheckChanges(IN PIRP  Irp);
STDMETHODIMP_(NTSTATUS) BdaCommitChanges(IN PIRP  Irp);

STDMETHODIMP_(NTSTATUS) BdaCreateFilterFactory(
  IN PKSDEVICE pKSDevice,
  IN const KSFILTER_DESCRIPTOR *pFilterDescriptor,
  IN const BDA_FILTER_TEMPLATE *pBdaFilterTemplate);

STDMETHODIMP_(NTSTATUS) BdaCreateFilterFactoryEx(
  IN PKSDEVICE pKSDevice,
  IN const KSFILTER_DESCRIPTOR *pFilterDescriptor,
  IN const BDA_FILTER_TEMPLATE *pBdaFilterTemplate,
  OUT PKSFILTERFACTORY  *ppKSFilterFactory);

STDMETHODIMP_(NTSTATUS) BdaCreatePin(
  IN PKSFILTER pKSFilter,
  IN ULONG ulPinType,
  OUT ULONG *pulPinId);

STDMETHODIMP_(NTSTATUS) BdaCreateTopology(
  IN PKSFILTER pKSFilter,
  IN ULONG InputPinId,
  IN ULONG OutputPinId);

STDMETHODIMP_(NTSTATUS) BdaDeletePin(
  IN PKSFILTER pKSFilter,
  IN ULONG *pulPinId);

STDMETHODIMP_(NTSTATUS) BdaFilterFactoryUpdateCacheData(
  IN PKSFILTERFACTORY pFilterFactory,
  IN const KSFILTER_DESCRIPTOR *pFilterDescriptor OPTIONAL);

STDMETHODIMP_(NTSTATUS) BdaGetChangeState(
  IN PIRP Irp,
  OUT BDA_CHANGE_STATE *pChangeState);

STDMETHODIMP_(NTSTATUS) BdaInitFilter(
  IN PKSFILTER pKSFilter,
  IN const BDA_FILTER_TEMPLATE *pBdaFilterTemplate);

STDMETHODIMP_(NTSTATUS) BdaMethodCreatePin(
  IN PIRP Irp,
  IN KSMETHOD *pKSMethod,
  OUT ULONG *pulPinFactoryID);

STDMETHODIMP_(NTSTATUS) BdaMethodCreateTopology(
  IN PIRP Irp,
  IN KSMETHOD *pKSMethod,
  OPTIONAL PVOID pvIgnored);

STDMETHODIMP_(NTSTATUS) BdaMethodDeletePin(
  IN PIRP Irp,
  IN KSMETHOD *pKSMethod,
  OPTIONAL PVOID pvIgnored);

STDMETHODIMP_(NTSTATUS) BdaPropertyGetControllingPinId(
  IN PIRP Irp,
  IN KSP_BDA_NODE_PIN *pProperty,
  OUT ULONG *pulControllingPinId);

STDMETHODIMP_(NTSTATUS) BdaPropertyGetPinControl(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty,
  OUT ULONG *pulProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyNodeDescriptors(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty,
  OUT BDANODE_DESCRIPTOR *pNodeDescriptorProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyNodeEvents(
  IN PIRP Irp,
  IN KSP_NODE *pKSProperty,
  OUT GUID *pguidProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyNodeMethods(
  IN PIRP Irp,
  IN KSP_NODE *pKSProperty,
  OUT GUID *pguidProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyNodeProperties(
  IN PIRP Irp,
  IN KSP_NODE *pKSProperty,
  OUT GUID *pguidProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyNodeTypes(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty,
  OUT ULONG *pulProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyPinTypes(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty,
  OUT ULONG *pulProperty);

STDMETHODIMP_(NTSTATUS) BdaPropertyTemplateConnections(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty,
  OUT KSTOPOLOGY_CONNECTION *pConnectionProperty);

STDMETHODIMP_(NTSTATUS) BdaStartChanges(IN PIRP Irp);
STDMETHODIMP_(NTSTATUS) BdaUninitFilter(IN PKSFILTER pKSFilter);

STDMETHODIMP_(NTSTATUS) BdaValidateNodeProperty(
  IN PIRP Irp,
  IN KSPROPERTY *pKSProperty);

#if defined(__cplusplus)
}
#endif
