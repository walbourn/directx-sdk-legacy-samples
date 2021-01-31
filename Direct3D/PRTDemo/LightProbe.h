//-----------------------------------------------------------------------------
// File: lightprobe.h
//
// Desc: Encapsulation of a light probe
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef _LIGHTPROBE_H
#define _LIGHTPROBE_H


class CLightProbe
{
public:
                            CLightProbe();

    HRESULT                 OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, const WCHAR* strCubeMapFile,
                                            bool bCreateSHEnvironmentMapTexture );
    void                    OnResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
    void                    Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale,
                                    bool bRenderSHProjection );
    void                    OnLostDevice();
    void                    OnDestroyDevice();

    LPDIRECT3DCUBETEXTURE9  GetSHEnvironmentMap()
    {
        return m_pSHEnvironmentMap;
    }
    LPDIRECT3DCUBETEXTURE9  GetEnvironmentMap()
    {
        return m_pEnvironmentMap;
    }
    float* GetSHData( int nChannel )
    {
        if( nChannel < 0 || nChannel > 3 ) return NULL;
        else
            return m_fSHData[nChannel];
    };
    bool                    IsCreated()
    {
        return m_bCreated;
    };

protected:
    float                   m_fSHData[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    bool m_bCreated;
    LPDIRECT3DCUBETEXTURE9 m_pEnvironmentMap;
    LPDIRECT3DCUBETEXTURE9 m_pSHEnvironmentMap;
    ID3DXEffect* m_pEffect;
    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    IDirect3DVertexDeclaration9* m_pVertexDecl;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
};

#endif //_LIGHTPROBE_H_
