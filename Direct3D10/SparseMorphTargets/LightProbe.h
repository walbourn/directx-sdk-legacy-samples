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

    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice, const WCHAR* strCubeMapFile,
                            bool bCreateSHEnvironmentMapTexture );
    void    OnSwapChainResized( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
    void    Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale, bool bRenderSHProjection );
    void    OnSwapChainReleasing();
    void    OnDestroyDevice();

    ID3D10ShaderResourceView* GetEnvironmentMapRV()
    {
        return m_pEnvironmentMapRV;
    }
    float* GetSHData( int nChannel )
    {
        if( nChannel < 0 || nChannel > 3 ) return NULL;
        else
            return m_fSHData[nChannel];
    };
    bool    IsCreated()
    {
        return m_bCreated;
    };

protected:
    float   m_fSHData[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    bool m_bCreated;
    ID3D10ShaderResourceView* m_pEnvironmentMapRV;
    ID3D10Effect* m_pEffect;
    ID3D10Buffer* m_pVB;
    ID3D10InputLayout* m_pVertLayout;
    ID3D10Device* m_pd3dDevice;

    ID3D10EffectTechnique* m_pLightProbe;
    ID3D10EffectMatrixVariable* m_pmInvWorldViewProjection;
    ID3D10EffectScalarVariable* m_pfAlpha;
    ID3D10EffectScalarVariable* m_pfScale;
    ID3D10EffectShaderResourceVariable* m_ptxEnvironmentTexture;
};

#endif //_LIGHTPROBE_H_
