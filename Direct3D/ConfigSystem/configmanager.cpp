//--------------------------------------------------------------------------------------
// File: ConfigManager.cpp
//
// Implementation of configuration system functions.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#pragma warning(disable: 4995)
#include <stdio.h>
#include "ConfigDatabase.h"
#include "ConfigManager.h"
#include <ddraw.h>
#include <dsound.h>
#include <dsetup.h>
#include <oleauto.h>
#include <wbemidl.h>
#pragma warning(default: 4995)


typedef struct
{
    const char* Property;
    bool (CConfigManager::*Func)(const char*);
} CONFIGTABLE;

CONFIGTABLE ConfigTable[]= {
    { "ForceShader",                      &CConfigManager::InitForceShader                      },
    { "DisableDriverManagement",          &CConfigManager::InitDisableDriverManagement          },
    { "LinearTextureAddressing",          &CConfigManager::InitLinearTextureAddressing          },
    { "MaximumResolution",                &CConfigManager::InitMaximumResolution                },
    { "UseFixedFunction",                 &CConfigManager::InitUseFixedFunction                 },
    { "UnsupportedCard",                  &CConfigManager::InitUnsupportedCard                  },
    { "DisableRenderTargets",             &CConfigManager::InitDisableRenderTargets             },
    { "DisableAlphaRenderTargets",        &CConfigManager::InitDisableAlphaRenderTargets        },
    { "OldDriver",                        &CConfigManager::InitOldDriver                        },
    { "EnableStopStart",                  &CConfigManager::InitEnableStopStart                  },
    { "OldSoundDriver",                   &CConfigManager::InitOldSoundDriver                   },
    { "InvalidDriver",                    &CConfigManager::InitInvalidDriver                    },
    { "InvalidSoundDriver",               &CConfigManager::InitInvalidSoundDriver               },
    { "SafeMode",                         &CConfigManager::InitSafeMode                         },
    { "DisableSpecular",                  &CConfigManager::InitDisableSpecular                  },
    { "UseAnisotropicFilter",             &CConfigManager::InitUseAnisotropicFilter             },
    { "UMA",                              &CConfigManager::InitUMA                              },
    { "DoNotUseMinMaxBlendOp",            &CConfigManager::InitDoNotUseMinMaxBlendOp            },
    { "PrototypeCard",                    &CConfigManager::InitPrototypeCard                    },
};


static DWORD GetNumber( const char* value )
{
    int result;
#ifdef _CRT_INSECURE_DEPRECATE
    int nRet = sscanf_s( value, "%d", &result );   
#else
    int nRet = sscanf( value, "%d", &result );   
#endif
    if( nRet == 0 || nRet == EOF )
        result = 0;
    return result;
}


static float GetFloatNumber( const char* value )
{
    float result;
#ifdef _CRT_INSECURE_DEPRECATE
    int nRet = sscanf_s( value, "%f", &result );
#else
    int nRet = sscanf( value, "%f", &result );
#endif
    if( nRet == 0 || nRet == EOF )
        result = 0.0f;
    return result;
}


BOOL WINAPI DDEnumCB( GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm )
{
    CConfigManager *pCM = (CConfigManager*)lpContext;

    if( pCM->m_nAdapterCount )
    {
        memcpy( &pCM->m_AdapterArray[pCM->m_nAdapterCount].guid, lpGUID, sizeof(GUID) );
    }
    if( strlen( lpDriverName )<31 )
        strcpy_s( pCM->m_AdapterArray[pCM->m_nAdapterCount].DriverName, 32, lpDriverName );
    else
        pCM->m_AdapterArray[pCM->m_nAdapterCount].DriverName[0] = 0;

    pCM->m_nAdapterCount++;
    if( pCM->m_nAdapterCount == 10 )
        return false;
    else
        return true;
}


CConfigManager::CConfigManager()
{
    InitConfigProperties();
    ZeroMemory( m_AdapterArray, sizeof(m_AdapterArray) );
    m_nAdapterCount = 0;
}


