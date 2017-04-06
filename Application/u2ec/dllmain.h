/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   dllmain.h

Abstract:

   Main module. Working with U2EC service.

Environment:

    Kernel mode

--*/

#ifndef U2EC_H_000000001
#define U2EC_H_000000001

#include "stdafx.h"

#ifndef U2EC_EXPORTS_STATIC

#ifdef U2EC_EXPORTS
#define FUNC_API __declspec(dllexport)
#else
#define FUNC_API __declspec(dllimport)
#endif

#else

#define FUNC_API 

#endif

#define U2EC_STATE_DISCONNECT 0
#define U2EC_STATE_WAITING 1
#define U2EC_STATE_CONNECTED 2
#define U2EC_STATE_CONNECTED_FROM_RDP 3

typedef void (__stdcall *FNOnChangeDevList) ();
extern FNOnChangeDevList pOnChangeDevList;

extern "C" {

//** Server
// enum hub

//++
//
// BOOL
// ServerCreateEnumUsbDev(
//		OUT PVOID *ppContext
//		)
//
// Routine Description:
//
//		Creates and returns USB devices enumerator. This enumerator is used in all 
//		subsequent functions starting with "Server".
//
// Arguments:
//
//		ppContext - a pointer to USB devices enumerator.
//
// Return Value:
//
//		True - if USB devices enumerator was created successfully. False – if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerCreateEnumUsbDev (PVOID *ppContext);

//++
//
// BOOL
// ServerRemoveEnumUsbDev(
//		IN PVOID pContext
//		)
//
// Routine Description:
//
//		Removes USB devices enumerator.
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
// Return Value:
//
//		True - if USB devices enumerator was removed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerRemoveEnumUsbDev (PVOID pContext);

//++
//
// BOOL
// ServerGetUsbDevFromHub(
//		IN PVOID pContext,
//		IN PVOID pHubContext,
//		IN int iIndex,
//		OUT PVOID *ppDevContext
//		)
//
// Routine Description:
//
//		Gets USB device descriptor from USB hub.
// 
// Arguments:
//
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pHubContext -	a pointer to USB hub returned by ServerGetUsbDevFromHub function. 
//						If this parameter is NULL, root hubs are enumerated.
//
//		iIndex - USB port number.
//
//		pDevContext - a pointer to USB devices descriptor which is used in subsequent 
//					  operations with devices.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API	BOOL WINAPI ServerGetUsbDevFromHub (PVOID pContext, PVOID pHubContext, int iIndex, PVOID *ppDevContext);

//++
//
// BOOL
// ServerUsbDevIsHub(
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		)
//
// Routine Description:
//
//		Determines, whether the USB device is a USB hub.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
// 
// Return Value:
//
//		True - if the USB device is a USB hub. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerUsbDevIsHub (PVOID pContext, PVOID pDevContext);

//++
//
// BOOL
// ServerUsbDevIsShared(
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		)
//
// Routine Description:
//
//		Determines whether the USB device is shared.
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//	    
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
// Return Value:
//
//		True - if the USB device is shared. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerUsbDevIsShared (PVOID pContext, PVOID pDevContext);

//++
//
// BOOL
// ServerUsbDevIsConnected(
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		)
//
// Routine Description:
//
//		Determines whether the real USB device connected to USB port pointed by pDevContext.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
// Return Value:
//
//		True - if the real USB device connected to USB port. 
//			   False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerUsbDevIsConnected (PVOID pContext, PVOID pDevContext);

//++
//
// BOOL
// ServerGetUsbDevName(
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		OUT VARIANT *strName
//		)
//
// Routine Description:
//
//		Returns USB device name. 
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//	
//		strName - a pointer to USB device name.
//
// Return Value:
//      
//		True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API	BOOL WINAPI ServerGetUsbDevName (PVOID pContext, PVOID pDevContext, VARIANT *strName);

//++
//
// BOOL
// ServerGetUsbDevNameEx(
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      OUT VARIANT *strName,
//      OUT VARIANT *strUserDescription
//      )
//
// Routine Description:
//
//      Returns USB device name and user description (as separate objects). 
// 
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//  
//      strName - a pointer to USB device name.
//
//      strUserDescription - a pointer to USB device user description.
//
// Return Value:
//      
//      True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API    BOOL WINAPI ServerGetUsbDevNameEx (PVOID pContext, PVOID pDevContext, VARIANT *strName, VARIANT *strUserDescription);

