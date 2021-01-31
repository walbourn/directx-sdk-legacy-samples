//-----------------------------------------------------------------------------
// File: skybox.cpp
//
// Desc: Encapsulation of skybox geometry and textures
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "skybox.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

struct SKYBOX_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR3 tex;
};


D3DVERTEXELEMENT9 g_aSkyboxDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
};


//-----------------------------------------------------------------------------
// Name: CSkybox
// Desc: Constructor
//-----------------------------------------------------------------------------
CSkybox::CSkybox()
{
    m_pVB = NULL;
    m_pd3dDevice = NULL;
    m_pEffect = NULL;
    m_pVertexDecl = NULL;
    m_pEnvironmentMap = NULL;
    m_pEnvironmentMapSH = NULL;
    m_fSize = 1.0f;
    m_bDrawSH = false;
}


//-----------------------------------------------------------------------------
HRESULT CSkybox::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, LPDIRECT3DCUBETEXTURE9 pEnvMap,
                                 WCHAR* strEffectFileName )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;
    m_fSize = fSize;
    m_pEnvironmentMap = pEnvMap;

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the shader debugger.  
    // Debugging vertex shaders requires either REF or software vertex processing, and debugging 
    // pixel shaders requires REF.  The D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug 
    // experience in the shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile against the next 
    // higher available software target, which ensures that the unoptimized shaders do not exceed 
    // the shader model limitations.  Setting these flags will cause slower rendering since the shaders 
    // will be unoptimized and forced into software.  See the DirectX documentation for more information 
    // about using the shader debugger.
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

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strEffectFileName ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                        dwShaderFlags, NULL, &m_pEffect, NULL ) );

    // Create vertex declaration
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aSkyboxDecl, &m_pVertexDecl ) );

    return S_OK;
}

void CSkybox::InitSH( LPDIRECT3DCUBETEXTURE9 pSHTex )
{
    m_pEnvironmentMapSH = pSHTex;
}


//-----------------------------------------------------------------------------
HRESULT CSkybox::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice, float fSize, WCHAR* strCubeMapFile,
                                 WCHAR* strEffectFileName )
{
    HRESULT hr;

    WCHAR strPath[MAX_PATH];
    LPDIRECT3DCUBETEXTURE9 pEnvironmentMap;
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, strCubeMapFile ) );
    V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, 512, 1, 0, D3DFMT_A16B16G16R16F,
                                               D3DPOOL_MANAGED, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0, NULL,
                                               NULL, &pEnvironmentMap ) );

    V_RETURN( OnCreateDevice( pd3dDevice, fSize, pEnvironmentMap, strEffectFileName ) );

    return S_OK;
}

//-----------------------------------------------------------------------------
void CSkybox::OnResetDevice( const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr;

    if( m_pEffect )
        V( m_pEffect->OnResetDevice() );

    V( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( SKYBOX_VERTEX ),
                                         D3DUSAGE_WRITEONLY, 0,
                                         D3DPOOL_DEFAULT, &m_pVB, NULL ) );

    // Fill the vertex buffer
    SKYBOX_VERTEX* pVertex = NULL;
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
// Name: Render
// Desc: 
//-----------------------------------------------------------------------------
void CSkybox::Render( D3DXMATRIX* pmWorldViewProj, float fAlpha, float fScale )
{
    HRESULT hr;

    D3DXMATRIX mInvWorldViewProj;
    D3DXMatrixInverse( &mInvWorldViewProj, NULL, pmWorldViewProj );
    V( m_pEffect->SetMatrix( "g_mInvWorldViewProjection", &mInvWorldViewProj ) );

    if( ( fScale == 0.0f ) || ( fAlpha == 0.0f ) ) return; // do nothing if no intensity...

    // Draw the skybox
    UINT uiPass, uiNumPasses;
    V( m_pEffect->SetTechnique( "Skybox" ) );
    V( m_pEffect->SetFloat( "g_fAlpha", fAlpha ) );
    V( m_pEffect->SetFloat( "g_fScale", fAlpha * fScale ) );

    if( this->m_bDrawSH )
    {
        V( m_pEffect->SetTexture( "g_EnvironmentTexture", m_pEnvironmentMapSH ) );
    }
    else
    {
        V( m_pEffect->SetTexture( "g_EnvironmentTexture", m_pEnvironmentMap ) );
    }

    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( SKYBOX_VERTEX ) );
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
void CSkybox::OnLostDevice()
{
    HRESULT hr;
    if( m_pEffect )
        V( m_pEffect->OnLostDevice() );
    SAFE_RELEASE( m_pVB );
}


//-----------------------------------------------------------------------------
void CSkybox::OnDestroyDevice()
{
    SAFE_RELEASE( m_pEnvironmentMap );
    SAFE_RELEASE( m_pEnvironmentMapSH );
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pVertexDecl );
}