void CConfigManager::InitConfigProperties()
{
    cf_MaximumResolution = 4096;
    cf_LinearTextureAddressing = 0;
    cf_UseFixedFunction = 0;
    cf_PrototypeCard = 0;
    cf_DisableDriverManagement = 0;
    cf_UseAnisotropicFilter = 0;
    cf_DisableSpecular = 0;
    cf_UnsupportedCard = 0;
    cf_EnableStopStart = 0;
    cf_DisableRenderTargets = 0;
    cf_DisableAlphaRenderTargets = 0;
    cf_OldDriver = 0;
    cf_OldSoundDriver = 0;
    cf_InvalidDriver = 0;
    cf_InvalidSoundDriver = 0;
    cf_SafeMode = 0;
    cf_ForceShader = 0;
    cf_DoNotUseMinMaxBlendOp = 0;
    req_CPUSpeed = 0;
    req_Memory = 0;
    req_VideoMemory = 0;
    req_DirectXMajor = 0;
    req_DirectXMinor = 0;
    req_DirectXLetter = 0;
    req_DiskSpace = 0;
    req_OSMajor = 0;
    req_OSMinor = 0;
}


typedef BOOL (WINAPI* PfnCoSetProxyBlanket)(
                                    IUnknown                 *pProxy,
                                    DWORD                     dwAuthnSvc,
                                    DWORD                     dwAuthzSvc,
                                    OLECHAR                  *pServerPrincName,
                                    DWORD                     dwAuthnLevel,
                                    DWORD                     dwImpLevel,
                                    RPC_AUTH_IDENTITY_HANDLE  pAuthInfo,
                                    DWORD                     dwCapabilities );


