#include "api.h"

#include <circle/net/in.h>
#include <circle/util.h>

extern KB11 cpu ;
static const u16 API_PORT = 5366 ;
extern volatile bool kb11hrottle ;

API::API(CNetSubSystem *pNet)
:   pnet(pNet),
    inpSocket(0),
    outSocket(0),
    mode(ShutdownNone)
{
    api_responce_packet_t arp ;
    queue_init(&api_queue, sizeof arp, 64) ;
}

API::~API(void) {
    delete inpSocket ;
    inpSocket = 0 ;

    delete outSocket ;
    outSocket = 0 ;

    pnet = 0 ;
}

void API::Run(void) {
    inpSocket = new CSocket(pnet, IPPROTO_UDP) ;
    if (inpSocket->Bind(API_PORT) < 0) {
        gprintf("API: Cannot bind to port") ;
        return ;
    }

    outSocket = new CSocket(pnet, IPPROTO_UDP) ;

    while(true) {
        api_command_packet_t acp ;
        u16 clientPort ;
        CIPAddress panelIP ;
        u8 buffer[FRAME_BUFFER_SIZE] ;
        int bufcnt ;

        size_t sz = sizeof(acp) ;
        bufcnt = 0 ;

        int res = inpSocket->ReceiveFrom(buffer, FRAME_BUFFER_SIZE, 0, &panelIP, &clientPort) ;
        if (res < 0) {
            gprintf("API: Cannot receive request") ;
            continue ;
        }

        if (res == 0) {
            continue ;
        }

        while (bufcnt < res) {
            memcpy(&acp, buffer + bufcnt, sz) ;
            bufcnt += sz ;

            if (acp.preamble != API_PACKET_PREAMBLE) {
                gprintf("API: Incorrect preamble 0x%04X", acp.preamble) ;
                continue ;
            }

            this->processCommand(acp) ;
        }
    }
}