//++
//
// BOOL
// ServerGetUsbDevValueByName(
//		IN PVOID pContext, 
//		IN PVOID pDevContext, 
//		IN VARIANT strValueName,
//		OUT VARIANT *pValue
//		)
//
// Routine Description:
//
//		Returns value by name for USB device.
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//		strValueName - name of interested value.
//
//		pValue - pointer to output value.
//
// Notes:
//
//		- Values are cached on the moment of ServerCreateEnumUsbDev call. 
//		- It's possible to change some values with ServerSetUsbDevValueByName function.
//			See notes for ServerSetUsbDevValueByName.
//		- Supported value names are: 
//			NETPARAM		(string)	network settings (e.g., ip:port), 
//			NAME			(string)	name of the USB device, 
//			NICK			(string)	user description of the USB device, 
//			AUTH			(bool)		authorization, 
//			ENCR			(bool)		encryption, 
//			COMPR			(bool)		compression, 
//			COMPR_TYPE		(int)		compression type, 
//			STATUS			(int)		connection status (for shared devices), 
//			SHARED_WITH		(string)	client connected to shared device,
//			ALLOW_RDISCONN	(bool)		whether any remote client is allowed to disconnect the device,
//			USBLOC			(string)	USB port location,
//			USBCLASS		(string)	USB device class triple as single hex-ascii string xxyyzz (where xx=class, yy=subclass, zz=protocol),
//			BCDUSB			(uint)		version of the USB specification,
//			VID				(uint)		vendor identifier for the device,
//			PID				(uint)		product identifier,
//			REV				(uint)		version of the device,
//			SERIAL			(string)	serial number of the device.
//
// Return Value:
//      
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerGetUsbDevValueByName (PVOID pContext, PVOID pDevContext, 
													VARIANT strValueName, VARIANT *pValue);

//++
//
// BOOL
// ServerSetUsbDevValueByName(
//		IN PVOID pContext, 
//		IN PVOID pDevContext, 
//		IN VARIANT strValueName,
//		IN VARIANT varValue
//		)
//
// Routine Description:
//
//		Sets new value by name for USB device.
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//		strValueName - name of the value.
//
//		varValue - the value.
//
// Notes:
//
//		- Values are cached on the moment of ServerCreateEnumUsbDev call. 
//		- With this function it's possible to change some values for unshared device 
//			and use them later on sharing.
//		- Supported value names are: 
//			NETPARAM		(string)	network settings (e.g., ip:port), 
//			NICK			(string)	user description of the USB device, 
//			AUTH			(bool)		authorization, 
//			ENCR			(bool)		encryption, 
//			COMPR			(bool)		compression, 
//			COMPR_TYPE		(int)		compression type, 
//			ALLOW_RDISCONN	(bool)		whether any remote client is allowed to disconnect the device.
//
// Return Value:
//      
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerSetUsbDevValueByName (PVOID pContext, PVOID pDevContext, 
													VARIANT strValueName, VARIANT varValue);

//++
//
// BOOL
// ServerGetUsbDevClassCode (
//		IN PVOID pContext, 
//		IN PVOID pDevContext, 
//		OUT BYTE *bBaseClass,
//		OUT BYTE *bSubClass,
//		OUT BYTE *bProtocol
//		)
//
// Routine Description:
//
//		Returns USB device Class Code triple 
//		(Base Class, SubClass and Protocol as separate objects).
//		http://www.usb.org/developers/defined_class
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//		bBaseClass - a pointer to variable that receives Base Class byte value.
//
//		bSubClass - a pointer to variable that receives SubClass byte value.
//
//		bProtocol - a pointer to variable that receives Protocol byte value.
//
// Return Value:
//      
//      True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API    BOOL WINAPI ServerGetUsbDevClassCode (PVOID pContext, PVOID pDevContext, 
	BYTE *bBaseClass, BYTE *bSubClass, BYTE *bProtocol);

//++
//
// BOOL
// ServerShareUsbDev (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		IN VARIANT varNetSettings,
//		IN VARIANT strDescription,
//		IN BOOL bAuth,
//		IN VARIANT strPassword,
//		IN BOOL bCrypt,
//		IN BOOL bCompress
//		)
//
// Routine Description:
//
// Shares USB device.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//	
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//	
//		varNetSettings - network settings. If not set, default network settings are used. 
//						 TCP port is determined automatically. TCP port can be also set manually, 
//						 for example "5000". To set callback connection for the shared USB 
//						 device, full client address and connection port should be indicated 
//						 this way: "host_name_Or_Ip:TCP Port". For example: "localhost:5000".
//	
//		strDescription - additional USB device description to simplify its identification. 
//
//		bAuth - enables authorization for the USB device if set to TRUE. strPassword parameter 
//				should be set.
//		
//		strPassword - authorization password. Is used if bAuth is set to TRUE.
//		
//		bCrypt - enables encryption for all network traffic, if set to True.
//
//		bCompress - enables compression for all network traffic, if set to True.
//
// Return Value:
//
//		True - if USB device was successfully shared. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerShareUsbDev (PVOID pContext, PVOID pDevContext, 
	VARIANT varNetSettings, VARIANT strDescription, BOOL bAuth, VARIANT strPassword, 
	BOOL bCrypt, BOOL bCompress);

