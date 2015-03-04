
#pragma once

#include "interface.h"

class LoopbackInterface : public Interface
{
public:
    static NTSTATUS Init(void);
private:
    /* This can be private, as we only call this from Init */
    static void* operator new (size_t Size)
    {
        return ExAllocatePoolWithTag(NonPagedPool, Size, 'oLpI');
    }

    static void operator delete(void* ptr)
    {
        ExFreePoolWithTag(ptr, 'oLpI');
    }
};
