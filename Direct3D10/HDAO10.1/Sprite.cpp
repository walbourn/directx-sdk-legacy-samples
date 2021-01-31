//--------------------------------------------------------------------------------------
// File: Sprite.cpp
//
// Sprite class definition. This class provides functionality to render sprites, at a given position and scale. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "SDKMisc.h"
#include "Sprite.h"

struct SpriteVertex
{
    D3DXVECTOR3 v3Pos;
    D3DXVECTOR2 v2TexCoord;
};

struct SpriteBorderVertex
{
    D3DXVECTOR3 v3Pos;
};

//===============================================================================================================================//
//
// Constructor(s) / Destructor(s) Block
//
//===============================================================================================================================//

Sprite::Sprite()
{
    m_pVertexLayout = NULL;
    m_pVertexBuffer = NULL;

    m_pEffect = NULL;
    m_pSpriteTechnique = NULL;
    m_pSpriteMSTechnique = NULL;
    m_pSpriteAsDepthTechnique = NULL;
    m_pSpriteAsDepthMSTechnique = NULL;
    m_pAlphaSpriteTechnique = NULL;
    m_pBorderTechnique = NULL;
    m_pSpriteTextureVar = NULL;
    m_pSpriteTextureMSVar = NULL;
    m_pSpriteDepthTextureMSVar = NULL;
    m_pViewportWidthVar = NULL;
    m_pViewportHeightVar = NULL;
    m_pStartPosXVar = NULL;
    m_pStartPosYVar = NULL;
    m_pWidthVar = NULL;
    m_pHeightVar = NULL;
    m_pBorderColorVar = NULL;
    m_pTextureWidthVar = NULL;
    m_pTextureHeightVar = NULL;
    m_pDepthRangeMinVar = NULL;
    m_pDepthRangeMaxVar = NULL;
    m_pSampleIndexVar = NULL;
}

Sprite::~Sprite()
{
}

//===============================================================================================================================//
//
// Public functions
//
//===============================================================================================================================//