FUNC_API	BOOL WINAPI ServerShareUsbDev2 (PVOID pContext, PVOID pDevContext, 
	VARIANT varNetSettings, VARIANT strDescription, BOOL bAuth, VARIANT strPassword, 
	BOOL bCrypt, BOOL bCompress);

//++
//
// BOOL
// ServerShareUsbWithPredefinedValues (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		)
//
// Routine Description:
//
//		Shares USB device with predefined parameters. 
//		Parameters should be set with ServerSetUsbDevValueByName function.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//	
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
// Notes:
//
//		Desired sharing parameters (net settings, description, authorization, encryption,
//		compression) should be set with ServerSetUsbDevValueByName function before calling
//		ServerShareUsbWithPredefinedValues. 
//		Otherwise sharing with default parameters will be performed.
//
// Return Value:
//
//		True - if USB device was successfully shared. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerShareUsbWithPredefinedValues (PVOID pContext, PVOID pDevContext);


//++
//
// BOOL
// ServerUnshareUsbDev (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		)
//
// Routine Description:
// 
//		Unshares USB device. USB device becomes accessible for usage on the local machine. 
//
// Arguments:
//
//		pContext - a pointer to USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
// Return Value:
//
//      True - if USB device was successfully unshared. False - if an error occurred, 
//			   or the USB device was not unshared.
//
//--
FUNC_API	BOOL WINAPI ServerUnshareUsbDev (PVOID pContext, PVOID pDevContext);

//++
//
// BOOL
// ServerUnshareAllUsbDev ()
//
// Routine Description:
// 
//		Unshares all previously shared USB devices.
//
// Arguments:
//
//		None.
//
// Return Value:
//
//      True - if USB devices were successfully unshared. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ServerUnshareAllUsbDev (void);

//++
//
// BOOL
// ServerDisconnectRemoteDev (
//		IN PVOID pContext,
//		IN PVOID pDevContext
//		)
//
// Routine Description:
//	
//		Disconnect remote device.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
// Return Value:
//
//		True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API	BOOL WINAPI ServerDisconnectRemoteDev (PVOID pContext, PVOID pDevContext);

//++
//
// BOOL
// ServerGetUsbDevStatus (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		OUT LONG *piState,
//		OUT VARIANT *strHostConnected
//		)
//
// Routine Description:
//
//		Gets the current status of the shared USB device.
//
// Arguments:
//
//		pContext - a pointer to USB devices enumerator returned by ServerCreateEnumUsbDev function.
//	
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function. 
//		
//		piState - returns the current status of the shared USB device:
//			1 – the device is waiting for connection;
//			2 – the device is already connected.
//		
//		strHostConnected - hostname of the client to which the server is currently connected. 
//						   Set only when piState is 2.
//
// Return Value:
//
//		True -  if USB device was successfully shared. False - if an error occurred, 
//				or the USB device was not shared.
//
//--
FUNC_API	BOOL WINAPI ServerGetUsbDevStatus (PVOID pContext, PVOID pDevContext, LONG *piState, VARIANT *strHostConnected);

//++
//
// BOOL
// ServerGetSharedUsbDevNetSettings (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		OUT VARIANT *strNetSettings,
//		)
//
// Routine Description:
//
//		Gets current network settings of the shared USB device.
// 
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//		strNetSettings - returns network settings of the shared USB device. If only TCP port 
//						 number is returned, it is standard network connection. If full network 
//						 address is returned ("host_name:tcp_port"), it is a callback connection.
//
// Return Value:
//
//		True - if USB device was successfully shared. False - if an error occurred, 
//			   or the USB device was not shared.
//--
FUNC_API	BOOL WINAPI ServerGetSharedUsbDevNetSettings (PVOID pContext, PVOID pDevContext, VARIANT *strParam);

//++
//
// BOOL
// ServerGetSharedUsbDevIsCrypt (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		OUT BOOL *bCrypt,
//		)
//
// Routine Description:
//
//		Returns whether shared USB device traffic is encrypted.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor, returned by ServerGetUsbDevFromHub function.
//
//		bCrypt – is TRUE if network traffic is encrypted. Otherwise "False" value is returned.
//
// Return Value:
//
//		True - if USB device was successfully shared. False - if an error occurred, 
//			   or the USB device was not shared.
//
//--
FUNC_API	BOOL WINAPI ServerGetSharedUsbDevIsCrypt (PVOID pContext, PVOID pDevContext, BOOL *bCrypt);

