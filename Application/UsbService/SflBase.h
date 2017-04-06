/////////////////////////////////////////////////////////////////////////////
//
//  Windows Service Framework Library (SFL)
//
//
//  SflBase.h
//
//  2004 (c) Igor Vartanov
//
//  Version 1.0
//
//  Last update: Dec 17 2004
//
//  FIDOnet 2:5025/38.70 AKA 2:5025/16.26
//
//
//
//
/////////////////////////////////////////////////////////////////////////////

/****************************************************************************
History List
Version 1.0.4.6 (Dec 17 2004)
	Little fixes. Release Candidate.
Version 1.0.4.5 (Oct 23 2004)
    Control handling map macros are in compliance with VC++ 8 now.
Version 1.0.4.4 (Aug 19 2004)
    Now static construction is partially compatible with VC++6
Version 1.0.4.3 (Aug 08 2004)
    Service map for static construction now made sensitive to use service 
    clones.
Version 1.0.4.2 (Jul 12 2004)
    Service map now made dynamic and static construction sensitive.
Version 1.0.4.1 (Jun 27 2004)
    Some macros rearranged.
Version 1.0.4.0 (Jun 16 2004)
    Function thunk redesigned.
    All new-operators eliminated. Now SFL constructs service objects 
    statically.
Version 1.0.3.0 (May 16 2004)
    Cosmetic changes in CServiceBaseT<> for VC++6 compatibility.
    All macros rearranged.
Version 1.0.2.0 (May 15 2004)
    Extended control handling added.
Version 1.0.1.0 (May 08 2004)
    SFL namespace added. All macros refined.
Version 1.0.0.0 (Mar 04 2004)
    The very first one. 
****************************************************************************/

#ifndef IV_SFLBASE_H
#define IV_SFLBASE_H

#include "crtdbg.h"
#include "tchar.h"
#include "malloc.h"

#define _SFL_VER 0x0100

#ifndef _X86_
#pragma message("SFL requires ix86 platform.")
#endif

#ifdef _MSC_VER
#pragma comment( lib, "advapi32.lib" )
#endif

#if (_MSC_VER < 1300)
#pragma warning( push )
#pragma warning( disable: 4127 )
#endif

#if !defined (SFL_USE_DYNAMIC_CONSTRUCTION) && !defined (SFL_USE_STATIC_CONSTRUCTION)
#define SFL_USE_STATIC_CONSTRUCTION
#endif

#define SFLASSERT(x)   _ASSERTE(x)

#ifndef SFL_USE_NO_NAMESPACE
namespace SFL 
{
#endif // SFL_USE_NO_NAMESPACE

/////////////////////////////////////////////////////////////////////////////
//
//  class CServiceStatusObject
//
//  Version 1.2 (Dec 17 2004)
//
//
/////////////////////////////////////////////////////////////////////////////

class CServiceStatusObject: public SERVICE_STATUS
{
private:
    SERVICE_STATUS_HANDLE m_handle;

public:
    CServiceStatusObject(): m_handle(NULL) {}

    operator LPSERVICE_STATUS()
    {
        return static_cast<LPSERVICE_STATUS>(this);
    }

    SERVICE_STATUS& operator=( SERVICE_STATUS& status )
    {
    	*static_cast<LPSERVICE_STATUS>(this) = status;
        return *this;
    }

    SERVICE_STATUS_HANDLE GetHandle() const
    {
        return m_handle;
    }

    SERVICE_STATUS_HANDLE SetHandle( SERVICE_STATUS_HANDLE handle )
    {
        m_handle = handle;
        return m_handle;
    }
};


/////////////////////////////////////////////////////////////////////////////
//
//  class CServiceRoot
//
//  Version 1.3 (Dec 17 2004)
//
//
/////////////////////////////////////////////////////////////////////////////
#ifndef SFL_MAX_SERVICENAME_BUF_LEN
#define SFL_MAX_SERVICENAME_BUF_LEN  256
#endif


class CServiceRoot
{
private:
    LPTSTR m_pszBuffer;
protected:
    LPCTSTR  m_pszServiceName;
    CServiceStatusObject m_status;
private:
    CServiceRoot& operator=( const CServiceRoot& )   {}
    CServiceRoot( const CServiceRoot& ){}

	

protected:
    CServiceRoot( LPCTSTR pszServiceName ): 
         m_pszBuffer(NULL),
         m_pszServiceName(NULL)
    {
        m_pszServiceName = pszServiceName;
    }