HRESULT Sprite::OnCreateDevice( ID3D10Device* pd3dDevice )
{
    if( pd3dDevice == NULL )
    {
        return E_FAIL;
    }

    HRESULT hr;
    LPCSTR pszTarget;
    D3D10_SHADER_MACRO* pShaderMacros = NULL;
    D3D10_SHADER_MACRO ShaderMacros[2];

    // Check we have a valid device pointer
    assert( NULL != pd3dDevice );

    // Check to see if we have a DX10.1 device
    if( NULL == DXUTGetD3D10Device1() ) 
    {
        pszTarget = "fx_4_0";
        pShaderMacros = NULL;
    }
    else
    {
        pszTarget = "fx_4_1";
        ShaderMacros[0].Name = "DX10_1_ENABLED";
        ShaderMacros[0].Definition = "1";
        ShaderMacros[1].Name = NULL;
        pShaderMacros = ShaderMacros;
    }

    // Create the sprite effect
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Sprite.fx" ) );
    hr = D3DX10CreateEffectFromFile( str, pShaderMacros, NULL, pszTarget, dwShaderFlags, 0, 
                                     pd3dDevice, NULL, NULL, &m_pEffect, NULL, NULL );
    assert( D3D_OK == hr );

     // Obtain the technique and variables
    m_pSpriteTechnique = m_pEffect->GetTechniqueByName( "RenderSprite" );
    m_pSpriteMSTechnique = m_pEffect->GetTechniqueByName( "RenderSpriteMS" );
    m_pSpriteAsDepthTechnique = m_pEffect->GetTechniqueByName( "RenderSpriteAsDepth" );
    m_pSpriteAsDepthMSTechnique = m_pEffect->GetTechniqueByName( "RenderSpriteAsDepthMS" );
    m_pAlphaSpriteTechnique = m_pEffect->GetTechniqueByName( "RenderAlphaSprite" );
    m_pBorderTechnique = m_pEffect->GetTechniqueByName( "RenderSpriteBorder" );
    m_pSpriteTextureVar = m_pEffect->GetVariableByName( "g_SpriteTexture" )->AsShaderResource();
    m_pSpriteTextureMSVar = m_pEffect->GetVariableByName( "g_SpriteTextureMS" )->AsShaderResource();
    m_pSpriteDepthTextureMSVar = m_pEffect->GetVariableByName( "g_SpriteDepthTextureMS" )->AsShaderResource();
    m_pViewportWidthVar = m_pEffect->GetVariableByName( "g_fViewportWidth" )->AsScalar();
    m_pViewportHeightVar = m_pEffect->GetVariableByName( "g_fViewportHeight" )->AsScalar();
    m_pStartPosXVar = m_pEffect->GetVariableByName( "g_fStartPosX" )->AsScalar();
    m_pStartPosYVar = m_pEffect->GetVariableByName( "g_fStartPosY" )->AsScalar();
    m_pWidthVar = m_pEffect->GetVariableByName( "g_fWidth" )->AsScalar();
    m_pHeightVar = m_pEffect->GetVariableByName( "g_fHeight" )->AsScalar();
    m_pBorderColorVar = m_pEffect->GetVariableByName( "g_v4BorderColor" )->AsVector();
    m_pTextureWidthVar = m_pEffect->GetVariableByName( "g_fTextureWidth" )->AsScalar();
    m_pTextureHeightVar = m_pEffect->GetVariableByName( "g_fTextureHeight" )->AsScalar();
    m_pDepthRangeMinVar = m_pEffect->GetVariableByName( "g_fDepthRangeMin" )->AsScalar();
    m_pDepthRangeMaxVar = m_pEffect->GetVariableByName( "g_fDepthRangeMax" )->AsScalar();
    m_pSampleIndexVar = m_pEffect->GetVariableByName( "g_nSampleIndex" )->AsScalar();

    ///////////////////////////////////////////////////////////////////////////
    // Setup input layout and VB for the textured sprite
    ///////////////////////////////////////////////////////////////////////////

    // Define the input layout
    D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	    { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof(layout) / sizeof(layout[0]);

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    m_pSpriteTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature, 
	    PassDesc.IAInputSignatureSize, &m_pVertexLayout );
    assert( D3D_OK == hr );

    // Fill out a unit quad
    SpriteVertex QuadVertices[6];
    QuadVertices[0].v3Pos = D3DXVECTOR3( 0.0f, -1.0f, 0.5f );
    QuadVertices[0].v2TexCoord = D3DXVECTOR2( 0.0f, 0.0f );

    QuadVertices[1].v3Pos = D3DXVECTOR3( 0.0f, 0.0f, 0.5f );
    QuadVertices[1].v2TexCoord = D3DXVECTOR2( 0.0f, 1.0f );

    QuadVertices[2].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[2].v2TexCoord = D3DXVECTOR2( 1.0f, 0.0f );

    QuadVertices[3].v3Pos = D3DXVECTOR3( 0.0f, 0.0f, 0.5f );
    QuadVertices[3].v2TexCoord = D3DXVECTOR2( 0.0f, 1.0f );

    QuadVertices[4].v3Pos = D3DXVECTOR3( 1.0f, 0.0f, 0.5f );
    QuadVertices[4].v2TexCoord = D3DXVECTOR2( 1.0f, 1.0f );

    QuadVertices[5].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[5].v2TexCoord = D3DXVECTOR2( 1.0f, 0.0f );

    // Create the vertex buffer
    D3D10_BUFFER_DESC BD;
    BD.Usage = D3D10_USAGE_DYNAMIC;
    BD.ByteWidth = sizeof( SpriteVertex ) * 6;
    BD.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    BD.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = QuadVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &m_pVertexBuffer );
    assert( D3D_OK == hr );

    ///////////////////////////////////////////////////////////////////////////
    // Setup input layout and VB for the sprite border
    ///////////////////////////////////////////////////////////////////////////

    // Define the input layout
    D3D10_INPUT_ELEMENT_DESC BorderLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    numElements = sizeof(BorderLayout) / sizeof(BorderLayout[0]);

    // Create the input layout
    m_pBorderTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( BorderLayout, numElements, PassDesc.pIAInputSignature, 
	    PassDesc.IAInputSignatureSize, &m_pBorderVertexLayout );
    assert( D3D_OK == hr );

    // Fill out a unit quad
    SpriteBorderVertex QuadBorderVertices[5];
    QuadBorderVertices[0].v3Pos = D3DXVECTOR3( 0.0f, -1.0f, 0.5f );
    QuadBorderVertices[1].v3Pos = D3DXVECTOR3( 0.0f, 0.0f, 0.5f );
    QuadBorderVertices[2].v3Pos = D3DXVECTOR3( 1.0f, 0.0f, 0.5f );
    QuadBorderVertices[3].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadBorderVertices[4].v3Pos = D3DXVECTOR3( 0.0f, -1.0f, 0.5f );
    	
    // Create the vertex buffer
    BD.Usage = D3D10_USAGE_DEFAULT;
    BD.ByteWidth = sizeof( SpriteBorderVertex ) * 5;
    BD.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = 0;
    BD.MiscFlags = 0;
    InitData.pSysMem = QuadBorderVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &m_pBorderVertexBuffer );
    assert( D3D_OK == hr );

    return hr;
}