//++
//
// BOOL
// ServerGetSharedUsbDevRequiresAuth (
//		IN PVOID pContext,
//		IN PVOID pDevContext,
//		OUT BOOL *bAuth,
//		)
//
// Routine Description:
//	
//		Determines whether the shared USB device requires authorization.
//
// Arguments:
//
//		pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//		bAuth - is True if the shared USB device requires authorization on connection. 
//				Otherwise "False" value is returned.
//
// Return Value:
//
//		True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API	BOOL WINAPI ServerGetSharedUsbDevRequiresAuth (PVOID pContext, PVOID pDevContext, BOOL *bAuth);

//++
//
// BOOL
// ServerGetSharedUsbDevIsCompressed (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      OUT BOOL *bCompress,
//      )
//
// Routine Description:
//  
//      Determines whether the shared USB device traffic is compressed.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//      bCompress - is TRUE if network traffic is compressed. Otherwise "False" value is returned
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerGetSharedUsbDevIsCompressed (PVOID pContext, PVOID pDevContext, BOOL *bCompress);

//++
//
// BOOL
// ServerSetSharedUsbDevCrypt (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      IN BOOL  bCrypt
//      )
//
// Routine Description:
//
//      Changes traffic encryption settings for shared USB device.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor, returned by ServerGetUsbDevFromHub function.
//
//      bCrypt – enables encryption for all network traffic, if set to True.
//
// Return Value:
//
//      True - if USB device was successfully shared. False - if an error occurred, 
//             or the USB device was not shared.
//
//--
FUNC_API    BOOL WINAPI ServerSetSharedUsbDevCrypt (PVOID pContext, PVOID pDevContext, BOOL bCrypt);

//++
//
// BOOL
// ServerSetSharedUsbDevCompress (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      IN BOOL  bCompress
//      )
//
// Routine Description:
//  
//      Changes traffic compression settings for shared USB device.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//      bCompress – enables compression for all network traffic, if set to True.
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerSetSharedUsbDevCompress (PVOID pContext, PVOID pDevContext, BOOL bCompress);

//++
//
// BOOL
// ServerAllowDevRemoteDisconnect (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      IN BOOL  bCompress
//      )
//
// Routine Description:
//  
//      Enable client to be disconnected by another client 
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//      bEnable – enables client to be disconnected by another client, if set to True.
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerAllowDevRemoteDisconnect  (PVOID pContext, PVOID pDevContext, BOOL bEnable);

//++
//
// BOOL
// ServerSetSharedUsbDevAuth (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      IN BOOL  bAuth,
//      IN VARIANT strPassword
//      )
//
// Routine Description:
//  
//      Changes traffic authorization settings for shared USB device.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//
//      bAuth - enables authorization for the USB device if set to TRUE. strPassword parameter 
//              should be set.
//      
//      strPassword - authorization password. It's used if bAuth is set to TRUE.
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerSetSharedUsbDevAuth (PVOID pContext, PVOID pDevContext, 
                                                   BOOL bAuth, VARIANT strPassword);

//++
//
// BOOL
// ServerSetDevUserDescription (
//      IN PVOID pContext, 
//      IN PVOID pDevContext, 
//      IN VARIANT strUserDescription
//      )
//
// Routine Description:
//  
//      Changes optional user description for USB device.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//      pDevContext - USB devices descriptor returned by ServerGetUsbDevFromHub function.
//      
//      strUserDescription - additional USB device description to simplify its identification. 
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerSetDevUserDescription (PVOID pContext, PVOID pDevContext,
                                                     VARIANT strUserDescription);

//++
//
// BOOL
// ServerGetLastError (
//		IN PVOID pContext,
//		OUT VARIANT *strErrorCode
//		)
//
// Routine Description:
//
//		Gets error code from the last server operation.
//
// Arguments:
//
//      pContext - USB devices enumerator returned by ServerCreateEnumUsbDev function.
//
//		strErrorCode - returns error code.
//
// Possible error codes:
//
//		OK - the last command was executed successfully;
//		ERR - general error code;
//		DEMO - sharing operation failed due to demo limitations;
//		TCP_PORT - specified TCP port is already taken;
//		NOT_FOUND - specified USB port/device not found.
//
// Return Value:
//
//      True – if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API    BOOL WINAPI ServerGetLastError (PVOID pContext, VARIANT *strErrorCode);

