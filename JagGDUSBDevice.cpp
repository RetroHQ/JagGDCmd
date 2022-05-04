#include "JagGDUSBDevice.h"

////////////////////////////////////////////////////////////////////////////////
//
// Structs
//
////////////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
enum EJagGDCommand
{
	ECmd_Reset = 0,
	ECmd_ResetDebug,
	ECmd_Server
};

enum EServerCommand
{
	ECmd_Upload = 4,
	ECmd_Execute,
	ECmd_EnableEEPROM
};

struct SJagGDCommand
{
	BYTE					nCmdSize;
	BYTE					nCommand;	// EJagGDCommand
};

struct SServerCommand
{
	BYTE					nCmdSize;
	BYTE					nCommand; // EServerCommand
};

struct SServerCommandUpload : SServerCommand
{
	DWORD					nAddr;
	DWORD					nSize;
	DWORD					nExecute;
};

struct SServerCommanExecute : SServerCommand
{
	DWORD					nAddr;
};

struct SJagGDCommandEnableEEPROM : SServerCommand
{
	unsigned char			nEEPROMSize; // 0-2
	char					szFilename[15]; // filename on memory card
};

union SCommandUnion
{
	SServerCommand					sCmd;
	SServerCommandUpload			sCmdUpload;
	SServerCommanExecute			sExecute;
	SJagGDCommandEnableEEPROM		sCmdEEPROM;
};


struct SJagGDCommandServer : SJagGDCommand
{
	DWORD					nStreamSize;
	SCommandUnion			uCmd;
};

#pragma pack(pop)


DEFINE_GUID(GUID_JagGD, 0xb8559b55, 0x9744, 0x447d, 0xa5, 0xf2, 0x6d, 0xa5, 0x11, 0x68, 0x18, 0xa6);
#define PIPE_IN 0x81
#define PIPE_OUT 0x02

////////////////////////////////////////////////////////////////////////////////
//
// Construct / destruct
//
////////////////////////////////////////////////////////////////////////////////

CJagGDCmd::CJagGDCmd() : CWinUsbDevice(&GUID_JagGD)
{
	m_request.Bits.Recipient = 1;  // Interface
	m_request.Bits.Type = 2; // vendor
	m_request.Bits.DataDirection = 0; // host to device

	m_packet.RequestType = m_request.bmRequestType;
	m_packet.Request = 1; // command
	m_packet.Index = 0;
	m_packet.Value = 0;
}

CJagGDCmd::~CJagGDCmd()
{
}

////////////////////////////////////////////////////////////////////////////////
//
// Connect to programmer device
//
////////////////////////////////////////////////////////////////////////////////

DWORD CJagGDCmd::Connect()
{
	return OpenUsbDevice();
}

////////////////////////////////////////////////////////////////////////////////
//
// Reset the jaguar, optionally entering debug mode
//
////////////////////////////////////////////////////////////////////////////////

inline DWORD BYTESWAP(DWORD d)
{
	return	((d & 0x000000ff) << 24) | 
			((d & 0x0000ff00) << 8) |
			((d & 0x00ff0000) >> 8) |
			((d & 0xff000000) >> 24);
}

DWORD CJagGDCmd::Reset(bool bDebugMode)
{

//-- Reset Jaguar command

	SJagGDCommand cmd;
	cmd.nCmdSize = sizeof(SJagGDCommand);
	cmd.nCommand = bDebugMode ? ECmd_ResetDebug : ECmd_Reset;
	m_packet.Length = cmd.nCmdSize;

	ULONG bytesReturned;
	DWORD r = DoControlTransfer(m_packet, (PUCHAR) &cmd, cmd.nCmdSize, bytesReturned, NULL);

	return r;
}

////////////////////////////////////////////////////////////////////////////////
//
// Send command through to the program running on the Jaguar, either menu or
// stub.
//
////////////////////////////////////////////////////////////////////////////////

