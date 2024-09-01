/*
 * dlfcn-win32
 * Copyright (c) 2007 Ramiro Polla
 * Copyright (c) 2015 Tiancheng "Timothy" Gu
 * Copyright (c) 2019 Pali Roh�r <pali.rohar@gmail.com>
 *
 * dlfcn-win32 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * dlfcn-win32 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlfcn-win32; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
 /* https://docs.microsoft.com/en-us/cpp/intrinsics/returnaddress */
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#else
 /* https://gcc.gnu.org/onlinedocs/gcc/Return-Address.html */
#ifndef _ReturnAddress
#define _ReturnAddress() (__builtin_extract_return_addr(__builtin_return_address(0)))
#endif
#endif

#include "dlfcn_win.h"

/* Note:
 * MSDN says these functions are not thread-safe. We make no efforts to have
 * any kind of thread safety.
 */

typedef struct local_object {
	HMODULE hModule;
	struct local_object* previous;
	struct local_object* next;
} local_object;

static local_object first_object;

/* These functions implement a double linked list for the local objects. */
static local_object* local_search(HMODULE hModule)
{
	local_object* pobject;

	if (hModule == NULL)
		return NULL;

	for (pobject = &first_object; pobject; pobject = pobject->next)
		if (pobject->hModule == hModule)
			return pobject;

	return NULL;
}

static BOOL local_add(HMODULE hModule)
{
	local_object* pobject;
	local_object* nobject;

	if (hModule == NULL)
		return TRUE;

	pobject = local_search(hModule);

	/* Do not add object again if it's already on the list */
	if (pobject)
		return TRUE;

	for (pobject = &first_object; pobject->next; pobject = pobject->next);

	nobject = (local_object*)malloc(sizeof(local_object));

	if (!nobject)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	pobject->next = nobject;
	nobject->next = NULL;
	nobject->previous = pobject;
	nobject->hModule = hModule;

	return TRUE;
}

static void local_rem(HMODULE hModule)
{
	local_object* pobject;

	if (hModule == NULL)
		return;

	pobject = local_search(hModule);

	if (!pobject)
		return;

	if (pobject->next)
		pobject->next->previous = pobject->previous;
	if (pobject->previous)
		pobject->previous->next = pobject->next;

	free(pobject);
}

/* POSIX says dlerror( ) doesn't have to be thread-safe, so we use one
 * static buffer.
 * MSDN says the buffer cannot be larger than 64K bytes, so we set it to
 * the limit.
 */
static char error_buffer[65535];
static char* current_error;
static char dlerror_buffer[65536];

static void save_err_str(const char* str)
{
	DWORD dwMessageId;
	DWORD ret;
	size_t pos, len;

	dwMessageId = GetLastError();

	if (dwMessageId == 0)
		return;

	len = strlen(str);
	if (len > sizeof(error_buffer) - 5)
		len = sizeof(error_buffer) - 5;

	/* Format error message to:
	 * "<argument to function that failed>": <Windows localized error message>
	  */
	pos = 0;
	error_buffer[pos++] = '"';
	memcpy(error_buffer + pos, str, len);
	pos += len;
	error_buffer[pos++] = '"';
	error_buffer[pos++] = ':';
	error_buffer[pos++] = ' ';

	ret = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwMessageId,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		error_buffer + pos, (DWORD)(sizeof(error_buffer) - pos), NULL);
	pos += ret;

	/* When FormatMessageA() fails it returns zero and does not touch buffer
	 * so add trailing null byte */
	if (ret == 0)
		error_buffer[pos] = '\0';

	if (pos > 1)
	{
		/* POSIX says the string must not have trailing <newline> */
		if (error_buffer[pos - 2] == '\r' && error_buffer[pos - 1] == '\n')
			error_buffer[pos - 2] = '\0';
	}

	current_error = error_buffer;
}

static void save_err_ptr_str(const void* ptr)
{
	char ptr_buf[19]; /* 0x<pointer> up to 64 bits. */
	save_err_str(ptr_buf);
}

/* Load Psapi.dll at runtime, this avoids linking caveat */
static BOOL MyEnumProcessModules(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded)
{
	static BOOL(WINAPI * EnumProcessModulesPtr)(HANDLE, HMODULE*, DWORD, LPDWORD);
	HMODULE psapi;

	if (!EnumProcessModulesPtr)
	{
		psapi = LoadLibraryA("Psapi.dll");
		if (psapi)
			EnumProcessModulesPtr = (BOOL(WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD)) GetProcAddress(psapi, "EnumProcessModules");
		if (!EnumProcessModulesPtr)
			return 0;
	}

	return EnumProcessModulesPtr(hProcess, lphModule, cb, lpcbNeeded);
}