    CServiceRoot( UINT nServiceNameId ): 
         m_pszBuffer(NULL),
         m_pszServiceName(NULL)
    {
        m_pszBuffer = (LPTSTR) ::LocalAlloc( LPTR, SFL_MAX_SERVICENAME_BUF_LEN * sizeof(TCHAR) );
        if( m_pszBuffer && ::LoadString( GetModuleHandle(NULL), 
                                         nServiceNameId, 
                                         m_pszBuffer, 
                                         SFL_MAX_SERVICENAME_BUF_LEN ) )
            m_pszServiceName = m_pszBuffer;
    }

public:
    virtual ~CServiceRoot()
    {
        if (m_pszBuffer)
            ::LocalFree(m_pszBuffer);
    }

#ifdef SFL_USE_STATIC_CONSTRUCTION
    template<class T>
    static CServiceRoot* Construct(T* pDummy, LPCTSTR pszName)
    {
        pDummy; // prevent warning C4100
        static T theService( pszName );
        return static_cast<CServiceRoot*>(&theService);
    }

    template<class T>
    static CServiceRoot* Construct(T* pDummy, UINT nNameId)
    {
        pDummy; // prevent warning C4100
        static T theService( nNameId );
        return static_cast<CServiceRoot*>(&theService);
    }
#endif // SFL_USE_STATIC_CONSTRUCTION

    DWORD GetCurrentState() const
    {
        return m_status.dwCurrentState;
    }

    LPCTSTR GetServiceName() const
    {
        return m_pszServiceName;
    }


    void SetServiceType( DWORD dwType )
    {
        m_status.dwServiceType = dwType;
        if (IsInteractive())
        {
            m_status.dwServiceType |= SERVICE_INTERACTIVE_PROCESS;
        }
    }

    DWORD GetControlsAccepted() const
    {
        return m_status.dwControlsAccepted;
    }