void CConfigManager::InitConfigInformation( D3DCAPS9 &d3dCaps )
{
    //
    // Get system memory size
    //
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx( &ms );
    SysMemory = (DWORD)(((ms.ullTotalPhys+(16*1024*1024-1)) & ~(16*1024*1024-1) ) >> 20);

    //
    // Get CPU speed. First try WMI. If that fails, fall back to detecting ourselves.
    //
    HRESULT        hr;
    IWbemLocator*  pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    BSTR           pNamespace = NULL;

    CoInitialize(0);

    hr = CoCreateInstance( CLSID_WbemLocator,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_IWbemLocator,
                           (LPVOID*) &pIWbemLocator );

    if( SUCCEEDED(hr) && pIWbemLocator )
    {
        // Using the locator, connect to WMI in the given namespace.
        pNamespace = SysAllocString( L"\\\\.\\root\\cimv2" );

        hr = pIWbemLocator->ConnectServer( pNamespace, NULL, NULL, 0L,
                                        0L, NULL, NULL, &pIWbemServices );
        if( SUCCEEDED(hr) && pIWbemServices != NULL )
        {
            HINSTANCE hinstOle32 = NULL;

            hinstOle32 = LoadLibraryW( L"ole32.dll" );
            if( hinstOle32 )
            {
                PfnCoSetProxyBlanket pfnCoSetProxyBlanket = NULL;

                pfnCoSetProxyBlanket = (PfnCoSetProxyBlanket)GetProcAddress( hinstOle32, "CoSetProxyBlanket" );
                if( pfnCoSetProxyBlanket != NULL )
                {
                    // Switch security level to IMPERSONATE. 
                    pfnCoSetProxyBlanket( pIWbemServices,               // proxy
                                          RPC_C_AUTHN_WINNT,            // authentication service
                                          RPC_C_AUTHZ_NONE,             // authorization service
                                          NULL,                         // server principle name
                                          RPC_C_AUTHN_LEVEL_CALL,       // authentication level
                                          RPC_C_IMP_LEVEL_IMPERSONATE,  // impersonation level
                                          NULL,                         // identity of the client
                                          EOAC_NONE );                  // capability flags
                }

                FreeLibrary( hinstOle32 );
            }

            IEnumWbemClassObject* pEnumProcessorDevs = NULL;
            BSTR pClassName = NULL;

            pClassName = SysAllocString( L"Win32_Processor" );

            hr = pIWbemServices->CreateInstanceEnum( pClassName,
                                                     0,
                                                     NULL, &pEnumProcessorDevs );

            if( SUCCEEDED(hr) && pEnumProcessorDevs )
            {
                IWbemClassObject* pProcessorDev = NULL;
                DWORD uReturned = 0;
                BSTR pPropName = NULL;

                // Get the first one in the list
                pEnumProcessorDevs->Reset();
                hr = pEnumProcessorDevs->Next( 5000,             // timeout in 5 seconds
                                               1,                // return just one storage device
                                               &pProcessorDev,   // pointer to storage device
                                               &uReturned );     // number obtained: one or zero

                if( SUCCEEDED(hr) && uReturned != 0 && pProcessorDev != NULL )
                {
                    // MaxClockSpeed should be used on WinXP & beyond.
                    OSVERSIONINFO OSVersionInfo;
                    OSVersionInfo.dwOSVersionInfoSize = sizeof OSVersionInfo;
                    GetVersionEx( &OSVersionInfo );
                    if( ( OSVersionInfo.dwPlatformId   == VER_PLATFORM_WIN32_NT && 
                        OSVersionInfo.dwMajorVersion == 5 &&
                        OSVersionInfo.dwMinorVersion == 0 ) ||
                        ( OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) )
                    {
                        pPropName = SysAllocString( L"CurrentClockSpeed" );
                    }
                    else
                    {
                        pPropName = SysAllocString( L"MaxClockSpeed" );
                    }

                    VARIANT var;
                    hr = pProcessorDev->Get( pPropName, 0L, &var, NULL, NULL );
                    if( SUCCEEDED(hr) )
                        CpuSpeed = var.lVal;

                    if( pPropName )
                        SysFreeString( pPropName );
                }

                SAFE_RELEASE( pProcessorDev );
            }

            if( pClassName )
                SysFreeString( pClassName );
            SAFE_RELEASE( pEnumProcessorDevs );
        }

        if( pNamespace )
            SysFreeString( pNamespace );
        SAFE_RELEASE( pIWbemServices );
    }

    SAFE_RELEASE( pIWbemLocator );

    if( FAILED(hr) )
    {
        // Failed to retrieve CPU speed via WMI.
        // Now we detect it ourselves.

#if defined( _WIN64 ) && defined( _IA64_ )
        CpuSpeed = ULONG_MAX;  // On Ia64, skip processor speed detection and just return a large value
                               // so requirement would pass.
#else
        __int64 time,qpc,compare;

        HANDLE hThread = GetCurrentThread();
        HANDLE hProcess = GetCurrentProcess();
        long OldPriority = GetThreadPriority(hThread);
        DWORD OldClass = GetPriorityClass(hProcess);
        SetPriorityClass( hProcess,REALTIME_PRIORITY_CLASS );
        SetThreadPriority( hThread,THREAD_PRIORITY_TIME_CRITICAL ); // Run code at realtime priority
        Sleep(100);                                                 // Give windows a chance to run background tasks

        QueryPerformanceFrequency((LARGE_INTEGER*)&qpc);
        QueryPerformanceCounter((LARGE_INTEGER*)&compare);
        compare+=qpc/4;                                             // Find time .5 seconds ahead

#ifdef _WIN64
        time = __rdtsc();  // Use intrinsic for AMD64 environment
#else
        __asm rdtsc
        __asm mov dword ptr time,eax
        __asm mov dword ptr time+4,edx
#endif

        do
        {
            QueryPerformanceCounter((LARGE_INTEGER*)&qpc);          // Spin until time equal
        }
        while( compare>qpc );

#ifdef _WIN64
        time = __rdtsc() - time;  // Use intrinsic for AMD64 environment
#else
        __asm rdtsc
        __asm sub eax,dword ptr time
        __asm sbb edx,dword ptr time+4
        __asm mov dword ptr time,eax
        __asm mov dword ptr time+4,edx
#endif

        SetThreadPriority(hThread,OldPriority);
        SetPriorityClass(hProcess,OldClass);

        //
        // Normalize CPU speed
        //
        CpuSpeed=(DWORD)(time/250000);

        if( CpuSpeed>1000 )
        {
            CpuSpeed=((CpuSpeed+50)/100)*100;
        }

        DWORD CpuSpeed2=CpuSpeed%100;

        if( CpuSpeed<200 )
        {
            if( CpuSpeed2>95 )
            {
                CpuSpeed+=100-CpuSpeed2;
            }
            if( CpuSpeed2>61 && CpuSpeed2<71 )
                CpuSpeed+=66-CpuSpeed2;
            if( CpuSpeed2>45 && CpuSpeed2<55 )
                CpuSpeed+=50-CpuSpeed2;
            if( CpuSpeed2>28 && CpuSpeed2<38 )
                CpuSpeed+=33-CpuSpeed2;
        }
        else
        {
            if( CpuSpeed2>84 )
            {
                CpuSpeed+=100-CpuSpeed2;
            }
            else
            {
                if( CpuSpeed2>58 )
                {
                    CpuSpeed+=66-CpuSpeed2;
                }
                else
                {
                    if( CpuSpeed2>42 )
                    {
                        CpuSpeed+=50-CpuSpeed2;
                    }
                    else
                    {
                        if( CpuSpeed2>16 )
                        {
                            CpuSpeed+=33-CpuSpeed2;
                        }
                        else
                        {
                            if( CpuSpeed2<16 )
                            {
                                CpuSpeed-=CpuSpeed2;
                            }
                        }
                    }
                }
            }
        }
#endif   // _WIN64 && _IA64_
    }

    //
    // Get video memory size for all devices using ddraw 7 interfaces
    //
    IDirectDraw7* DDobject;
    HINSTANCE ddrawLib = LoadLibraryW( L"ddraw.dll" );
    if( ddrawLib )
    {
        HRESULT (WINAPI* _DirectDrawCreateEx)( GUID* lpGUID, void** lplpDD, REFIID iid, IUnknown* pUnkOuter ) = (HRESULT (WINAPI*)( GUID* lpGUID, void** lplpDD, REFIID iid, IUnknown* pUnkOuter ))GetProcAddress( ddrawLib, "DirectDrawCreateEx" ); 
        if( _DirectDrawCreateEx )
        {
            HRESULT (WINAPI* _DirectDrawEnumerateEx)( LPDDENUMCALLBACKEXA, LPVOID, DWORD ) = (HRESULT (WINAPI*)( LPDDENUMCALLBACKEXA, LPVOID, DWORD )) GetProcAddress( ddrawLib, "DirectDrawEnumerateExA" ); 
            if( _DirectDrawEnumerateEx )
            {
                //
                // DirectDraw 7 will enumerate 1 device (NULL). Or a NULL device and then a GUID for each adapter.
                //
                _DirectDrawEnumerateEx( DDEnumCB, this, DDENUM_ATTACHEDSECONDARYDEVICES );

                //
                // Create a DirectDraw object for the device specified
                //
                for( int t0 = 0; t0 < m_nAdapterCount; ++t0 )
                {
                    HRESULT result = _DirectDrawCreateEx( t0 ? &m_AdapterArray[t0].guid : NULL, (void**)&DDobject, IID_IDirectDraw7, NULL );
                    if( SUCCEEDED(result) )
                    {
                        DDobject->SetCooperativeLevel( NULL, DDSCL_NORMAL );
                        //
                        // See how much memory is on the card
                        //
                        DDSCAPS2 ddscaps2;
                        DWORD Junk;
                        DWORD VideoMemory=-1;
                        DWORD Temp=0;
                        memset( &ddscaps2, 0, sizeof(ddscaps2) );
                        ddscaps2.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
                        DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
                        if( Temp>0 && Temp<VideoMemory )
                            VideoMemory=Temp;
                        ddscaps2.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
                        DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
                        if( Temp>0 && Temp<VideoMemory )
                            VideoMemory=Temp;
                        ddscaps2.dwCaps = DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
                        DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
                        if( Temp>0 && Temp<VideoMemory )
                            VideoMemory=Temp;
                        ddscaps2.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
                        DDobject->GetAvailableVidMem( &ddscaps2, &Temp, &Junk );
                        if( Temp>0 && Temp<VideoMemory )
                            VideoMemory=Temp;
                        //
                        // If cannot get memory size, there must be a problem with D3D / DirectDraw
                        //
                        if( -1==VideoMemory )
                        {
                            VideoMemory = 0;
                        } else
                        {
                            //
                            // Round the video memory number depending upon the size
                            //
                            if( VideoMemory<=16*1024*1024 )
                            {
                                VideoMemory=((VideoMemory+8*1024*1024-1) & ~(8*1024*1024-1));               // Round up to nearest 8 under 20Meg
                            }
                            else
                            {
                                if( VideoMemory <= 64*1024*1024 )
                                {
                                    VideoMemory=((VideoMemory+32*1024*1024-1) & ~(32*1024*1024-1));         // Round to neaest 32 Meg under 64Meg
                                }
                                else
                                {
                                    VideoMemory=((VideoMemory+64*1024*1024-1) & ~(64*1024*1024-1));         // Round to neaest 64 Meg over 64Meg
                                }
                            }
                        }

                        //
                        // Clean up ddraw7
                        //
                        m_AdapterArray[t0].MemorySize = VideoMemory;
                        DDobject->Release();
                    }
                }
            }
        }
        //
        // Release ddraw7 object
        //
        FreeLibrary( ddrawLib );
    }

    //
    // Find the display adapter (ddraw and d3d9 do not enumerate the same order)
    //
    D3DADAPTER_IDENTIFIER9 d3di;
    DXUTGetD3D9Object()->GetAdapterIdentifier( d3dCaps.AdapterOrdinal, 0, &d3di );
    DriverVersionLowPart = d3di.DriverVersion.LowPart;
    DriverVersionHighPart = d3di.DriverVersion.HighPart;
    VideoMemory = m_AdapterArray[0].MemorySize;
    for( int i = 0; i < m_nAdapterCount; ++i )
    {
        if( !_stricmp( m_AdapterArray[i].DriverName, d3di.DeviceName ) )
            VideoMemory = m_AdapterArray[i].MemorySize;
    }

    //
    // Detect sound configuration
    //
    GUID Guid;
    GetDeviceID( &DSDEVID_DefaultPlayback, &Guid );
    InitSoundInformation( Guid, &Sound_device );

    CoUninitialize();
}