static HMODULE MyGetModuleHandleFromAddress( const void *addr )
{
    static BOOL (WINAPI *GetModuleHandleExAPtr)(DWORD, LPCSTR, HMODULE *) = NULL;
    static BOOL failed = FALSE;
    HMODULE kernel32;
    HMODULE hModule;
    MEMORY_BASIC_INFORMATION info;
    size_t sLen;

    if( !failed && GetModuleHandleExAPtr == NULL )
    {
        kernel32 = GetModuleHandleA( "Kernel32.dll" );
        if( kernel32 != NULL )
            GetModuleHandleExAPtr = (BOOL (WINAPI *)(DWORD, LPCSTR, HMODULE *)) (LPVOID) GetProcAddress( kernel32, "GetModuleHandleExA" );
        if( GetModuleHandleExAPtr == NULL )
            failed = TRUE;
    }

    if( !failed )
    {
        /* If GetModuleHandleExA is available use it with GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS */
        if( !GetModuleHandleExAPtr( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &hModule ) )
            return NULL;
    }
    else
    {
        /* To get HMODULE from address use undocumented hack from https://stackoverflow.com/a/2396380
         * The HMODULE of a DLL is the same value as the module's base address.
         */
        sLen = VirtualQuery( addr, &info, sizeof( info ) );
        if( sLen != sizeof( info ) )
            return NULL;
        hModule = (HMODULE) info.AllocationBase;
    }

    return hModule;
}

static const char *get_export_symbol_name( HMODULE module, IMAGE_EXPORT_DIRECTORY *ied, const void *addr, void **func_address )
{
    DWORD i;
    void *candidateAddr = NULL;
    int candidateIndex = -1;
    BYTE *base = (BYTE *) module;
    DWORD *functionAddressesOffsets = (DWORD *) (base + (DWORD) ied->AddressOfFunctions);
    DWORD *functionNamesOffsets = (DWORD *) (base + (DWORD) ied->AddressOfNames);
    USHORT *functionNameOrdinalsIndexes = (USHORT *) (base + (DWORD) ied->AddressOfNameOrdinals);

    for( i = 0; i < ied->NumberOfFunctions; i++ )
    {
        if( (void *) ( base + functionAddressesOffsets[i] ) > addr || candidateAddr >= (void *) ( base + functionAddressesOffsets[i] ) )
            continue;

        candidateAddr = (void *) ( base + functionAddressesOffsets[i] );
        candidateIndex = i;
    }

    if( candidateIndex == -1 )
        return NULL;

    *func_address = candidateAddr;

    for( i = 0; i < ied->NumberOfNames; i++ )
    {
        if( functionNameOrdinalsIndexes[i] == candidateIndex )
            return (const char *) ( base + functionNamesOffsets[i] );
    }

    return NULL;
}

static BOOL is_valid_address( const void *addr )
{
    MEMORY_BASIC_INFORMATION info;
    size_t result;

    if( addr == NULL )
        return FALSE;

    /* check valid pointer */
    result = VirtualQuery( addr, &info, sizeof( info ) );

    if( result == 0 || info.AllocationBase == NULL || info.AllocationProtect == 0 || info.AllocationProtect == PAGE_NOACCESS )
        return FALSE;

    return TRUE;
}

static BOOL is_import_thunk( const void *addr )
{
#if defined(_M_ARM64) || defined(__aarch64__)
    ULONG opCode1 = * (ULONG *) ( (BYTE *) addr );
    ULONG opCode2 = * (ULONG *) ( (BYTE *) addr + 4 );
    ULONG opCode3 = * (ULONG *) ( (BYTE *) addr + 8 );

    return (opCode1 & 0x9f00001f) == 0x90000010    /* adrp x16, [page_offset] */
        && (opCode2 & 0xffe003ff) == 0xf9400210    /* ldr  x16, [x16, offset] */
        && opCode3 == 0xd61f0200                   /* br   x16 */
        ? TRUE : FALSE;
#else
    return *(short *) addr == 0x25ff ? TRUE : FALSE;
#endif
}

