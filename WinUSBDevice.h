#pragma once
#include <atlbase.h>
#include <winusb.h>
#include <usb100.h>
#include <list>

typedef struct _USB_DEVICE_REQUEST {
	union {
		BYTE	bmRequestType;
		struct  {
			// D7: Data transfer direction
			//	0 = Host-to-device
			//	1 = Device-to-host
			// D6...5: Type
			//	0 = Standard
			//	1 = Class
			//	2 = Vendor
			//	3 = Reserved
			// D4...0: Recipient
			//	0 = Device
			//	1 = Interface
			//	2 = Endpoint
			//	3 = Other
			BYTE  Recipient		: 5;
			BYTE  Type			: 2;
			BYTE  DataDirection	: 1;
		} Bits;
	};
} USB_DEVICE_REQUEST, *PUSB_DEVICE_REQUEST;

// CWinUsbDevice command target

class CWinUsbDevice
{
public:
	CWinUsbDevice(LPCGUID Guid);
	virtual ~CWinUsbDevice();

	DWORD OpenUsbDevice();
	DWORD QueryUsbDeviceInformation(ULONG InformationType, PVOID Buffer,PULONG BufferLength);

	DWORD QueryUsbInterfaceSettings(DWORD Limit,DWORD& Count);
	DWORD GetUsbDeviceInterfaceCount();
	const USB_INTERFACE_DESCRIPTOR* GetUsbDeviceInterface(DWORD n);

	DWORD QueryUsbPipeInformation(DWORD Interface,DWORD& Count);
	const WINUSB_PIPE_INFORMATION * GetUsbDevicePipeInformation(DWORD n);
	DWORD GetUsbDevicePipeInformationCount();

	DWORD WriteToDevice(UCHAR PipeId,PUCHAR Buffer,ULONG BytesToWrite, ULONG& BytesWritten,LPOVERLAPPED POverlapped);
	DWORD GetOverlappedResult(UCHAR PipeId, ULONG& BytesWritten, LPOVERLAPPED POverlapped, BOOL bWait);
	DWORD ReadFromDevice(UCHAR PipeId,PUCHAR Buffer,ULONG BytesToRead, ULONG& BytesRead,LPOVERLAPPED POverlapped);
	DWORD DoControlTransfer(WINUSB_SETUP_PACKET& Packet,PUCHAR Buffer,ULONG BufferSize, ULONG& BytesTransfered,LPOVERLAPPED POverlapped);

	DWORD FlushUsbPipe(UCHAR PipeId);
	DWORD AbortUsbPipe(UCHAR PipeId);
	DWORD ResetUsbPipe(UCHAR PipeId);

	DWORD EnableWaitWake(BOOL Enable=TRUE);

private:
	LPCGUID			m_DeviceGuid;
	HANDLE			m_DeviceHandle;
	CHAR			m_DevicePath[256];
	HANDLE			m_WinUsbHandle;
	BOOL			m_bInitialized;
	BOOL			m_bDeviceFound;

	std::list<USB_INTERFACE_DESCRIPTOR> m_UsbInterfaceDescriptors;
	std::list<WINUSB_PIPE_INFORMATION> m_UsbPipeInformation;

	DWORD GetDevicePath();
};
