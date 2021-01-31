//----------------------------------------------------------------------------
// File: PRTSim.cpp
//
// Desc: 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include <stdio.h>
#include <conio.h>
#include "config.h"
#include "PRTSim.h"

struct PRT_STATE
{
    CRITICAL_SECTION cs;
    bool bUserAbort;
    bool bStopSimulator;
    bool bRunning;
    bool bFailed;
    bool bProgressMode;
    DWORD nNumPasses;
    DWORD nCurPass;
    float fPercentDone;
    WCHAR strCurPass[256];
};

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
HRESULT RunPRTSimulator( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions, CONCAT_MESH* pPRTMesh,
                         CONCAT_MESH* pBlockerMesh );
HRESULT WINAPI StaticPRTSimulatorCB( float fPercentDone, LPVOID pParam );

//-----------------------------------------------------------------------------
// static helper function
//-----------------------------------------------------------------------------
DWORD WINAPI StaticUIThreadProc( LPVOID lpParameter )
{
    PRT_STATE* pPRTState = ( PRT_STATE* )lpParameter;
    WCHAR sz[256] = {0};
    bool bStop = false;
    float fLastPercent = -1.0f;
    double fLastPercentAnnounceTime = 0.0f;

    DXUTGetGlobalTimer()->Start();

    while( !bStop )
    {
        EnterCriticalSection( &pPRTState->cs );

        if( pPRTState->bProgressMode )
        {
            if( pPRTState->fPercentDone < 0.0f || DXUTGetGlobalTimer()->GetTime() < fLastPercentAnnounceTime + 5.0f )
                swprintf_s( sz, 256, L"." );
            else
            {
                swprintf_s( sz, 256, L"%0.0f%%%%.", pPRTState->fPercentDone * 100.0f );
                fLastPercent = pPRTState->fPercentDone;
                fLastPercentAnnounceTime = DXUTGetGlobalTimer()->GetTime();
            }
            wprintf( sz );
        }

        while( _kbhit() )
        {
            int nChar = _getch();
            if( nChar == VK_ESCAPE )
            {
                wprintf( L"\n\nEscape key pressed. Aborting!\n" );
                pPRTState->bStopSimulator = true;
                pPRTState->bUserAbort = true;
                break;
            }
        }

        bStop = pPRTState->bStopSimulator;
        LeaveCriticalSection( &pPRTState->cs );

        if( !bStop )
            Sleep( 1000 );
    }

    return 0;
}