static BOOL get_image_section( HMODULE module, int index, void **ptr, DWORD *size )
{
    IMAGE_DOS_HEADER *dosHeader;
    IMAGE_NT_HEADERS *ntHeaders;
    IMAGE_OPTIONAL_HEADER *optionalHeader;

    dosHeader = (IMAGE_DOS_HEADER *) module;

    if( dosHeader->e_magic != IMAGE_DOS_SIGNATURE )
        return FALSE;

    ntHeaders = (IMAGE_NT_HEADERS *) ( (BYTE *) dosHeader + dosHeader->e_lfanew );

    if( ntHeaders->Signature != IMAGE_NT_SIGNATURE )
        return FALSE;

    optionalHeader = &ntHeaders->OptionalHeader;

    if( optionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
        return FALSE;

    if( index < 0 || index >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES || index >= optionalHeader->NumberOfRvaAndSizes )
        return FALSE;

    if( optionalHeader->DataDirectory[index].Size == 0 || optionalHeader->DataDirectory[index].VirtualAddress == 0 )
        return FALSE;

    if( size != NULL )
        *size = optionalHeader->DataDirectory[index].Size;

    *ptr = (void *)( (BYTE *) module + optionalHeader->DataDirectory[index].VirtualAddress );

    return TRUE;
}

static void *get_address_from_import_address_table( void *iat, DWORD iat_size, const void *addr )
{
    BYTE *thkp = (BYTE *) addr;
#if defined(_M_ARM64) || defined(__aarch64__)
    /*
     *  typical import thunk in ARM64:
     *  0x7ff772ae78c0 <+25760>: adrp   x16, 1
     *  0x7ff772ae78c4 <+25764>: ldr    x16, [x16, #0xdc0]
     *  0x7ff772ae78c8 <+25768>: br     x16
     */
    ULONG opCode1 = * (ULONG *) ( (BYTE *) addr );
    ULONG opCode2 = * (ULONG *) ( (BYTE *) addr + 4 );

    /* Extract the offset from adrp instruction */
    UINT64 pageLow2 = (opCode1 >> 29) & 3;
    UINT64 pageHigh19 = (opCode1 >> 5) & ~(~0ull << 19);
    INT64 page = sign_extend((pageHigh19 << 2) | pageLow2, 21) << 12;

    /* Extract the offset from ldr instruction */
    UINT64 offset = ((opCode2 >> 10) & ~(~0ull << 12)) << 3;

    /* Calculate the final address */
    BYTE *ptr = (BYTE *) ( (ULONG64) thkp & ~0xfffull ) + page + offset;
#else
    /* Get offset from thunk table (after instruction 0xff 0x25)
     *   4018c8 <_VirtualQuery>: ff 25 4a 8a 00 00
     */
    ULONG offset = *(ULONG *)( thkp + 2 );
#if defined(_M_AMD64) || defined(__x86_64__)
    /* On 64 bit the offset is relative
     *      4018c8:   ff 25 4a 8a 00 00    jmpq    *0x8a4a(%rip)    # 40a318 <__imp_VirtualQuery>
     * And can be also negative (MSVC in WDK)
     *   100002f20:   ff 25 3a e1 ff ff    jmpq   *-0x1ec6(%rip)    # 0x100001060
     * So cast to signed LONG type
     */
    BYTE *ptr = (BYTE *)( thkp + 6 + (LONG) offset );
#else
    /* On 32 bit the offset is absolute
     *   4019b4:    ff 25 90 71 40 00    jmp    *0x40719
     */
    BYTE *ptr = (BYTE *) offset;
#endif
#endif

    if( !is_valid_address( ptr ) || ptr < (BYTE *) iat || ptr > (BYTE *) iat + iat_size )
        return NULL;

    return *(void **) ptr;
}

static char module_filename[2*MAX_PATH];

static BOOL fill_info( const void *addr, Dl_info *info )
{
    HMODULE hModule;
    DWORD dwSize;
    IMAGE_EXPORT_DIRECTORY *ied;
    void *funcAddress = NULL;

    /* Get module of the specified address */
    hModule = MyGetModuleHandleFromAddress( addr );

    if( hModule == NULL )
        return FALSE;

    dwSize = GetModuleFileNameA( hModule, module_filename, sizeof( module_filename ) );

    if( dwSize == 0 || dwSize == sizeof( module_filename ) )
        return FALSE;

    info->dli_fname = module_filename;
    info->dli_fbase = (void *) hModule;

    /* Find function name and function address in module's export table */
    if( get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_EXPORT, (void **) &ied, NULL ) )
        info->dli_sname = get_export_symbol_name( hModule, ied, addr, &funcAddress );
    else
        info->dli_sname = NULL;

    info->dli_saddr = info->dli_sname == NULL ? NULL : funcAddress != NULL ? funcAddress : (void *) addr;

    return TRUE;
}