HRESULT CConfigManager::Initialize( WCHAR* FileName, D3DADAPTER_IDENTIFIER9 &AdapterID, D3DCAPS9 &Caps,
                                    REFGUID DSoundDeviceGuid )
{
    HRESULT hr = S_OK;

    InitConfigInformation( Caps );

    SOUND_DEVICE SndDev;
    InitSoundInformation( DSoundDeviceGuid, &SndDev );

    DXUTOutputDebugStringA( "Media device found:\n" );
    DXUTTRACE( L"%s\n", SndDev.name );

    IConfigDatabase* pCDB = IConfigDatabase::Create();
    if( pCDB->Load( FileName, SndDev, AdapterID, Caps, 1024, 64, 1700 ) )
    {
        DXUTOutputDebugStringA( "Graphics device string: %s\n", pCDB->GetGfxDeviceString() );
        DXUTOutputDebugStringA( "Graphics vendor string: %s\n", pCDB->GetGfxVendorString() );
        DXUTOutputDebugStringA( "Sound device string: %s\n", pCDB->GetSoundDeviceString() );
        DXUTOutputDebugStringA( "Sound vendor string: %s\n", pCDB->GetSoundVendorString() );

        // Interate the properties and initialize the config flags.
        DXUTOutputDebugStringA( "Properties:\n" );
        int nPropCount = pCDB->GetDevicePropertyCount();
        for( int i = 0; i < nPropCount; ++i )
        {
            DXUTOutputDebugStringA( "\"%s\" : \"%s\"\n", pCDB->GetDeviceProperty( i ), pCDB->GetDeviceValue( i ) );

            for( int f = 0; f < sizeof(ConfigTable) / sizeof(ConfigTable[0]); ++f )
            {
                if( !_stricmp( ConfigTable[f].Property, pCDB->GetDeviceProperty( i ) ) )
                {
                    // Property found. Call the init function.
                    (this->*ConfigTable[f].Func)( pCDB->GetDeviceValue( i ) );
                }
            }
        }

        ::DXUTOutputDebugStringA( "\nRequirements:\n" );
        int nReqCount = pCDB->GetRequirementsPropertyCount();
        for( int i = 0; i < nReqCount; ++i )
        {
            char* Property = (char*)pCDB->GetRequirementsProperty( i );
            char* Value = (char*)pCDB->GetRequirementsValue( i );
            int nRet;

            DXUTOutputDebugStringA( "\"%s\" : \"%s\"\n", Property, Value );

            if( !_stricmp( "CpuSpeed", Property ) )
            {
#ifdef _CRT_INSECURE_DEPRECATE
                nRet = sscanf_s( Value, "%d",&req_CPUSpeed );
#else
                nRet = sscanf( Value, "%d",&req_CPUSpeed );
#endif
                if( nRet == 0 || nRet == EOF )
                    req_CPUSpeed = 0;
            } else
            if( !_stricmp( "Memory", Property ) )
            {
#ifdef _CRT_INSECURE_DEPRECATE
                nRet = sscanf_s( Value, "%d",&req_Memory );
#else
                nRet = sscanf( Value, "%d",&req_Memory );
#endif
                if( nRet == 0 || nRet == EOF )
                    req_Memory = 0;
            } else
            if( !_stricmp( "VideoMemory", Property ) )
            {
#ifdef _CRT_INSECURE_DEPRECATE 
                nRet = sscanf_s( Value, "%d",&req_VideoMemory );
#else
                nRet = sscanf( Value, "%d",&req_VideoMemory );
#endif
                if( nRet == 0 || nRet == EOF )
                    req_VideoMemory = 0;
            } else
            if( !_stricmp( "DirectX", Property ) )
            {
#ifdef _CRT_INSECURE_DEPRECATE
                nRet = sscanf_s( Value, "%d.%d%c", &req_DirectXMajor, &req_DirectXMinor, &req_DirectXLetter, sizeof(req_DirectXLetter) );
#else
                nRet = sscanf( Value, "%d.%d%c", &req_DirectXMajor, &req_DirectXMinor, &req_DirectXLetter );
#endif
                req_DirectXLetter = (char)req_DirectXLetter;  // Convert to wide char
                if( nRet == 0 || nRet == EOF )
                {
                    req_DirectXMajor = req_DirectXMinor = 0;
                    req_DirectXLetter = L' ';
                }
            } else
            if( !_stricmp( "DiskSpace", Property ) )
            {
#ifdef _CRT_INSECURE_DEPRECATE
                nRet = sscanf_s( Value, "%d",&req_DiskSpace );
#else
                nRet = sscanf( Value, "%d",&req_DiskSpace );
#endif
                if( nRet == 0 || nRet == EOF )
                    req_DiskSpace = 0;
            } else
            if( !_stricmp( "OS", Property ) )
            {
                if( !_stricmp( "Win95", Value ) )
                {
                    req_OSMajor = 4;
                    req_OSMinor = 0;
                } else
                if( !_stricmp( "Win98", Value ) )
                {
                    req_OSMajor = 4;
                    req_OSMinor = 10;
                } else
                if( !_stricmp( "WinME", Value ) )
                {
                    req_OSMajor = 4;
                    req_OSMinor = 90;
                } else
                if( !_stricmp( "Win2k", Value ) )
                {
                    req_OSMajor = 5;
                    req_OSMinor = 0;
                } else
                if( !_stricmp( "WinXP", Value ) )
                {
                    req_OSMajor = 5;
                    req_OSMinor = 1;
                } else
                if( !_stricmp( "Win2003", Value ) )
                {
                    req_OSMajor = 5;
                    req_OSMinor = 2;
                }
            }
        }
    }
    else
    {
        hr = E_FAIL;
    }

    pCDB->Release();

    return hr;
}


