/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    consts.h

Environment:

    User mode

--*/
#ifndef CONSTS_SH_DF
#define CONSTS_SH_DF

#include <windows.h>
#include <tchar.h>

#define VIRTUAL_CHANNEL_NAME "usb4rdp"

#define BUFSIZE_1K 1024
#define BUFSIZE_2K 1024*2
#define BUFSIZE_4K 1024*4
#define BUFSIZE_8K 1024*8
#define BUFSIZE_16K 1024*16
#define BUFSIZE_128K 1024*128
#define BUFSIZE_PIPE 8128

// rdp commands 
#define USB4RDP_REQUEST_GET_SHARES_LIST "GET SHARES LIST"
#define USB4RDP_ANSWER_SHARES_LIST "ANSW SHARES LIST"
#define USB4RDP_DATA_PACKET "DATA" 
#define USB4RDP_CONNECT_REQUEST "CONNECT TO THE SERVER"
#define USB4RDP_DISCONNECT_REQUEST "DISCONNECT SERVER"

#define USB4RDP_CONNECT_ESTABLISHED_ANSWER "USB4RDP_CONNECT_ESTABLISHED_ANSWER"
#define USB4RDP_CONNECT_ERROR_ANSWER "USB4RDP_CONNECT_ERROR_ANSWER"
//#define USB4RDP_DISCONNECT_ACCEPT_ANSWER "USB4RDP_DISCONNECT_ACCEPT_ANSWER"
#define USB4RDP_MANUAL_DISCONNECT_COMMAND "MANUAL_DISCONNECT_COMMAND"
#define USB4RDP_ENABLE_COMPRESS "ENABLE_COMPRESS"

#define USB4RDP_GETNAME_ANSWER "GETNAME ANSWER STRING"

extern wchar_t * g_wsPipeName;

//typedef boost::shared_mutex CLock;
//typedef boost::shared_lock <boost::shared_mutex> CLockRead;
//typedef boost::unique_lock <boost::shared_mutex> CLockWrite;

namespace Consts
{ 