    void SetControlsAccepted( DWORD dwAccept )
    {
        m_status.dwControlsAccepted = dwAccept;
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Declare main interface function as abstract
    // We call they in the _tmain routine.
    // 
    virtual LPSERVICE_MAIN_FUNCTION GetServiceMain() const  = 0;
    virtual SERVICE_STATUS_HANDLE   RegisterHandler() const = 0;
    
    virtual bool IsInteractive() const
    {
        return false;
    }

    //
    //////////////////////////////////////////////////////////////////////

protected:  
    BOOL SetServiceStatus( LPSERVICE_STATUS pStatus = NULL )
    {
        SFLASSERT( m_status.GetHandle() || !"SFL: Handler not registered yet" );
        
        if( pStatus )
            m_status = *pStatus;

        switch( m_status.dwCurrentState )
        {
        case SERVICE_START_PENDING:
        case SERVICE_STOP_PENDING:
        case SERVICE_CONTINUE_PENDING:
        case SERVICE_PAUSE_PENDING:
            break;

        default:
            m_status.dwCheckPoint = m_status.dwWaitHint = 0;
            break;
        }

        if( ( ERROR_SUCCESS != m_status.dwServiceSpecificExitCode ) &&
            ( ERROR_SERVICE_SPECIFIC_ERROR != m_status.dwWin32ExitCode ) )
            m_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        
        return ::SetServiceStatus( m_status.GetHandle(), m_status );
    }

    BOOL SetServiceStatus( DWORD dwState, DWORD dwWin32ExitCode = ERROR_SUCCESS )
    {
        m_status.dwCurrentState  = dwState;
        m_status.dwWin32ExitCode = dwWin32ExitCode;
        m_status.dwServiceSpecificExitCode = 0;
        
        return SetServiceStatus();
    }

    BOOL SetServiceStatusSpecific( DWORD dwState, DWORD dwSpecificExitCode )
    {
        if( ERROR_SUCCESS == dwSpecificExitCode )
            return SetServiceStatus( dwState );

        m_status.dwServiceSpecificExitCode = dwSpecificExitCode;        
        return SetServiceStatus( dwState, ERROR_SERVICE_SPECIFIC_ERROR );
    }

    BOOL CheckPoint( DWORD dwWaitHint, DWORD dwCheckPoint = DWORD(-1) )
    {
        switch( m_status.dwCurrentState )
        {
        case SERVICE_START_PENDING:
        case SERVICE_STOP_PENDING:
        case SERVICE_CONTINUE_PENDING:
        case SERVICE_PAUSE_PENDING:
            {
                if( DWORD(-1) == dwCheckPoint )
                    m_status.dwCheckPoint++;
                else
                    m_status.dwCheckPoint = dwCheckPoint;

                m_status.dwWaitHint = dwWaitHint;
            }
            break;

        default:
            SFLASSERT( !"SFL: Checking the point in nonpending state" );
            m_status.dwCheckPoint = m_status.dwWaitHint = 0;
            break;
        }
        
        return SetServiceStatus();
    }
};


/////////////////////////////////////////////////////////////////////////////
//
//  class CServiceBaseT<T,t_dwControlAccepted>
//
//  Version 1.3 (Jun 16 2004)
//
//
/////////////////////////////////////////////////////////////////////////////

template< typename TService, DWORD t_dwControlsAccepted >
class CServiceBaseT: public CServiceRoot
{
public:
    typedef CServiceBaseT<TService, t_dwControlsAccepted> CServiceBaseClass;


protected:
    CServiceBaseT( LPCTSTR pszServiceName ): 
        CServiceRoot( pszServiceName )
    {
        m_spThisPointer = static_cast<TService*>(this);
        SFLASSERT( m_spThisPointer );
    }
    void ServiceMain( DWORD dwArgc, LPTSTR* lpszArgv )
    {
        TService* pThis = static_cast<TService*>(this);
        SFLASSERT( pThis );

        BOOL  bRes;
        DWORD dwSpecific = 0;
        
        SetControlsAccepted( t_dwControlsAccepted );

        m_status.SetHandle( pThis->RegisterHandler() );
        SFLASSERT( m_status.GetHandle() || !"SFL: Handler registration failed" );
        
		//_ASSERT_EXPR(FALSE, _T("ServiceMain() breakpoint!"));

		SetServiceStatus( SERVICE_START_PENDING );
        
        bRes = pThis->InitInstance( dwArgc, lpszArgv, dwSpecific );
        if( !bRes )
        {
            SetServiceStatusSpecific( SERVICE_STOPPED, dwSpecific );
            return;
        }

        bRes = SetServiceStatus( SERVICE_RUNNING );
        //SFLASSERT( bRes || !"SFL: Failed to set RUNNING status" );
    }

    LPSERVICE_MAIN_FUNCTION GetServiceMain() const
    {
        return (LPSERVICE_MAIN_FUNCTION)_ServiceMain;
    }

    BOOL InitInstance( DWORD dwArgc, LPTSTR* lpszArgv, DWORD& dwSpecific )
    {
        dwArgc; lpszArgv; dwSpecific; // prevent warning C4100

        return TRUE;
    }

protected:
    static TService* m_spThisPointer;

private:
    static void WINAPI _ServiceMain( DWORD dwArgc, LPTSTR* lpszArgv )
    {
        m_spThisPointer->ServiceMain( dwArgc, lpszArgv );
    }
};

template< typename TService, DWORD t_dwControlsAccepted >
TService*
CServiceBaseT<TService, t_dwControlsAccepted>::m_spThisPointer = NULL;

/////////////////////////////////////////////////////////////////////////////
//
//  class CServiceAppT<T>
//
//  Version 1.3 (Jun 16 2004)
//
//
/////////////////////////////////////////////////////////////////////////////
template <class T>
class CServiceAppT
{
private:
    int m_nServiceCount;
    CServiceRoot** m_pServices;

protected:
    typedef CServiceAppT<T> CAppBaseClass;

    CServiceAppT(): m_nServiceCount(0), m_pServices(NULL)
    {
    }

    BOOL Run( LPSERVICE_TABLE_ENTRY pTable )
    {
        T* pThis = static_cast<T*>(this);

        if( !pThis->InitApp() )
            return FALSE;

        BOOL bRes = ::StartServiceCtrlDispatcher( pTable );
        SFLASSERT( bRes || !"SFL: StartServiceCtrlDispatcher() failed" );

        pThis->ExitApp();

        return bRes;
    }

public:
    int GetServiceCount() const
    {
        return m_nServiceCount;
    }