//** Client
// enum client dev
//++
//
// BOOL
// ClientEnumAvailRemoteDevOnServer (
//		IN VARIANT szServer,
//		OUT PVOID *ppFindContext
//		)
//
// Routine Description:
// 
//		Creates enumerator of all shared USB devices on the remote server.
//
// Arguments:
//
//		szServer - the name of the server for which enumerator of all shared USB devices is created.
//
//		ppFindContext - a pointer to the remote USB devices context to be used in subsequent operations.  
//
// Return Value:
//
//      True - if the remote server exists, and enumerator of all shared USB devices was created. 
//			   Otherwise "False" value is returned.
//
//--
FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDevOnServer (VARIANT szServer, PVOID *ppFindContext);

//++
//
// BOOL
// ClientEnumAvailRemoteDevOnServerTimeout (
//		IN VARIANT szServer, 
//		OUT PVOID *ppFindContext,
//		IN DWORD dwTimeoutMs
//		)
//
// Routine Description:
// 
//		Creates enumerator of all shared USB devices on the remote server with specified timeout.
//
// Arguments:
//
//		szServer - the name of the server for which enumerator of all shared USB devices is created.
//
//		ppFindContext - a pointer to the remote USB devices context to be used in subsequent operations.
//
//		dwTimeoutMs - timeout to connect to remote server, in milliseconds.
//
// Return Value:
//
//      True - if the remote server exists, and enumerator of all shared USB devices was created. 
//		False - if remote server is offline or didn't accept our connection within specified timeout.
//
//--
FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDevOnServerTimeout (VARIANT szServer, PVOID *ppFindContext, DWORD dwTimeoutMs);

//++
//
// BOOL
// ClientEnumAvailRemoteDev (
//		OUT PVOID *ppFindContext
//		)
//
// Routine Description:
//
//		Creates enumerator of all remote USB devices on the server.
//
// Arguments:
//
//		ppFindContext - remote USB devices context to be used in subsequent operations. 
//
// Return Value:
//
//		True - if the function was executed successfully. Otherwise "False" value is returned.
//
//--
FUNC_API	BOOL WINAPI ClientEnumAvailRemoteDev (PVOID *ppFindContext);

//++
//
// BOOL
// ClientRemoveEnumOfRemoteDev (
//		IN PVOID pFindContext
//		)
//
// Routine Description:
// 
//		Removes remote USB devices enumerator.
// 
// Arguments:
//
//		pFindContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//					   or ClientEnumAvailRemoteDev functions.
//
// Return Value:
//
//		True - if the function was executed successfully. Otherwise "False" value is returned. 
//
//--
FUNC_API	BOOL WINAPI ClientRemoveEnumOfRemoteDev (PVOID pFindContext);

//++
//
// BOOL
// ClientGetRemoteDevNetSettings (
//		IN PVOID pFindContext,
//		IN long iIndex,
//		OUT VARIANT *NetSettings
//		)
//
// Routine Description:
//
//		Gets network settings for the remote USB device.
// 
// Arguments:
//
//		pFindContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//					   or ClientEnumAvailRemoteDev functions.
//	
//		iIndex - serial number of the remote USB device.
//
//		NetSettings - 	network settings of the remote USB device. If full network path is set   
//						("host_name:tcp_port"), it is standard network connection. If only "tcp_port" 
//						is set, it is callback connection, and the client is waiting for the incoming
//						connection from the server.
//				
// Return Value: 
//	
//		True - if the function was executed successfully. Otherwise "False" value is returned.		
//
//--
FUNC_API	BOOL WINAPI ClientGetRemoteDevNetSettings (PVOID pFindContext, long iIndex, VARIANT *NetSettings);

//++
//
// BOOL
// ClientGetRemoteDevName (
//		IN PVOID pFindContext,
//		IN long iIndex,
//		OUT VARIANT *strName
//		)
//
// Routine Description:
//
//		Gets remote USB device name.
// 
// Arguments:
//
//		pFindContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//					   or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		strName - the remote USB device name.
//
// Return Value:
//
//		True - if the function was executed successfully. Otherwise "False" value is returned.	
//--
FUNC_API	BOOL WINAPI ClientGetRemoteDevName (PVOID pFindContext, long iIndex, VARIANT *strName);

// client
//++
//
// BOOL
// ClientAddRemoteDevManually (
//		IN VARIANT szNetSettings
//		)
//
// Routine Description:
//
//		Is used to add remote USB device to the list manually. This function is required when adding a 
//		remote USB device, which initializes callback connection, and if the server is not in the same 
//		network with the client. If network settings are known, remote USB devices with standard connection 
//		type also can be added. For example: 
//			localhost:5000 – standard client;
//			5000 – callback client.
//
// Arguments:
//
//		szNetSettings - network settings for the remote USB device. If full network path is set   
//						("host_name:tcp_port"), it is standard network connection. If only "tcp_port" 
//						is set, it is callback connection, and the client is waiting for the incoming 
//						connection from the server.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientAddRemoteDevManually (VARIANT szNetSettings);

