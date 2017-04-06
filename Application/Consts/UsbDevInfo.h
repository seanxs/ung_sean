/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   UsbDevInfo.h

Abstract:

	Class that builds USB devices tree 

--*/

#pragma once

#include <vector>
#include <atlstr.h>
#include <map>
#include <Winioctl.h>
#include "usbioctl.h"


class CUsbDevInfo
{
public:
    CUsbDevInfo(void);
    CUsbDevInfo(bool bHub, LPCTSTR szName, LPCTSTR szGiud, bool bConnected = false);

    ~CUsbDevInfo(void);

public:
	// dev info parameters
	CString         m_strName;
	CString         m_strGuid;
	CString         m_strDescription;
	CString         m_strHubDev;
	bool            m_bConnected;
	bool            m_bHub;
	int             m_iPort;
	CString         m_strUsbLoc;		// USB port location as N:M:X:Y:Z

	bool            m_bShare;
	bool            m_bAuth;
	CStringA		m_strPswdEncrypted;
	bool            m_bCrypt;
	bool            m_bCompress;
	int				m_iCompressType;
	bool			m_bAllowRDisconn;

	// net settings
	CString			m_strServer;
	int				m_iTcpPort;

	//// USB descriptor data
	BYTE			m_bBaseClass;
	BYTE			m_bSubClass;
	BYTE			m_bProtocol;
	USHORT			m_bcdUSB;
	USHORT			m_idVendor;
	USHORT			m_idProduct;
	USHORT			m_bcdDevice;
	CString			m_strSerialNumber;

	CString			m_strDevUnicId;

	// connection status (valid for shared dev only) - see dllmain.h for constants U2EC_STATE_xxx
	int				m_iStatus;
	bool			m_bRdp;
	CString			m_strSharedWith;

    std::vector <CUsbDevInfo> m_arrDevs;
	std::map <PVOID, CUsbDevInfo*> m_mapDevs;

	static std::map <ULONG, CString> m_mapName;
	static CString GetNameDevById (ULONG uId) 
	{
		std::map <ULONG, CString>::iterator find;
		find = m_mapName.find (uId);
		if (find != m_mapName.end ())
		{
			return find->second;
		}
		return L"";
	}
	//static CCriticalSection	m_cs;

public:
    static void EnumerateUsbTree (CUsbDevInfo& devInfo);
	void Clear ();
	const CUsbDevInfo* FindDevice (LPCTSTR szHub, int iPort) const;
	const CUsbDevInfo* FindHub (LPCTSTR szHub) const;
	static ULONG BuildIdFromDescription (PUSB_DEVICE_DESCRIPTOR pDesc);
	CString GetNetSettings () const;
	void SetNetSettings (const CString &strNetSettings);
	void SetNetSettings (const CString &strServer, int iTcpPort);
	int GetCompressType () const;

	CStringA UsbClassAsHexStrA () const;
	static CStringA UsbClassAsHexStrA (BYTE uBaseClass, BYTE uSubClass, BYTE uProtocol);
	static bool UsbClassFromHexStrA (const CStringA &strUsbClassHex, BYTE &uBaseClass, BYTE &uSubClass, BYTE &uProtocol);

private:
    static void EnumerateHostControllers ( CUsbDevInfo& devInfo );
    static bool EnumerateHostController (CUsbDevInfo& devInfo, HANDLE hHCDev, LPCTSTR szName, LPCTSTR szUsbLoc);
    static void EnumerateHub (CUsbDevInfo& devInfo, LPCTSTR HubName, LPCTSTR HubId, LPCTSTR Description, LPCTSTR UsbLoc);
    static void EnumerateHubPorts (CUsbDevInfo& devInfo, HANDLE hHubDevice, ULONG NumPorts, LPCTSTR HubId, LPCTSTR UsbLoc);

    static CString GetDriverKeyName (HANDLE  Hub, ULONG   ConnectionIndex);
    static CString GetStringDescriptor (HANDLE  hHubDevice,ULONG   ConnectionIndex,UCHAR   DescriptorIndex,USHORT  LanguageID);
    static CString GetExternalHubName (HANDLE  Hub,ULONG   ConnectionIndex);

    static CString GetRootHubName ( HANDLE HostController, DWORD dwIoControlCode = IOCTL_USB_GET_ROOT_HUB_NAME );
    static CString GetRootHubDriverKeyName ( LPCTSTR HubName, LPCTSTR szHcdDevice = NULL );
	static bool GetConfigDescriptor (HANDLE  hHubDevice, ULONG ConnectionIndex, UCHAR   DescriptorIndex, PUSB_DESCRIPTOR_REQUEST pDescriptor, ULONG &iSize);
	static bool InitDevClass (HANDLE  hHubDevice, PUSB_NODE_CONNECTION_INFORMATION_EX pConnInfo);

	// get required data from USB_DEVICE_DESCRIPTOR and store it to this object, only for real devices.
	void StoreDeviceDescriptorData (HANDLE  hHubDevice, PUSB_NODE_CONNECTION_INFORMATION_EX pConnInfo);

    enum ToParam
    {
        DeviceId,
        DeviceDesc,
        DeviceClass,
		ParentDevice,
		DeviceDisable,
    };

    static CString DriverNameToParam (LPCTSTR DriverName, ToParam param);
	bool IsPresent (LPCTSTR szGuid);
	void Init ();
};
