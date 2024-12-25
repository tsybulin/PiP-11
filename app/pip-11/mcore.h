#pragma once

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/types.h>
#include <circle/cputhrottle.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>

#include <cons/cons.h>

class API ;

class MultiCore : public CMultiCoreSupport {
    public:
        MultiCore(CMemorySystem *pMemorySystem, Console *pConsole, CCPUThrottle *pCpuThrottle) ;
        ~MultiCore() ;
        boolean Initialize(char *kf, char *lf, bool bm) ;
        void Run(unsigned ncore) ;
        virtual void IPIHandler (unsigned ncore, unsigned nipi) ;
    private:
        char *rkfile ;
        char *rlfile ;
        bool bootmon ;
        Console *console ;
        CCPUThrottle *cpuThrottle ;
        API *api ;
} ;

