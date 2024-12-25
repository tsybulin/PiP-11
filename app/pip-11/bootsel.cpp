#include "bootsel.h"

#include <circle/net/in.h>
#include <circle/sched/scheduler.h>
#include <circle/logger.h>
#include <circle/util.h>

static const u16 API_PORT = 5366 ;

BootSelector::BootSelector(CNetSubSystem *pNet) :
    pnet(pNet),
    inpSocket(0),
    terminated(false),
    switchRegister(0),
    cpuStatus(CPU_STATUS_UNKNOWN)
{
}

BootSelector::~BootSelector() {
    delete inpSocket ;
    inpSocket = 0 ;

    pnet = 0 ;
}

void BootSelector::SetTerminated() {
    terminated = true ;
}

void BootSelector::Run() {
    inpSocket = new CSocket(pnet, IPPROTO_UDP) ;
    if (inpSocket->Bind(API_PORT) < 0) {
        gprintf("BootSelector: Cannot bind to port") ;
        return ;
    }

    u8 buffer[FRAME_BUFFER_SIZE] ;
    int bufcnt ;
    api_command_packet_t acp ;
    size_t sz = sizeof(acp) ;
    CScheduler *scheduler = CScheduler::Get() ;

    while (!terminated) {
        int res = inpSocket->Receive(buffer, FRAME_BUFFER_SIZE, MSG_DONTWAIT) ;

        if (res < 0) {
            CLogger::Get()->Write("BootSelector", LogError, "Cannot receive request") ;
            scheduler->Yield() ;
            continue ;
        }

        if (res == 0) {
            scheduler->Yield() ;
            continue ;
        }

        bufcnt = 0 ;
        while (bufcnt < res) {
            memcpy(&acp, buffer + bufcnt, sz) ;
            bufcnt += sz ;

            if (acp.preamble != API_PACKET_PREAMBLE) {
                CLogger::Get()->Write("BootSelector", LogError, "Incorrect preamble 0x%04X", acp.preamble) ;
                continue ;
            }

            // CLogger::Get()->Write("BootSelector", LogError, "SWR %06o, arg0 %06o", acp.switchRegister, acp.arg0) ;
            if (acp.command == API_COMMAND_CPU_STATUS) {
                switchRegister = acp.switchRegister ;
                if (acp.arg0) {
                    cpuStatus = (CPUStatus) acp.arg0 ;
                }
            }
            scheduler->Yield() ;
        }

    }

    CTask::Terminate() ;
}