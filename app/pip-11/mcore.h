#pragma once

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/types.h>
#include <circle/cputhrottle.h>

#include <cons/cons.h>

class MultiCore : public CMultiCoreSupport {
    public:
        MultiCore(CMemorySystem *pMemorySystem, Console *pConsole, CCPUThrottle *pCpuThrottle) ;
        ~MultiCore() ;
        boolean Initialize(void) ;
        void Run(unsigned ncore) ;
        virtual void IPIHandler (unsigned ncore, unsigned nipi) ;

    private:
        Console *console ;
        CCPUThrottle *cpuThrottle ;
} ;


