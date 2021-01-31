//--------------------------------------------------------------------------------------
// File: SceneMesh.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "scenemesh.h"
#include <stdio.h>

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

//--------------------------------------------------------------------------------------
CSceneMesh::CSceneMesh( void )
{
    m_pMesh = NULL;
    m_pEffect = NULL;

    m_pTexture = NULL;
    //m_pCapturedRadiance = NULL;
    //m_pRenderToEnvMap = NULL;

    m_fObjectRadius = 0.0f;
    m_vObjectCenter = D3DXVECTOR3( 0, 0, 0 );

    m_vBoundingBoxMin = D3DXVECTOR3( 0, 0, 0 );
    m_vBoundingBoxMax = D3DXVECTOR3( 0, 0, 0 );

    m_pMaterials = NULL;
    m_dwNumMaterials = 0;

    ZeroMemory( &m_ReloadState, sizeof( RELOAD_STATE ) );

}


//--------------------------------------------------------------------------------------
CSceneMesh::~CSceneMesh( void )
{
    SAFE_RELEASE( m_pMesh );
    SAFE_RELEASE( m_pTexture );
    //   SAFE_RELEASE( m_pCapturedRadiance );
    //   SAFE_RELEASE ( m_pRenderToEnvMap );

    SAFE_RELEASE( m_pMaterialBuffer );
    SAFE_RELEASE( m_pEffect );

    return;
}