//++
//
// BOOL
// ClientAddRemoteDevManuallyEx (
//		IN VARIANT szNetSettings,
//		IN BOOL bRemember,
//		IN BOOL bAutoAdded
//		)
//
// Routine Description:
//
//		Is used to add remote USB device to the list manually. 
//		See description for ClientAddRemoteDevManually for details.
//		This routine allows caller to control how USB service should
//		save settings to non-volatile memory.
//
// Arguments:
//
//		szNetSettings - network settings for the remote USB device. If full network path is set   
//						("host_name:tcp_port"), it is standard network connection. If only "tcp_port" 
//						is set, it is callback connection, and the client is waiting for the incoming 
//						connection from the server.
//
//		bRemember - if TRUE new client is added to non-volatile settings
//					of USB service and will be re-activated after restart.
//
//		bAutoAdded - flag to indicate whether device was added implicitly
//					 (saved to non-volatile memory and could be later asked
//					  with ClientGetRemoteDevValueByName).
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientAddRemoteDevManuallyEx (VARIANT szNetSettings, BOOL bRemember, BOOL bAutoAdded);

//++
//
// BOOL
// ClientAddRemoteDev (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		)
//
// Routine Description:
//
//		Adds remote USB device to the list.
// 
// Arguments:
//
//		pFindContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer.
//
//		iIndex - serial number of the remote USB device.
//	
// Return Value:
//
//		True - if the function was executed successfully. False – if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientAddRemoteDev (PVOID pClientContext, long iIndex);

//++
//
// BOOL
// ClientAddRemoteDevEx (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		IN BOOL bRemember,
//		IN BOOL bAutoAdded
//		)
//
// Routine Description:
//
//		Adds remote USB device to the list.
//		This routine allows caller to control how USB service should
//		save settings to non-volatile memory.
// 
// Arguments:
//
//		pFindContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer.
//
//		iIndex - serial number of the remote USB device.
//
//		bRemember - if TRUE new client is added to non-volatile settings
//					of USB service and will be re-activated after restart.
//
//		bAutoAdded - flag to indicate whether device was added implicitly
//					 (saved to non-volatile memory and could be later asked
//					  with ClientGetRemoteDevValueByName).
//	
// Return Value:
//
//		True - if the function was executed successfully. False – if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientAddRemoteDevEx (PVOID pClientContext, long iIndex, BOOL bRemember, BOOL bAutoAdded);

//++
//
// BOOL
// ClientStartRemoteDev (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		IN BOOL bAutoReconnect,
//		IN VARIANT strPassword
//		)
//
// Routine Description:
//
//		Initializes client’s connection to the server.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//		
//		bAutoReconnect - if this parameter is True, reconnection attempts will be made after 
//						 the connection break. If False - the client disconnects.
//		
//		strPassword -   authorization password if required. You can find out whether 
//						authorization is required by calling ClientRemoteDevRequiresAuth function.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientStartRemoteDev (PVOID pClientContext, long iIndex, BOOL bAutoReconnect, VARIANT strPassword);

//++
//
// BOOL
// ClientStopRemoteDev (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		)
//
// Routine Description:
//
//		Stops connection of the remote USB device to the server.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
// Return Value:
//
//		True - if the function was executed successfully. 	False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientStopRemoteDev (PVOID pClientContext, long iIndex);

//++
//
// BOOL
// ClientRemoteDevDisconnect (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		)
//
// Routine Description:
//
//		Stops connection of the remote USB device to the remote server.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
// Return Value:
//
//		True - if the function was executed successfully. 	False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientRemoteDevDisconnect (PVOID pClientContext, long iIndex);

//++
//
// BOOL
// ClientRemoveRemoteDev (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		)
//
// Routine Description:
//
//		Removes the remote USB device from the list. 
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientRemoveRemoteDev (PVOID pClientContext, long iIndex);

//++
//
// BOOL
// ClientGetStateRemoteDev (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		OUT LONG *piState,
//		OUT VARIANT *RemoteHost
//		)
//
// Routine Description:
//
//      Gets the current status of the remote USB device connection to the server. 
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		piState		- the current connection status:
//					0 – the device is not connected to the server;
//					2 – the device is connected to the server.
//
//		RemoteHost - name of the remote host, currently connected to the client. This parameter 
//					 is set when piState is 2.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientGetStateRemoteDev (PVOID pClientContext, long iIndex, LONG *piState, VARIANT *RemoteHost);

