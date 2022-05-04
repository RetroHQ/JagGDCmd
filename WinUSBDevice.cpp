// WinUsbDevice.cpp : implementation file
//

#include "WinUsbDevice.h"
#include <setupapi.h>

#define ENABLE_WAKE		0x82



// CWinUsbDevice

CWinUsbDevice::CWinUsbDevice(LPCGUID Guid) 
	: m_DeviceGuid(Guid)
	, m_bInitialized(FALSE)
	, m_DeviceHandle(INVALID_HANDLE_VALUE)
	, m_WinUsbHandle(INVALID_HANDLE_VALUE)
	, m_bDeviceFound(FALSE)
{
}

CWinUsbDevice::~CWinUsbDevice()
{
	if(m_bInitialized) 
	{
		WinUsb_Free(m_WinUsbHandle);
		CloseHandle(m_DeviceHandle);
	}
}


// CWinUsbDevice member functions

DWORD CWinUsbDevice::GetDevicePath()
{
	BOOL bResult = FALSE;
	HDEVINFO deviceInfo;
	SP_DEVICE_INTERFACE_DATA interfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
	ULONG length;
	ULONG requiredLength=0;

	//
	// Find device information about this GUID.
	//

	deviceInfo = SetupDiGetClassDevs(m_DeviceGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if(deviceInfo == INVALID_HANDLE_VALUE) 
	{
		return GetLastError();
	}

	//
	// Enumerate the devices that support this guid.
	//
	memset(&interfaceData,0,sizeof(interfaceData));
	interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
								NULL,
								m_DeviceGuid,
								0,
								&interfaceData);

	if(!bResult) 
	{
		DWORD error = GetLastError();
		SetupDiDestroyDeviceInfoList(deviceInfo);
		if(error == ERROR_NO_MORE_ITEMS) 
		{
			return ERROR_SUCCESS;
		}
		return error;
	}


	//
	// Get information about the found device interface.  We don't
	// know how much memory to allocate to get this information, so
	// we will ask by passing in a null buffer and location to
	// receive the size of the buffer needed.
	//
	bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
							&interfaceData,
							NULL, 0,
							&requiredLength,
							NULL);

	if(!requiredLength) 
	{
		DWORD error = GetLastError();
		SetupDiDestroyDeviceInfoList(deviceInfo);
		return error;
	}

	//
	// Okay, we got a size back, so let's allocate memory 
	// for the interface detail information we want.
	//
	detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LMEM_FIXED,
							requiredLength);

	if(NULL == detailData) {
		SetupDiDestroyDeviceInfoList(deviceInfo);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
	length = requiredLength;

	bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
									&interfaceData,
									detailData,
									length,
									&requiredLength,
									NULL);

	if(!bResult) {
		DWORD error = GetLastError();
		SetupDiDestroyDeviceInfoList(deviceInfo);
		LocalFree(detailData);
		return error;
	}

	//
	// Okay, we have information on the device.   Let's
	// save the name of this device
	//
	strcpy(m_DevicePath, detailData->DevicePath);
	m_bDeviceFound = TRUE;

	LocalFree(detailData);

	//
	// Release all the information we don't need anymore.
	//
	SetupDiDestroyDeviceInfoList(deviceInfo);

	//
	// Tell the caller we are happy....
	//
	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::OpenUsbDevice()
{
	if(m_bInitialized)
	{
		return ERROR_SUCCESS;
	}

	if(!m_DeviceGuid) 
	{
		return ERROR_INVALID_PARAMETER;
	}

	DWORD result = GetDevicePath();
	if (result != ERROR_SUCCESS)
	{
		return result;
	}

	if (!m_bDeviceFound)
	{
		return ERROR_PATH_NOT_FOUND;
	}

	//
	// Open the device contained within the device path list.
	//
	m_DeviceHandle = CreateFile(m_DevicePath, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

	//
	// Make sure that we successfully opened the device.
	//
	if(m_DeviceHandle != INVALID_HANDLE_VALUE) 
	{
		//
		// The device was opened, so now we need to initialize
		// our USB device via WinUsb_Initialize.
		//
		BOOL bResult = WinUsb_Initialize(m_DeviceHandle, &m_WinUsbHandle);
		if(!bResult) 
		{
			CloseHandle(m_DeviceHandle);
			return GetLastError();
		}
		m_bInitialized = TRUE;
		return ERROR_SUCCESS;
	}

	return GetLastError();
}

DWORD CWinUsbDevice::QueryUsbDeviceInformation(ULONG InformationType, PVOID Buffer,PULONG BufferLength)
{
	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	//
	// Query our USB device for specific information specified
	// by the caller.
	//
	if(!WinUsb_QueryDeviceInformation(m_WinUsbHandle,InformationType,
		BufferLength,Buffer)) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::QueryUsbInterfaceSettings(DWORD Limit,DWORD& Count)
{
	USB_INTERFACE_DESCRIPTOR usbInterfaceDescriptor;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	Count = 0;

	m_UsbInterfaceDescriptors.clear();

	//
	// Query the USB device for all its supported interfaces.
	//
	for(UCHAR interfaceNumber = 0; interfaceNumber < Limit; interfaceNumber++) {
		if(!WinUsb_QueryInterfaceSettings(m_WinUsbHandle,interfaceNumber,&usbInterfaceDescriptor)) {
			DWORD error = GetLastError();
			if(error == ERROR_NO_MORE_ITEMS) {
				error =  ERROR_SUCCESS;
				break;
			}
			return error;
		}

		//
		// Add the found interface to the list.
		//
		m_UsbInterfaceDescriptors.push_back(usbInterfaceDescriptor);
	}

	//
	// Return how many interfaces were found.
	//
	Count = m_UsbInterfaceDescriptors.size();

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::GetUsbDeviceInterfaceCount()
{
	return (DWORD) m_UsbInterfaceDescriptors.size();
}

const USB_INTERFACE_DESCRIPTOR* CWinUsbDevice::GetUsbDeviceInterface(DWORD n)
{
	if (n >= m_UsbInterfaceDescriptors.size())
	{
		return 0;
	}
	std::list<USB_INTERFACE_DESCRIPTOR>::iterator iter;
	iter = m_UsbInterfaceDescriptors.begin();
	while (n--)
	{
		iter++;
	}
	return &iter._Ptr->_Myval;
}

DWORD CWinUsbDevice::QueryUsbPipeInformation(DWORD Interface,DWORD& Count) 
{
	BOOL					bResult;
	WINUSB_PIPE_INFORMATION	pipeInfo;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	if(Interface >= m_UsbInterfaceDescriptors.size()) {
		return ERROR_INVALID_PARAMETER;
	}

	Count = 0;

	m_UsbPipeInformation.clear();

	const USB_INTERFACE_DESCRIPTOR *pInterface = GetUsbDeviceInterface(Interface);
	for(UCHAR index = 0; index < pInterface->bNumEndpoints;index++) {
		bResult = WinUsb_QueryPipe(m_WinUsbHandle,(UCHAR) Interface,index,&pipeInfo);

		if(!bResult) {
			DWORD error = GetLastError();
			if(error == ERROR_NO_MORE_ITEMS) {
				error = ERROR_SUCCESS;
				break;
			}
			return error;
		}

		m_UsbPipeInformation.push_back(pipeInfo);
	}

	Count = m_UsbPipeInformation.size();

	return ERROR_SUCCESS;
}

const WINUSB_PIPE_INFORMATION *CWinUsbDevice::GetUsbDevicePipeInformation(DWORD n)
{
	if (n >= m_UsbPipeInformation.size())
	{
		return 0;
	}
	std::list<WINUSB_PIPE_INFORMATION>::iterator iter;
	iter = m_UsbPipeInformation.begin();
	while (n--)
	{
		iter++;
	}

	return &iter._Ptr->_Myval;
}

DWORD CWinUsbDevice::GetUsbDevicePipeInformationCount()
{
	return m_UsbPipeInformation.size();
}

DWORD CWinUsbDevice::GetOverlappedResult(UCHAR PipeId,ULONG& BytesWritten,LPOVERLAPPED POverlapped, BOOL bWait)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_GetOverlappedResult(m_WinUsbHandle,
							 POverlapped,
							 &BytesWritten,
							 bWait);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}


DWORD CWinUsbDevice::WriteToDevice(UCHAR PipeId,PUCHAR Buffer,ULONG BytesToWrite,
								  ULONG& BytesWritten,LPOVERLAPPED POverlapped)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_WritePipe(m_WinUsbHandle,
							 PipeId,
							 Buffer,
							 BytesToWrite,
							 &BytesWritten,
							 POverlapped);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::ReadFromDevice(UCHAR PipeId,PUCHAR Buffer,ULONG BytesToRead,
								   ULONG& BytesRead,LPOVERLAPPED POverlapped)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_ReadPipe(m_WinUsbHandle,
							 PipeId,
							 Buffer,
							 BytesToRead,
							 &BytesRead,
							 POverlapped);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::DoControlTransfer(WINUSB_SETUP_PACKET& Packet,PUCHAR Buffer,ULONG BufferSize,
										 ULONG& BytesTransfered,LPOVERLAPPED POverlapped)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_ControlTransfer(m_WinUsbHandle,
									Packet,
									Buffer,
									BufferSize,
									&BytesTransfered,
									POverlapped);
	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::FlushUsbPipe(UCHAR PipeId)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_FlushPipe(m_WinUsbHandle,PipeId);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::AbortUsbPipe(UCHAR PipeId)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_AbortPipe(m_WinUsbHandle,PipeId);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::ResetUsbPipe(UCHAR PipeId)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_ResetPipe(m_WinUsbHandle,PipeId);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

DWORD CWinUsbDevice::EnableWaitWake(BOOL Enable)
{
	BOOL bResult;

	if(!m_bInitialized) {
		return ERROR_SUCCESS;
	}

	bResult = WinUsb_SetPowerPolicy(m_WinUsbHandle,0x02,sizeof(Enable),&Enable);

	if(!bResult) {
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

