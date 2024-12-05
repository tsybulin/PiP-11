#pragma once

#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>

class NetSend: public CTask {
    public:
        NetSend(CNetSubSystem *pnet) ;
        ~NetSend();

        void Run (void) ;

    private:
        CNetSubSystem *net ;
        CSocket *outSocket ;
} ;