bool CConfigManager::InitMaximumResolution( const char* value )
{
    DWORD val = GetNumber( value );
    if( val < 640 || val > 4096 )
    {
        return false;
    }
    cf_MaximumResolution=val;
    return true;
}


bool CConfigManager::InitForceShader( const char* value )
{
    if( *(WORD*)value=='a2' || *(WORD*)value=='A2' )
    {
        cf_ForceShader=9998;
    }
    else
    if( *(WORD*)value=='b2' || *(WORD*)value=='B2' )
    {
        cf_ForceShader=9997;
    }
    else
    {
        cf_ForceShader = GetNumber( value );
        if( 0==cf_ForceShader )
            cf_ForceShader=9999;
    }
    return true;
}


bool CConfigManager::InitLinearTextureAddressing( const char* value )
{
    cf_LinearTextureAddressing=1;
    return true;
}


bool CConfigManager::InitUseAnisotropicFilter( const char* value )
{
    cf_UseAnisotropicFilter=1;
    return true;
}


bool CConfigManager::InitDisableSpecular( const char* value )
{
    cf_DisableSpecular=1;
    return true;
}


bool CConfigManager::InitUseFixedFunction( const char* value )
{
    cf_UseFixedFunction=1;
    return true;
}



bool CConfigManager::InitDisableRenderTargets( const char* value )
{
    cf_DisableRenderTargets=1;
    return true;
}


