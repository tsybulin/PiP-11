#pragma once

#include <circle/types.h>
#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include "api.h"

class BootSelector : public CTask {
    public:
        BootSelector(CNetSubSystem *pNet) ;
        ~BootSelector() ;
        void Run(void) ;
        void SetTerminated(void) ;

        inline u16 getSwitchRegister() {
            return switchRegister ;
        }

        inline CPUStatus getCPUStatus() {
            return cpuStatus ;
        }

    private:
        CNetSubSystem *pnet ;
    	CSocket *inpSocket ;
        bool terminated ;
        u16 switchRegister ;
        CPUStatus cpuStatus ;
} ;
