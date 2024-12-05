#include "netsend.h"

#include <circle/net/in.h>
#include <circle/sched/scheduler.h>
#include <circle/logger.h>
#include <util/queue.h>

extern queue_t netsend_queue ;
static const u8 ipaddress[] = {172, 16, 103, 103} ;
static const u16 CONSOLE_PORT = 5367 ;

NetSend::NetSend(CNetSubSystem *pnet) :
    net(pnet),
    outSocket(0)
{
}

NetSend::~NetSend() {
    delete outSocket ;
    outSocket = 0 ;

    net = 0 ;
}

void NetSend::Run() {
    outSocket = new CSocket(net, IPPROTO_UDP) ;
    CIPAddress panelIP(ipaddress) ;

    u8 c ;

    while (true) {
        if (queue_try_remove(&netsend_queue, &c)) {
            if (outSocket->Connect(panelIP, CONSOLE_PORT) < 0) {
                CLogger::Get()->Write("NetSend", LogError, "connect error") ;
                CScheduler::Get()->Yield() ;
                continue ;
            }

            if (outSocket->Send(&c, 1, 0) < 0) {
                CLogger::Get()->Write("NetSend", LogError, "send error") ;
            }
        }

        CScheduler::Get()->Yield() ;
    }
    
}