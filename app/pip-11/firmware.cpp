#include "firmware.h"

Firmware::Firmware(CNetSubSystem *net, FATFS *pFileSystem, const char *pPath)
:   CTFTPDaemon (net),
    fileSystem(pFileSystem),
    path(pPath),
    fileOpen(false)
{

}

Firmware::~Firmware(void) {
    if (fileOpen) {
        FileClose() ;
    }

    fileSystem = 0 ;
}

boolean Firmware::FileOpen(const char *pFileName) {
    filename = path ;
    filename.Append(pFileName) ;
    if (f_open (&file, filename, FA_READ | FA_OPEN_EXISTING) == FR_OK) {
        fileOpen = true ;
        return true ;
    }

    fileOpen = false ;
    return false ;
}

boolean Firmware::FileCreate(const char *pFileName) {
    if (fileOpen) {
        return false ;
    }

    filename = path ;
    filename.Append(pFileName) ;
    if (f_open (&file, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        fileOpen = true ;
        return true ;
    }

    fileOpen = false ;
    return false ;
}

boolean Firmware::FileClose(void) {
    if (!fileOpen) {
        return true ;
    }

    fileOpen = false ;
    return f_close(&this->file) == FR_OK ;
}

int Firmware::FileRead(void *pBuffer, unsigned nCount) {
    if (!fileOpen) {
        return -1 ;
    }

	unsigned nbytes ;
    FRESULT result = f_read (&file, pBuffer, nCount, &nbytes) ;
    if (result != FR_OK) {
        return -1 ;
    }
    return nbytes ;
}

int Firmware::FileWrite(const void *pBuffer, unsigned nCount) {
    if (!fileOpen) {
        return -1 ;
    }

	unsigned nbytes;
	FRESULT Result = f_write (&file, pBuffer, nCount, &nbytes);
	if (Result != FR_OK)
	{
		return -1;
	}

	return nbytes;
}
