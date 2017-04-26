/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

   BaseClass.h

Environment:

    Kernel mode

--*/
#ifndef Base_3C44DB95_4E71_4F48_B227_8E6DDEF0EA06
#define Base_3C44DB95_4E71_4F48_B227_8E6DDEF0EA06

#include <stdio.h>
#include <stdarg.h>

extern "C" 
{
#pragma pack(push, 8)
#if _WIN32_WINNT==0x0500
    #define _WIN2K_COMPAT_SLIST_USAGE
#endif
#include "ntifs.h"
#include "Ntstrsafe.h"
#include "Ntddstor.h"
#include <ntddk.h>

#pragma pack(pop)
#include <minmax.h>
};

#include "RefCounter.h"
#include "intrusive_ptr.hpp"

class CBaseClass : public Common::CRefCounter
{
public:
	CBaseClass () 
	{
		m_CurrentDebugLevel = m_DefaultDebugLevel;
		SetDebugName ("CDebugPrint");
	}
	virtual ~CBaseClass () {}
	NEW_ALLOC_DEFINITION ('CBDE')

public:

        // main function
        void DebugPrint(LONG DebugLevel, PCHAR Format, ...)
		{
#if _FEATURE_ENABLE_DEBUGPRINT
			if((DebugLevel <= m_CurrentDebugLevel)) 
			{
				va_list Arguments; 
				CHAR Buffer[512];
				CHAR BufSpace[41];
				int		iSizeSpace;
				LARGE_INTEGER TickCount;
				va_start(Arguments, Format); 
				vsprintf (Buffer, Format, Arguments); 

				KeQueryTickCount (&TickCount);

				iSizeSpace = max (0, int(40 - strlen (m_szName)));
				RtlFillMemory (BufSpace, iSizeSpace, ' ');
				BufSpace [iSizeSpace] = 0;

				switch(DebugLevel) 
				{
				case DebugError: 
					{ 
						DbgPrint("%10d [%s - <ERROR> ]%s %s\n", TickCount.LowPart, m_szName, BufSpace, Buffer); 
						break; 
					} 
				case DebugWarning: 
					{ 
						DbgPrint("%10d [%s - <WARNING>]%s \t%s\n", TickCount.LowPart, m_szName, BufSpace, Buffer); 
						break; 
					} 
				case DebugTrace: 
					{ 
						DbgPrint("%10d [%s - <TRACE> ]%s \t\t%s\n", TickCount.LowPart, m_szName, BufSpace, Buffer); 
						break; 
					} 
				case DebugInfo: 
					{ 
						DbgPrint("%10d [%s - <INFO>  ]%s \t\t\t%s\n", TickCount.LowPart, m_szName, BufSpace, Buffer); 
						break; 
					} 
				case DebugSpecial:
					{ 
						DbgPrint("%10d [%s - <Special>  ]%s %s\n", TickCount.LowPart, m_szName, BufSpace, Buffer); 
						break; 
					} 
				} 
				va_end(Arguments); 
			} 
#endif
		}
        void DebugPrint2(LONG DebugLevel, PCHAR Format, ...)
		{
#if _FEATURE_ENABLE_DEBUGPRINT
			va_list Arguments; 
			CHAR Buffer[512];
			CHAR BufSpace[41];
			int		iSizeSpace;
			va_start(Arguments, Format); 
			vsprintf (Buffer, Format, Arguments); 

			iSizeSpace = max (0, int(40 - strlen (m_szName)));
			RtlFillMemory (BufSpace, iSizeSpace, ' ');
			BufSpace [iSizeSpace] = 0;

			switch(DebugLevel) 
			{
			case DebugError: 
				{ 
					DbgPrint("[%s - <ERROR> ]%s %s\n", m_szName, BufSpace, Buffer); 
					break; 
				} 
			case DebugWarning: 
				{ 
					DbgPrint("[%s - <WARNING>]%s \t%s\n", m_szName, BufSpace, Buffer); 
					break; 
				} 
			case DebugTrace: 
				{ 
					DbgPrint("[%s - <TRACE> ]%s \t\t%s\n", m_szName, BufSpace, Buffer); 
					break; 
				} 
			case DebugInfo: 
				{ 
					DbgPrint("[%s - <INFO>  ]%s \t\t\t%s\n", m_szName, BufSpace, Buffer); 
					break; 
				} 
			case DebugSpecial:
				{ 
					DbgPrint("[%s - <Special>  ]%s %s\n", m_szName, BufSpace, Buffer); 
					break; 
				} 
			} 
			va_end(Arguments); 
#endif
		}
        // Debug level ebun
        enum DebugLevel
        {
                DebugNone,
                DebugError,
                DebugWarning,
                DebugSpecial,
                DebugTrace,
                DebugInfo,
                DebugAll, 
        };

		// debug level
		static DebugLevel m_DefaultDebugLevel;

        // set DebugPrint Name
        void SetDebugName (PCHAR szName)
		{
#if _FEATURE_ENABLE_DEBUGPRINT
			strcpy (m_szName, szName);
#endif
		}

public:
		DebugLevel SetDebugLevel (DebugLevel Level)
		{
			DebugLevel Old = m_CurrentDebugLevel;
			m_CurrentDebugLevel = Level;
			return Old;
		}
		static void SetDefaultDebugLevel (DebugLevel Level) {m_DefaultDebugLevel = Level;}
		static DebugLevel GetDefaultDebugLevel (){return m_DefaultDebugLevel;}

private:
	DebugLevel m_CurrentDebugLevel;

#if _FEATURE_ENABLE_DEBUGPRINT
private:
        CHAR    m_szName [256];
#endif

};

#endif