bool CConfigManager::InitDisableAlphaRenderTargets( const char* value )
{
    cf_DisableAlphaRenderTargets=1;
    return true;
}


bool CConfigManager::InitPrototypeCard( const char* value )
{
    cf_PrototypeCard=1;
    return true;
}


bool CConfigManager::InitUnsupportedCard( const char* value )
{
    cf_UnsupportedCard=1;
    return true;
}


bool CConfigManager::InitOldDriver( const char* value )
{
    cf_OldDriver=1;
    return true;
}


bool CConfigManager::InitEnableStopStart( const char* value )
{
    cf_EnableStopStart=1;
    return true;
}


bool CConfigManager::InitOldSoundDriver( const char* value )
{
    cf_OldSoundDriver=1;
    return true;
}


bool CConfigManager::InitInvalidDriver( const char* value )
{
    cf_InvalidDriver=1;
    return true;
}


bool CConfigManager::InitInvalidSoundDriver( const char* value )
{
    cf_InvalidSoundDriver=1;
    return true;
}


bool CConfigManager::InitSafeMode( const char* value )
{
    cf_SafeMode=1;
    return true;
}


bool CConfigManager::InitDisableDriverManagement( const char* value )
{
    cf_DisableDriverManagement=1;
    return true;
}


//
//   0-64Meg  = 8Meg video memory
//  64-128Meg = 16Meg video memory
// 128-256    = 32Meg video memory
// 256+       = 64Meg video memory
//
bool CConfigManager::InitUMA( const char* value )
{
    VideoMemory = 8*1024*1024;
    if( SysMemory >= 64 )
        VideoMemory = 16*1024*1024;
    if( SysMemory >= 128 )
        VideoMemory = 32*1024*1024;
    if( SysMemory >= 256 )
        VideoMemory = 64*1024*1024;
    return true;
}