void Sprite::OnDestroyDevice()
{
    SAFE_RELEASE( m_pEffect );
    SAFE_RELEASE( m_pVertexLayout );
    SAFE_RELEASE( m_pVertexBuffer );
    SAFE_RELEASE( m_pBorderVertexLayout );
    SAFE_RELEASE( m_pBorderVertexBuffer );
}

void Sprite::OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    if( pBackBufferSurfaceDesc == NULL )
        return;

    HRESULT hr;

    assert( NULL != pBackBufferSurfaceDesc );

    hr = m_pViewportWidthVar->SetFloat( (float)pBackBufferSurfaceDesc->Width ); 
    assert( D3D_OK == hr );
    hr = m_pViewportHeightVar->SetFloat( (float)pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );
}

void Sprite::SetBorderColor( D3DXCOLOR Color )
{
    HRESULT hr;

    hr = m_pBorderColorVar->SetFloatVector( (float*)&Color );
    assert( D3D_OK == hr );
}

void Sprite::SetUVs( float fU1, float fV1, float fU2, float fV2 )
{
    float* pfData = NULL;
    SpriteVertex QuadVertices[6];

    QuadVertices[0].v3Pos = D3DXVECTOR3( 0.0f, -1.0f, 0.5f );
    QuadVertices[0].v2TexCoord = D3DXVECTOR2( fU1, fV1 );
    QuadVertices[1].v3Pos = D3DXVECTOR3( 0.0f, 0.0f, 0.5f );
    QuadVertices[1].v2TexCoord = D3DXVECTOR2( fU1, fV2 );
    QuadVertices[2].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[2].v2TexCoord = D3DXVECTOR2( fU2, fV1 );
    QuadVertices[3].v3Pos = D3DXVECTOR3( 0.0f, 0.0f, 0.5f );
    QuadVertices[3].v2TexCoord = D3DXVECTOR2( fU1, fV2 );
    QuadVertices[4].v3Pos = D3DXVECTOR3( 1.0f, 0.0f, 0.5f );
    QuadVertices[4].v2TexCoord = D3DXVECTOR2( fU2, fV2 );
    QuadVertices[5].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[5].v2TexCoord = D3DXVECTOR2( fU2, fV1 );

    m_pVertexBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, (void**)&pfData );

    memcpy( pfData, QuadVertices, sizeof( SpriteVertex ) * 6 );

    m_pVertexBuffer->Unmap();
}