    const CServiceRoot** GetServices() const
    {
        return m_pServices;
    }

    BOOL InitApp()
    {
        return TRUE;
    }

    void ExitApp()
    {
    }

    BOOL PreMain( int argc, LPTSTR* argv )
    {
        return TRUE;
    }

    void PostMain()
    {
    }


    int Main( int argc, LPTSTR* argv, CServiceRoot** pMap )
    {
        argc; argv; // prevent warning C4100
        LPSERVICE_TABLE_ENTRY pTable = NULL;
        UINT nTableSize = 0;
        BOOL bRes       = 0;

        // get map size
        while( pMap[nTableSize] )
            nTableSize++;
        m_pServices     = pMap;
        m_nServiceCount = nTableSize;

        UINT nSize = sizeof(SERVICE_TABLE_ENTRY) * (nTableSize + 1);
        __try
        {
            pTable = (LPSERVICE_TABLE_ENTRY)_alloca( nSize );
            ::ZeroMemory( pTable, nSize );

            // fill service table with service map objects data
            for( UINT i = 0; i < nTableSize; i++ )
            {
                SFLASSERT( pMap[i]->GetServiceName() || !"SFL: Service NULL name not allowed" );
                pTable[i].lpServiceName = (LPTSTR)pMap[i]->GetServiceName();
                pTable[i].lpServiceProc = pMap[i]->GetServiceMain();
                pMap[i]->SetServiceType( (nTableSize > 1) ? SERVICE_WIN32_SHARE_PROCESS : SERVICE_WIN32_OWN_PROCESS );
            }

            T* pThis = static_cast<T*>(this);
            bRes = pThis->Run( pTable );
        }
        __except( STATUS_STACK_OVERFLOW == GetExceptionCode() )
        {
            SFLASSERT(!"SFL: Stack overflow in CServiceAppT<>::Main()");
        }
        return !bRes;
    }
};

#if (_MSC_VER < 1300)
#pragma warning( pop )
#endif // _MSC_VER < 1300

#ifndef SFL_USE_NO_NAMESPACE

} // namespace SFL

#ifndef SFL_USE_NAMESPACE_EXPLICITLY
using namespace SFL;
#endif // SFL_USE_NAMESPACE_EXPLICITLY

typedef SFL::CServiceRoot  CSflServiceRoot;

#else // SFL_USE_NO_NAMESPACE not defined

typedef CServiceRoot  CSflServiceRoot;

#endif // SFL_USE_NO_NAMESPACE

/////////////////////////////////////////////////////////////////////////////
//
//  The set of macros
//
//  Version 1.8 (Oct 23 2004)
//
/////////////////////////////////////////////////////////////////////////////

#ifndef ARRAYSIZE
#define ARRAYSIZE(x)  (sizeof(x)/sizeof(x[0]))
#endif

#define SFL_DECLARE_SERVICEAPP(T) class T; const T* SflGetServiceApp(void); \


//////////// SERVICE MAP ////////////////////////////////////////////////////
//

#define SFL_BEGIN_SERVICE_MAP(T)                                            \
    const T* SflGetServiceApp(void) { static T theApp; return &theApp; }    \
    int _tmain( int argc, LPTSTR* argv )                                    \
    {                                                                       \
        T* pApp = const_cast<T*>( SflGetServiceApp() );                     \
        CSflServiceRoot* pServiceMap[] = {                                  \


#ifdef SFL_USE_STATIC_CONSTRUCTION //////////////////////////////////////////
#define SFL_DECLARE_SERVICECLASS_FRIENDS()                                  \
    friend class CServiceBaseClass;                                         \
    friend class CServiceRoot;                                              \

#define SFL_SERVICE_ENTRY( TService, resID )                                \
    TService::Construct( (TService*)NULL, resID ),                          \

#define SFL_SERVICE_ENTRY( TService, name )                                 \
    TService::Construct( (TService*)NULL, name ),                           \

#define SFL_END_SERVICE_MAP()                                               \
            NULL                                                            \
        };                                                                  \
        int retMain = -1;                                                   \
        if( pApp->PreMain( argc, argv ) )                                   \
            retMain = pApp->Main( argc, argv, pServiceMap );                \
        pApp->PostMain();                                                   \
        return retMain;                                                     \
    }

#else // SFL_USE_STATIC_CONSTRUCTION not defined ////////////////////////////
      // so it should be dynamic construction used
#define SFL_DECLARE_SERVICECLASS_FRIENDS()                                  \
    friend class CServiceBaseClass;                                         \
    friend int _tmain( int, LPTSTR* );                                      \

#define SFL_SERVICE_ENTRY( TService, resID )                                \
    static_cast<CSflServiceRoot*>(new TService( MAKEINTRESOURCE(resID) )),  \

#define SFL_SERVICE_ENTRY_( TService, name )                                \
    static_cast<CSflServiceRoot*>(new TService( TEXT(name) )),              \

#define SFL_SERVICE_ENTRY_CLONE( TService, cloneID, name )   SFL_SERVICE_ENTRY_( TService, name )

#define SFL_END_SERVICE_MAP()                                               \
            NULL                                                            \
        };                                                                  \
        int index, retMain = -1;                                            \
        bool bOK = true;                                                    \
        for( index = 0; index < (ARRAYSIZE(pServiceMap) - 1); index++ )     \
        {                                                                   \
           if( NULL == pServiceMap[index] )                                 \
           {                                                                \
               bOK = false;                                                 \
               break;                                                       \
           }                                                                \
        }                                                                   \
        if( bOK )                                                           \
        {                                                                   \
            if( pApp->PreMain( argc, argv ) )                               \
                retMain = pApp->Main( argc, argv, pServiceMap );            \
            pApp->PostMain();                                               \
        }                                                                   \
        for( index = 0; index < (ARRAYSIZE(pServiceMap) - 1); index++ )     \
        { delete pServiceMap[index]; }                                      \
        return retMain;                                                     \
    }

