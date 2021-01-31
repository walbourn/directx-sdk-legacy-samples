//--------------------------------------------------------------------------------------
// File: PRTOptionsDlg.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "DXUTcamera.h"

class CPRTMesh
{
public:
            CPRTMesh( void );
            ~CPRTMesh( void );

    HRESULT OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, D3DFORMAT fmt );
    HRESULT OnResetDevice();
    void    OnLostDevice();
    void    OnDestroyDevice();

    // General
    HRESULT LoadEffects( IDirect3DDevice9* pd3dDevice, const D3DCAPS9* pDeviceCaps );

    // Mesh 
    HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strMeshFileName );
    HRESULT SetMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh );
    HRESULT AdjustMeshDecl( IDirect3DDevice9* pd3dDevice, ID3DXMesh** ppMesh );
    DWORD   GetNumVertices()
    {
        return m_pMesh->GetNumVertices();
    }
    ID3DXMesh* GetMesh()
    {
        return m_pMesh;
    }
    D3DXMATERIAL* GetMaterials()
    {
        return m_pMaterials;
    }
    DWORD   GetNumMaterials()
    {
        return m_dwNumMaterials;
    }
    bool    IsMeshLoaded()
    {
        return ( m_pMesh != NULL );
    }
    IDirect3DTexture9* GetAlbedoTexture()
    {
        return m_pAlbedoTextures[0];
    }
    float   GetObjectRadius()
    {
        return m_fObjectRadius;
    }
    const D3DXVECTOR3& GetObjectCenter()
    {
        return m_vObjectCenter;
    }

    // Misc
    void    GetVertexUnderMouse( const D3DXMATRIX* pmProj, const D3DXMATRIX* pmView, const D3DXMATRIX* pmWorld,
                                 unsigned int* uVert );
    void    GetSHTransferFunctionAtVertex( unsigned int uVert, int uTechnique, unsigned int uChan, float* pfVals );
    void    GetVertexPosition( unsigned int uVert, D3DXVECTOR3* pvPos );

    // N dot L
    void    RenderWithNdotL( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, D3DXMATRIX* pmWorldInv,
                             bool bRenderWithAlbedoTexture, CDXUTDirectionWidget* aLightControl, int nNumLights,
                             float fLightScale );

    // SHIrradEnvMap
    void    RenderWithSHIrradEnvMap( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj,
                                     bool bRenderWithAlbedoTexture );
    void    ComputeSHIrradEnvMapConstants( float* pSHCoeffsRed, float* pSHCoeffsGreen, float* pSHCoeffsBlue );

    // PRT
    void    SetPRTBuffer( ID3DXPRTBuffer* pPRTBuffer, WCHAR* strFile );
    HRESULT LoadPRTBufferFromFile( WCHAR* strFile );
    HRESULT LoadCompPRTBufferFromFile( WCHAR* strFile );
    HRESULT CompressPRTBuffer( D3DXSHCOMPRESSQUALITYTYPE Quality, UINT NumClusters, UINT NumPCA, LPD3DXSHPRTSIMCB pCB=
                               NULL, LPVOID lpUserContext=NULL );
    void    ExtractCompressedDataForPRTShader();
    void    ComputeShaderConstants( float* pSHCoeffsRed, float* pSHCoeffsGreen, float* pSHCoeffsBlue,
                                    DWORD dwNumCoeffsPerChannel );
    void    RenderWithPRT( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, bool bRenderWithAlbedoTexture );
    HRESULT SavePRTBufferToFile( WCHAR* strFile );
    HRESULT SaveCompPRTBufferToFile( WCHAR* strFile );

    DWORD   GetPRTOrder()
    {
        return m_dwPRTOrder;
    }
    bool    IsPRTUncompressedBufferLoaded()
    {
        return ( m_pPRTBuffer != NULL );
    }
    bool    IsPRTCompBufferLoaded()
    {
        return ( m_pPRTCompBuffer != NULL );
    }
    ID3DXPRTCompBuffer* GetPRTCompBuffer()
    {
        return m_pPRTCompBuffer;
    }
    ID3DXPRTBuffer* GetPRTBuffer()
    {
        return m_pPRTBuffer;
    }
    bool    IsPRTShaderDataExtracted()
    {
        return ( m_aPRTClusterBases != NULL );
    }
    bool    IsPRTEffectLoaded()
    {
        return ( m_pPRTEffect != NULL );
    }
    UINT    GetOrderFromNumCoeffs( UINT dwNumCoeffs );

    // LDPRT
    void    RenderWithLDPRT( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj, D3DXMATRIX* pmNormalXForm,
                             bool bUniform, bool bRenderWithAlbedo );
    void    SetLDPRTData( ID3DXPRTBuffer* pLDPRTBuff, D3DXVECTOR3* pShadingNormal );
    void    ComputeLDPRTConstants( float* pSHCoeffsRed, float* pSHCoeffsGreen, float* pSHCoeffsBlue,
                                   DWORD dwNumCoeffsPerChannel );
    static VOID WINAPI StaticFillCubeTextureWithSHCallback( D3DXVECTOR4* pOut, CONST D3DXVECTOR3* pTexCoord, CONST D3DXVECTOR3* pTexelSize, LPVOID pData );
    HRESULT LoadLDPRTFromFiles( WCHAR* strLDPRTFile, WCHAR* strShadingNormalsFile );
    HRESULT SaveLDPRTToFiles( WCHAR* strLDPRTFile, WCHAR* strShadingNormalsFile );
    HRESULT CreateLDPRTData();