HRESULT Sprite::RenderSprite( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, bool bAlpha, bool bBordered )
{
    HRESULT hr;

    assert( NULL != pTextureView );

    // Set position and dimensions
    hr = m_pStartPosXVar->SetFloat( (float)nStartPosX );
    assert( D3D_OK == hr );
    hr = m_pStartPosYVar->SetFloat( (float)nStartPosY ); 
    assert( D3D_OK == hr );
    hr = m_pWidthVar->SetFloat( (float)nWidth ); 
    assert( D3D_OK == hr );
    hr = m_pHeightVar->SetFloat( (float)nHeight ); 
    assert( D3D_OK == hr );

    // Set the texture
    hr = m_pSpriteTextureVar->SetResource( pTextureView );
    assert( D3D_OK == hr );

    // Apply technique and changes
    if( bAlpha )
    {
	    m_pAlphaSpriteTechnique->GetPassByIndex( 0 )->Apply( 0 );
    }
    else
    {
	    m_pSpriteTechnique->GetPassByIndex( 0 )->Apply( 0 );
    }

    // Do the render
    Render();

    // Unbind the texture
    hr = m_pSpriteTextureVar->SetResource( NULL );
    assert( D3D_OK == hr );

    // Apply changes
    if( bAlpha )
    {
	    m_pAlphaSpriteTechnique->GetPassByIndex( 0 )->Apply( 0 );
    }
    else
    {
	    m_pSpriteTechnique->GetPassByIndex( 0 )->Apply( 0 );
    }

    // Optionally render a border
    if( bBordered )
    {
	    RenderBorder();
    }

    return hr;
}

HRESULT Sprite::RenderSpriteMS( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight, 
        bool bAlpha, bool bBordered, int nSampleIndex )
{
    HRESULT hr;

    assert( NULL != pTextureView );

    // Set the texture dimensions for MS textures
    hr = m_pTextureWidthVar->SetFloat( (float)nTextureWidth ); 
    assert( D3D_OK == hr );
    hr = m_pTextureHeightVar->SetFloat( (float)nTextureHeight ); 
    assert( D3D_OK == hr );

    // Set the sample index
    hr = m_pSampleIndexVar->SetInt( nSampleIndex ); 
    assert( D3D_OK == hr );

    // Set position and dimensions
    hr = m_pStartPosXVar->SetFloat( (float)nStartPosX );
    assert( D3D_OK == hr );
    hr = m_pStartPosYVar->SetFloat( (float)nStartPosY ); 
    assert( D3D_OK == hr );
    hr = m_pWidthVar->SetFloat( (float)nWidth ); 
    assert( D3D_OK == hr );
    hr = m_pHeightVar->SetFloat( (float)nHeight ); 
    assert( D3D_OK == hr );

    // Set the texture
    hr = m_pSpriteTextureMSVar->SetResource( pTextureView );
    assert( D3D_OK == hr );

    // Apply technique and changes
    m_pSpriteMSTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Do the render
    Render();

    // Unbind texture
    hr = m_pSpriteTextureMSVar->SetResource( NULL );
    assert( D3D_OK == hr );

    // Apply change
    m_pSpriteMSTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Optionally render a border
    if( bBordered )
    {
	    RenderBorder();
    }

    return hr;
}

HRESULT Sprite::RenderSpriteAsDepth( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, bool bBordered, float fDepthRangeMin, 
        float fDepthRangeMax )
{
    HRESULT hr;

    assert( NULL != pTextureView );

    // Set position and dimensions
    hr = m_pStartPosXVar->SetFloat( (float)nStartPosX );
    assert( D3D_OK == hr );
    hr = m_pStartPosYVar->SetFloat( (float)nStartPosY ); 
    assert( D3D_OK == hr );
    hr = m_pWidthVar->SetFloat( (float)nWidth ); 
    assert( D3D_OK == hr );
    hr = m_pHeightVar->SetFloat( (float)nHeight ); 
    assert( D3D_OK == hr );

    // Set the depth scale
    hr = m_pDepthRangeMinVar->SetFloat( fDepthRangeMin ); 
    assert( D3D_OK == hr );
    hr = m_pDepthRangeMaxVar->SetFloat( fDepthRangeMax ); 
    assert( D3D_OK == hr );

    // Set the texture
    hr = m_pSpriteTextureVar->SetResource( pTextureView );
    assert( D3D_OK == hr );

    // Apply technique and changes
    m_pSpriteAsDepthTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Do the render
    Render();

    // Unbind the texture
    hr = m_pSpriteTextureVar->SetResource( NULL );
    assert( D3D_OK == hr );

    // Apply changes
    m_pSpriteAsDepthTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Optionally render a border
    if( bBordered )
    {
	    RenderBorder();
    }

    return hr;
}

