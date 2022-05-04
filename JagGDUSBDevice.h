#pragma once
#include "WinUsbDevice.h"

union SCommandUnion;

class CJagGDCmd : private CWinUsbDevice
{
public:
	CJagGDCmd();
	virtual ~CJagGDCmd();

	DWORD	Connect();
	DWORD	Reset(bool bDebugMode = false);
	DWORD	UploadFile(const char *pFilename, DWORD offset, DWORD addr, DWORD size, DWORD entry, BOOL execute);
	DWORD	EnableEEPROM(const char* pFilename, WORD size);
	DWORD	Execute(DWORD addr);
	DWORD	BootWriteEnable(bool bEnable);

private:
	DWORD	SendServerCommand(SCommandUnion *svr, FILE *f, DWORD size);

	USB_DEVICE_REQUEST		m_request;
	WINUSB_SETUP_PACKET		m_packet;
};