void* dlopen(const char* file, int mode)
{
	HMODULE hModule;
	UINT uMode;

	current_error = NULL;

	/* Do not let Windows display the critical-error-handler message box */
	uMode = SetErrorMode(SEM_FAILCRITICALERRORS);

	if (file == 0)
	{
		/* POSIX says that if the value of file is 0, a handle on a global
		 * symbol object must be provided. That object must be able to access
		 * all symbols from the original program file, and any objects loaded
		 * with the RTLD_GLOBAL flag.
		 * The return value from GetModuleHandle( ) allows us to retrieve
		 * symbols only from the original program file. For objects loaded with
		 * the RTLD_GLOBAL flag, we create our own list later on. For objects
		 * outside of the program file but already loaded (e.g. linked DLLs)
		 * they are added below.
		 */
		hModule = GetModuleHandle(NULL);

		if (!hModule)
			save_err_ptr_str(file);
	}
	else
	{
		HANDLE hCurrentProc;
		DWORD dwProcModsBefore, dwProcModsAfter;
		char lpFileName[MAX_PATH];
		size_t i, len;

		len = strlen(file);

		if (len >= sizeof(lpFileName))
		{
			SetLastError(ERROR_FILENAME_EXCED_RANGE);
			save_err_str(file);
			hModule = NULL;
		}
		else
		{
			/* MSDN says backslashes *must* be used instead of forward slashes. */
			for (i = 0; i < len; i++)
			{
				if (file[i] == '/')
					lpFileName[i] = '\\';
				else
					lpFileName[i] = file[i];
			}
			lpFileName[len] = '\0';

			hCurrentProc = GetCurrentProcess();

			if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwProcModsBefore) == 0)
				dwProcModsBefore = 0;

			/* POSIX says the search path is implementation-defined.
			 * LOAD_WITH_ALTERED_SEARCH_PATH is used to make it behave more closely
			 * to UNIX's search paths (start with system folders instead of current
			 * folder).
			 */
			hModule = LoadLibraryExA(lpFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

			if (!hModule)
			{
				save_err_str(lpFileName);
			}
			else
			{
				if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwProcModsAfter) == 0)
					dwProcModsAfter = 0;

				/* If the object was loaded with RTLD_LOCAL, add it to list of local
				 * objects, so that its symbols cannot be retrieved even if the handle for
				 * the original program file is passed. POSIX says that if the same
				 * file is specified in multiple invocations, and any of them are
				 * RTLD_GLOBAL, even if any further invocations use RTLD_LOCAL, the
				 * symbols will remain global. If number of loaded modules was not
				 * changed after calling LoadLibraryEx(), it means that library was
				 * already loaded.
				 */
				if ((mode & RTLD_LOCAL) && dwProcModsBefore != dwProcModsAfter)
				{
					if (!local_add(hModule))
					{
						save_err_str(lpFileName);
						FreeLibrary(hModule);
						hModule = NULL;
					}
				}
				else if (!(mode & RTLD_LOCAL) && dwProcModsBefore == dwProcModsAfter)
				{
					local_rem(hModule);
				}
			}
		}
	}

	/* Return to previous state of the error-mode bit flags. */
	SetErrorMode(uMode);

	return (void*)hModule;
}

int dlclose(void* handle)
{
	HMODULE hModule = (HMODULE)handle;
	BOOL ret;

	current_error = NULL;

	ret = FreeLibrary(hModule);

	/* If the object was loaded with RTLD_LOCAL, remove it from list of local
	 * objects.
	 */
	if (ret)
		local_rem(hModule);
	else
		save_err_ptr_str(handle);

	/* dlclose's return value in inverted in relation to FreeLibrary's. */
	ret = !ret;

	return (int)ret;
}

