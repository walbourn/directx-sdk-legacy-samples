//-----------------------------------------------------------------------------
// File: lightprobe.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "dxut.h"
#include "SDKmisc.h"
#include "lightprobe.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


struct LIGHTPROBE_VERTEX
{
    D3DXVECTOR4 pos;
};


//-----------------------------------------------------------------------------
struct SHCubeProj
{
    float* pRed,*pGreen,*pBlue;
    int iOrderUse; // Order to use
    float   fConvCoeffs[6]; // Convolution coefficients

    void    InitDiffCubeMap( float* pR, float* pG, float* pB )
    {
        pRed = pR;
        pGreen = pG;
        pBlue = pB;

        iOrderUse = 3; // Go to 5 is a bit more accurate...
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
    m_pVertLayout = NULL;
    m_pEnvironmentMapRV = NULL;
}


//-----------------------------------------------------------------------------
HRESULT CLightProbe::OnCreateDevice( ID3D10Device* pd3dDevice, const WCHAR* strCubeMapFile,
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
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, NULL, NULL, &m_pEnvironmentMapRV, NULL ) );

    float RData[36] =
    {
        3.787913f, -2.886524f, 0.391213f, 1.032084f, -0.616243f, -0.083525f, -0.933374f, 0.583966f,
        0.210076f, -1.372014f, -0.563307f, -0.049150f, 0.114473f, 0.005523f, 0.781353f,
        0.584010f, -1.036219f, -0.819325f, -0.157718f, -0.563764f, -0.215438f, -0.164376f, -0.225869f,
        0.001518f, -0.345120f, -1.023554f, -0.417133f, 0.093116f, 0.046153f, -0.017157f, -0.347141f, -0.561792f,
        -0.237663f, 0.341074f, -0.073624f, -0.231714f
    };
    float GData[36] =
    {
        4.270733f, -3.584769f, 0.308809f, 1.029689f, -0.548223f, 0.140985f, -1.254231f,
        0.515082f, -0.036361f, -1.459587f, -0.539529f, 0.061784f, 0.308022f, 0.033875f, 0.952719f,
        0.729536f, -1.145294f, -0.904861f, -0.195431f, -0.742667f, -0.185562f, -0.180466f, -0.365853f, -0.098596f,
        -0.411357f, -1.113933f, -0.324883f, 0.309895f, 0.073759f, -0.008792f, -0.452596f, -0.621296f, -0.248816f,
        0.360404f, -0.139558f, -0.188815f
    };
    float BData[36] =
    {
        4.517528f, -4.147918f, 0.112664f, 0.882139f, -0.390195f, 0.462362f, -1.528599f,
        0.376815f, -0.443945f, -1.357518f, -0.462659f, 0.200063f, 0.593371f, 0.049003f, 1.095982f,
        0.825668f, -1.158036f, -0.941781f, -0.198669f, -0.954177f, -0.144703f, -0.146712f, -0.499070f, -0.233005f,
        -0.459772f, -1.102844f, -0.133849f, 0.557828f, 0.070334f, 0.033467f, -0.555929f, -0.622049f, -0.259303f,
        0.364954f, -0.250672f, -0.138154f
    };

    memcpy( m_fSHData[0], RData, 36 * sizeof( float ) );
    memcpy( m_fSHData[1], GData, 36 * sizeof( float ) );
    memcpy( m_fSHData[2], BData, 36 * sizeof( float ) );

    // Create an effect to render the light probe as a skybox
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"lightprobe.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    // Check for D3D10 linear filtering on DXGI_FORMAT_R32G32B32A32.  If the card can't do it, use point filtering.  Pointer filter may cause
    // banding in the lighting.
    UINT FormatSupport = 0;
    V_RETURN( pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R32G32B32A32_FLOAT, &FormatSupport ) );

    char strUsePointCubeSampling[MAX_PATH];
    sprintf_s( strUsePointCubeSampling, ARRAYSIZE( strUsePointCubeSampling ), "%d", ( FormatSupport & D3D10_FORMAT_SUPPORT_SHADER_SAMPLE ) > 0 );

    D3D10_SHADER_MACRO macros[] =
    {
        { "USE_POINT_CUBE_SAMPLING", strUsePointCubeSampling },
        { NULL, NULL }
    };

    V_RETURN( D3DX10CreateEffectFromFile( str, macros, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &m_pEffect, NULL, NULL ) );

    // Get variables and techniques
    m_pLightProbe = m_pEffect->GetTechniqueByName( "LightProbe" );
    m_pmInvWorldViewProjection = m_pEffect->GetVariableByName( "g_mInvWorldViewProjection" )->AsMatrix();
    m_pfAlpha = m_pEffect->GetVariableByName( "g_fAlpha" )->AsScalar();
    m_pfScale = m_pEffect->GetVariableByName( "g_fScale" )->AsScalar();
    m_ptxEnvironmentTexture = m_pEffect->GetVariableByName( "g_txEnvironmentTexture" )->AsShaderResource();

    // Create our quad vertex input layout
    const D3D10_INPUT_ELEMENT_DESC vertlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    m_pLightProbe->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( vertlayout, sizeof( vertlayout ) / sizeof( vertlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &m_pVertLayout ) );

    m_bCreated = true;

    return S_OK;
}


