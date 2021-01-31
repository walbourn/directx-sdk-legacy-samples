//--------------------------------------------------------------------------------------
// File: SceneMesh.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

class CSceneMesh
{
public :

            CSceneMesh( void );
            ~CSceneMesh( void );

    HRESULT OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT OnResetDevice();
    void    OnLostDevice();
    void    OnDestroyDevice();

    // General
    HRESULT LoadEffects( IDirect3DDevice9* pd3dDevice, const D3DCAPS9* pDeviceCaps );

    // Mesh 
    HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strMeshFileName, WCHAR* strTextureFileName );
    HRESULT SetMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh );
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
    float   GetObjectRadius()
    {
        return m_fObjectRadius;
    }
    const D3DXVECTOR3& GetObjectCenter()
    {
        return m_vObjectCenter;
    }
    bool    GetBoundingBox( D3DXVECTOR3* pMin, D3DXVECTOR3* pMax );

    void    Render( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj );
    void    RenderRadiance( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj );
    void    RenderDepth( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldView, D3DXMATRIX* pmWorldViewProj );

    // Effect
    bool    IsEffectLoaded()
    {
        return ( m_pEffect != NULL );
    }

protected :

    struct RELOAD_STATE
    {
        bool bUseReloadState;
        WCHAR   strMeshFileName[MAX_PATH];
        WCHAR   strTextureFileName[MAX_PATH];
    } m_ReloadState;

    ///////////
    // Mesh
    ID3DXMesh* m_pMesh;
    IDirect3DTexture9* m_pTexture;
    D3DXMATERIAL* m_pMaterials;
    ID3DXBuffer* m_pMaterialBuffer;
    DWORD m_dwNumMaterials;
    float m_fObjectRadius;
    D3DXVECTOR3 m_vObjectCenter;
    D3DXVECTOR3 m_vBoundingBoxMin;
    D3DXVECTOR3 m_vBoundingBoxMax;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    D3DVIEWPORT9 m_ViewPort;

    ///////////
    // Scene object's effect
    ID3DXEffect* m_pEffect;

    ///////////
    // Techniques for drawing
    D3DXHANDLE m_hValidTechnique;
};
