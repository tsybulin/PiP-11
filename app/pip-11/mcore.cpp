#include "mcore.h"

#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <cons/cons.h>
#include <util/queue.h>
#include "api.h"

extern queue_t console_queue ;
volatile bool interrupted = false ;
volatile bool halted = false ;

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
        char c ;
        while (!interrupted) {
            if (queue_try_remove(&console_queue, &c)) {
                console->putCharVT(c) ;
            }
        }
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
        }
    }

    if (ncore == 3) {
        CScheduler *scheduler = 0 ;
        CNetSubSystem *net = 0 ;

        while (!interrupted) {
            if (!core3inited) {
                core3inited = true ;

                scheduler = new CScheduler() ;

                const u8 ipaddress[] = {172, 16, 103, 101} ;
                const u8 netmask[]   = {255, 255, 255, 0} ;
                const u8 gateway[]   = {172, 16, 103, 254} ;
                const u8 dns[]       = {172, 16, 103, 254} ;
                net = new CNetSubSystem(ipaddress, netmask, gateway, dns, "pip11") ;
                if (!net->Initialize()) {
                    gprintf("Core3: Net initilize error") ;
                    return ;
                }

                api = new API(net) ;
                if (!api->init()) {
                    return ;
                }
            }

            if (!api || !scheduler) {
                return ;
            }

            TShutdownMode mode = api->loop() ;
            if (mode != ShutdownNone) {
                // interrupted = true ;
                return ;
            }

            scheduler->Yield() ;
        }
    }
}

void MultiCore::IPIHandler(unsigned ncore, unsigned nipi) {
    if (nipi == IPI_USER) {
        interrupted = true ;
    }

    if (nipi == IPI_USER + 1) {
        halted = true ;
    }

    CMultiCoreSupport::IPIHandler(ncore, nipi) ;
}