//-----------------------------------------------------------------------------
HRESULT RunPRTSimulator( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions,
                         CONCAT_MESH* pPRTMesh, CONCAT_MESH* pBlockerMesh, SETTINGS* pSettings )
{
    DWORD dwResult;
    DWORD dwThreadId;

    int nNumMaterials = pPRTMesh->shMaterialArray.GetSize();

    // Check if any SH material need subsurface scattering
    bool bSubsurfaceScattering = false;
    for( int iSH = 0; iSH < nNumMaterials; iSH++ )
    {
        if( pPRTMesh->shMaterialArray[iSH].bSubSurf )
        {
            bSubsurfaceScattering = true;
            break;
        }
    }

    PRT_STATE prtState;
    ZeroMemory( &prtState, sizeof( PRT_STATE ) );
    InitializeCriticalSection( &prtState.cs );
    prtState.bStopSimulator = false;
    prtState.bRunning = true;
    prtState.nNumPasses = pOptions->dwNumBounces;
    if( bSubsurfaceScattering )
        prtState.nNumPasses *= 2;
    if( pOptions->bEnableTessellation && pOptions->bRobustMeshRefine )
        prtState.nNumPasses++;
    if( pOptions->bEnableTessellation )
        prtState.nNumPasses++;
    if( pOptions->bEnableCompression )
        prtState.nNumPasses += 2;
    prtState.nNumPasses += 2;
    prtState.nCurPass = 1;
    prtState.fPercentDone = -1.0f;
    prtState.bProgressMode = true;
    wprintf( L"\nStarting simulator.  Press ESC to abort\n" );
    swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Initializing PRT engine..", prtState.nCurPass,
                     prtState.nNumPasses );
    wprintf( prtState.strCurPass );

    HANDLE hThreadId = CreateThread( NULL, 0, StaticUIThreadProc, ( LPVOID )&prtState, 0, &dwThreadId );

    HRESULT hr;
    ID3DXPRTEngine* pPRTEngine = NULL;

    ID3DXPRTBuffer* pDataTotal = NULL;
    ID3DXPRTBuffer* pBufferA = NULL;
    ID3DXPRTBuffer* pBufferB = NULL;
    ID3DXPRTBuffer* pBufferC = NULL;
    ID3DXPRTCompBuffer* pPRTCompBuffer = NULL;

    DWORD* pdwAdj = new DWORD[pPRTMesh->pMesh->GetNumFaces() * 3];
    pPRTMesh->pMesh->GenerateAdjacency( 1e-6f, pdwAdj );
    V( D3DXCreatePRTEngine( pPRTMesh->pMesh, pdwAdj, FALSE, pBlockerMesh->pMesh, &pPRTEngine ) );
    delete[] pdwAdj;

    V( pPRTEngine->SetCallBack( StaticPRTSimulatorCB, 0.001f, &prtState ) );
    V( pPRTEngine->SetSamplingInfo( pOptions->dwNumRays, FALSE, TRUE, FALSE, 0.0f ) );

    //    if( pOptions->bEnableTessellation && pPRTMesh->materialArray.GetAt(1).->GetAlbedoTexture() )
    {
        //      V( pPRTEngine->SetPerTexelAlbedo( m_pPRTMesh->GetAlbedoTexture(), 
        //                                        pOptions->dwNumChannels, NULL ) );
    }

    bool bSetAlbedoFromMaterial = true;
    //    if( pOptions->bEnableTessellation && m_pPRTMesh->GetAlbedoTexture() )
    //        bSetAlbedoFromMaterial = false;

    D3DXSHMATERIAL* pMatPtr = pPRTMesh->shMaterialArray.GetData();
    D3DXSHMATERIAL** pMatPtrArray = new D3DXSHMATERIAL*[nNumMaterials];
    if( pMatPtrArray == NULL )
        return E_OUTOFMEMORY;
    for( int i = 0; i < nNumMaterials; ++i )
        pMatPtrArray[i] = &pMatPtr[i];

    V( pPRTEngine->SetMeshMaterials( ( const D3DXSHMATERIAL** )pMatPtrArray, nNumMaterials,
                                     pOptions->dwNumChannels,
                                     bSetAlbedoFromMaterial, pOptions->fLengthScale ) );

    if( !bSubsurfaceScattering )
    {
        // Not doing subsurface scattering
        if( pOptions->bEnableTessellation && pOptions->bRobustMeshRefine )
        {
            EnterCriticalSection( &prtState.cs );
            prtState.nCurPass++;
            prtState.fPercentDone = -1.0f;
            swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Robust Mesh Refine..", prtState.nCurPass,
                             prtState.nNumPasses );
            wprintf( prtState.strCurPass );
            LeaveCriticalSection( &prtState.cs );

            V( pPRTEngine->RobustMeshRefine( pOptions->fRobustMeshRefineMinEdgeLength,
                                             pOptions->dwRobustMeshRefineMaxSubdiv ) );
        }

        DWORD dwNumSamples = pPRTEngine->GetNumVerts();
        V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                pOptions->dwNumChannels, &pDataTotal ) );

        EnterCriticalSection( &prtState.cs );
        prtState.nCurPass++;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Direct Lighting..", prtState.nCurPass,
                         prtState.nNumPasses );
        wprintf( prtState.strCurPass );
        prtState.fPercentDone = 0.0f;
        LeaveCriticalSection( &prtState.cs );

        if( pOptions->bEnableTessellation && pOptions->bAdaptiveDL )
        {
            hr = pPRTEngine->ComputeDirectLightingSHAdaptive( pOptions->dwOrder,
                                                              pOptions->fAdaptiveDLThreshold,
                                                              pOptions->fAdaptiveDLMinEdgeLength,
                                                              pOptions->dwAdaptiveDLMaxSubdiv,
                                                              pDataTotal );
            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 
        }
        else
        {
            hr = pPRTEngine->ComputeDirectLightingSH( pOptions->dwOrder, pDataTotal );
            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 
        }

        if( pOptions->dwNumBounces > 1 )
        {
            dwNumSamples = pPRTEngine->GetNumVerts();
            V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                    pOptions->dwNumChannels, &pBufferA ) );
            V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                    pOptions->dwNumChannels, &pBufferB ) );
            V( pBufferA->AddBuffer( pDataTotal ) );
        }

        for( UINT iBounce = 1; iBounce < pOptions->dwNumBounces; ++iBounce )
        {
            EnterCriticalSection( &prtState.cs );
            prtState.nCurPass++;
            swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Bounce %d Lighting..",
                             prtState.nCurPass, prtState.nNumPasses, iBounce + 1 );
            wprintf( prtState.strCurPass );
            prtState.fPercentDone = 0.0f;
            LeaveCriticalSection( &prtState.cs );

            if( pOptions->bEnableTessellation && pOptions->bAdaptiveBounce )
                hr = pPRTEngine->ComputeBounceAdaptive( pBufferA, pOptions->fAdaptiveBounceThreshold,
                                                        pOptions->fAdaptiveBounceMinEdgeLength,
                                                        pOptions->dwAdaptiveBounceMaxSubdiv, pBufferB, pDataTotal );
            else
                hr = pPRTEngine->ComputeBounce( pBufferA, pBufferB, pDataTotal );

            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 

            // Swap pBufferA and pBufferB
            ID3DXPRTBuffer* pPRTBufferTemp = NULL;
            pPRTBufferTemp = pBufferA;
            pBufferA = pBufferB;
            pBufferB = pPRTBufferTemp;
        }
    }
    else
    {
        // Doing subsurface scattering
        if( pOptions->bEnableTessellation && pOptions->bRobustMeshRefine )
            V( pPRTEngine->RobustMeshRefine( pOptions->fRobustMeshRefineMinEdgeLength,
                                             pOptions->dwRobustMeshRefineMaxSubdiv ) );

        DWORD dwNumSamples = pPRTEngine->GetNumVerts();
        V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                pOptions->dwNumChannels, &pBufferA ) );
        V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                pOptions->dwNumChannels, &pBufferB ) );
        V( D3DXCreatePRTBuffer( dwNumSamples, pOptions->dwOrder * pOptions->dwOrder,
                                pOptions->dwNumChannels, &pDataTotal ) );

        EnterCriticalSection( &prtState.cs );
        prtState.nCurPass++;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Direct Lighting..", prtState.nCurPass,
                         prtState.nNumPasses );
        wprintf( prtState.strCurPass );
        prtState.fPercentDone = 0.0f;
        LeaveCriticalSection( &prtState.cs );

        if( pOptions->bEnableTessellation && pOptions->bAdaptiveDL )
        {
            hr = pPRTEngine->ComputeDirectLightingSHAdaptive( pOptions->dwOrder,
                                                              pOptions->fAdaptiveDLThreshold,
                                                              pOptions->fAdaptiveDLMinEdgeLength,
                                                              pOptions->dwAdaptiveDLMaxSubdiv,
                                                              pBufferA );
            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 
        }
        else
        {
            hr = pPRTEngine->ComputeDirectLightingSH( pOptions->dwOrder, pBufferA );
            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 
        }

        EnterCriticalSection( &prtState.cs );
        prtState.nCurPass++;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Subsurface Direct Lighting..",
                         prtState.nCurPass, prtState.nNumPasses );
        wprintf( prtState.strCurPass );
        LeaveCriticalSection( &prtState.cs );

        hr = pPRTEngine->ComputeSS( pBufferA, pBufferB, pDataTotal );
        if( FAILED( hr ) )
            goto LEarlyExit; // handle user aborting simulator via callback 

        for( UINT iBounce = 1; iBounce < pOptions->dwNumBounces; ++iBounce )
        {
            EnterCriticalSection( &prtState.cs );
            prtState.nCurPass++;
            swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Bounce %d Lighting..",
                             prtState.nCurPass, prtState.nNumPasses, iBounce + 1 );
            wprintf( prtState.strCurPass );
            prtState.fPercentDone = 0.0f;
            LeaveCriticalSection( &prtState.cs );

            if( pOptions->bEnableTessellation && pOptions->bAdaptiveBounce )
                hr = pPRTEngine->ComputeBounceAdaptive( pBufferB, pOptions->fAdaptiveBounceThreshold,
                                                        pOptions->fAdaptiveBounceMinEdgeLength,
                                                        pOptions->dwAdaptiveBounceMaxSubdiv, pBufferA, NULL );
            else
                hr = pPRTEngine->ComputeBounce( pBufferB, pBufferA, NULL );

            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 

            EnterCriticalSection( &prtState.cs );
            prtState.nCurPass++;
            swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Computing Subsurface Bounce %d Lighting..",
                             prtState.nCurPass, prtState.nNumPasses, iBounce + 1 );
            wprintf( prtState.strCurPass );
            LeaveCriticalSection( &prtState.cs );

            hr = pPRTEngine->ComputeSS( pBufferA, pBufferB, pDataTotal );
            if( FAILED( hr ) )
                goto LEarlyExit; // handle user aborting simulator via callback 
        }

    }

    if( pOptions->bEnableTessellation )
    {
        ID3DXMesh* pAdaptedMesh;
        V( pPRTEngine->GetAdaptedMesh( pd3dDevice, NULL, NULL, NULL, &pAdaptedMesh ) );

        DWORD dwNumAttribs = 0;
        V( pAdaptedMesh->GetAttributeTable( NULL, &dwNumAttribs ) );
        if( dwNumAttribs == 0 )
        {
            // Compact & attribute sort mesh
            DWORD* rgdwAdjacency = NULL;
            rgdwAdjacency = new DWORD[pAdaptedMesh->GetNumFaces() * 3];
            if( rgdwAdjacency == NULL )
                return E_OUTOFMEMORY;
            V_RETURN( pAdaptedMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
            V_RETURN( pAdaptedMesh->OptimizeInplace( D3DXMESHOPT_COMPACT | D3DXMESHOPT_ATTRSORT,
                                                     rgdwAdjacency, NULL, NULL, NULL ) );
            delete []rgdwAdjacency;
        }
        V( pAdaptedMesh->GetAttributeTable( NULL, &dwNumAttribs ) );
        assert( dwNumAttribs == pPRTMesh->dwNumMaterials );

        EnterCriticalSection( &prtState.cs );
        prtState.nCurPass++;
        prtState.fPercentDone = 0.0f;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Saving tessellated mesh to %s",
                         prtState.nCurPass, prtState.nNumPasses, pOptions->strOutputTessellatedMesh );
        wprintf( prtState.strCurPass );
        prtState.bProgressMode = false;
        LeaveCriticalSection( &prtState.cs );

        // Save the mesh
        DWORD dwFlags = ( pOptions->bBinaryXFile ) ? D3DXF_FILEFORMAT_BINARY : D3DXF_FILEFORMAT_TEXT;
        V( D3DXSaveMeshToX( pOptions->strOutputTessellatedMesh, pAdaptedMesh, NULL,
                            pPRTMesh->materialArray.GetData(), NULL, pPRTMesh->dwNumMaterials,
                            dwFlags ) );
    }

    SAFE_RELEASE( pBufferA );
    SAFE_RELEASE( pBufferB );

    EnterCriticalSection( &prtState.cs );
    prtState.nCurPass++;
    prtState.fPercentDone = 0.0f;
    swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Saving PRT buffer to %s", prtState.nCurPass,
                     prtState.nNumPasses, pOptions->strOutputPRTBuffer );
    wprintf( prtState.strCurPass );
    prtState.bProgressMode = false;
    LeaveCriticalSection( &prtState.cs );

    // Save the PRT buffer results for future sessions so you can skip the simulator step
    // You can also use D3DXSavePRTCompBufferToFile to save the compressed PRT buffer to 
    // skip the PRT compression step upon load.  
    if( FAILED( hr = D3DXSavePRTBufferToFile( pOptions->strOutputPRTBuffer, pDataTotal ) ) )
    {
        WCHAR sz[256];
        swprintf_s( sz, 256, L"\nError: Failed saving to %s", pOptions->strOutputPRTBuffer );
        wprintf( sz );
        goto LEarlyExit;
    }

    if( pOptions->bEnableCompression )
    {
        EnterCriticalSection( &prtState.cs );
        prtState.bProgressMode = true;
        prtState.nCurPass++;
        prtState.fPercentDone = 0.0f;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Compressing PRT buffer..", prtState.nCurPass,
                         prtState.nNumPasses );
        wprintf( prtState.strCurPass );
        LeaveCriticalSection( &prtState.cs );

        // Limit the max number of PCA vectors to # channels * # coeffs
        hr = D3DXCreatePRTCompBuffer( pOptions->Quality, pOptions->dwNumClusters, pOptions->dwNumPCA,
                                      StaticPRTSimulatorCB, &prtState, pDataTotal, &pPRTCompBuffer );
        if( FAILED( hr ) )
            goto LEarlyExit; // handle user aborting simulator via callback 

        EnterCriticalSection( &prtState.cs );
        prtState.bProgressMode = false;
        prtState.nCurPass++;
        prtState.fPercentDone = 0.0f;
        swprintf_s( prtState.strCurPass, 256, L"\nStage %d of %d: Saving Compressed PRT buffer to %s",
                         prtState.nCurPass, prtState.nNumPasses, pOptions->strOutputCompPRTBuffer );
        wprintf( prtState.strCurPass );
        swprintf_s( prtState.strCurPass, 256, L"" );
        LeaveCriticalSection( &prtState.cs );

        if( FAILED( hr = D3DXSavePRTCompBufferToFile( pOptions->strOutputCompPRTBuffer, pPRTCompBuffer ) ) )
        {
            WCHAR sz[256];
            swprintf_s( sz, 256, L"\nError: Failed saving to %s", pOptions->strOutputCompPRTBuffer );
            wprintf( sz );
            goto LEarlyExit;
        }
    }

