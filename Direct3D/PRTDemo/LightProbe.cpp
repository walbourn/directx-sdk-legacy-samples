//-----------------------------------------------------------------------------
// File: lightprobe.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "lightprobe.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

struct LIGHTPROBE_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR3 tex;
};


D3DVERTEXELEMENT9 g_aLightProbeDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
};


//-----------------------------------------------------------------------------
struct SHCubeProj
{
    float* pRed,*pGreen,*pBlue;
    int iOrderUse; // order to use
    float   fConvCoeffs[6]; // convolution coefficients

    void    InitDiffCubeMap( float* pR, float* pG, float* pB )
    {
        pRed = pR;
        pGreen = pG;
        pBlue = pB;

        iOrderUse = 3; // go to 5 is a bit more accurate...
        fConvCoeffs[0] = 1.0f;
        fConvCoeffs[1] = 2.0f / 3.0f;
        fConvCoeffs[2] = 1.0f / 4.0f;
        fConvCoeffs[3] = fConvCoeffs[5] = 0.0f;
        fConvCoeffs[4] = -6.0f / 144.0f; // 
    }

    void    Init( float* pR, float* pG, float* pB )
    {
        pRed = pR;
        pGreen = pG;
        pBlue = pB;

        iOrderUse = 6;
        for( int i = 0; i < 6; i++ ) fConvCoeffs[i] = 1.0f;
    }
};


//-----------------------------------------------------------------------------
CLightProbe::CLightProbe()
{
    m_bCreated = FALSE;
    m_pVB = NULL;
    m_pd3dDevice = NULL;
    m_pEffect = NULL;
    m_pVertexDecl = NULL;
    m_pEnvironmentMap = NULL;
    m_pSHEnvironmentMap = NULL;
}


//--------------------------------------------------------------------------------------
void WINAPI SHCubeFill( D3DXVECTOR4* pOut,
                       CONST D3DXVECTOR3* pTexCoord,
                       CONST D3DXVECTOR3* pTexelSize,
                       LPVOID pData )
 {
    SHCubeProj* pCP = ( SHCubeProj* ) pData;
    D3DXVECTOR3 vDir;

    D3DXVec3Normalize( &vDir,pTexCoord );

    float fVals[36];
    D3DXSHEvalDirection( fVals, pCP->iOrderUse, &vDir );

    ( *pOut ) = D3DXVECTOR4( 0,0,0,0 ); // just clear it out...

    int l, m, uIndex = 0;
    for( l=0; l<pCP->iOrderUse; l++ )
 {
        const float fConvUse = pCP->fConvCoeffs[l];
        for( m=0; m<2*l+1; m++ )
 {
            pOut->x += fConvUse*fVals[uIndex]*pCP->pRed[uIndex];
            pOut->y += fConvUse*fVals[uIndex]*pCP->pGreen[uIndex];
            pOut->z += fConvUse*fVals[uIndex]*pCP->pBlue[uIndex];
            pOut->w = 1;

            uIndex++;
        }
    }
}


//-----------------------------------------------------------------------------
HRESULT CLightProbe::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, const WCHAR* strCubeMapFile,
                                     bool bCreateSHEnvironmentMapTexture )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;

    // Note that you should optimally read these HDR light probes on device that supports
    // floating point.  If this current device doesn't support floating point then
    // D3DX will automatically convert them to interger format which won't be optimal.
    // If you wanted to perform this step in a machine without a good video card, then 
    // use a D3DDEVTYPE_NULLREF device instead.
    WCHAR strPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, strCubeMapFile ) );
    V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, 512, 1, 0, D3DFMT_A16B16G16R16F,
                                               D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, NULL,
                                               NULL, &m_pEnvironmentMap ) );

    // Some devices don't support D3DFMT_A16B16G16R16F textures and thus
    // D3DX will return the texture in a HW compatible format with the possibility of losing its 
    // HDR lighting information.  This will change the SH values returned from D3DXSHProjectCubeMap()
    // as the cube map will no longer be HDR.  So if this happens, create a load the cube map on 
    // scratch memory and project using that cube map.  But keep the other one around to render the 
    // background texture with.
    bool bUsedScratchMem = false;
    D3DSURFACE_DESC desc;
    ZeroMemory( &desc, sizeof( D3DSURFACE_DESC ) );
    m_pEnvironmentMap->GetLevelDesc( 0, &desc );
    if( desc.Format != D3DFMT_A16B16G16R16F )
    {
        LPDIRECT3DCUBETEXTURE9 pScratchEnvironmentMap = NULL;
        hr = D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, 512, 1, 0, D3DFMT_A16B16G16R16F,
                                              D3DPOOL_SCRATCH, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, NULL,
                                              NULL, &pScratchEnvironmentMap );
        if( SUCCEEDED( hr ) )
        {
            // prefilter the lighting environment by projecting onto the order 6 SH basis.  
            V( D3DXSHProjectCubeMap( 6, pScratchEnvironmentMap, m_fSHData[0], m_fSHData[1], m_fSHData[2] ) );
            bUsedScratchMem = true;
            SAFE_RELEASE( pScratchEnvironmentMap );
        }
    }

    if( !bUsedScratchMem )
    {
        // prefilter the lighting environment by projecting onto the order 6 SH basis.  
        V( D3DXSHProjectCubeMap( 6, m_pEnvironmentMap, m_fSHData[0], m_fSHData[1], m_fSHData[2] ) );
    }

    if( bCreateSHEnvironmentMapTexture )
    {
        // reconstruct the prefiltered lighting environment into a cube map
        V( D3DXCreateCubeTexture( pd3dDevice, 256, 1, 0, D3DFMT_A16B16G16R16F,
                                  D3DPOOL_MANAGED, &m_pSHEnvironmentMap ) );
        SHCubeProj projData;
        projData.Init( m_fSHData[0], m_fSHData[1], m_fSHData[2] );
        V( D3DXFillCubeTexture( m_pSHEnvironmentMap, SHCubeFill, &projData ) );

        // This code will save a cubemap texture that represents the SH projection of the light probe if you want it
        // V( D3DXSaveTextureToFile( "shprojection.dds", D3DXIFF_DDS, m_pSHEnvironmentMap, NULL ) ); 
    }

    // Create an effect to render the light probe as a skybox
    WCHAR str[MAX_PATH];
    WCHAR* strEffectFileName = L"lightprobe.fx";
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strEffectFileName ) );

    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                        dwShaderFlags, NULL, &m_pEffect, NULL ) );

    // Create vertex declaration
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aLightProbeDecl, &m_pVertexDecl ) );

    m_bCreated = true;

    return S_OK;
}



