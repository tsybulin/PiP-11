#pragma once

#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>

class NetRecv: public CTask {
    public:
        NetRecv(CNetSubSystem *pnet) ;
        ~NetRecv();

        void Run (void) ;

    private:
        CNetSubSystem *net ;
        CSocket *inpSocket ;
} ;