protected:
    struct RELOAD_STATE
    {
        bool bUseReloadState;
        bool bLoadCompressed;
        WCHAR   strMeshFileName[MAX_PATH];
        WCHAR   strPRTBufferFileName[MAX_PATH];
        WCHAR   strLDPRTFile[MAX_PATH];
        WCHAR   strShadingNormalsFile[MAX_PATH];
        D3DXSHCOMPRESSQUALITYTYPE quality;
        UINT dwNumClusters;
        UINT dwNumPCA;
    } m_ReloadState;

    ///////////
    // Mesh
    ID3DXMesh* m_pMesh;
    CGrowableArray <IDirect3DTexture9*> m_pAlbedoTextures;
    D3DXMATERIAL* m_pMaterials;
    ID3DXBuffer* m_pMaterialBuffer;
    DWORD m_dwNumMaterials;
    float m_fObjectRadius;
    D3DXVECTOR3 m_vObjectCenter;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    D3DVIEWPORT9 m_ViewPort;

    ///////////
    // N dot L
    ID3DXEffect* m_pNDotLEffect;

    ///////////
    // SHIrradEnvMap
    ID3DXEffect* m_pSHIrradEnvMapEffect;

    ///////////
    // PRT
    ID3DXPRTBuffer* m_pPRTBuffer;
    ID3DXEffect* m_pPRTEffect;
    DWORD m_dwPRTOrder;
    ID3DXPRTCompBuffer* m_pPRTCompBuffer;
    // The basis buffer is a large array of floats where 
    // Call ID3DXPRTCompBuffer::ExtractBasis() to extract the basis 
    // for every cluster.  The basis for a cluster is an array of
    // (NumPCAVectors+1)*(NumChannels*Order^2) floats. 
    // The "1+" is for the cluster mean.
    float* m_aPRTClusterBases;
    // m_aPRTConstants stores the incident radiance dotted with the transfer function.
    // Each cluster has an array of floats which is the size of 
    // 4+MAX_NUM_CHANNELS*NUM_PCA_VECTORS. This number comes from: there can 
    // be up to 3 channels (R,G,B), and each channel can 
    // have up to NUM_PCA_VECTORS of PCA vectors.  Each cluster also has 
    // a mean PCA vector which is described with 4 floats (and hence the +4).
    float* m_aPRTConstants;

    ///////////
    // LDPRT
    D3DXVECTOR3* m_pLDPRTShadingNormals;
    ID3DXPRTBuffer* m_pLDPRTBuffer;
    ID3DXMesh* m_pLDPRTMesh;
    ID3DXEffect* m_pLDPRTEffect;
    D3DXHANDLE m_hValidLDPRTTechnique;
    D3DXHANDLE m_hValidLDPRTTechniqueWithTex;
    IDirect3DCubeTexture9* m_pSHBasisTextures[9];  // Store SH basis functions using textures
};
