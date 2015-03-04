
#pragma once

#include "address.h"

class Ipv4Address : public IpAddress
{
public:
    Ipv4Address(ULONG AsUlong)
    {
        m_AsUlong = AsUlong;
    }

    static void* operator new(size_t Size)
    {
        return ExAllocatePoolWithTag(NonPagedPool, Size, '4vpI');
    }

    static void operator delete(void* ptr)
    {
        ExFreePoolWithTag(ptr, '4vpI');
    }

private:
    ULONG m_AsUlong;
};