DWORD CJagGDCmd::SendServerCommand(SCommandUnion *svr, FILE *f, DWORD size)
{

//-- Setup command packet

	SJagGDCommandServer cmd;
	cmd.nCmdSize = sizeof(cmd);
	cmd.nCommand = ECmd_Server;
	cmd.nStreamSize = size;
	cmd.uCmd = *svr;
	m_packet.Length = cmd.nCmdSize;

	ULONG bytesReturned;
	DWORD status = DoControlTransfer(m_packet, (PUCHAR) &cmd, cmd.nCmdSize, bytesReturned, NULL);

//-- Stream data over, if any

	BOOL bFirstTime = TRUE;
	if (f)
	{
		if (status == ERROR_SUCCESS)
		{
			const DWORD TEMPSIZE = 64;
			char temp[TEMPSIZE];
		
			OVERLAPPED asyncWrite;
			ZeroMemory(&asyncWrite, sizeof(asyncWrite));
			asyncWrite.hEvent = ::CreateEvent(NULL, TRUE, FALSE, "WriteUSBPipe");
			asyncWrite.Internal = STATUS_PENDING;

			while (size)
			{
				DWORD out = size < TEMPSIZE ? size : TEMPSIZE;
				ULONG written = 0;
				fread(temp, 1, out, f);

			retry:
				ResetEvent(asyncWrite.hEvent);
				status = WriteToDevice(PIPE_OUT, (PUCHAR) temp, out, written, &asyncWrite);

				::WaitForSingleObject(asyncWrite.hEvent, INFINITE);

				status = GetOverlappedResult(PIPE_OUT,written,&asyncWrite, FALSE);
				if (status != ERROR_SUCCESS)
				{
					FlushUsbPipe(PIPE_OUT);
					ResetUsbPipe(PIPE_OUT);
					if (bFirstTime)
					{
						bFirstTime = FALSE;
						goto retry;
					}
					break;
				}

				size -= out;
				bFirstTime = FALSE;
			}

			::CloseHandle(asyncWrite.hEvent);
		}

		fclose(f);
	}

	return status;
}

////////////////////////////////////////////////////////////////////////////////
//
// Upload a file to the Jaguar, must be running the stub
//
////////////////////////////////////////////////////////////////////////////////

DWORD CJagGDCmd::UploadFile(const char *pFilename, DWORD offset, DWORD addr, DWORD size, DWORD entry, BOOL execute)
{
	FILE *f = fopen(pFilename, "rb");
	if (!f)
	{
		return ERROR_FILE_NOT_FOUND;
	}

	fseek(f, 0, SEEK_END);
	DWORD fsize = ftell(f) - offset;
	fseek(f, offset, SEEK_SET);

	if (fsize <= 0)
	{
		fclose(f);
		return ERROR_SUCCESS;
	}

	if (!size || size > fsize)
	{
		size = fsize;
	}

	SCommandUnion uCmd;
	uCmd.sCmdUpload.nCmdSize = sizeof(uCmd.sCmdUpload);
	uCmd.sCmdUpload.nCommand = ECmd_Upload;
	uCmd.sCmdUpload.nSize = BYTESWAP(size);
	uCmd.sCmdUpload.nAddr = BYTESWAP(addr);
	uCmd.sCmdUpload.nExecute = execute ? BYTESWAP(entry) : 0;

	return SendServerCommand(&uCmd, f, size);
}

////////////////////////////////////////////////////////////////////////////////
//
// Execute from the given address
//
////////////////////////////////////////////////////////////////////////////////

DWORD CJagGDCmd::Execute(DWORD addr)
{
	SCommandUnion uCmd;
	uCmd.sExecute.nCmdSize = sizeof(uCmd.sExecute);
	uCmd.sExecute.nCommand = ECmd_Execute;
	uCmd.sExecute.nAddr = BYTESWAP(addr);

	return SendServerCommand(&uCmd, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Enable EEPROM with given filename (on cart) and size
//
////////////////////////////////////////////////////////////////////////////////

DWORD CJagGDCmd::EnableEEPROM(const char* pFilename, WORD size)
{
	SCommandUnion uCmd;
	memset(&uCmd, 0, sizeof(uCmd));
	uCmd.sCmdEEPROM.nCmdSize = sizeof(uCmd.sCmdEEPROM);
	uCmd.sCmdEEPROM.nCommand = ECmd_EnableEEPROM;
	uCmd.sCmdEEPROM.nEEPROMSize = (unsigned char) size;
	strncpy(uCmd.sCmdEEPROM.szFilename, pFilename, sizeof(uCmd.sCmdEEPROM.szFilename)-1);

	return SendServerCommand(&uCmd, 0, 0);
}
