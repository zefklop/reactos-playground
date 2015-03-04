
#pragma once

#include "address.h"

class Interface : public LIST_ENTRY
{
public:
    void SetAddress(IpAddress* Address);
    void SetGateway(IpAddress* Gateway);
    void SetMask(IpAddress* Mask);

protected:
    IpAddress* m_Address;
    IpAddress* m_Gateway;
    IpAddress* m_Mask;
};
