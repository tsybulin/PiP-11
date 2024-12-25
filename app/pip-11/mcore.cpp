#include "mcore.h"

#include <circle/logger.h>
#include <cons/cons.h>
#include <util/queue.h>
#include "api.h"

volatile bool interrupted = false ;
volatile bool halted = false ;
volatile bool kb11hrottle = false ;

MultiCore::MultiCore(CMemorySystem *pMemorySystem, Console *pConsole, CCPUThrottle *pCpuThrottle)
:   CMultiCoreSupport (pMemorySystem),
    rkfile(0),
    rlfile(0),
    bootmon(true),
    console(pConsole),
    cpuThrottle(pCpuThrottle),
    api(0)
{
}

MultiCore::~MultiCore() {
}

boolean MultiCore::Initialize(char *kf, char *lf, bool bm) {
    interrupted = false ;
    rkfile = kf ;
    rlfile = lf ;
    bootmon = bm ;
    return CMultiCoreSupport::Initialize() ;
}

TShutdownMode startup(const char *rkfile, const char *rlfile, const bool bootmon) ;

void MultiCore::Run(unsigned ncore) {
    CLogger::Get()->Write("mcore", LogNotice, "Run %d", ncore) ;

    if (ncore == 0) {
        CNetSubSystem *net = CNetSubSystem::Get() ;
        api = new API(net) ;

        while (!interrupted) {
            TShutdownMode mode = api->loop() ;
            if (mode != ShutdownNone) {
                // interrupted = true ;
                interrupted = true ;
                console->shutdownMode = mode ;
                return ;
            }

            CScheduler::Get()->Yield() ;
        }
    }

    if (ncore == 1) {
        TShutdownMode mode = startup(rkfile, rlfile, bootmon) ;
        console->shutdownMode = mode ;
        interrupted = true ;
    }

    if (ncore == 2) {
        while(!interrupted) {
            TShutdownMode mode = console->loop() ;
            if (mode != ShutdownNone) {
                interrupted = true ;
                return ;
            }

            cpuThrottle->Update() ;
        }
    }

    if (ncore == 3) {
        return ;
    }
}

void MultiCore::IPIHandler(unsigned ncore, unsigned nipi) {
    if (nipi == IPI_USER) {
        console->shutdownMode = ShutdownReboot ;
        interrupted = true ;
    }

    if (nipi == IPI_USER + 1) {
        halted = true ;
    }

    if (nipi == IPI_USER + 2) {
        kb11hrottle = !kb11hrottle ;
        CLogger::Get()->Write("MultiCore", LogError, "KB11 Throttle %s", kb11hrottle ? "LOW" : "HIGH") ;
    }

    CMultiCoreSupport::IPIHandler(ncore, nipi) ;
}