bool CConfigManager::InitDoNotUseMinMaxBlendOp( const char* value )
{
    cf_DoNotUseMinMaxBlendOp = 1;
    return true;
}


HRESULT CConfigManager::VerifyRequirements()
{
    HRESULT hr = S_OK;
    WCHAR wszText[2048] = L"This system does not meet the following minimum requirement(s):\n\n";

    //
    // OS version
    //
    OSVERSIONINFO ovi;
    ovi.dwOSVersionInfoSize = sizeof(ovi);
    GetVersionEx( &ovi );
    if( req_OSMajor > ovi.dwMajorVersion ||
        ( req_OSMajor == ovi.dwMajorVersion && req_OSMinor > ovi.dwMinorVersion ) )
    {
        // OS version not supported.
        hr = E_FAIL;
        wcscat_s( wszText, 2048, L"- Operating system not supported\n" );
    }

    //
    // CPU speed
    //
    if( CpuSpeed < req_CPUSpeed )
    {
        hr = E_FAIL;
        WCHAR wsz[256];
        swprintf_s( wsz, 256, L"- Minimum processor speed of %u MHz\n", req_CPUSpeed );
        wcscat_s( wszText, 2048, wsz );
    }

    //
    // Memory size
    //
    if( SysMemory < req_Memory )
    {
        hr = E_FAIL;
        WCHAR wsz[256];
        swprintf_s( wsz, 256, L"- Minimum memory of %u megabytes\n", req_Memory );
        wcscat_s( wszText, 2048, wsz );
    }

    //
    // Video memory size
    //
    if( (VideoMemory >> 20) < req_VideoMemory )
    {
        hr = E_FAIL;
        WCHAR wsz[256];
        swprintf_s( wsz, 256, L"- Minimum video memory of %u megabytes\n", req_VideoMemory );
        wcscat_s( wszText, 2048, wsz );
    }

    //
    // DirectX version
    //
    DWORD dwDXMajor = 0;
    DWORD dwDXMinor = 0;
    TCHAR cDXLetter = ' ';
    GetDXVersion( &dwDXMajor, &dwDXMinor, &cDXLetter );

    if( dwDXMajor < req_DirectXMajor ||
        ( dwDXMajor == req_DirectXMajor && dwDXMinor < req_DirectXMinor ) ||
        ( dwDXMajor == req_DirectXMajor && dwDXMinor == req_DirectXMinor && cDXLetter < req_DirectXLetter ) )
    {
        hr = E_FAIL;
        WCHAR wsz[256];
        swprintf_s( wsz, 256, L"- DirectX %u.%u%c\n", req_DirectXMajor, req_DirectXMinor, req_DirectXLetter );
        wcscat_s( wszText, 2048, wsz );
    }

    if( FAILED( hr ) )
    {
        wcscat_s( wszText, 2048, L"\n The application may not work properly if run.  Do you wish to continue anyway?" );
        if( ::MessageBox( NULL, wszText, L"Minimum Requirement", MB_YESNO|MB_ICONQUESTION ) == IDYES )
            hr = S_OK;
    }

    return hr;
}
