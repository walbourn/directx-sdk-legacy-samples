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

    HRESULT OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, WCHAR* strCubeMapFile,
                            WCHAR* strEffectFileName );
    HRESULT OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, IDirect3DCubeTexture9* pCubeTexture,
                            WCHAR* strEffectFileName );

    void    OnResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
    void    Render( D3DXMATRIX* pmWorldViewProj );
    void    OnLostDevice();
    void    OnDestroyDevice();

    IDirect3DCubeTexture9* GetEnvironmentMap()
    {
        return m_pEnvironmentMap;
    }
    void    SetEnvironmentMap( IDirect3DCubeTexture9* pCubeTexture )
    {
        m_pEnvironmentMap = pCubeTexture;
    }

protected:
    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentMap;
    ID3DXEffect* m_pEffect;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    IDirect3DVertexDeclaration9* m_pVertexDecl;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    float m_fSize;
};

#endif //_SKYBOX_H_