	extern LPCWSTR wsRemoteControlMutexName;

// message to log
extern LPCTSTR szpLogMes [];

#define LOG_MESSAGES_LIST \
	/* general service events */	\
	X(LM_NOT_START,				"Unable to load internal configuration from registry")					\
	X(LM_ERR_5475,				"CService: error listening to port 5475")								\
	X(LM_ACTIVATE_SUCCESS,		"Activated successfully")												\
	/* server (sharing devices) */	\
	X(LM_ADD_SHARE,				"\"%s\\Port %d\" was successfully shared on TCP port %s")				\
	X(LM_DEL_SHARE,				"\"%s\\Port %d\" on TCP port %d was unshared successfully")				\
	X(LM_DEVICE_PRESENT,		"The following device is available for sharing: %s, \"%s\\Port %d\" ")	\
	X(LM_SHARED_DEVICE_WAITING,	"Shared device \"%s\" at \"%s\\Port %d\" is waiting for client connection on TCP port %s")	\
	X(LM_SHARED_DEVICE_MISSING,	"There is no USB device connected to shared USB port \"%s\\Port %d\"")	\
	X(LM_SHARE_CONNECTED,		"\"%s\\Port %d (%s)\" the following host was connected to the shared device: %s")			\
	X(LM_SHARE_DISCONNECTED,	"\"%s\\Port %d (%s)\" the following host was disconnected from the shared device: %s")		\
	/* server errors */	\
	X(LM_NOT_SHARE_DEVICE,		"Error while sharing device \"%s\\Port %d\" on TCP port %s")			\
	X(LM_MAX_SHARE,				"Maximum allowed number of shared devices reached")						\
	X(LM_ERR_AUTH,				"\"%s\\Port %d\": Error while initializing authorization module")		\
	X(LM_ERR_CRYPT,				"\"%s\\Port %d\": Error while initializing traffic encryption module")	\
	X(LM_ERR_SHARE_LISTEN,		"\"%s\\Port %d\" error while starting listening to port %d")			\
	X(LM_SHARE_NO_AUTH,			"\"%s\\Port %d\" error during authorization, most probably the password is incorrect, host: %s")	\
	X(LM_SHARE_NO_CRYPT,		"\"%s\\Port %d\" error while starting encrypted data transmission, host: %s")						\
	X(LM_ZIP_SERVER_ERROR,		"Server \"%s\\Port %d\" compression error detected.")					\
	/* client (connect remote devices) */	\
	X(LM_ADD_CLIENT,			"Connection to \"%s\" was added successfully")					\
	X(LM_ADD_CLIENT_RDP,		"Connection to \"%s [RDP]\"  was added successfully")			\
	X(LM_DEL_CLIENT,			"Connection to \"%s:%d\" was deleted")							\
	X(LM_DEL_CLIENT_RDP,		"Connection to \"%s [RDP]\" was deleted")						\
	X(LM_START_CLIENT,			"Activating connection to remote device on \"%s:%d\"")			\
	X(LM_START_CLIENT_RDP,		"Activating connection to remote device on \"%s [RDP]\"")		\
	X(LM_STOP_CLIENT,			"Deactivating connection to remote device on \"%s:%d\"")		\
	X(LM_STOP_CLIENT_RDP,		"Deactivating connection to remote device on \"%s [RDP]\"")		\
	X(LM_TRY_CONNECTION,		"\"%s:%d\": trying to establish connection")					\
	X(LM_TRY_CONNECTION_RDP,	"\"%s [RDP]\": trying to establish connection")					\
	X(LM_CLIENT_CONN,			"\"%s:%d\": connection established")							\
	X(LM_CLIENT_CONN_RDP,		"\"%s [RDP]\": connection established")							\
	/* client errors */	\
	X(LM_ERROR_VUHUB_CTRL,		"Error opening control device of virtual hub: %d. Try reinstalling USB Network Gate.")	\
	X(LM_ERROR_VUHUB_ADD_DEV,	"Error adding new device to virtual hub: %d. Try reinstalling USB Network Gate.")		\
	X(LM_CLIENT_CANT_CRYPT,		"\"%s:%d\": Error while initializing traffic encryption module")	\
	X(LM_HANDSHAKE_ERROR,		"\"%s:%d\": Handshake error detected (unknown protocol)")			\
	X(LM_COMPR_TYPE_ERROR,		"\"%s:%d\": Handshake error detected (compression type error)")	\
	X(LM_NOT_CONN_DEVICE,		"Error while creating connection to \"%s\" on TCP port %d")		\
	X(LM_MAX_CLIENT,			"Maximum allowed number of connected devices reached")			\
	X(LM_ERR_AUTH_CLINT,		"\"%s\": Error while initializing authorization module")		\
	X(LM_CLIENT_ERR_CONN,		"\"%s:%d\": unable to establish connection")	\
	X(LM_CLIENT_NO_AUTH,		"\"%s:%d\" error during authorization")			\
	X(LM_CLIENT_NO_CRYPT,		"\"%s:%d\" error during encryption")			\
	X(LM_CLIENT_STOP,			"\"%s:%d\" connection broken")					\
	X(LM_CLIENT_STOP_RDP,		"\"%s [RDP]\" connection broken")								\
	X(LM_ZIP_CLIENT_RDP_ERROR,	"Client \"%s [RDP]\" compression error detected.")						\
	X(LM_ZIP_CLIENT_ERROR,		"Client \"%s:%d\" compression error detected")							\
// LOG_MESSAGES_LIST

#define X(e,s) e,
enum LogMessage
{
	LOG_MESSAGES_LIST
};
#undef X

// device info
extern LPCTSTR szpInfoName [];
extern LPCTSTR szNotDetials;

enum IN
{
    IN_NO_DEVICE,
};
// remote communication
extern const char chHub;
extern const USHORT uTcpPortConfig;
extern const USHORT uTcpUdp;
extern const USHORT uTcpControl;
// tree
extern LPCTSTR szRootHub;
extern LPCTSTR szFreePort;
extern LPCTSTR szUnknowDevice;
extern LPCTSTR szEltimaHub;
extern LPCTSTR szRdpHub;
extern LPCTSTR szNotDevice;
extern LPCTSTR szTextNoConnected;
extern LPCTSTR szTextNoConnected2;

extern const LPCTSTR szRegistry_usb4rdp;
extern const LPCTSTR szRegistryShare_usb4rdp;
extern const LPCTSTR szRegistryConnect_usb4rdp;
extern const LPCTSTR szRegistryHubName_usb4rdp;

extern const LPCTSTR szAutoConnectParam;
extern const LPCTSTR szIsolation;

extern LPCTSTR g_wsControlDeviceName;

extern LPCTSTR szDeviceBusFilterName;
extern LPCTSTR szDeviceBusFilterDisplay;
extern LPCTSTR szDeviceBusFilterFileName;


enum TreeImage
{
	TREE_SERVER,
	TREE_SERVER_OFF,
    TREE_SERVER_ON,
	TREE_HUB,
	TREE_HUB_ELTIMA,
	TREE_CLIENT,
	TREE_CLIENT_OFF,
	TREE_CLIENT_DISABLE,
    TREE_SERVER_ON_AU,
    TREE_SERVER_ON_CR,
    TREE_SERVER_ON_CR_AU,
    TREE_SERVER_OFF_AU,
    TREE_SERVER_OFF_CR,
    TREE_SERVER_OFF_CR_AU,
	TREE_CLIENT_AU,
	TREE_CLIENT_CR,
	TREE_CLIENT_CR_AU,
};

// Config
extern const LPCTSTR szRegistry;
extern const LPCTSTR szRegistryShare;
extern const LPCTSTR szRegistryConnect;
extern const LPCTSTR szRegistryHubName;
extern const LPCTSTR szRegistryHost;
extern const LPCTSTR szRegLogFile;

// AppStatico
namespace appstatico
{
extern int app_id;
extern const char *app_key;
}

// service
extern const LPCTSTR szServiceName;
extern const LPCTSTR szDisplayName;

// command service
#define COMMANDS_LIST  \
	COM(SC_INSTALL,			_T("install")) \
	COM(SC_UNINSTALL,		_T("uninstall")) \
    COM(SC_ENABLE,			_T("enable")) \
	COM(SC_DISABLE,			_T("disable")) \
	COM(SC_REG,				_T("reg"))\
	COM(SC_SHARE,			_T("share-usb-port"))\
	COM(SC_UNSHARE,			_T("unshare-usb-port"))\
	COM(SC_SHOW_TREE,		_T("show-usb-list"))\
	COM(SC_SHOW_SHARED,		_T("show-shared-usb"))\
	COM(SC_ADD_CLIENT,		_T("add-remote-device"))\
	COM(SC_REMOTE_CLIENT,	_T("remove-remote-device"))\
	COM(SC_CONNECT,			_T("connect-remote-device"))\
	COM(SC_DISCONNECT,		_T("disconnect-remote-device"))\
	COM(SC_SHOW_REMOTE,		_T("show-remote-devices"))\
	COM(SC_FIND_CLIENT,		_T("find-remote-devices"))\
	COM(SC_SHOW_RDP_REMOTE,	_T("show-remote-rdp-devices"))\
	COM(SC_SERV_MANUAL_STOP,_T("shared-usb-disconnect"))\
	COM(SC_UNSHARE_ALL,		_T("unshare-all-usb-ports"))\
	COM(SC_HELP,			_T("help")) \
	COM(SC_SHOW_USB_DEV,	_T("show-usb-dev"))	\
	//COM(SC_CITRIX,			_T("citrix"))\
	//COM(SC_UNCITRIX,		_T("uncitrix"))\

extern const LPCTSTR szpCommand [];
#define COM(e,s)  e,
enum SrvCommand
{
	COMMANDS_LIST
};
#undef COM

// message string 
extern const LPCTSTR szpMessage [];
enum Message 
{
    MES_INSTALL,
	MES_BAD_INSTALL,
    MES_UNINSTALL,
	MES_BAD_UNINSTALL,
	MES_ENABLE,
	MES_BAD_ENABLE,
	MES_DISABLE,
	MES_BAD_DISABLE,
	MES_PARAM,
	MES_SERVICE_NOT_RESPOND,
	MES_ERROR_CREATE_SHARE,
	MES_MAX_SINGLE,
	MES_MAX_DEMO,
	MES_CAPTION_SINGLE,
	MES_CAPTION_DEMO,
	MES_ERROR_ADD,
    MES_BETWEEN_PORT,
    MES_PASSW_CONFIRM,
    MES_EMPLY_PASSW,
    MES_ALREADY_USE_TCPPORT,
};

// Pipe
//extern const TCHAR chEnd;
extern LPCTSTR szPipName;
extern LPCSTR szpTypeItem [];
extern LPCSTR szTokens;
extern LPCTSTR szPipeNameLog;


extern LPCSTR szpPipeCommand [];
// Using X macros trick to keep CommandPipe enum and szpPipeCommand array in sync
// about X macros: http://www.drdobbs.com/the-new-c-x-macros/184401387
#define PIPE_COMMANDS_LIST \
	X(CP_ADD,   "ADD")		/* ADD Server,HUBNAME,UsbPort,TcpPort!    ADD Client,HostName,TcpPort! */ \
	X(CP_DEL,   "DEL")		/* DEL Server,HubName,UsbPort!            DEL Cleint,HostNamr,TcpPort! */ \
	X(CP_INFO,  "INFO")                             \
	X(CP_GET_CONFIG,      "GETCONFIG")              \
	X(CP_GET_NAME_DEVICE, "GETNAME")                \
	X(CP_GET_IP,          "GETIP")                  \
	X(CP_START_CLIENT,    "START")                  \
	X(CP_STOP_CLIENT,     "STOP")                   \
	X(CP_HUBNAME,         "HUBNAME")                \
	X(CP_SRVDISCONN,      "SRVDISCONN" /*server disconnect client*/) \
	X(CP_GETDEVID,        "GETDEVICEID")            \
	X(CP_GETRDPSHARES,          "GETRDPSHARES")     \
	X(CP_ADD_RDC_CONNECTION,    "ADDRDCCONNECTION") \
	X(CP_REM_RDC_CONNECTION,    "REMRDCCONNECTION") \
	X(CP_SET_AUTOCONNECT,       "SETAUTOCONNECT")   \
	X(CP_GETLISTOFCLIENTS,      "GETLISTOFCLIENTS") \
	X(CP_GETUSBTREE,            "GETUSBTREE")       \
	X(CP_GETNUMUSBTREE,         "GETNUMUSBTREE")    \
	X(CP_UPDATEUSBTREE,         "UPDATEUSBTREE")    \
	X(CP_DEL_ALL,               "DELALL"     /*server unshare all shared devices*/)      \
	X(CP_CHANGEDESK,            "CHANGEDESK" /*change user description of shared port*/) \
	X(CP_SET_COMPRESS,          "SETCOMPRESS"/*change channel compression option*/)      \
	X(CP_SET_CRYPT,             "SETCRYPT"   /*change channel encryption option*/)       \
	X(CP_SET_AUTH,              "SETAUTH"    /*change channel auth option*/)             \
	X(CP_GET_LIC_INFO,          "GETLICINFO" /*get info about license (type,user,code,limits*/) \
	X(CP_GET_CONN_CLIENT_INFO,  "GETCONNCLIENTINFO" /*get info about connected client (name,nick,etc)*/) \
	X(CP_RDISCONNECT,			"RDISCONNECT" /*remote disconnect*/) \
	X(CP_SET_RDISCONNECT,		"SETRDISCONNECT" /*set param of remote disconnect*/) \
	X(CP_SET_RDP_ISOLATION,		"SETRDPISOLATION" /*set RDP isolation option*/) \
// PIPE_COMMANDS_LIST

#define X(e,s)  e,
enum CommandPipe
{
	PIPE_COMMANDS_LIST
};
#undef X

enum TypeItem
{
	TI_SERVER,
	TI_CLIENT,
};


extern LPCSTR szUngNetSettings [];

#define UNG_NET_SETTINGS_LIST         				\
	X(UNS_TYPE,				"TYPE")        			\
	X(UNS_HUB_NAME,			"USBHUB")      			\
	X(UNS_USB_PORT,			"USBPORT")     			\
	X(UNS_USB_LOC,			"USBLOC"			/* USB port location as N:M:X:Y:Z */)	\
	X(UNS_TCP_PORT,			"TCPPORT")     			\
	X(UNS_AUTH,				"AUTH")        			\
	X(UNS_CRYPT,			"ENCR")        			\
	X(UNS_COMPRESS,			"COMPR"				/*enable compression*/)					\
	X(UNS_COMPRESS_TYPE,	"COMPR_TYPE"		/*compression type, zip is default */)	\
	X(UNS_NAME,				"NAME")        			\
	X(UNS_DESK,				"NICK")        			\
	X(UNS_REMOTE_HOST,		"RHOST")       			\
	X(UNS_SHARED_WITH,		"SHARED_WITH") 			\
	X(UNS_COMMAND,			"COMMAND")     			\
	X(UNS_ERROR,			"ERR")		 			\
	X(UNS_PARAM,			"PARAM")       			\
	X(UNS_ID,				"ID")          			\
	X(UNS_SESSION_ID,		"SESSION_ID")			\
	X(UNS_STATUS,			"STATUS")				\
	X(UNS_RDP,				"RDP")					\
	X(UNS_DEV_SHARED,		"DEV_SAHRED")			\
	X(UNS_DEV_CONN,			"DEV_CONN")				\
	X(UNS_DEV_HUB,			"DEV_HUB")				\
	X(UNS_DEV_COUNT,		"DEV_COUNT")			\
	X(UNS_DEV_CLASS,		"UNS_DEV_CLASS"		/* usb device descriptor: usb class byte */)	\
	X(UNS_DEV_SUBCLASS,		"UNS_DEV_SUBCLASS"	/* usb device descriptor: usb sub class */)		\
	X(UNS_DEV_PROTOCOL,		"UNS_DEV_PROTOCOL"	/* usb device descriptor: usb protocol */)		\
	X(UNS_DEV_USBCLASS,		"USBCLASS"			/* usb class triple as single hex ascii */)		\
	X(UNS_DEV_BCDUSB,		"BCDUSB"			/* usb device descriptor: bcdUSB */)			\
	X(UNS_DEV_VID,			"VID"				/* usb device descriptor: idVendor */)			\
	X(UNS_DEV_PID,			"PID"				/* usb device descriptor: idProduct */)			\
	X(UNS_DEV_REV,			"REV"				/* usb device descriptor: bcdDevice (rev) */)	\
	X(UNS_DEV_SN,			"SERIAL"			/* usb device descriptor: serial number */)		\
	X(UNS_AUTO_ADDED,		"AUTO_ADDED"		/* client was auto-added by new gui */)			\
	X(UNS_DONT_SAVE_REG,	"DONT_SAVE_REG"		/* don't save client settings in registry */)	\
	X(UNS_DEV_UNIC_ID,		"USB_ID"			/* unique ID of usb device as USB\VID_xxxx&PID_xxxx&REV_xxxx\serial */)	\
	X(UNS_DEV_NET_SETTINGS,	"NETPARAM"			/* combined net settings as RHOST:TCPPORT */)		\
	X(UNS_DEV_ALLOW_RDISCON,"ALLOW_RDISCONN"	/* allow remote client to disconnect */)		\
	X(UNS_ACT_KEY,			"ACT_KEY"			/* activation key */)							\
	X(UNS_ACT_OFFLINE,		"ACT_OFFLINE"		/* offline activation */)						\
	X(UNS_ACT_ANSWER,		"ACT_ANSWER"		/* server's answer on offline request */)		\
// UNG_NET_SETTINGS_LIST

#define X(e,s)  e,
enum UNGNetSettings
{
    UNG_NET_SETTINGS_LIST
    UNG_NET_SETTINGS_MAX
};
#undef X


extern LPCSTR szUngErrorCodes [];

#define UNG_ERROR_CODES_LIST \
	/* answer on commands: UNS_ERROR key: possible values: OK | ERR */ \
	X(UEC_OK,    "OK")  \
	X(UEC_ERROR, "ERR") \
	/* answer on commands: UNS_PARAM key (if UNS_ERROR is ERR): explanation of error */ \
	X(UEC_TCP_PORT,  "TCP_PORT"  /*specified TCP port is already taken*/) \
	X(UEC_NOT_FOUND, "NOT_FOUND" /*specified USB port/device not found*/) \
	/* set value by name errors */ \
	X(UEC_CANT_SET,		"CANT_SET"		/*can't set new net settings for already shared device*/)		\
	X(UEC_BAD_VALUE,	"BAD_VALUE"		/*bad value passed (wrong type most likely)*/)					\
	X(UEC_BAD_NAME,		"BAD_NAME"		/*bad name passed (not supported to set/change)*/)				\
	X(UEC_ERROR_UPDATE,	"ERROR_UPDATE"	/*error while updating value for shared device in service*/)	\
	/* activation errors */		\
// UNG_ERROR_CODES_LIST

#define X(e,s)  e,
enum UNGErrorCodes
{
	UNG_ERROR_CODES_LIST
	UNG_ERROR_CODES_MAX
};
#undef X


typedef struct {
    USHORT  usVendorID;
    PCHAR   szVendor;
} USBVENDORID, *PUSBVENDORID;

extern USBVENDORID USBVendorIDs [];


#define UNG_LICENSE_DATA_LIST \
	/* enum, string name */   \
	X(ULD_TYPE,		"LICTYPE"		/*license type*/)							\
	X(ULD_NAME,		"LICUSERNAME"	/*registered user name*/)					\
	X(ULD_CODE,		"LICREGCODE"	/*registration code (mangled)*/)			\
	X(ULD_DEV_MAX,	"LICDEVMAX"		/*maximum devices allowed to share*/)		\
	X(ULD_DEV_REM,	"LICDEVREM"		/*how many devices could be shared now*/)	\
	X(ULD_TIME_REM,	"LICTIMEREM"	/*time limit remaining*/)					\
	X(ULD_COPIES,	"LICKOPIES"		/*exhausted copies flag*/)					\
	X(ULD_EXPIRED,	"LICEXPIRED"	/*license expired flag*/)					\
	X(ULD_DESCR,	"LICDESCR"		/*license description - see UNG_LICENSE_TYPES_LIST below*/)	\
	X(ULD_STATUS,	"LICSTATUS"		/*license status - whether all OK or user intervention required*/)	\
//UNG_LICENSE_DATA_LIST

#define X(e,s) e,
enum UNGLicenseDataName
{
	UNG_LICENSE_DATA_LIST
	UNG_LICENSE_DATA_MAX
};
#undef X

extern LPCSTR szLicenseDataName [];

/* class - license class defines properties of license, e.g. whether it's OK to register via command line */
typedef enum ELicenseClass {
	ELC_plain,
	ELC_oem,
	ELC_site,
} ELicenseClass;

#define UNG_LICENSE_TYPES_LIST \
	/* enum code,		code value,					class		description */ \
	X(ULT_SINGLE01DEV,	"Single1Device",			ELC_plain,	"Single License [1 shared USB device]")    \
	X(ULT_SINGLE02DEV,	"Single2Device",			ELC_plain,	"Single License [2 shared USB devices]")   \
	X(ULT_SINGLE10DEV,	"Single10Device",			ELC_plain,	"Single License [10 shared USB devices]")  \
	X(ULT_SINGLE_ALL,	"SingleAllDevice",			ELC_plain,	"Single License [Unlimited USB devices]")  \
	X(ULT_COMPANY1DEV,	"Company1dev",				ELC_site,	"Company License [1 shared USB device]")   \
	X(ULT_COMPANYUNLIM,	"CompanyUnlimited",			ELC_site,	"Company License [Unlimited USB devices]") \
	X(ULT_1COMPANYALL,	"SingleCompanyAllDevice",	ELC_plain,	"Company License [1 Computer, Unlimited USB  Devices]") \
	X(ULT_OEM,			"oem",						ELC_oem,	"OEM")                                     \
	X(ULT_SINGLEDEVEL,	"SingleDeveloper",			ELC_plain,	"Single Developer License for GUI")        \
	X(ULT_SUPPORT,		"Support",					ELC_plain,	"Support Operator License")                \
	X(ULT_DEMO,			"Demo",						ELC_plain,	"Demo")                                    \
	X(ULT_DEMO_10DEV,	"Demo10dev",				ELC_plain,	"Demo [10 shared USB devices]")				\
	X(ULT_DEMO_20DEV,	"Demo20dev",				ELC_plain,	"Demo [20 shared USB devices]")				\
	X(ULT_DEMO_UDEV,	"DemoUdev",					ELC_plain,	"Demo [Unlimited USB devices]")				\
	/* from activator */	\
	X(ULT_TIME_LIMITED,	"TimeLimited",				ELC_plain,	"Time-limited license")						\
// UNG_LICENSE_TYPES_LIST

#define X(e,v,k,d) e,
enum UNGLicenseType {
	UNG_LICENSE_TYPES_LIST
	UNG_LICENSE_TYPES_MAX
};
#undef X

extern LPCTSTR szLicenseType [];
extern LPCTSTR szLicenseDescription [];

#define UNG_LICENSE_ACTIONS		\
	X(ULA_ACTIVATE,			"ACTIVATE")			\
	X(ULA_OFFLINEANSWER,	"OFFLINEANSWER")	\
	X(ULA_TYPE,				"TYPE")				\
// UNG_LICENSE_ACTIONS

#define X(e,s) e,
enum UNGLicenseAction { UNG_LICENSE_ACTIONS };
#undef X

#define UNG_COMPRESSION_TYPES_LIST								\
	X(UCT_COMPRESSION_NONE,	0,	"no")							\
	X(UCT_COMPRESSION_ZLIB,	1,	"best size" /* zlib: best size, default */)	\
	X(UCT_COMPRESSION_LZ4,	2,	"best speed" /* lz4: best speed */)			\
// UNG_COMPRESSION_TYPES_LIST

#define X(e,n,s) e = n,
typedef enum UngCompressionType
{
	UNG_COMPRESSION_TYPES_LIST
	UNG_COMPRESSION_TYPES_MAX,

	UCT_COMPRESSION_DEFAULT = UCT_COMPRESSION_ZLIB,
	UCT_COMPRESSION_BEST_SIZE = UCT_COMPRESSION_ZLIB,
	UCT_COMPRESSION_BEST_SPEED = UCT_COMPRESSION_LZ4,
} UngCompressionType;
#undef X

extern const LPCTSTR szUngCompressionType [];

} // namespace Consts

#endif
