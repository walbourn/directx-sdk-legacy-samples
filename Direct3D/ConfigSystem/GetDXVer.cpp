//-----------------------------------------------------------------------------
// File: GetDXVer.cpp
//
// Desc: Demonstrates how applications can detect what version of DirectX
//       is installed.
//
// (C) Copyright Microsoft Corp.  All rights reserved.
//-----------------------------------------------------------------------------
#include "dxut.h"
#pragma warning(disable: 4995)
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "ConfigDatabase.h"
#include "ConfigManager.h"
#define INITGUID
#include <guiddef.h>
#include <dxdiag.h>
#pragma warning(default: 4995)


HRESULT GetDirectXVersionViaDxDiag( DWORD* pdwDirectXVersionMajor, DWORD* pdwDirectXVersionMinor,
                                    WCHAR* pcDirectXVersionLetter );
HRESULT GetDirectXVersionViaFileVersions( DWORD* pdwDirectXVersionMajor, DWORD* pdwDirectXVersionMinor,
                                          WCHAR* pcDirectXVersionLetter );
HRESULT GetFileVersion( WCHAR* szPath, ULARGE_INTEGER* pllFileVersion );
ULARGE_INTEGER MakeInt64( WORD a, WORD b, WORD c, WORD d );
int CompareLargeInts( ULARGE_INTEGER ullParam1, ULARGE_INTEGER ullParam2 );




