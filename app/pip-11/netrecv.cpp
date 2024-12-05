#include "netrecv.h"

#include <circle/net/in.h>
#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include <cons/cons.h>
#include <circle/logger.h>

static const u16 CONSOLE_PORT = 5367 ;
extern queue_t netrecv_queue ;

NetRecv::NetRecv(CNetSubSystem *pnet) :
    net(pnet),
    inpSocket(0)
{
}

NetRecv::~NetRecv() {
    delete inpSocket ;
    inpSocket = 0 ;

    net = 0 ;
}

void NetRecv::Run() {
    inpSocket = new CSocket(net, IPPROTO_UDP) ;
    if (inpSocket->Bind(CONSOLE_PORT) < 0) {
        gprintf("NetRecv: Cannot bind to port") ;
        return ;
    }

    u8 c[81] ;
    u16 clientPort ;
    CIPAddress clientIP ;

    while (true) {
        memset(c, 0, 81) ;
        int res = inpSocket->ReceiveFrom(&c, 80, 0, &clientIP, &clientPort) ;
        if (res < 0) {
            gprintf("NetRecv: Cannot receive char") ;
            CScheduler::Get()->Yield() ;
            continue ;
        }

        if (res == 0) {
            gprintf("NetRecv: Got empty char") ;
            CScheduler::Get()->Yield() ;
            continue ;
        }

        for (int i = 0; i < res; i++) {
            u8 cc = c[i] ;
            CLogger::Get()->Write("NetRecv", LogError, "%03o", cc) ;
            queue_try_add(&netrecv_queue, &cc) ;
        }

        CScheduler::Get()->Yield() ;
    }
}