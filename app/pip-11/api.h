#pragma once

#include <circle/types.h>
#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <cons/cons.h>
#include <util/queue.h>
#include <arm11/kb11.h>

enum ApiCommand : u8 {
    API_COMMAND_UNKNOWN,
    API_COMMAND_SET_DR,
    API_COMMAND_SET_AR,
    API_COMMAND_EXAMINE,
    API_COMMAND_CPU_STATUS,
    API_COMMAND_DEPOSIT,
    API_COMMAND_REBOOT,
    API_COMMAND_SHUTDOWN,
    API_COMMAND_THROTTLE
} ;


#define API_PACKET_PREAMBLE 0x5AA5

typedef struct api_command_packet {
    u16 preamble ;
    ApiCommand command ;
    u32 switchRegister ;
    u32 arg0 ;
    u16 arg1 ;
} PACKED api_command_packet_t ;

typedef struct api_responce_packet {
    u16 preamble ;
    ApiCommand command ;
    u16 CSW ; // Console Status Word
    u16 r7 ;
    u16 displayRegister ;
    u16 datapath ;
    u32 arg0 ;
    u16 arg1 ;
} PACKED api_responce_packet_t ;

class API : public CTask {
    public:
        API(CNetSubSystem *pNet) ;
        ~API(void) ;
        void Run(void) ;
        TShutdownMode loop(void) ;

    private:
        void processCommand(api_command_packet_t acp) ;
        void sendResponce(ApiCommand command, u32 arg0, u16 arg1) ;

        CNetSubSystem *pnet ;
    	CSocket *inpSocket ;
    	CSocket *outSocket ;
        queue_t api_queue ;
        TShutdownMode mode ;
} ;
