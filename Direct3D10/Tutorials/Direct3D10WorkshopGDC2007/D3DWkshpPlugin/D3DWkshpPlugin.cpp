//==================================================================================================
// D3DWkshpPlugin.cpp
//
// Copyright (c) Microsoft Corporation, All rights Reserved
//==================================================================================================

#include <Windows.h>
#include <stdio.h>
#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )
#include <PIXPlugin.h>
#include "Resource.h"
#include "MemoryMapping.h"

// Indicate the version of this plugin
#define SAMPLE_PLUGIN_VERSION 0x0102


// Macro for determining the number of elements in an array
#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(x)  (sizeof(x)/sizeof((x)[0]))
#endif


// Unique identifiers for each counter implemented in this plugin
enum MYCOUNTERID
{
    CTR_FIRST,
    CTR_EFFECTSCALING = CTR_FIRST,
    CTR_EFFECTLIMIT,
    CTR_EFFECTLIFE,
    CTR_NORMALIZATION,
    CTR_UPDATEFREQUENCY,
    CTR_STREAMOUT,
    CTR_SCREENEFFECTS,
    CTR_FIREWORKS,
    CTR_COUNT
};

//==================================================================================================
// pixCounterSet - Lists all counters exposed by this plugin.  It is used by PIXGetCounterInfo.  In
//                 this example we provide a fixed set of counters, but a plugin could use a 
//                 variable set instead.
//==================================================================================================
PIXCOUNTERINFO pixCounterSet[] =
{
    { CTR_EFFECTSCALING,    L"Scaling",			   PCDT_FLOAT },
    { CTR_EFFECTLIMIT,      L"EffectLimit",        PCDT_FLOAT },
    { CTR_EFFECTLIFE,		L"EffectLife",         PCDT_FLOAT },
    { CTR_NORMALIZATION,	L"Normalization",      PCDT_FLOAT },
    { CTR_UPDATEFREQUENCY,	L"UpdateFrequency",    PCDT_FLOAT },

    { CTR_STREAMOUT,		L"StreamOut",          PCDT_INT },
    { CTR_SCREENEFFECTS,	L"ScreenEffects",      PCDT_INT },
    { CTR_FIREWORKS,		L"Fireworks",		   PCDT_INT },
};


//==================================================================================================
// pixCounterDesc - Description strings for all counters.  It is used by PIXGetCounterDesc.  This
//                  example shows how to use constant strings embedded in the code.  Another 
//                  approach would be to read the description strings from resources.
//==================================================================================================
WCHAR* pixCounterDesc[] =
{
    L"Height of the spectrogram.",
    L"Limit at which geometry amplification kicks in.",
    L"Life of the amplified geometry in seconds.",
    L"How much normalization to apply to the spectrogram.",
    L"Update frequency for the spectrogram data and FFT.",
    L"Whether StreamOut and amplification are active.",
    L"Whether screen effects are active.",
    L"Whether fireworks are active."
};


//==================================================================================================
// SharedMemoryInit - Called at the beginning of the experiment
//==================================================================================================
BOOL SharedMemoryInit( const WCHAR* pApplication )
{
    if( g_bSharedMemoryInit )
        return TRUE;

    // Init shared memory
    g_hMapFile = CreateFileMapping( INVALID_HANDLE_VALUE,
                                    NULL,
                                    PAGE_READWRITE,
                                    0,
                                    sizeof( SHARED_MEMORY_DATA ),
                                    g_szSharedName );
    if( NULL == g_hMapFile )
    {
        return FALSE;
    }

    // map a view of the file
    g_pSharedData = ( SHARED_MEMORY_DATA* )MapViewOfFile( g_hMapFile,
                                                          FILE_MAP_READ,
                                                          0,
                                                          0,
                                                          sizeof( SHARED_MEMORY_DATA ) );
    if( NULL == g_pSharedData )
    {
        return FALSE;
    }

    g_bSharedMemoryInit = true;
    return TRUE;
}



//==================================================================================================
// SharedMemoryUninit - Called at the end of the experiment
//==================================================================================================
BOOL SharedMemoryUninit()
{
    if( !g_bSharedMemoryInit )
        return TRUE;

    UnmapViewOfFile( g_pSharedData );
    CloseHandle( g_hMapFile );

    g_bSharedMemoryInit = false;
    return TRUE;
}


//==================================================================================================
BOOL GetScaling( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( float );
    *ppReturnData = ( BYTE* )&g_pSharedData->Scaling;
    return TRUE;
}


//==================================================================================================
BOOL GetEffectLimit( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( float );
    *ppReturnData = ( BYTE* )&g_pSharedData->EffectLimit;
    return TRUE;
}


//==================================================================================================
BOOL GetEffectLife( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( float );
    *ppReturnData = ( BYTE* )&g_pSharedData->EffectLife;
    return TRUE;
}


//==================================================================================================
BOOL GetNormalization( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( float );
    *ppReturnData = ( BYTE* )&g_pSharedData->Normalization;
    return TRUE;
}