//-----------------------------------------------------------------------------
void CLightProbe::OnSwapChainResized( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr;

    if( !m_bCreated )
        return;

    // Fill the vertex buffer
    LIGHTPROBE_VERTEX* pVertex = new LIGHTPROBE_VERTEX[ 4 ];
    if( !pVertex )
        return;

    // Map texels to pixels 
    float fHighW = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fHighH = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    float fLowW = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fLowH = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );

    pVertex[0].pos = D3DXVECTOR4( fLowW, fLowH, 1.0f, 1.0f );
    pVertex[1].pos = D3DXVECTOR4( fLowW, fHighH, 1.0f, 1.0f );
    pVertex[2].pos = D3DXVECTOR4( fHighW, fLowH, 1.0f, 1.0f );
    pVertex[3].pos = D3DXVECTOR4( fHighW, fHighH, 1.0f, 1.0f );

    UINT uiVertBufSize = 4 * sizeof( LIGHTPROBE_VERTEX );
    D3D10_BUFFER_DESC vbdesc =
    {
        uiVertBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertex;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V( m_pd3dDevice->CreateBuffer( &vbdesc, &InitData, &m_pVB ) );
    SAFE_DELETE_ARRAY( pVertex );
}


//-----------------------------------------------------------------------------
void CLightProbe::Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale, bool bRenderSHProjection )
{
    if( !m_bCreated )
        return;

    m_pd3dDevice->IASetInputLayout( m_pVertLayout );

    D3DXMATRIX mInvWorldViewProj;
    D3DXMatrixInverse( &mInvWorldViewProj, NULL, pmWorldViewProj );
    m_pmInvWorldViewProjection->SetMatrix( ( float* )&mInvWorldViewProj );

    if( ( fScale == 0.0f ) || ( fAlpha == 0.0f ) ) return; // do nothing if no intensity...

    // Draw the light probe
    m_pfAlpha->SetFloat( fAlpha );
    m_pfScale->SetFloat( fAlpha * fScale );
    m_ptxEnvironmentTexture->SetResource( m_pEnvironmentMapRV );

    UINT uStrides = sizeof( LIGHTPROBE_VERTEX );
    UINT uOffsets = 0;
    ID3D10Buffer* pBuffers[1] = { m_pVB };
    m_pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &uStrides, &uOffsets );
    m_pd3dDevice->IASetIndexBuffer( NULL, DXGI_FORMAT_R32_UINT, 0 );
    m_pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;

    m_pLightProbe->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        m_pLightProbe->GetPassByIndex( p )->Apply( 0 );
        m_pd3dDevice->Draw( 4, 0 );
    }
}


//-----------------------------------------------------------------------------
void CLightProbe::OnSwapChainReleasing()
{
    SAFE_RELEASE( m_pVB );
}


//-----------------------------------------------------------------------------
void CLightProbe::OnDestroyDevice()
{
    SAFE_RELEASE( m_pEnvironmentMapRV );
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pVertLayout );
    m_bCreated = false;
}