//-----------------------------------------------------------------------------
void CLightProbe::OnResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr;

    if( !m_bCreated )
        return;

    if( m_pEffect )
        V( m_pEffect->OnResetDevice() );

    // Create a vertex buffer to render the light probe as a skybox
    V( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( LIGHTPROBE_VERTEX ),
                                         D3DUSAGE_WRITEONLY, 0,
                                         D3DPOOL_DEFAULT, &m_pVB, NULL ) );

    // Fill the vertex buffer
    LIGHTPROBE_VERTEX* pVertex = NULL;
    V( m_pVB->Lock( 0, 0, ( void** )&pVertex, 0 ) );

    // Map texels to pixels 
    float fHighW = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fHighH = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    float fLowW = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fLowH = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );

    pVertex[0].pos = D3DXVECTOR4( fLowW, fLowH, 1.0f, 1.0f );
    pVertex[1].pos = D3DXVECTOR4( fLowW, fHighH, 1.0f, 1.0f );
    pVertex[2].pos = D3DXVECTOR4( fHighW, fLowH, 1.0f, 1.0f );
    pVertex[3].pos = D3DXVECTOR4( fHighW, fHighH, 1.0f, 1.0f );

    m_pVB->Unlock();
}


//-----------------------------------------------------------------------------
void CLightProbe::Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale, bool bRenderSHProjection )
{
    HRESULT hr;

    if( !m_bCreated )
        return;

    D3DXMATRIX mInvWorldViewProj;
    D3DXMatrixInverse( &mInvWorldViewProj, NULL, pmWorldViewProj );
    V( m_pEffect->SetMatrix( "g_mInvWorldViewProjection", &mInvWorldViewProj ) );

    if( ( fScale == 0.0f ) || ( fAlpha == 0.0f ) ) return; // do nothing if no intensity...

    // Draw the light probe
    UINT uiPass, uiNumPasses;
    V( m_pEffect->SetTechnique( "LightProbe" ) );
    V( m_pEffect->SetFloat( "g_fAlpha", fAlpha ) );
    V( m_pEffect->SetFloat( "g_fScale", fAlpha * fScale ) );

    if( bRenderSHProjection )
    {
        V( m_pEffect->SetTexture( "g_EnvironmentTexture", m_pSHEnvironmentMap ) );
    }
    else
    {
        V( m_pEffect->SetTexture( "g_EnvironmentTexture", m_pEnvironmentMap ) );
    }

    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( LIGHTPROBE_VERTEX ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    V( m_pEffect->Begin( &uiNumPasses, 0 ) );
    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        V( m_pEffect->BeginPass( uiPass ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        V( m_pEffect->EndPass() );
    }
    V( m_pEffect->End() );
}


//-----------------------------------------------------------------------------
void CLightProbe::OnLostDevice()
{
    HRESULT hr;
    if( m_pEffect )
        V( m_pEffect->OnLostDevice() );
    SAFE_RELEASE( m_pVB );
}


//-----------------------------------------------------------------------------
void CLightProbe::OnDestroyDevice()
{
    SAFE_RELEASE( m_pEnvironmentMap );
    SAFE_RELEASE( m_pSHEnvironmentMap );
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pVertexDecl );
    m_bCreated = false;
}




