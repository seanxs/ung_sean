/*++

Copyright (c) 2000-2015 ELTIMA Software All Rights Reserved

Module Name:

    Registry.h

Abstract:

	Class for work with the registry

Environment:

    User mode

--*/

#ifndef REGISTRY_H__FA2B7E38
#define REGISTRY_H__FA2B7E38

//-------------------------------- VC --------------------------------------
#ifdef _MSC_VER
#pragma warning (disable:4786)
#pragma warning (disable:4503)
#endif
//--------------------------------------------------------------------------

#include <windows.h>
#include <string>
#include <map>
#include <vector>
#include <map>
#include <exception>
#include <assert.h>
#include <memory>
#include <new>

#ifdef _UNICODE
typedef std::wstring	CRegString;
#else
typedef std::string		CRegString;
#endif


template<class T> class CWinAllocator
{
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;
	
	pointer address(reference r) const
	{
		return &r;
	}
	const_pointer address(const_reference r) const
	{
		return &r;
	}
	pointer allocate(size_type n, const void *)
	{
		void *p=::HeapAlloc(::GetProcessHeap(),0,n);
		if(!p)
			throw std::bad_alloc();
		return (pointer)p;
	}
	void deallocate(void *p, size_type)
	{
		if(p)
			::HeapFree(::GetProcessHeap(),0,(void*)p);
	}
	void construct(pointer p, const T& v)
	{
		new(p) T(v);
	}
	void destroy(pointer p)
	{
		p->~T();
	}
	size_type max_size() const
	{
		size_type n=(size_type)(-1)/sizeof(T);
		return (0 < n ? n : 1);
	}
};

template <class T, class _U> inline bool operator==(const CWinAllocator<T>&, const CWinAllocator<_U>&)
{
	return true;
}

template <class T, class _U> inline bool operator!=(const CWinAllocator<T>&, const CWinAllocator<_U>&)
{
	return false;
}

template <class _AL> class CBasicBuffer // _AL - allocator
{
	_AL m_Allocator;

	void Init()
	{
		m_Buffer=NULL;
		m_BufferSize=0;
		m_DataSize=0;
		m_UserObject=NULL;
	}
	void SetOptionalTerminatedNull()
	{
		IncreaseBufferSize(m_DataSize+1);
		m_Buffer[m_DataSize]='\0';
	}
	void InitBySmallBuffer()
	{
		const size_t initSize=50;
		Init();
		m_Buffer=m_Allocator.allocate(sizeof(char)*initSize,NULL);
		if(!m_Buffer) // For VC6
			throw std::bad_alloc();
		m_BufferSize=initSize;
		m_DataSize=0;
		m_UserObject=NULL;
	}

public:

	char*  m_Buffer;
	void*  m_UserObject;
	size_t m_BufferSize;
	size_t m_DataSize;
	
	CBasicBuffer(size_t size=500, void* userObject=NULL)
	{
		assert(size >= 0);
		/*
		if(size > 0)
		{
			m_Buffer=m_Allocator.allocate(sizeof(char)*size,NULL);
			if(!m_Buffer) // For VC6
				throw std::bad_alloc();
		}
		else
			m_Buffer=NULL;
		*/
		m_Buffer=m_Allocator.allocate(sizeof(char)*size,NULL);
		if(!m_Buffer) // For VC6
			throw std::bad_alloc();
		m_BufferSize=size;
		m_DataSize=0;
		m_UserObject=userObject;
	}
	CBasicBuffer(CRegString s)
	{
		size_t size=s.length()+1;
		m_Buffer=m_Allocator.allocate(sizeof(char)*size,NULL);
		if(!m_Buffer) // For VC6
			throw std::bad_alloc();
		m_BufferSize=size;
		m_DataSize=size;
		m_UserObject=NULL;
		strcpy(m_Buffer,s.c_str());
	}
	CBasicBuffer(const char *s)
	{
		size_t size=strlen(s)+1;
		m_Buffer=m_Allocator.allocate(sizeof(char)*size,NULL);
		if(!m_Buffer) // For VC6
			throw std::bad_alloc();
		m_BufferSize=size;
		m_DataSize=size;
		m_UserObject=NULL;
		strcpy(m_Buffer,s);
	}
	CBasicBuffer(CBasicBuffer& buffer)
	{
		Init();
		(*this)=buffer;
	}

	~CBasicBuffer()
	{
		Release();
	}
	CBasicBuffer& operator=(CBasicBuffer& buffer)
	{
		if(&buffer != this)
		{
			Release();
			m_Allocator=buffer.m_Allocator;
			m_Buffer=buffer.m_Buffer;
			m_BufferSize=buffer.m_BufferSize;
			m_DataSize=buffer.m_DataSize;
			m_UserObject=buffer.m_UserObject;

			buffer.Init();
		}
		return *this;
	}
	void IncreaseBufferSize(size_t newSize)
	{
		if(newSize <= m_BufferSize)
			return;
		//char* newBuffer=new char[newSize];
		char* newBuffer=m_Allocator.allocate(sizeof(char)*newSize,NULL);
		if(!newBuffer) // For VC6
			throw std::bad_alloc("Don't allocate memory for CBasicBuffer::m_Buffer.");
		memcpy(newBuffer,m_Buffer,m_DataSize);
		//delete m_Buffer;
		m_Allocator.deallocate(m_Buffer,m_BufferSize);
		m_Buffer=newBuffer;
		m_BufferSize=newSize;
	}
	void Release()
	{
		//delete[] m_Buffer;
		m_Allocator.deallocate(m_Buffer,m_BufferSize);
		Init();
	}
	void Append(CRegString& s)
	{
		Append(s.c_str(),s.length());
		SetOptionalTerminatedNull();
	}
	void Append(const char *s)
	{
		Append(s,strlen(s));
		SetOptionalTerminatedNull();
	}
	void Append(const char *buffer, size_t size)
	{
		size_t newSize=m_DataSize+size;
		IncreaseBufferSize(newSize+1); // +1 for optional terminated null
		memcpy(m_Buffer+m_DataSize,buffer,size);
		m_DataSize=newSize;
		SetOptionalTerminatedNull();
	}
	CBasicBuffer<_AL> GetCopy() const
	{
		// +1 for possible '\0'.
		CBasicBuffer<_AL> copy(m_DataSize+1,m_UserObject);
		memcpy(copy.m_Buffer,m_Buffer,m_DataSize);
		copy.m_DataSize=m_DataSize;
		copy.SetOptionalTerminatedNull();
		return copy;
	}
	char& operator[](size_t index)
	{
		return m_Buffer[index];
	}
	char operator[](size_t index) const
	{
		return m_Buffer[index];
	}
	char& At(size_t index)
	{
		if(index >= m_BufferSize)
			throw std::out_of_range("CBasicBuffer::at()");
		return m_Buffer[index];
	}
	char At(size_t index) const
	{
		if(index >= m_BufferSize)
			throw std::out_of_range("CBasicBuffer::at() const");
		return m_Buffer[index];
	}
	void CopyData(const CBasicBuffer<_AL>& buffer)
	{
		m_DataSize=0;
		Append(buffer.m_Buffer,buffer.m_DataSize);
		SetOptionalTerminatedNull();
	}
};

