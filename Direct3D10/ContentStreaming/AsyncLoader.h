//--------------------------------------------------------------------------------------
// File: AsyncLoader.h
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

#include "DXUT.h"
#include "SDKMesh.h"
#include "ResourceReuseCache.h"

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
class IDataLoader;
class IDataProcessor;
void WarmIOCache( BYTE* pData, SIZE_T size );

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct RESOURCE_REQUEST
{
    IDataLoader* pDataLoader;
    IDataProcessor* pDataProcessor;
    HRESULT* pHR;
    void** ppDeviceObject;
    bool bLock;
    bool bCopy;

    bool bError;
};

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
class CAsyncLoader
{
private:
    BOOL m_bDone;
    BOOL m_bProcessThreadDone;
    BOOL m_bIOThreadDone;
    UINT m_NumResourcesToService;
    UINT m_NumOustandingResources;
    CGrowableArray <RESOURCE_REQUEST> m_IOQueue;
    CGrowableArray <RESOURCE_REQUEST> m_ProcessQueue;
    CGrowableArray <RESOURCE_REQUEST> m_RenderThreadQueue;
    CRITICAL_SECTION m_csIOQueue;
    CRITICAL_SECTION m_csProcessQueue;
    CRITICAL_SECTION m_csRenderThreadQueue;
    HANDLE m_hIOQueueSemaphore;
    HANDLE m_hProcessQueueSemaphore;
    HANDLE m_hCopyQueueSemaphore;
    HANDLE m_hIOThread;
    UINT m_NumProcessingThreads;
    HANDLE* m_phProcessThreads;
    UINT m_NumIORequests;
    UINT m_NumProcessRequests;

private:
    unsigned int                FileIOThreadProc();
    unsigned int                ProcessingThreadProc();
    bool                        InitAsyncLoadingThreadObjects( UINT NumProcessingThreads );
    void                        DestroyAsyncLoadingThreadObjects();

public:
    friend unsigned int WINAPI  _FileIOThreadProc( LPVOID lpParameter );
    friend unsigned int WINAPI  _ProcessingThreadProc( LPVOID lpParameter );

                                CAsyncLoader( UINT NumProcessingThreads );
                                ~CAsyncLoader();

    HRESULT                     AddWorkItem( IDataLoader* pDataLoader, IDataProcessor* pDataProcessor,
                                             HRESULT* pHResult, void** ppDeviceObject );
    void                        WaitForAllItems();
    void                        ProcessDeviceWorkItems( UINT CurrentNumResourcesToService, BOOL bRetryLoads=TRUE );
};

#endif