LEarlyExit:

    // Usually fails becaused user stoped the simulator
    EnterCriticalSection( &prtState.cs );
    prtState.bRunning = false;
    prtState.bStopSimulator = true;
    if( FAILED( hr ) )
        prtState.bFailed = true;
    pSettings->bUserAbort = prtState.bUserAbort;
    LeaveCriticalSection( &prtState.cs );

    if( SUCCEEDED( hr ) )
        wprintf( L"\n\nDone!\n" );

    // Wait for it to close
    dwResult = WaitForSingleObject( hThreadId, 10000 );
    if( dwResult == WAIT_TIMEOUT )
        return E_FAIL;

    DeleteCriticalSection( &prtState.cs );

    SAFE_RELEASE( pBufferA );
    SAFE_RELEASE( pBufferB );
    SAFE_RELEASE( pBufferC );
    SAFE_RELEASE( pDataTotal );
    SAFE_RELEASE( pPRTCompBuffer );
    SAFE_RELEASE( pPRTEngine );
    SAFE_DELETE_ARRAY( pMatPtrArray );

    // It returns E_FAIL if the simulation was aborted from the callback
    if( hr == E_FAIL )
        return E_FAIL;

    if( FAILED( hr ) )
    {
        DXTRACE_ERR( TEXT( "ID3DXPRTEngine" ), hr );
        return hr;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// static helper function
//-----------------------------------------------------------------------------
HRESULT WINAPI StaticPRTSimulatorCB( float fPercentDone, LPVOID pParam )
{
    PRT_STATE* pPRTState = ( PRT_STATE* )pParam;

    EnterCriticalSection( &pPRTState->cs );
    pPRTState->fPercentDone = fPercentDone;

    // In this callback, returning anything except S_OK will stop the simulator
    HRESULT hr = S_OK;
    if( pPRTState->bStopSimulator )
        hr = E_FAIL;

    LeaveCriticalSection( &pPRTState->cs );

    return hr;
}