//++
//
// BOOL
// ClientTrafficRemoteDevIsEncrypted (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		OUT BOOL *bCrypt
//		)
//
// Routine Description:
//
//		Determines whether network traffic for the remote USB device is encrypted.
// 
// Arguments:
//
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		bCrypt – is True if network traffic is encrypted. Otherwise "False" value is returned.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientTrafficRemoteDevIsEncrypted (PVOID pClientContext, long iIndex, BOOL *bCrypt);

//++
//
// BOOL
// ClientRemoteDevRequiresAuth (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		OUT BOOL *bAuth
//		)
//
// Routine Description:
//
//		Determines whether authorization is required to establish connection with the remote USB device.
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		bAuth -  is True if authorization is required. Otherwise "False" value is returned.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API	BOOL WINAPI ClientRemoteDevRequiresAuth (PVOID pClientContext, long iIndex, BOOL *bAuth);

//++
//
// BOOL
// ClientRemoteDisconnectIsEnabled (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		OUT BOOL *bEnable
//		)
//
// Routine Description:
//
//		Determines whether allow client to be disconnected by another client .
// 
// Arguments:
//
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		bEnable – is True if enabled client to be disconnected by another client
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientRemoteDisconnectIsEnabled (PVOID pClientContext, long iIndex, BOOL *bEnable);

//++
//
// BOOL
// ClientGetStateSharedDevice (
//		IN PVOID pClientContext,
//		IN LONG iIndex ,
//		OUT LONG *piState,
//		OUT VARIANT *RemoteHost
//		)
//
// Routine Description:
//
//        This function requests the current status of shared remote device from the remote server.
//
// Arguments:
//    
//        pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer
//                         or ClientEnumAvailRemoteDev functions.
//
//        iIndex - serial number of the remote USB device.
//
//        piState - returns the current status of the shared USB device:
//            1 – the device is waiting for connection;
//            2 – the device is already connected.
//
//		  RemoteHost -  the name or IP-address of the client connected to the shared port. Valid only when piState = 2
//
//Return Value:
//
//        True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API	BOOL WINAPI ClientGetStateSharedDevice  (PVOID pClientContext, long iIndex, LONG *piState, VARIANT *RemoteHost);

//++
//
// BOOL
// ClientEnumRemoteDevOverRdp (
//		OUT PVOID *ppFindContext
//		)
//
// Routine Description:
//
//       This function enumerates all remote devices currently available over RDP.
//
// Return Value:
//
//        True - if the function was executed successfully. False - if an error occurred.
//--
FUNC_API	BOOL WINAPI ClientEnumRemoteDevOverRdp (PVOID *ppFindContext);

//++
//
// BOOL
// ClientTrafficRemoteDevIsCompressed (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		OUT BOOL *bCompress
//		)
//
// Routine Description:
//
//		Determines whether network traffic for the remote USB device is compressed.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev functions.
//
//		iIndex - serial number of the remote USB device.
//
//		bCompress – is True if network traffic is compressed. Otherwise "False" value is returned.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientTrafficRemoteDevIsCompressed (PVOID pClientContext, long iIndex, BOOL *bCompress);

//++
//
// BOOL
// ClientGetRdpAutoconnect (void)
//
// Routine Description:
//
//		Get settings of RDP auto-connect function.
//
// Arguments:
//
//		None.
//
// Return Value:
//
//		True - if RDP auto-connect function is enabled.
//
//--
FUNC_API	BOOL WINAPI ClientGetRdpAutoconnect (void);

//++
//
// BOOL
// ClientSetRdpAutoconnect (
//		IN BOOL bEnableAutoconnect
//		)
//
// Routine Description:
//
//		Enable or disable RDP auto-connect function.
// 
// Arguments:
//
//		bEnableAutoconnect – enables RDP auto-connect, if set to TRUE;
//							disables RDP auto-connect, if set to FALSE.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientSetRdpAutoconnect (BOOL bEnableAutoconnect);

