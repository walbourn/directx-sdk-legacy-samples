//==================================================================================================
// PIXWkshpPlugin.cpp
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
    CTR_TERRAINTILESDRAWN = CTR_FIRST,
    CTR_GRASSTILESDRAWN,
    CTR_BALLSDRAWN,
    CTR_MAXBALLSPERGRID,
    CTR_COUNT
};

//==================================================================================================
// pixCounterSet - Lists all counters exposed by this plugin.  It is used by PIXGetCounterInfo.  In
//                 this example we provide a fixed set of counters, but a plugin could use a 
//                 variable set instead.
//==================================================================================================
PIXCOUNTERINFO pixCounterSet[] =
{
    { CTR_TERRAINTILESDRAWN,    L"TerrainTilesDrawn",      PCDT_INT },
    { CTR_GRASSTILESDRAWN,      L"GrassTilesDrawn",        PCDT_INT },
    { CTR_BALLSDRAWN,			L"BallsDrawn",             PCDT_INT },
    { CTR_MAXBALLSPERGRID,		L"MaxBallsPerGrid",        PCDT_INT },
};


//==================================================================================================
// pixCounterDesc - Description strings for all counters.  It is used by PIXGetCounterDesc.  This
//                  example shows how to use constant strings embedded in the code.  Another 
//                  approach would be to read the description strings from resources.
//==================================================================================================
WCHAR* pixCounterDesc[] =
{
    L"Number of terrain tiles drawn in this frame.",
    L"Number of grass tiles drawn in this frame.",
    L"Number of balls drawn in this frame.",
    L"Maximum number of balls in one grid cell during this frame."
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
BOOL GetTerrainTiles( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->NumTerrainTiles;
    return TRUE;
}


//==================================================================================================
BOOL GetGrassTiles( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->NumGrassTiles;
    return TRUE;
}


//==================================================================================================
BOOL GetNumBalls( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->NumBalls;
    return TRUE;
}


//==================================================================================================
BOOL GetMaxBalls( DWORD* pdwReturnBytes, BYTE** ppReturnData )
{
    *pdwReturnBytes = sizeof( int );
    *ppReturnData = ( BYTE* )&g_pSharedData->MaxBalls;
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

    pPIXPluginInfo->pstrPluginName = L"PIX GDC2007 Workshop Plugin"; // name of this plugin

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
        case CTR_TERRAINTILESDRAWN:
            return SharedMemoryInit( pstrApplication );
        case CTR_GRASSTILESDRAWN:
            return SharedMemoryInit( pstrApplication );
        case CTR_BALLSDRAWN:
            return SharedMemoryInit( pstrApplication );
        case CTR_MAXBALLSPERGRID:
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
        case CTR_TERRAINTILESDRAWN:
            return GetTerrainTiles( pdwReturnBytes, ppReturnData );
        case CTR_GRASSTILESDRAWN:
            return GetGrassTiles( pdwReturnBytes, ppReturnData );
        case CTR_BALLSDRAWN:
            return GetNumBalls( pdwReturnBytes, ppReturnData );
        case CTR_MAXBALLSPERGRID:
            return GetMaxBalls( pdwReturnBytes, ppReturnData );
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
        case CTR_TERRAINTILESDRAWN:
            return SharedMemoryUninit();
        case CTR_GRASSTILESDRAWN:
            return SharedMemoryUninit();
        case CTR_BALLSDRAWN:
            return SharedMemoryUninit();
        case CTR_MAXBALLSPERGRID:
            return SharedMemoryUninit();
        default:
            return FALSE;
    }
}


//==================================================================================================
// eof: PIXWkshpPlugin.cpp
//==================================================================================================