HRESULT Sprite::RenderSpriteAsDepthMS( ID3D10ShaderResourceView1* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight, 
        bool bBordered, float fDepthRangeMin, float fDepthRangeMax, int nSampleIndex )
{
    HRESULT hr;

    assert( NULL != pTextureView );

    // Set the texture dimensions for MS textures
    hr = m_pTextureWidthVar->SetFloat( (float)nTextureWidth ); 
    assert( D3D_OK == hr );
    hr = m_pTextureHeightVar->SetFloat( (float)nTextureHeight ); 
    assert( D3D_OK == hr );

    // Set the sample index
    hr = m_pSampleIndexVar->SetInt( nSampleIndex ); 
    assert( D3D_OK == hr );

    // Set the depth scale
    hr = m_pDepthRangeMinVar->SetFloat( fDepthRangeMin ); 
    assert( D3D_OK == hr );
    hr = m_pDepthRangeMaxVar->SetFloat( fDepthRangeMax ); 
    assert( D3D_OK == hr );

    // Set position and dimensions
    hr = m_pStartPosXVar->SetFloat( (float)nStartPosX );
    assert( D3D_OK == hr );
    hr = m_pStartPosYVar->SetFloat( (float)nStartPosY ); 
    assert( D3D_OK == hr );
    hr = m_pWidthVar->SetFloat( (float)nWidth ); 
    assert( D3D_OK == hr );
    hr = m_pHeightVar->SetFloat( (float)nHeight ); 
    assert( D3D_OK == hr );

    // Set the texture
    hr = m_pSpriteDepthTextureMSVar->SetResource( pTextureView );
    assert( D3D_OK == hr );

    // Apply technique and changes
    m_pSpriteAsDepthMSTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Do the render
    Render();

    // Unbind texture
    hr = m_pSpriteDepthTextureMSVar->SetResource( NULL );
    assert( D3D_OK == hr );

    // Apply change
    m_pSpriteAsDepthMSTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Optionally render a border
    if( bBordered )
    {
	    RenderBorder();
    }

    return hr;
}

//===============================================================================================================================//
//
// Protected functions
//
//===============================================================================================================================//

//===============================================================================================================================//
//
// Private functions
//
//===============================================================================================================================//

void Sprite::RenderBorder()
{
    // Set the input layout
    DXUTGetD3D10Device()->IASetInputLayout( m_pBorderVertexLayout );

    // Set vertex buffer
    UINT Stride = sizeof( SpriteBorderVertex );
    UINT Offset = 0;
    DXUTGetD3D10Device()->IASetVertexBuffers( 0, 1, &m_pBorderVertexBuffer, &Stride, &Offset );

    // Set primitive topology
    DXUTGetD3D10Device()->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP );

    // Apply technique
    m_pBorderTechnique->GetPassByIndex( 0 )->Apply( 0 );

    // Render
    DXUTGetD3D10Device()->Draw( 5, 0 );
}

void Sprite::Render()
{
    // Set the input layout
    DXUTGetD3D10Device()->IASetInputLayout( m_pVertexLayout );

    // Set vertex buffer
    UINT Stride = sizeof( SpriteVertex );
    UINT Offset = 0;
    DXUTGetD3D10Device()->IASetVertexBuffers( 0, 1, &m_pVertexBuffer, &Stride, &Offset );

    // Set primitive topology
    DXUTGetD3D10Device()->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Render
    DXUTGetD3D10Device()->Draw( 6, 0 );
}

//=================================================================================================================================
// EOF.
//=================================================================================================================================
