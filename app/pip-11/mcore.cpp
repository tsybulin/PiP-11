#include "mcore.h"

#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
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
    core3inited(false),
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
        TShutdownMode mode = startup(rkfile, rlfile, bootmon) ;
        console->shutdownMode = mode ;
    }

    if (ncore == 1) {
        return ;
    }

    if (ncore == 2) {
        while(!interrupted) {
            TShutdownMode mode = console->loop() ;
            if (mode != ShutdownNone) {
                return ;
            }

            // if (api) {
            //     api->step() ;
            // }

            cpuThrottle->Update() ;
            CScheduler::Get()->Yield() ;
        }
    }

    if (ncore == 3) {
        // CScheduler *scheduler = 0 ;
        // CNetSubSystem *net = 0 ;

        // while (!interrupted) {
        //     if (!core3inited) {
        //         core3inited = true ;

        //         scheduler = CScheduler::Get() ;
        //         net =  CNetSubSystem::Get() ;

        //         api = new API(net) ;
        //         if (!api->init()) {
        //             return ;
        //         }
        //     }

        //     if (!api || !scheduler) {
        //         return ;
        //     }

        //     TShutdownMode mode = api->loop() ;
        //     if (mode != ShutdownNone) {
        //         // interrupted = true ;
        //         return ;
        //     }

        //     scheduler->Yield() ;
        // }
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
        CLogger::Get()->Write("MultiCore", LogError, "KB11 Throttle %d", kb11hrottle) ;
    }

    CMultiCoreSupport::IPIHandler(ncore, nipi) ;
}