// This module is safe for transferring data between modules(exe and dll's).
typedef CBasicBuffer<CWinAllocator<char> > CSafeBuffer;

typedef CBasicBuffer<std::allocator<char> > CStdBuffer;

void testCBasicBuffer();



class CRegistryException : public std::exception
{
public:
	CRegistryException();
	CRegistryException(const char *message);
	CRegistryException(const CRegistryException& rhs);
	CRegistryException& operator=(const CRegistryException& rhs);
	virtual ~CRegistryException();
};

class CRegistry
{

public:

	CRegistry();
	CRegistry(HKEY rootKey, CRegString subKey, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS, LPSECURITY_ATTRIBUTES pSecurityAttributes=NULL);
	CRegistry(CRegString key, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS, LPSECURITY_ATTRIBUTES pSecurityAttributes=NULL);
	virtual ~CRegistry();

	
	HKEY GetRootKey();
	CRegString GetSubKey();

	bool ExistsValue(CRegString name);

	DWORD GetUInt(CRegString name);
	void  SetUInt(CRegString name, DWORD value);
	DWORD GetUIntMaybe(CRegString name, DWORD defaultValue);

	bool GetBool(CRegString name);
	void SetBool(CRegString name, bool value);
    bool GetBoolMaybe(CRegString name, bool defaultValue);

	CRegString GetString(CRegString name);
	void SetString(CRegString name, CRegString value);
	CRegString GetStringMaybe(CRegString name, CRegString defaultValue);

	CRegString GetExpandString(CRegString name);
	void SetExpandString(CRegString name, CRegString value);
	
	std::vector<CRegString> GetStringArray(CRegString name);
	void SetStringArray(CRegString name, std::vector<CRegString> stringArray);
	void SetStringArray(CRegString name, const char* stringArray);

	void SetBinaryData(CRegString name, const char* buffer, size_t size);
	void SetBinaryData(CRegString name, const CStdBuffer &buffer);
	CStdBuffer GetBinaryData(CRegString name);
	
	void SetUnknownData(CRegString name, const char* buffer, size_t size);
	void SetUnknownData(CRegString name, const CStdBuffer &buffer);
	CStdBuffer GetUnknownData(CRegString name);

	void DeleteValue(CRegString name);
	
	void DeleteKey();
	static void DeleteKey(HKEY hRootKey, CRegString subKey);
	
	int GetSubKeyCount(); //RegQueryInfoKey
	int GetValueCount();
	std::vector<CRegString> GetSubKeys();
	std::vector<CRegString> GetValueNames();
	//?? GetInfo();


	HKEY Detach();
	void Attach(HKEY hKey);
	
	HKEY GetHKEY();
	
	bool IsOpened();
	bool IsCreated();
	void CreateKey(HKEY rootKey, CRegString subKey, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS, LPSECURITY_ATTRIBUTES pSecurityAttributes=NULL);
	void CreateKey(CRegString key, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS, LPSECURITY_ATTRIBUTES pSecurityAttributes=NULL);
	
	void OpenKey(HKEY hKey, CRegString subKey, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS);
	void OpenKey(CRegString key, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS);
	
	static bool ExistsKey(HKEY hKey, CRegString subKey, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS);
	static bool ExistsKey(CRegString key, DWORD options=REG_OPTION_NON_VOLATILE, REGSAM samDesired=KEY_ALL_ACCESS);
	
	void Flush();
	void CloseKey(bool flush=false);
	

private:
	
	typedef std::pair<CRegString,HKEY> RootKeyPair;
	
	std::map<CRegString,HKEY> m_RootKeys;

	HKEY m_hKey;
	HKEY m_hRootKey;
	CRegString m_SubKey;
	
	bool m_Opened;
	bool m_Created;

	void Init();
	void InitRootKeysMap();
	HKEY GetRootKey(CRegString key);
	CRegString GetSubKey(CRegString key);
};

#endif //REGISTRY_H__FA2B7E38
