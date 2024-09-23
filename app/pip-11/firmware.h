#pragma once

#include <circle/net/tftpdaemon.h>
#include <fatfs/ff.h>

class Firmware : public CTFTPDaemon {
    public:
        Firmware(CNetSubSystem *net, FATFS *pFileSystem, const char *pPath = "SD:/") ;
        ~Firmware(void) ;

        boolean FileOpen(const char *pFileName);
        boolean FileCreate(const char *pFileName);
        boolean FileClose(void);
        int FileRead(void *pBuffer, unsigned nCount);
        int FileWrite(const void *pBuffer, unsigned nCount);

    private:
        FATFS *fileSystem ;
        CString path ;

        CString filename ;
        FIL file ;
        boolean fileOpen ;
} ;