//--------------------------------------------------------------------------------------
HRESULT CSceneMesh::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice )
{
    HRESULT hr;

    m_pd3dDevice = pd3dDevice;

    if( m_ReloadState.bUseReloadState )
    {
        LoadMesh( pd3dDevice, m_ReloadState.strMeshFileName, m_ReloadState.strTextureFileName );
        LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );

        WCHAR str[MAX_PATH];
        wcscpy_s( str, MAX_PATH, m_ReloadState.strMeshFileName );
        wcscat_s( str, MAX_PATH, TEXT( "bmp" ) );

        V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                               D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                               D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               NULL, NULL, &m_pTexture ) );
    }

    //IDirect3DSurface9* pSurface;
    //V_RETURN( pd3dDevice->GetDepthStencilSurface( &pSurface ) );

    //D3DSURFACE_DESC depthDesc;
    //V_RETURN( pSurface->GetDesc(&depthDesc) );
    //SAFE_RELEASE(pSurface);

    //V_RETURN( D3DXCreateRenderToEnvMap(pd3dDevice, 64, 0, renderTargetColorFormat, true, renderTargetDepthFormat, &m_pRenderToEnvMap ) );
    //V_RETURN( D3DXCreateCubeTexture(pd3dDevice, 64, 0, 0, renderTargetColorFormat, D3DPOOL_MANAGED, &m_pCapturedRadiance ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT CSceneMesh::OnResetDevice()
{
    HRESULT hr;
    if( m_pEffect )
    {
        V( m_pEffect->OnResetDevice() );
    }

    // Cache this information for picking
    m_pd3dDevice->GetViewport( &m_ViewPort );

    //m_pRenderToEnvMap->OnResetDevice();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also optimizes the 
// mesh for the graphics card's vertex cache, which improves performance by organizing 
// the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT CSceneMesh::LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strMeshFileName, WCHAR* strTextureFileName )
{
    WCHAR str[MAX_PATH];
    HRESULT hr;

    // Release any previous mesh object
    SAFE_RELEASE( m_pMesh );
    SAFE_RELEASE( m_pMaterialBuffer );

    SAFE_RELEASE( m_pTexture );


    // Load the mesh object
    DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strMeshFileName );
    wcscpy_s( m_ReloadState.strMeshFileName, MAX_PATH, str );
    V_RETURN( D3DXLoadMeshFromX( m_ReloadState.strMeshFileName, D3DXMESH_MANAGED, pd3dDevice, NULL, &m_pMaterialBuffer,
                                 NULL, &m_dwNumMaterials, &m_pMesh ) );
    m_pMaterials = ( D3DXMATERIAL* )m_pMaterialBuffer->GetBufferPointer();

    DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strTextureFileName );
    wcscpy_s( m_ReloadState.strTextureFileName, MAX_PATH, str );

    // Change the current directory to the mesh's directory so we can
    // find the textures.
    WCHAR* pLastSlash = wcsrchr( str, L'\\' );
    if( pLastSlash )
    {
        *( pLastSlash + 1 ) = 0;
    }
    WCHAR strCWD[MAX_PATH];
    GetCurrentDirectory( MAX_PATH, strCWD );
    //SetCurrentDirectory( str );

    // Lock the vertex buffer to get the object's radius & center
    // simply to help position the camera a good distance away from the mesh.
    IDirect3DVertexBuffer9* pVB = NULL;
    void* pVertices;
    V_RETURN( m_pMesh->GetVertexBuffer( &pVB ) );
    V_RETURN( pVB->Lock( 0, 0, &pVertices, 0 ) );

    D3DVERTEXELEMENT9 Declaration[MAXD3DDECLLENGTH + 1];
    m_pMesh->GetDeclaration( Declaration );
    DWORD dwStride = D3DXGetDeclVertexSize( Declaration, 0 );
    V_RETURN( D3DXComputeBoundingSphere( ( D3DXVECTOR3* )pVertices, m_pMesh->GetNumVertices(),
                                         dwStride, &m_vObjectCenter, &m_fObjectRadius ) );
    V_RETURN( D3DXComputeBoundingBox( ( D3DXVECTOR3* )pVertices, m_pMesh->GetNumVertices(),
                                      dwStride, &m_vBoundingBoxMin, &m_vBoundingBoxMax ) );

    pVB->Unlock();
    SAFE_RELEASE( pVB );

    // Optimize the mesh for this graphics card's vertex cache 
    // so when rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader 
    // on those vertices so it will improve perf.     
    DWORD* rgdwAdjacency = NULL;
    rgdwAdjacency = new DWORD[m_pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
    {
        return E_OUTOFMEMORY;
    }
    V( m_pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( m_pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_IGNOREVERTS,
                                 rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    // Create the textures from a file
    V( D3DXCreateTextureFromFileEx( pd3dDevice, m_ReloadState.strTextureFileName, D3DX_DEFAULT, D3DX_DEFAULT,
                                    D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                    D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                    NULL, NULL, &m_pTexture ) );

    //SetCurrentDirectory( strCWD );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CSceneMesh::SetMesh( IDirect3DDevice9* pd3dDevice, ID3DXMesh* pMesh )
{
    HRESULT hr;

    // Release any previous mesh object
    SAFE_RELEASE( m_pMesh );

    m_pMesh = pMesh;

    // Sort the attributes
    DWORD* rgdwAdjacency = NULL;
    rgdwAdjacency = new DWORD[m_pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
    {
        return E_OUTOFMEMORY;
    }
    V( m_pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( m_pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_IGNOREVERTS,
                                 rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    return S_OK;
}

//-----------------------------------------------------------------------------
HRESULT CSceneMesh::LoadEffects( IDirect3DDevice9* pd3dDevice, const D3DCAPS9* pDeviceCaps )
{
    HRESULT hr;

    SAFE_RELEASE( m_pEffect );

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the shader debugger.  
    // Debugging vertex shaders requires either REF or software vertex processing, and debugging 
    // pixel shaders requires REF.  The D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug 
    // experience in the shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile against the next 
    // higher available software target, which ensures that the unoptimized shaders do not exceed 
    // the shader model limitations.  Setting these flags will cause slower rendering since the shaders 
    // will be unoptimized and forced into software.  See the DirectX documentation for more information 
    // about using the shader debugger.
    DWORD dwShaderFlags = 0;

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
    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Scene.fx" ) ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &m_pEffect, NULL ) );

    return S_OK;
}

//-----------------------------------------------------------------------------
bool CSceneMesh::GetBoundingBox( D3DXVECTOR3* pMin, D3DXVECTOR3* pMax )
{
    if( ( NULL == pMin ) || ( NULL == pMax ) )
    {
        return false;
    }

    *pMin = m_vBoundingBoxMin;
    *pMax = m_vBoundingBoxMax;

    return true;
}

//--------------------------------------------------------------------------------------
void CSceneMesh::Render( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj )
{
    HRESULT hr;
    UINT iPass, cPasses;

    if( ( m_pEffect == NULL ) || ( m_pMesh == NULL ) )
    {
        return;
    }

    V( m_pEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj ) );

    V( m_pEffect->SetTechnique( "RenderScene" ) );

    V( m_pEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            V( m_pEffect->SetTexture( "g_tTexture", m_pTexture ) );
            V( m_pEffect->CommitChanges() );

            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pEffect->EndPass() );
    }

    V( m_pEffect->End() );

    return;
}

//--------------------------------------------------------------------------------------
void CSceneMesh::RenderRadiance( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldViewProj )
{
    HRESULT hr;
    UINT iPass, cPasses;

    if( ( m_pEffect == NULL ) || ( m_pMesh == NULL ) )
    {
        return;
    }

    V( m_pEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj ) );

    V( m_pEffect->SetTechnique( "RenderSceneRadiance" ) );

    V( m_pEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            V( m_pEffect->SetTexture( "g_tTexture", m_pTexture ) );
            V( m_pEffect->CommitChanges() );

            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pEffect->EndPass() );
    }

    V( m_pEffect->End() );

    return;
}

//--------------------------------------------------------------------------------------
void CSceneMesh::RenderDepth( IDirect3DDevice9* pd3dDevice, D3DXMATRIX* pmWorldView, D3DXMATRIX* pmWorldViewProj )
{
    HRESULT hr;
    UINT iPass, cPasses;

    if( ( m_pEffect == NULL ) || ( m_pMesh == NULL ) )
    {
        return;
    }

    V( m_pEffect->SetMatrix( "g_mWorldViewProjection", pmWorldViewProj ) );
    V( m_pEffect->SetMatrix( "g_mWorldView", pmWorldView ) );

    V( m_pEffect->SetTechnique( "RenderSceneDepth" ) );

    V( m_pEffect->Begin( &cPasses, 0 ) );

    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pEffect->BeginPass( iPass ) );

        DWORD dwAttribs = 0;
        V( m_pMesh->GetAttributeTable( NULL, &dwAttribs ) );
        for( DWORD i = 0; i < dwAttribs; i++ )
        {
            V( m_pMesh->DrawSubset( i ) );
        }

        V( m_pEffect->EndPass() );
    }

    V( m_pEffect->End() );

    return;
}










































//--------------------------------------------------------------------------------------
void CSceneMesh::OnLostDevice()
{
    HRESULT hr;
    if( m_pEffect )
        V( m_pEffect->OnLostDevice() );
    //if ( m_pRenderToEnvMap )
    //   V(m_pRenderToEnvMap->OnLostDevice() );
}


//--------------------------------------------------------------------------------------
void CSceneMesh::OnDestroyDevice()
{
    if( !m_ReloadState.bUseReloadState )
        ZeroMemory( &m_ReloadState, sizeof( RELOAD_STATE ) );

    SAFE_RELEASE( m_pMesh );
    SAFE_RELEASE( m_pTexture );
    //SAFE_RELEASE( m_pRenderToEnvMap );
    //SAFE_RELEASE( m_pCapturedRadiance );

    SAFE_RELEASE( m_pMaterialBuffer );
    SAFE_RELEASE( m_pEffect );
}
