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

    HRESULT                 OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, LPDIRECT3DCUBETEXTURE9 pEnvMap,
                                            WCHAR* strEffectFileName );
    HRESULT                 OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, WCHAR* strCubeMapFile,
                                            WCHAR* strEffectFileName );
    void                    OnResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
    void                    Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale );
    void                    OnLostDevice();
    void                    OnDestroyDevice();

    void                    InitSH( LPDIRECT3DCUBETEXTURE9 pSHTex );
    LPDIRECT3DCUBETEXTURE9  GetSHMap()
    {
        return m_pEnvironmentMapSH;
    }

    void                    SetDrawSH( bool bVal )
    {
        m_bDrawSH = bVal;
    }

    LPDIRECT3DCUBETEXTURE9  GetEnvironmentMap()
    {
        return m_pEnvironmentMap;
    }

protected:

    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentMap;
    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentMapSH;
    ID3DXEffect* m_pEffect;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    IDirect3DVertexDeclaration9* m_pVertexDecl;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    float m_fSize;

    bool m_bDrawSH;
};

#endif //_SKYBOX_H_