//==================================================================================================
BOOL GetUpdateFrequency( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( float );
    *ppReturnData = ( BYTE* )&g_pSharedData->UpdateFrequency;
    return TRUE;
}


//==================================================================================================
BOOL GetStreamOut( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->StreamOut;
    return TRUE;
}


//==================================================================================================
BOOL GetScreenEffects( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->ScreenEffects;
    return TRUE;
}


//==================================================================================================
BOOL GetFireworks( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->Fireworks;
    return TRUE;
}


//==================================================================================================
// PIXGetPluginInfo
//==================================================================================================
BOOL WINAPI PIXGetPluginInfo( PIXPLUGININFO* pPIXPluginInfo )
{
    // If we want the hInst of the app, we can get it from pPIXPluginInfo->hinst

    //
    // Fill in our information and give it to PIX
    //
    pPIXPluginInfo->iPluginVersion = SAMPLE_PLUGIN_VERSION; //version of this plugin

    pPIXPluginInfo->iPluginSystemVersion = PIX_PLUGIN_SYSTEM_VERSION; //version of PIX we are designed to work with

    pPIXPluginInfo->pstrPluginName = L"D3D GDC2007 Workshop Plugin"; // name of this plugin

    return TRUE;
}


//==================================================================================================
// PIXGetCounterInfo
//==================================================================================================
BOOL WINAPI PIXGetCounterInfo( DWORD* pdwReturnCounters, PIXCOUNTERINFO** ppCounterInfoList )
{
    *pdwReturnCounters = ARRAY_LENGTH(pixCounterSet);
    *ppCounterInfoList = &pixCounterSet[0];
    return TRUE;
}


//==================================================================================================
// PIXGetCounterDesc
//==================================================================================================
BOOL WINAPI PIXGetCounterDesc( PIXCOUNTERID id, WCHAR** ppstrCounterDesc )
{
    if( id < CTR_FIRST || id >= CTR_COUNT )
        return FALSE;
    *ppstrCounterDesc = pixCounterDesc[id];
    return TRUE;
}


//==================================================================================================
// PIXBeginExperiment
//==================================================================================================
BOOL WINAPI PIXBeginExperiment( PIXCOUNTERID id, const WCHAR* pstrApplication )
{
    switch( id )
    {
        case CTR_EFFECTSCALING:
            return SharedMemoryInit( pstrApplication );
        case CTR_EFFECTLIMIT:
            return SharedMemoryInit( pstrApplication );
        case CTR_EFFECTLIFE:
            return SharedMemoryInit( pstrApplication );
        case CTR_NORMALIZATION:
            return SharedMemoryInit( pstrApplication );
        case CTR_UPDATEFREQUENCY:
            return SharedMemoryInit( pstrApplication );
        case CTR_STREAMOUT:
            return SharedMemoryInit( pstrApplication );
        case CTR_SCREENEFFECTS:
            return SharedMemoryInit( pstrApplication );
        case CTR_FIREWORKS:
            return SharedMemoryInit( pstrApplication );
        default:
            return FALSE;
    }
}


//==================================================================================================
// PIXEndFrame
//==================================================================================================
BOOL WINAPI PIXEndFrame( PIXCOUNTERID id, UINT iFrame, DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    switch( id )
    {
        case CTR_EFFECTSCALING:
            return GetScaling( pdwReturnBytes, ppReturnData );
        case CTR_EFFECTLIMIT:
            return GetEffectLimit( pdwReturnBytes, ppReturnData );
        case CTR_EFFECTLIFE:
            return GetEffectLife( pdwReturnBytes, ppReturnData );
        case CTR_NORMALIZATION:
            return GetNormalization( pdwReturnBytes, ppReturnData );
        case CTR_UPDATEFREQUENCY:
            return GetUpdateFrequency( pdwReturnBytes, ppReturnData );
        case CTR_STREAMOUT:
            return GetStreamOut( pdwReturnBytes, ppReturnData );
        case CTR_SCREENEFFECTS:
            return GetScreenEffects( pdwReturnBytes, ppReturnData );
        case CTR_FIREWORKS:
            return GetFireworks( pdwReturnBytes, ppReturnData );
        default:
            return FALSE;
    }
}


//==================================================================================================
// PIXEndExperiment
//==================================================================================================
BOOL WINAPI PIXEndExperiment( PIXCOUNTERID id )
{
    switch( id )
    {
        case CTR_EFFECTSCALING:
            return SharedMemoryUninit();
        case CTR_EFFECTLIMIT:
            return SharedMemoryUninit();
        case CTR_EFFECTLIFE:
            return SharedMemoryUninit();
        case CTR_NORMALIZATION:
            return SharedMemoryUninit();
        case CTR_UPDATEFREQUENCY:
            return SharedMemoryUninit();
        case CTR_STREAMOUT:
            return SharedMemoryUninit();
        case CTR_SCREENEFFECTS:
            return SharedMemoryUninit();
        case CTR_FIREWORKS:
            return SharedMemoryUninit();
        default:
            return FALSE;
    }
}


//==================================================================================================
// eof: D3DWkshpPlugin.cpp
//==================================================================================================
