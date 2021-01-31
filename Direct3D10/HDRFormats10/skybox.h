//-----------------------------------------------------------------------------
// File: skybox.h
//
// Desc: Encapsulation of skybox geometry and textures
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef _SKYBOX_H
#define _SKYBOX_H


class CSkybox
{
public:
            CSkybox();

    // D3D9 methods
    HRESULT OnD3D9CreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, WCHAR* strCubeMapFile,
                                WCHAR* strEffectFileName );
    HRESULT OnD3D9CreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, IDirect3DCubeTexture9* pCubeTexture,
                                WCHAR* strEffectFileName );

    void    OnD3D9ResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
    void    D3D9Render( D3DXMATRIX* pmWorldViewProj );
    void    OnD3D9LostDevice();
    void    OnD3D9DestroyDevice();

    IDirect3DCubeTexture9* GetD3D9EnvironmentMap()
    {
        return m_pEnvironmentMap9;
    }
    void    SetD3D9EnvironmentMap( IDirect3DCubeTexture9* pCubeTexture )
    {
        m_pEnvironmentMap9 = pCubeTexture;
    }

    // D3D10 methods
    HRESULT OnD3D10CreateDevice( ID3D10Device* pd3dDevice, float fSize, WCHAR* strCubeMapFile,
                                 WCHAR* strEffectFileName );
    HRESULT OnD3D10CreateDevice( ID3D10Device* pd3dDevice, float fSize, ID3D10Texture2D* pCubeTexture,
                                 ID3D10ShaderResourceView* pCubeRV, WCHAR* strEffectFileName );

    void    OnD3D10ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
    void    D3D10Render( D3DXMATRIX* pmWorldViewProj );
    void    OnD3D10ReleasingSwapChain();
    void    OnD3D10DestroyDevice();

    ID3D10Texture2D* GetD3D10EnvironmentMap()
    {
        return m_pEnvironmentMap10;
    }
    ID3D10ShaderResourceView* GetD3D10EnvironmentMapRV()
    {
        return m_pEnvironmentRV10;
    }
    void    SetD3D10EnvironmentMap( ID3D10Texture2D* pCubeTexture )
    {
        m_pEnvironmentMap10 = pCubeTexture;
    }

protected:
    // 9 members
    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentMap9;
    ID3DXEffect* m_pEffect9;
    LPDIRECT3DVERTEXBUFFER9 m_pVB9;
    IDirect3DVertexDeclaration9* m_pVertexDecl9;
    LPDIRECT3DDEVICE9 m_pd3dDevice9;
    D3DXHANDLE m_hmInvWorldViewProjection;
    D3DXHANDLE m_hSkyBox;
    D3DXHANDLE m_hEnvironmentTexture;

    // 10 members
    ID3D10Texture2D* m_pEnvironmentMap10;
    ID3D10ShaderResourceView* m_pEnvironmentRV10;
    ID3D10Effect* m_pEffect10;
    ID3D10Buffer* m_pVB10;
    ID3D10InputLayout* m_pVertexLayout10;
    ID3D10Device* m_pd3dDevice10;
    ID3D10EffectTechnique* m_pTechSkyBox;
    ID3D10EffectMatrixVariable* m_pmInvWorldViewProjection;
    ID3D10EffectShaderResourceVariable* g_pEnvironmentTexture;
    float m_fSize;
};

#endif //_SKYBOX_H_