//-----------------------------------------------------------------------------
// Name: GetDXVersion()
// Desc: This function returns the DirectX version.
//-----------------------------------------------------------------------------
HRESULT GetDXVersion( DWORD* pdwDirectXVersionMajor, DWORD* pdwDirectXVersionMinor, WCHAR* pcDirectXVersionLetter )
{
    bool bGotDirectXVersion = false;

    // Init values to unknown
    *pdwDirectXVersionMajor = 0;
    *pdwDirectXVersionMinor = 0;
    *pcDirectXVersionLetter = ' ';

    // First, try to use dxdiag's COM interface to get the DirectX version.  
    // The only downside is this will only work on DX9 or later.
    if( SUCCEEDED( GetDirectXVersionViaDxDiag( pdwDirectXVersionMajor, pdwDirectXVersionMinor,
                                               pcDirectXVersionLetter ) ) )
        bGotDirectXVersion = true;

    if( !bGotDirectXVersion )
    {
        // Getting the DirectX version info from DxDiag failed, 
        // so most likely we are on DX8.x or earlier
        if( SUCCEEDED( GetDirectXVersionViaFileVersions( pdwDirectXVersionMajor, pdwDirectXVersionMinor,
                                                         pcDirectXVersionLetter ) ) )
            bGotDirectXVersion = true;
    }

    // If both techniques failed, then return E_FAIL
    if( !bGotDirectXVersion )
        return E_FAIL;

    // Set the output values to what we got and return       
    *pcDirectXVersionLetter = ( char )tolower( *pcDirectXVersionLetter );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetDirectXVersionViaDxDiag()
// Desc: Tries to get the DirectX version from DxDiag's COM interface
//-----------------------------------------------------------------------------
HRESULT GetDirectXVersionViaDxDiag( DWORD* pdwDirectXVersionMajor,
                                    DWORD* pdwDirectXVersionMinor,
                                    WCHAR* pcDirectXVersionLetter )
{
    HRESULT hr;
    bool bCleanupCOM = false;

    bool bSuccessGettingMajor = false;
    bool bSuccessGettingMinor = false;
    bool bSuccessGettingLetter = false;

    // Init COM.  COM may fail if its already been inited with a different 
    // concurrency model.  And if it fails you shouldn't release it.
    hr = CoInitialize( NULL );
    bCleanupCOM = SUCCEEDED( hr );

    // Get an IDxDiagProvider
    bool bGotDirectXVersion = false;
    IDxDiagProvider* pDxDiagProvider = NULL;
    hr = CoCreateInstance( CLSID_DxDiagProvider,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_IDxDiagProvider,
                           ( LPVOID* )&pDxDiagProvider );
    if( SUCCEEDED( hr ) )
    {
        // Fill out a DXDIAG_INIT_PARAMS struct
        DXDIAG_INIT_PARAMS dxDiagInitParam;
        ZeroMemory( &dxDiagInitParam, sizeof( DXDIAG_INIT_PARAMS ) );
        dxDiagInitParam.dwSize = sizeof( DXDIAG_INIT_PARAMS );
        dxDiagInitParam.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
        dxDiagInitParam.bAllowWHQLChecks = false;
        dxDiagInitParam.pReserved = NULL;

        // Init the m_pDxDiagProvider
        hr = pDxDiagProvider->Initialize( &dxDiagInitParam );
        if( SUCCEEDED( hr ) )
        {
            IDxDiagContainer* pDxDiagRoot = NULL;
            IDxDiagContainer* pDxDiagSystemInfo = NULL;

            // Get the DxDiag root container
            hr = pDxDiagProvider->GetRootContainer( &pDxDiagRoot );
            if( SUCCEEDED( hr ) )
            {
                // Get the object called DxDiag_SystemInfo
                hr = pDxDiagRoot->GetChildContainer( L"DxDiag_SystemInfo", &pDxDiagSystemInfo );
                if( SUCCEEDED( hr ) )
                {
                    VARIANT var;
                    VariantInit( &var );

                    // Get the "dwDirectXVersionMajor" property
                    hr = pDxDiagSystemInfo->GetProp( L"dwDirectXVersionMajor", &var );
                    if( SUCCEEDED( hr ) && var.vt == VT_UI4 )
                    {
                        if( pdwDirectXVersionMajor )
                            *pdwDirectXVersionMajor = var.ulVal;
                        bSuccessGettingMajor = true;
                    }
                    VariantClear( &var );

                    // Get the "dwDirectXVersionMinor" property
                    hr = pDxDiagSystemInfo->GetProp( L"dwDirectXVersionMinor", &var );
                    if( SUCCEEDED( hr ) && var.vt == VT_UI4 )
                    {
                        if( pdwDirectXVersionMinor )
                            *pdwDirectXVersionMinor = var.ulVal;
                        bSuccessGettingMinor = true;
                    }
                    VariantClear( &var );

                    // Get the "szDirectXVersionLetter" property
                    hr = pDxDiagSystemInfo->GetProp( L"szDirectXVersionLetter", &var );
                    if( SUCCEEDED( hr ) && var.vt == VT_BSTR && SysStringLen( var.bstrVal ) != 0 )
                    {
                        *pcDirectXVersionLetter = var.bstrVal[0];
                        bSuccessGettingLetter = true;
                    }
                    VariantClear( &var );

                    // If it all worked right, then mark it down
                    if( bSuccessGettingMajor && bSuccessGettingMinor && bSuccessGettingLetter )
                        bGotDirectXVersion = true;

                    pDxDiagSystemInfo->Release();
                }

                pDxDiagRoot->Release();
            }
        }

        pDxDiagProvider->Release();
    }

    if( bCleanupCOM )
        CoUninitialize();

    if( bGotDirectXVersion )
        return S_OK;
    else
        return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: GetDirectXVersionViaFileVersions()
// Desc: Tries to get the DirectX version by looking at DirectX file versions
//-----------------------------------------------------------------------------
HRESULT GetDirectXVersionViaFileVersions( DWORD* pdwDirectXVersionMajor,
                                          DWORD* pdwDirectXVersionMinor,
                                          WCHAR* pcDirectXVersionLetter )
{
    ULARGE_INTEGER llFileVersion;
    WCHAR szPath[512];
    WCHAR szFile[512];
    BOOL bFound = false;

    if( GetSystemDirectory( szPath, MAX_PATH ) != 0 )
    {
        szPath[MAX_PATH - 1] = 0;

        // Switch off the ddraw version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\ddraw.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 2, 0, 95 ) ) >= 0 ) // Win9x version
            {
                // flle is >= DX1.0 version, so we must be at least DX1.0
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 1;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }

            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 3, 0, 1096 ) ) >= 0 ) // Win9x version
            {
                // flle is is >= DX2.0 version, so we must DX2.0 or DX2.0a (no redist change)
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 2;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }

            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 4, 0, 68 ) ) >= 0 ) // Win9x version
            {
                // flle is is >= DX3.0 version, so we must be at least DX3.0
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 3;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }
        }

        // Switch off the d3drg8x.dll version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\d3drg8x.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 4, 0, 70 ) ) >= 0 ) // Win9x version
            {
                // d3drg8x.dll is the DX3.0a version, so we must be DX3.0a or DX3.0b  (no redist change)
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 3;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L'a';
                bFound = true;
            }
        }

        // Switch off the ddraw version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\ddraw.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 5, 0, 155 ) ) >= 0 ) // Win9x version
            {
                // ddraw.dll is the DX5.0 version, so we must be DX5.0 or DX5.2 (no redist change)
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 5;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }

            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 6, 0, 318 ) ) >= 0 ) // Win9x version
            {
                // ddraw.dll is the DX6.0 version, so we must be at least DX6.0
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 6;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }

            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 6, 0, 436 ) ) >= 0 ) // Win9x version
            {
                // ddraw.dll is the DX6.1 version, so we must be at least DX6.1
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 6;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 1;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }
        }

        // Switch off the dplayx.dll version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\dplayx.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 6, 3, 518 ) ) >= 0 ) // Win9x version
            {
                // ddraw.dll is the DX6.1 version, so we must be at least DX6.1a
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 6;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 1;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L'a';
                bFound = true;
            }
        }

        // Switch off the ddraw version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\ddraw.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 7, 0, 700 ) ) >= 0 ) // Win9x version
            {
                // TODO: find win2k version

                // ddraw.dll is the DX7.0 version, so we must be at least DX7.0
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 7;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }
        }

        // Switch off the dinput version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\dinput.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 4, 7, 0, 716 ) ) >= 0 ) // Win9x version
            {
                // ddraw.dll is the DX7.0 version, so we must be at least DX7.0a
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 7;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L'a';
                bFound = true;
            }
        }

        // Switch off the ddraw version
        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\ddraw.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( ( HIWORD( llFileVersion.HighPart ) == 4 && CompareLargeInts( llFileVersion,
                                                                             MakeInt64( 4, 8, 0, 400 ) ) >= 0 ) || // Win9x version
                ( HIWORD( llFileVersion.HighPart ) == 5 && CompareLargeInts( llFileVersion, MakeInt64( 5, 1, 2258,
                                                                                                       400 ) ) >= 0 ) ) // Win2k/WinXP version
            {
                // ddraw.dll is the DX8.0 version, so we must be at least DX8.0 or DX8.0a (no redist change)
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 8;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }
        }

        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\d3d8.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( ( HIWORD( llFileVersion.HighPart ) == 4 && CompareLargeInts( llFileVersion,
                                                                             MakeInt64( 4, 8, 1, 881 ) ) >= 0 ) || // Win9x version
                ( HIWORD( llFileVersion.HighPart ) == 5 && CompareLargeInts( llFileVersion, MakeInt64( 5, 1, 2600,
                                                                                                       881 ) ) >= 0 ) ) // Win2k/WinXP version
            {
                // d3d8.dll is the DX8.1 version, so we must be at least DX8.1
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 8;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 1;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }

            if( ( HIWORD( llFileVersion.HighPart ) == 4 && CompareLargeInts( llFileVersion,
                                                                             MakeInt64( 4, 8, 1, 901 ) ) >= 0 ) || // Win9x version
                ( HIWORD( llFileVersion.HighPart ) == 5 && CompareLargeInts( llFileVersion, MakeInt64( 5, 1, 2600,
                                                                                                       901 ) ) >= 0 ) ) // Win2k/WinXP version
            {
                // d3d8.dll is the DX8.1a version, so we must be at least DX8.1a
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 8;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 1;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L'a';
                bFound = true;
            }
        }

        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\mpg2splt.ax" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( CompareLargeInts( llFileVersion, MakeInt64( 6, 3, 1, 885 ) ) >= 0 ) // Win9x/Win2k/WinXP version
            {
                // quartz.dll is the DX8.1b version, so we must be at least DX8.1b
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 8;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 1;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L'b';
                bFound = true;
            }
        }

        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\dpnet.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            if( ( HIWORD( llFileVersion.HighPart ) == 4 && CompareLargeInts( llFileVersion,
                                                                             MakeInt64( 4, 9, 0, 134 ) ) >= 0 ) || // Win9x version
                ( HIWORD( llFileVersion.HighPart ) == 5 && CompareLargeInts( llFileVersion, MakeInt64( 5, 2, 3677,
                                                                                                       134 ) ) >= 0 ) ) // Win2k/WinXP version
            {
                // dpnet.dll is the DX8.2 version, so we must be at least DX8.2
                if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 8;
                if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 2;
                if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
                bFound = true;
            }
        }

        wcscpy_s( szFile, 512, szPath );
        wcscat_s( szFile, 512, L"\\d3d9.dll" );
        if( SUCCEEDED( GetFileVersion( szFile, &llFileVersion ) ) )
        {
            // File exists, but be at least DX9
            if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 9;
            if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
            if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
            bFound = true;
        }
    }

    if( !bFound )
    {
        // No DirectX installed
        if( pdwDirectXVersionMajor ) *pdwDirectXVersionMajor = 0;
        if( pdwDirectXVersionMinor ) *pdwDirectXVersionMinor = 0;
        if( pcDirectXVersionLetter ) *pcDirectXVersionLetter = L' ';
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetFileVersion()
// Desc: Returns ULARGE_INTEGER with a file version of a file, or a failure code.
//-----------------------------------------------------------------------------
HRESULT GetFileVersion( WCHAR* szPath, ULARGE_INTEGER* pllFileVersion )
{
    if( szPath == NULL || pllFileVersion == NULL )
        return E_INVALIDARG;

    DWORD dwHandle;
    UINT cb;
    cb = GetFileVersionInfoSize( szPath, &dwHandle );
    if( cb > 0 )
    {
        BYTE* pFileVersionBuffer = new BYTE[cb];
        if( pFileVersionBuffer == NULL )
            return E_OUTOFMEMORY;

        if( GetFileVersionInfo( szPath, 0, cb, pFileVersionBuffer ) )
        {
            VS_FIXEDFILEINFO* pVersion = NULL;
            if( VerQueryValue( pFileVersionBuffer, L"\\", ( VOID** )&pVersion, &cb ) &&
                pVersion != NULL )
            {
                pllFileVersion->HighPart = pVersion->dwFileVersionMS;
                pllFileVersion->LowPart = pVersion->dwFileVersionLS;
                delete[] pFileVersionBuffer;
                return S_OK;
            }
        }

        delete[] pFileVersionBuffer;
    }

    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: MakeInt64()
// Desc: Returns a ULARGE_INTEGER where a<<48|b<<32|c<<16|d<<0
//-----------------------------------------------------------------------------
ULARGE_INTEGER MakeInt64( WORD a, WORD b, WORD c, WORD d )
{
    ULARGE_INTEGER ull;
    ull.HighPart = MAKELONG( b, a );
    ull.LowPart = MAKELONG( d, c );
    return ull;
}




//-----------------------------------------------------------------------------
// Name: CompareLargeInts()
// Desc: Returns 1 if ullParam1 > ullParam2
//       Returns 0 if ullParam1 = ullParam2
//       Returns -1 if ullParam1 < ullParam2
//-----------------------------------------------------------------------------
int CompareLargeInts( ULARGE_INTEGER ullParam1, ULARGE_INTEGER ullParam2 )
{
    if( ullParam1.HighPart > ullParam2.HighPart )
        return 1;
    if( ullParam1.HighPart < ullParam2.HighPart )
        return -1;

    if( ullParam1.LowPart > ullParam2.LowPart )
        return 1;
    if( ullParam1.LowPart < ullParam2.LowPart )
        return -1;

    return 0;
}