void API::processCommand(api_command_packet_t acp) {
    cpu.switchregister = acp.switchRegister ;

    switch (acp.command) {
        case API_COMMAND_CPU_STATUS:
            switch (acp.arg0) {
                case CPU_STATUS_UNKNOWN: // read-only
                    break ;
                case CPU_STATUS_ENABLE:
                    if (acp.arg1) {
                        gprintf("API: CPU reset") ;
                        cpu.RESET() ;
                    }
                    cpu.cpuStatus = CPU_STATUS_ENABLE ;
                    gprintf("API: set CPU state ENABLE") ;
                    break ;
                case CPU_STATUS_HALT:
                    gprintf("API: set CPU state HALT") ;
                    cpu.cpuStatus = CPU_STATUS_HALT ;
                    if (acp.arg1) {
                        gprintf("API: CPU reset") ;
                        cpu.RESET() ;
                    }
                    break ;
                case CPU_STATUS_STEP:
                    cpu.cpuStatus = CPU_STATUS_STEP ;
                    break ;
                default:
                    gprintf("API: unknown cpu state 0%06o", acp.arg0) ;
                    break ;
            }
            this->sendResponce(API_COMMAND_CPU_STATUS, cpu.cpuStatus, cpu.RR[7]) ;
            break;

        case API_COMMAND_EXAMINE: {
                u16 data = 0 ;
                switch (acp.arg0) {
                    case 017777707:
                        data = cpu.RR[7] ;
                        break ;
                    case 017777706:
                        data = cpu.RR[6] ;
                        break ;
                    case 017777705:
                        data = cpu.RR[5] ;
                        break ;
                    case 017777704:
                        data = cpu.RR[4] ;
                        break ;
                    case 017777703:
                        data = cpu.RR[3] ;
                        break ;
                    case 017777702:
                        data = cpu.RR[2] ;
                        break ;
                    case 017777701:
                        data = cpu.RR[1] ;
                        break ;
                    case 017777700:
                        data = cpu.RR[0] ;
                        break ;

                    case 017777715:
                        data = cpu.RR[13] ;
                        break ;
                    case 017777714:
                        data = cpu.RR[12] ;
                        break ;
                    case 017777713:
                        data = cpu.RR[11] ;
                        break ;
                    case 017777712:
                        data = cpu.RR[10] ;
                        break ;
                    case 017777711:
                        data = cpu.RR[9] ;
                        break ;
                    case 017777710:
                        data = cpu.RR[8] ;
                        break ;

                    case 017777716:
                        data = cpu.stackpointer[1] ;
                        break ;
                    case 017777717:
                        data = cpu.stackpointer[3] ;
                        break ;

                    case 017777570:
                        data = cpu.displayregister ;
                        break ;

                    default:
                        data = cpu.unibus.read16(acp.arg0) ;
                        break;
                }
                this->sendResponce(API_COMMAND_SET_DR, acp.arg0, data) ;
            }
            break ;

        case API_COMMAND_DEPOSIT:
            switch (acp.arg0) {
                    case 017777707:
                        cpu.RR[7] = acp.arg1 ;
                        cpu.wtstate = false ;
                        break ;
                    case 017777706:
                        cpu.RR[6] = acp.arg1 ;
                        break ;
                    case 017777705:
                        cpu.RR[5] = acp.arg1 ;
                        break ;
                    case 017777704:
                        cpu.RR[4] = acp.arg1 ;
                        break ;
                    case 017777703:
                        cpu.RR[3] = acp.arg1 ;
                        break ;
                    case 017777702:
                        cpu.RR[2] = acp.arg1 ;
                        break ;
                    case 017777701:
                        cpu.RR[1] = acp.arg1 ;
                        break ;
                    case 017777700:
                        cpu.RR[0] = acp.arg1 ;
                        break ;

                    case 017777715:
                        cpu.RR[13] = acp.arg1;
                        break ;
                    case 017777714:
                        cpu.RR[12] = acp.arg1;
                        break ;
                    case 017777713:
                        cpu.RR[11] = acp.arg1;
                        break ;
                    case 017777712:
                        cpu.RR[10] = acp.arg1;
                        break ;
                    case 017777711:
                        cpu.RR[9] = acp.arg1;
                        break ;
                    case 017777710:
                        cpu.RR[8] = acp.arg1;
                        break ;

                    case 017777716:
                        cpu.stackpointer[1] = acp.arg1 ;
                        break ;
                    case 017777717:
                        cpu.stackpointer[3] = acp.arg1 ;
                        break ;
                case 017777570:
                    cpu.switchregister = acp.arg1 ;
                    break ;
                default:
                    cpu.unibus.write16(acp.arg0, acp.arg1) ;
                    break ;
            }
            this->sendResponce(API_COMMAND_SET_DR, acp.arg0, acp.arg1) ;
            break ;

        case API_COMMAND_SET_DR:
        case API_COMMAND_SET_AR:
            break ;
        
        case API_COMMAND_REBOOT:
            mode = ShutdownReboot ;
            break ;
        case API_COMMAND_SHUTDOWN:
            mode = ShutdownHalt ;
            break ;

        case API_COMMAND_THROTTLE:
            kb11hrottle = acp.arg0 ;
            break;
        
        default:
            gprintf("API: unknown command 0x%02X", acp.command) ;
            break;
    }
}

void API::sendResponce(ApiCommand command, u32 arg0, u16 arg1) {
    api_responce_packet arp ;
    arp.preamble = API_PACKET_PREAMBLE ;
    arp.command = command ;
    arp.displayRegister = cpu.displayregister ;
    arp.datapath = cpu.datapath ;
    arp.r7 = cpu.RR[7] ; // cpu.lda ;
    arp.arg0 = arg0 ;
    arp.arg1 = arg1 ;

    arp.CSW = cpu.cpuStatus |
                ((cpu.PSW >> 14) << 2) |
                (kb11hrottle << 4) |
                (cpu.mmu.lastWasData << 5) |
                ((cpu.mmu.SR[0] & 1) << 6)
    ;
    queue_try_add(&api_queue, &arp) ;
}

TShutdownMode API::loop() {
    while (!queue_is_empty(&api_queue)) {
        if (!outSocket) {
            return ShutdownReboot ;
        }

        api_responce_packet_t arp ;
        if (!queue_try_remove(&api_queue, &arp)) {
            return ShutdownNone ;
        }

        const u8 ipaddress[] = {172, 16, 103, 102} ;
        CIPAddress panelIP(ipaddress) ;

        if (outSocket->Connect(panelIP, API_PORT) < 0) {
            gprintf("API: step connect error") ;
            return ShutdownNone ;
        }

        if (outSocket->Send(&arp, sizeof arp, 0) < 0) {
            gprintf("API: step send error") ;
        }
    }
    
    return mode ;
}
