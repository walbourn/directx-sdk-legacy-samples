//--------------------------------------------------------------------------------------
// File: PRTSimulator.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once


//--------------------------------------------------------------------------------------
struct SIMULATOR_OPTIONS
{
    // General settings
    WCHAR   strInitialDir[MAX_PATH];
    WCHAR   strInputMesh[MAX_PATH];
    WCHAR   strResultsFile[MAX_PATH];
    DWORD dwNumRays;
    DWORD dwOrder;
    DWORD dwNumChannels;
    DWORD dwNumBounces;
    bool bSubsurfaceScattering;
    float fLengthScale;
    bool bShowTooltips;

    // Material options
    DWORD dwPredefinedMatIndex;
    D3DXCOLOR Diffuse;
    D3DXCOLOR Absorption;
    D3DXCOLOR ReducedScattering;
    float fRelativeIndexOfRefraction;

    // Adaptive options
    bool bAdaptive;
    bool bRobustMeshRefine;
    float fRobustMeshRefineMinEdgeLength;
    DWORD dwRobustMeshRefineMaxSubdiv;
    bool bAdaptiveDL;
    float fAdaptiveDLMinEdgeLength;
    float fAdaptiveDLThreshold;
    DWORD dwAdaptiveDLMaxSubdiv;
    bool bAdaptiveBounce;
    float fAdaptiveBounceMinEdgeLength;
    float fAdaptiveBounceThreshold;
    DWORD dwAdaptiveBounceMaxSubdiv;
    WCHAR   strOutputMesh[MAX_PATH];
    bool bBinaryOutputXFile;

    // Compression options
    D3DXSHCOMPRESSQUALITYTYPE Quality;
    DWORD dwNumClusters;
    DWORD dwNumPCA;
};


//--------------------------------------------------------------------------------------
class CPRTSimulator
{
public:
                            CPRTSimulator( void );
                            ~CPRTSimulator( void );

    HRESULT                 RunInWorkerThread( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions,
                                               CPRTMesh* pPRTMesh );
    HRESULT                 CompressInWorkerThread( IDirect3DDevice9* pd3dDevice, SIMULATOR_OPTIONS* pOptions,
                                                    CPRTMesh* pPRTMesh );

    bool                    IsRunning();
    bool                    IsCompressing()
    {
        return m_bCompressInWorkerThread;
    }
    HRESULT                 Stop();

    float                   GetPercentComplete()
    {
        return m_fPercentDone * 100.0f;
    }
    int                     GetCurrentPass()
    {
        return m_nCurPass;
    }
    WCHAR* GetCurrentPassName()
    {
        return m_strCurPass;
    }
    int                     GetNumPasses()
    {
        return m_nNumPasses;
    }

    static DWORD WINAPI     StaticPRTSimulationThreadProc( LPVOID lpParameter );
    DWORD                   PRTSimulationThreadProc();

    static DWORD WINAPI     StaticPRTCompressThreadProc( LPVOID lpParameter );
    DWORD                   PRTCompressThreadProc();

protected:
    static HRESULT WINAPI   StaticPRTSimulatorCB( float fPercentDone, LPVOID pParam );
    HRESULT                 PRTSimulatorCB( float fPercentDone );

    CRITICAL_SECTION m_cs;

    SIMULATOR_OPTIONS m_Options;
    CPRTMesh* m_pPRTMesh;
    int m_nCurPass;
    int m_nNumPasses;
    WCHAR                   m_strCurPass[256];

    ID3DXPRTEngine* m_pPRTEngine;
    IDirect3DDevice9* m_pd3dDevice;

    bool m_bStopSimulator;
    bool m_bRunning;
    bool m_bFailed;
    bool m_bCompressInWorkerThread;

    HANDLE m_hThreadId;
    DWORD m_dwThreadId;
    float m_fPercentDone;
};