#endif // SFL_USE_STATIC_CONSTRUCTION ///////////////////////////////////////


//////////// CONTROL HANDLING MAP ///////////////////////////////////////////
//

#define SFL_BEGIN_CONTROL_MAP(T)                                            \
public:                                                                     \
    SERVICE_STATUS_HANDLE RegisterHandler() const                           \
    {                                                                       \
        return ::RegisterServiceCtrlHandler(                                \
                    GetServiceName(),                                       \
                    reinterpret_cast<LPHANDLER_FUNCTION>(_Handler) );    \
    }                                                                       \
private:                                                                    \
    static void WINAPI _Handler( DWORD dwControl )                          \
    {                                                                       \
        m_spThisPointer->Handler( dwControl );                              \
    }                                                                       \
    void T::Handler( DWORD dwControl )                                      \
    {                                                                       \
        BOOL    bHandled       = FALSE;                                     \
        DWORD   dwState        = m_status.dwCurrentState;                   \
        DWORD   dwWin32Err     = 0;                                         \
        DWORD   dwSpecificErr  = 0;                                         \
typedef DWORD (T::*t_handler)(DWORD&, DWORD&, BOOL&);                       \
typedef DWORD (T::*t_handler_range)(DWORD, DWORD&, DWORD&, BOOL&);          \
        do {                                                                \

#define SFL_END_CONTROL_MAP()                                               \
        } while(0);                                                         \
        if( bHandled )                                                      \
        {   m_status.dwCurrentState = dwState;                              \
            m_status.dwWin32ExitCode = dwWin32Err;                          \
            m_status.dwServiceSpecificExitCode = dwSpecificErr;             \
            SetServiceStatus();                                             \
        }                                                                   \
    }                                                                       \

#define SFL_HANDLE_CONTROL( code, handler )                                 \
        if( code == dwControl )                                             \
        {																	\
            dwState = handler( dwWin32Err, dwSpecificErr,					\
                                           bHandled ); break;               \
        }

#define SFL_HANDLE_CONTROL_RANGE( codeMin, codeMax, handler )               \
        if( (codeMin <= dwControl) && (codeMax >= dwControl ) )             \
        {   t_handler_range pfnHandler =                                    \
                static_cast<t_handler_range>(handler);                      \
            dwState = (this->*pfnHandler)( dwControl, dwWin32Err,           \
                                           dwSpecificErr, bHandled );       \
            break;                                                          \
        }