//++
//
// BOOL
// ClientGetRemoteDevValueByName (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		IN VARIANT strValueName,
//		OUT VARIANT *pValue
//		)
//
// Routine Description:
//
//		Returns value by name for the remote USB device.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned by ClientEnumAvailRemoteDevOnServer 
//						 or ClientEnumAvailRemoteDev or ClientEnumRemoteDevOverRdp functions.
//
//		iIndex - serial number of the remote USB device.
//
//		strValueName - name of interested value.
//
//		pValue - pointer to output value.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
// Notes:
//
//		- the function returns cached data for remote device. 
//		- possible value names: 
//			NAME			(string)	name of the USB device, 
//			NICK			(string)	user description of the USB device, 
//			AUTH			(bool)		authorization, 
//			ENCR			(bool)		encryption, 
//			COMPR			(bool)		compression, 
//			STATUS			(int)		connection status (for shared devices), 
//			SHARED_WITH		(string)	client connected to shared device,
//			ALLOW_RDISCONN	(bool)		whether any remote client is allowed to disconnect the device,
//			USBHUB			(string)	USB hub name
//			USBPORT			(string)	USB port name
//			USBLOC			(string)	USB port location
//			AUTO_ADDED		(bool)		whether USB device was automatically added on connect
//			USBCLASS		(string)	USB device class triple as single hex-ascii string xxyyzz (where xx=class, yy=subclass, zz=protocol), 
//			BCDUSB			(uint)		version of the USB specification,
//			VID				(uint)		vendor identifier for the device,
//			PID				(uint)		product identifier,
//			REV				(uint)		version of the device,
//			SERIAL			(string)	serial number of the device.
//
//--
FUNC_API	BOOL WINAPI ClientGetRemoteDevValueByName (PVOID pClientContext, long iIndex, 
                                                       VARIANT strValueName, VARIANT *pValue);

//++
//
// BOOL
// ClientGetConnectedDevValueByName (
//		IN PVOID pClientContext,
//		IN long iIndex,
//		IN VARIANT strInfoName,
//		OUT VARIANT *pVarInfo
//		)
//
// Routine Description:
//
//		Returns value by name for the connected remote USB device.
// 
// Arguments:
//
//		pClientContext - remote USB devices context returned ClientEnumAvailRemoteDev.
//
//		iIndex - serial number of the remote USB device.
//
//		strValueName - name of interested value.
//
//		pValue - pointer to output value.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
// Notes:
//
//		- possible value names:
//			NAME			(string)	name of the USB device, 
//			NICK			(string)	user description of the USB device, 
//			AUTH			(bool)		authorization, 
//			ENCR			(bool)		encryption, 
//			COMPR			(bool)		compression, 
//			STATUS			(int)		connection status (for shared devices), 
//			SHARED_WITH		(string)	client connected to shared device,
//			USBHUB			(string)	USB hub name
//			USBPORT			(string)	USB port name
//			USBLOC			(string)	USB port location
//			USBCLASS		(string)	USB device class triple as single hex-ascii string xxyyzz (where xx=class, yy=subclass, zz=protocol), 
//
//--
FUNC_API	BOOL WINAPI ClientGetConnectedDevValueByName (PVOID pClientContext, long iIndex,
														  VARIANT strValueName, VARIANT *pValue);

//** Miscellaneous
//++
//
// void
// SetCallBackOnChangeDevList (
//      IN FNOnChangeDevList pFunc
//      )
//
// Routine Description:
//
//		Sets callback function to be called when the device list is changed.
// 
// Arguments:
//
//		FNOnChangeDevList - address of the function to be called when the device list is changed.
//							
//		void (__stdcall *FNOnChangeDevList) () - callback function description.
//
// Return Value:
//
//      None
//			  
//--
FUNC_API	void WINAPI SetCallBackOnChangeDevList (FNOnChangeDevList pFunc);

//++
//
// BOOL
// CheckUsbService (void)
//
// Routine Description:
//
//		Checks whether usb service process is running.
//
// Arguments:
//
//		None.
//
// Return Value:
//
//		True - if usb service is running and ready to accept commands.
//
//--
FUNC_API	BOOL WINAPI CheckUsbService (void);

//++
//
// void
// UpdateUsbTree (void)
//
// Routine description:
//
//		Force usb service to re-read of the USB tree.
//
// Arguments:
//
//		None.
//
// Return Value:
//
//		None.
//
//--
FUNC_API	void WINAPI UpdateUsbTree (void);

//++
//
//	BOOL
//	ClientGetRdpIsolation (void)
//
// Routine Description:
//
//		Returns whether RDP isolation enabled or not.
// 
// Arguments:
//
//		None.
//
// Return Value:
//
//		True - if RDP isolation is enabled, False - otherwise.
//
//--
FUNC_API	BOOL WINAPI ClientGetRdpIsolation (void);

//++
//
//	BOOL
//	ClientSetRdpIsolation (
//		BOOL bEnable
//		)
//
// Routine Description:
//
//		Change RDP isolation settings.
// 
// Arguments:
//
//		bEnable - enables RDP isolation, if set to TRUE;
//					disables RDP isolation, if set to FALSE.
//
// Return Value:
//
//		True - if the function was executed successfully. False - if an error occurred.
//
//--
FUNC_API	BOOL WINAPI ClientSetRdpIsolation (BOOL bEnable);


} // extern "C"

#endif
