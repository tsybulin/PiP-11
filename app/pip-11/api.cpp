#include "api.h"

#include <circle/net/in.h>
#include <circle/util.h>
#include <arm11/kb11.h>

extern KB11 cpu ;
static const u16 API_PORT = 5366 ;

API::API(CNetSubSystem *pNet)
:   pnet(pNet),
    inpSocket(0),
    outSocket(0)
{
    api_command_packet_t acp ;
    queue_init(&api_queue, sizeof acp, 64) ;
}

API::~API(void) {
    delete inpSocket ;
    inpSocket = 0 ;

    delete outSocket ;
    outSocket = 0 ;

    pnet = 0 ;
}

bool API::init() {
    inpSocket = new CSocket(pnet, IPPROTO_UDP) ;
    if (inpSocket->Bind(API_PORT) < 0) {
        gprintf("API: Cannot bind to port") ;
        return false ;
    }

    outSocket = new CSocket(pnet, IPPROTO_UDP) ;

    return true ;
}

void API::processResponse(ApiCommand command, u32 arg0, u16 arg1) {
    switch (command) {
        case API_COMMAND_CPU_STATUS:
            switch (arg0) {
                case CPU_STATUS_UNKNOWN: // read-only
                    break ;
                case CPU_STATUS_ENABLE:
                    cpu.cpuStatus = CPU_STATUS_ENABLE ;
                    break ;
                case CPU_STATUS_HALT:
                    cpu.cpuStatus = CPU_STATUS_HALT ;
                    break ;
                case CPU_STATUS_STEP:
                    cpu.cpuStatus = CPU_STATUS_STEP ;
                    break ;
                default:
                    gprintf("API: unknown cpu state 0%06o", arg0) ;
                    break ;
            }
            this->sendCommand(API_COMMAND_CPU_STATUS, cpu.cpuStatus, cpu.RR[7]) ;
            break;

        case API_COMMAND_EXAMINE: {
                u16 data = 0 ;
                switch (arg0) {
                    case 0177707:
                        data = cpu.RR[7] ;
                        break ;
                    case 0177706:
                        data = cpu.RR[6] ;
                        break ;
                    case 0177705:
                        data = cpu.RR[5] ;
                        break ;
                    case 0177704:
                        data = cpu.RR[4] ;
                        break ;
                    case 0177703:
                        data = cpu.RR[3] ;
                        break ;
                    case 0177702:
                        data = cpu.RR[2] ;
                        break ;
                    case 0177701:
                        data = cpu.RR[1] ;
                        break ;
                    case 0177700:
                        data = cpu.RR[0] ;
                        break ;
                    case 0177570:
                        data = cpu.displayregister ;
                        break ;

                    default:
                        data = cpu.read16(arg0) ;
                        break;
                }
                this->sendCommand(API_COMMAND_SET_DR, arg0, data) ;
            }
            break ;

        case API_COMMAND_DEPOSIT:
            switch (arg0) {
                    case 0177707:
                        cpu.RR[7] = arg1 ;
                        cpu.wtstate = false ;
                        break ;
                    case 0177706:
                        cpu.RR[6] = arg1 ;
                        break ;
                    case 0177705:
                        cpu.RR[5] = arg1 ;
                        break ;
                    case 0177704:
                        cpu.RR[4] = arg1 ;
                        break ;
                    case 0177703:
                        cpu.RR[3] = arg1 ;
                        break ;
                    case 0177702:
                        cpu.RR[2] = arg1 ;
                        break ;
                    case 0177701:
                        cpu.RR[1] = arg1 ;
                        break ;
                    case 0177700:
                        cpu.RR[0] = arg1 ;
                        break ;
                case 0177570:
                    cpu.switchregister = arg1 ;
                    break ;
                default:
                    cpu.write16(arg0, arg1) ;
                    break ;
            }
            this->sendCommand(API_COMMAND_SET_DR, arg0, arg1) ;
            break ;

        case API_COMMAND_SET_DR:
        case API_COMMAND_SET_AR:
            break ;
        
        default:
            gprintf("API: unknown command 0x%02X", command) ;
            break;
    }
}

void API::sendCommand(ApiCommand command, u32 arg0, u16 arg1) {
    api_command_packet_t acp ;
    acp.preamble = API_PACKET_PREAMBLE ;
    acp.command = command ;
    acp.arg0 = arg0 ;
    acp.arg1 = arg1 ;
    queue_try_add(&api_queue, &acp) ;
}

TShutdownMode API::loop() {
    api_command_packet_t acp ;
    u16 clientPort ;
    CIPAddress panelIP ;

    memset(&acp, 0, sizeof acp) ;

    int res = inpSocket->ReceiveFrom(&acp, sizeof acp, 0, &panelIP, &clientPort) ;
    if (res < 0) {
        gprintf("API: Cannot receive request") ;
        return ShutdownNone ;
    }

    if (res == 0) {
        return ShutdownNone ;
    }

    if (acp.preamble != API_PACKET_PREAMBLE) {
        gprintf("API: Incorrect preamble 0x%04X", acp.preamble) ;
        return ShutdownNone ;
    }

    this->processResponse(acp.command, acp.arg0, acp.arg1) ;

    while (!queue_is_empty(&api_queue)) {
        this->step() ;
    }
    
    return ShutdownNone ;
}

void API::step() {
    if (!outSocket) {
        return ;
    }

    api_command_packet_t acp ;
    if (!queue_try_remove(&api_queue, &acp)) {
        return ;
    }

    const u8 ipaddress[] = {172, 16, 103, 102} ;
    CIPAddress panelIP(ipaddress) ;

    if (outSocket->Connect(panelIP, API_PORT) < 0) {
        gprintf("API: step connect error") ;
        return ;
    }

    if (outSocket->Send(&acp, sizeof acp, 0) < 0) {
        gprintf("API: step send error") ;
    }
}
