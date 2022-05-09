#pragma once
#include "WinUsbDevice.h"

union SCommandUnion;
struct SJagGDCommand;

enum EResetType
{
	EResetType_Menu = 0,
	EResetType_Debug,
	EResetType_ROM
};

#define EXEC_REBOOT 0xffffffff

class CJagGDCmd : private CWinUsbDevice
{
public:
	CJagGDCmd();
	virtual ~CJagGDCmd();

	DWORD	Connect();
	DWORD	Reset(EResetType eType);
	DWORD	UploadFile(const char *pFilename, DWORD offset, DWORD addr, DWORD size, DWORD entry, BOOL execute);
	DWORD	EnableEEPROM(const char* pFilename, WORD size);
	DWORD	Execute(DWORD addr);
	DWORD	BootWriteEnable(bool bEnable);
	DWORD	WriteFile(const char* pFilename);

private:
	DWORD	SendCommandStream(SJagGDCommand *cmd, FILE *f, DWORD size);
	DWORD	SendServerCommand(SCommandUnion *svr, FILE *f, DWORD size);

	USB_DEVICE_REQUEST		m_request;
	WINUSB_SETUP_PACKET		m_packet;
};