__declspec(noinline) /* Needed for _ReturnAddress() */
void* dlsym(void* handle, const char* name)
{
	FARPROC symbol;
	HMODULE hCaller;
	HMODULE hModule;
	HANDLE hCurrentProc;

	current_error = NULL;
	symbol = NULL;
	hCaller = NULL;
	hModule = GetModuleHandle(NULL);
	hCurrentProc = GetCurrentProcess();

	if (handle == RTLD_DEFAULT)
	{
		/* The symbol lookup happens in the normal global scope; that is,
		 * a search for a symbol using this handle would find the same
		 * definition as a direct use of this symbol in the program code.
		 * So use same lookup procedure as when filename is NULL.
		 */
		handle = hModule;
	}
	else if (handle == RTLD_NEXT)
	{
		/* Specifies the next object after this one that defines name.
		 * This one refers to the object containing the invocation of dlsym().
		 * The next object is the one found upon the application of a load
		 * order symbol resolution algorithm. To get caller function of dlsym()
		 * use _ReturnAddress() intrinsic. To get HMODULE of caller function
		 * use undocumented hack from https://stackoverflow.com/a/2396380
		 * The HMODULE of a DLL is the same value as the module's base address.
		 */
		MEMORY_BASIC_INFORMATION info;
		size_t sLen;
		sLen = VirtualQueryEx(hCurrentProc, _ReturnAddress(), &info, sizeof(info));
		if (sLen != sizeof(info))
		{
			if (sLen != 0)
				SetLastError(ERROR_INVALID_PARAMETER);
			goto end;
		}
		hCaller = (HMODULE)info.AllocationBase;
		if (!hCaller)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			goto end;
		}
	}

	if (handle != RTLD_NEXT)
	{
		symbol = GetProcAddress((HMODULE)handle, name);

		if (symbol != NULL)
			goto end;
	}

	/* If the handle for the original program file is passed, also search
	 * in all globally loaded objects.
	 */

	if (hModule == handle || handle == RTLD_NEXT)
	{
		HMODULE* modules;
		DWORD cbNeeded;
		DWORD dwSize;
		size_t i;

		/* GetModuleHandle( NULL ) only returns the current program file. So
		 * if we want to get ALL loaded module including those in linked DLLs,
		 * we have to use EnumProcessModules( ).
		 */
		if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwSize) != 0)
		{
			modules = (HMODULE*)malloc(dwSize);
			if (modules)
			{
				if (MyEnumProcessModules(hCurrentProc, modules, dwSize, &cbNeeded) != 0 && dwSize == cbNeeded)
				{
					for (i = 0; i < dwSize / sizeof(HMODULE); i++)
					{
						if (handle == RTLD_NEXT && hCaller)
						{
							/* Next modules can be used for RTLD_NEXT */
							if (hCaller == modules[i])
								hCaller = NULL;
							continue;
						}
						if (local_search(modules[i]))
							continue;
						symbol = GetProcAddress(modules[i], name);
						if (symbol != NULL)
							goto end;
					}

				}
				free(modules);
			}
		}
	}

end:
	if (symbol == NULL)
	{
		if (GetLastError() == 0)
			SetLastError(ERROR_PROC_NOT_FOUND);
		save_err_str(name);
	}

	//  warning C4054: 'type cast' : from function pointer 'FARPROC' to data pointer 'void *'
#ifdef _MSC_VER
#pragma warning( suppress: 4054 )
#endif
	return (void*)symbol;
}

char* dlerror(void)
{
	char* error_pointer = dlerror_buffer;

	/* If this is the second consecutive call to dlerror, return NULL */
	if (current_error == NULL)
	{
		return NULL;
	}

	memcpy(error_pointer, current_error, strlen(current_error) + 1);

	/* POSIX says that invoking dlerror( ) a second time, immediately following
	 * a prior invocation, shall result in NULL being returned.
	 */
	current_error = NULL;

	return error_pointer;
}

int dladdr( const void *addr, Dl_info *info )
{
    if( info == NULL )
        return 0;

    if( !is_valid_address( addr ) )
        return 0;

    if( is_import_thunk( addr ) )
    {
        void *iat;
        DWORD iatSize;
        HMODULE hModule;

        /* Get module of the import thunk address */
        hModule = MyGetModuleHandleFromAddress( addr );

        if( hModule == NULL )
            return 0;

        if( !get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_IAT, &iat, &iatSize ) )
        {
            /* Fallback for cases where the iat is not defined,
             * for example i586-mingw32msvc-gcc */
            IMAGE_IMPORT_DESCRIPTOR *iid;
            DWORD iidSize;

            if( !get_image_section( hModule, IMAGE_DIRECTORY_ENTRY_IMPORT, (void **) &iid, &iidSize ) )
                return 0;

            if( iid == NULL || iid->Characteristics == 0 || iid->FirstThunk == 0 )
                return 0;

            iat = (void *)( (BYTE *) hModule + (DWORD) iid->FirstThunk );
            /* We assume that in this case iid and iat's are in linear order */
            iatSize = iidSize - (DWORD) ( (BYTE *) iat - (BYTE *) iid );
        }

        addr = get_address_from_import_address_table( iat, iatSize, addr );

        if( !is_valid_address( addr ) )
            return 0;
    }

    if( !fill_info( addr, info ) )
        return 0;

    return 1;
}
