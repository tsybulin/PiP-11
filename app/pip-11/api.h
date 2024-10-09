#pragma once

#include <circle/types.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <cons/cons.h>
#include <util/queue.h>

enum ApiCommand : u8 {
    API_COMMAND_UNKNOWN,
    API_COMMAND_SET_DR,
    API_COMMAND_SET_AR,
    API_COMMAND_EXAMINE,
    API_COMMAND_CPU_STATUS
} ;


#define API_PACKET_PREAMBLE 0x5AA5

typedef struct api_command_packet {
    u16 preamble ;
    ApiCommand command ;
    u32 arg0 ;
    u16 arg1 ;
} PACKED api_command_packet_t ;

class API {
    public:
        API(CNetSubSystem *pNet) ;
        ~API(void) ;
        bool init() ;
        TShutdownMode loop(void) ;
        void step() ;

    private:
        void processResponse(ApiCommand command, u32 arg0, u16 arg1) ;
        void sendCommand(ApiCommand command, u32 arg0, u16 arg1) ;

        CNetSubSystem *pnet ;
    	CSocket *inpSocket ;
    	CSocket *outSocket ;
        queue_t api_queue ;
} ;
