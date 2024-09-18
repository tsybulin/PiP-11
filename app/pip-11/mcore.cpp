#include "mcore.h"

#include <circle/sched/scheduler.h>
#include <cons/cons.h>
#include <util/queue.h>


extern queue_t console_queue ;
volatile bool interrupted = false ;

MultiCore::MultiCore(CMemorySystem *pMemorySystem, Console *pConsole, CCPUThrottle *pCpuThrottle) :
CMultiCoreSupport (pMemorySystem),
console(pConsole),
cpuThrottle(pCpuThrottle)
{
}

MultiCore::~MultiCore() {
}

boolean MultiCore::Initialize(void) {
    interrupted = false ;
    return CMultiCoreSupport::Initialize() ;
}

void MultiCore::Run(unsigned ncore) {
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

            cpuThrottle->Update() ;
        }
    }
}

void MultiCore::IPIHandler(unsigned ncore, unsigned nipi) {
    if (nipi == IPI_USER) {
        interrupted = true ;
    }

    CMultiCoreSupport::IPIHandler(ncore, nipi) ;
}