#define SFL_HANDLE_CONTROL_CONTINUE()    SFL_HANDLE_CONTROL( SERVICE_CONTROL_CONTINUE, OnContinue )
#define SFL_HANDLE_CONTROL_PAUSE()       SFL_HANDLE_CONTROL( SERVICE_CONTROL_PAUSE, OnPause )
#define SFL_HANDLE_CONTROL_STOP()        SFL_HANDLE_CONTROL( SERVICE_CONTROL_STOP, OnStop )
#define SFL_HANDLE_CONTROL_SHUTDOWN()    SFL_HANDLE_CONTROL( SERVICE_CONTROL_SHUTDOWN, OnShutdown )
#define SFL_HANDLE_CONTROL_INTERROGATE() SFL_HANDLE_CONTROL( SERVICE_CONTROL_INTERROGATE, OnInterrogate )


//////////// EXTENDED CONTROL HANDLING MAP //////////////////////////////////
//

#define SFL_BEGIN_CONTROL_MAP_EX(T)                                         \
public:                                                                     \
    SERVICE_STATUS_HANDLE RegisterHandler() const                           \
    {                                                                       \
        return ::RegisterServiceCtrlHandlerEx(                              \
                    GetServiceName(),                                       \
                    reinterpret_cast<LPHANDLER_FUNCTION_EX>(_HandlerEx),    \
                    (LPVOID)this );                                         \
    }                                                                       \
private:                                                                    \
    static DWORD WINAPI _HandlerEx(                                         \
        DWORD dwControl,                                                    \
        DWORD dwEventType,                                                  \
        LPVOID lpEventData,                                                 \
        LPVOID lpContext                                                    \
    )                                                                       \
    {                                                                       \
        T* pService = reinterpret_cast<T*>(lpContext);                      \
                                                                            \
        return pService->HandlerEx( dwControl,                              \
                                    dwEventType,                            \
                                    lpEventData,                            \
                                    lpContext );                            \
    }                                                                       \
    DWORD T::HandlerEx( DWORD  dwControl,   DWORD  dwEventType,             \
                        LPVOID lpEventData, LPVOID lpContext )              \
    {                                                                       \
        BOOL    bHandled       = FALSE;                                     \
        DWORD   dwRet          = NO_ERROR;                                  \
        DWORD   dwState        = m_status.dwCurrentState;                   \
        DWORD   dwWin32Err     = 0;                                         \
        DWORD   dwSpecificErr  = 0;                                         \
typedef DWORD (T::*t_handler)(DWORD&, DWORD&, BOOL&);                       \
typedef DWORD (T::*t_handler_range)(DWORD, DWORD&, DWORD&, BOOL&);          \
typedef DWORD (T::*t_handler_ex)(DWORD&, DWORD&, DWORD&, BOOL&, DWORD, LPVOID, LPVOID); \
typedef DWORD (T::*t_handler_range_ex)(DWORD, DWORD&, DWORD&, DWORD&, BOOL&, DWORD, LPVOID, LPVOID); \
        do {                                                                \

#define SFL_END_CONTROL_MAP_EX()                                            \
        } while(0);                                                         \
        if( bHandled )                                                      \
        {   m_status.dwCurrentState = dwState;                              \
            m_status.dwWin32ExitCode = dwWin32Err;                          \
            m_status.dwServiceSpecificExitCode = dwSpecificErr;             \
            SetServiceStatus();                                             \
            return dwRet;                                                   \
        }                                                                   \
        return ERROR_CALL_NOT_IMPLEMENTED;                                  \
    }                                                                       

#define SFL_HANDLE_CONTROL_EX( code, handler )                              \
        if( code == dwControl )                                             \
        {   t_handler_ex pfnHandler = static_cast<t_handler_ex>(handler);   \
            dwRet = (this->*pfnHandler)( dwState, dwWin32Err, dwSpecificErr,\
                                         bHandled, dwEventType,             \
                                         lpEventData, lpContext ); break; }

#define SFL_HANDLE_CONTROL_RANGE_EX( codeMin, codeMax, handler )            \
        if( (codeMin <= dwControl) && (codeMax >= dwControl ) )             \
        {   t_handler_range_ex pfnHandler =                                 \
                static_cast<t_handler_range_ex>(handler);                   \
            dwRet = (this->*pfnHandler)( dwControl, dwState, dwWin32Err,    \
                                         dwSpecificErr, bHandled,           \
                                         dwEventType, lpEventData,          \
                                         lpContext ); break; }

#endif // IV_SFLBASE_H
