//--------------------------------------------------------------------------------------
// File: MultiStreamRendering9.cpp
//
// Illustrates multi-stream rendering using Direct3D 9
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

enum STREAM_TYPE
{
    ST_VERTEX_POSITION = 0,
    ST_VERTEX_NORMAL,
    ST_VERTEX_TEXTUREUV,
    ST_VERTEX_TEXTUREUV2,
    ST_MAX_VB_STREAMS,
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // dialog for standard controls
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern bool                         g_bRenderFF;            // Render fixed function or not
extern bool                         g_bUseAltUV;            // Render with an alternative UV stream

// Direct3D 9 resources
ID3DXFont*                          g_pFont9 = NULL;
ID3DXSprite*                        g_pSprite9 = NULL;
ID3DXEffect*                        g_pEffect9 = NULL;
IDirect3DVertexDeclaration9*        g_pDecl = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVBs[ ST_MAX_VB_STREAMS ] = {NULL};
LPDIRECT3DINDEXBUFFER9              g_pIB = NULL;
LPDIRECT3DTEXTURE9                  g_pMeshTex = NULL;

// Effect variable handles
D3DXHANDLE                          g_hLightDir;
D3DXHANDLE                          g_hLightDiffuse;
D3DXHANDLE                          g_hmWorld;
D3DXHANDLE                          g_hmWorldViewProjection;
D3DXHANDLE                          g_hMeshTexture;
D3DXHANDLE                          g_hRenderScene;

// External Mesh Data
extern DWORD                        g_dwNumVertices;
extern DWORD                        g_dwNumFaces;
extern D3DXVECTOR3 g_VertexPositions[];
extern D3DXVECTOR3 g_VertexNormals[];
extern D3DXVECTOR2 g_VertexTextureUV[];
extern D3DXVECTOR2 g_VertexTextureUV2[];
extern DWORD g_Indices[];

#define IDC_FIXEDFUNC           4

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );
extern void RenderText();


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                      D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont9 ) );

    // Enable the FF checkbox
    CDXUTCheckBox* pCheck = g_HUD.GetCheckBox( IDC_FIXEDFUNC );
    pCheck->SetVisible( true );
    pCheck->SetEnabled( true );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"MultiStreamRendering.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect9, NULL ) );

    // Get the effect variable handles
    g_hLightDir = g_pEffect9->GetParameterByName( NULL, "g_LightDir" );
    g_hLightDiffuse = g_pEffect9->GetParameterByName( NULL, "g_LightDiffuse" );
    g_hmWorld = g_pEffect9->GetParameterByName( NULL, "g_mWorld" );
    g_hmWorldViewProjection = g_pEffect9->GetParameterByName( NULL, "g_mWorldViewProjection" );
    g_hMeshTexture = g_pEffect9->GetParameterByName( NULL, "g_MeshTexture" );
    g_hRenderScene = g_pEffect9->GetTechniqueByName( "RenderScene" );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice,
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );
    if( g_pEffect9 ) V_RETURN( g_pEffect9->OnResetDevice() );

    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    //Verts 144 Faces 48
    DWORD dwPositionSize = g_dwNumVertices * sizeof( D3DXVECTOR3 );
    DWORD dwNormalSize = g_dwNumVertices * sizeof( D3DXVECTOR3 );
    DWORD dwTextureUVSize = g_dwNumVertices * sizeof( D3DXVECTOR2 );
    DWORD dwIndexSize = g_dwNumFaces * 3 * sizeof( DWORD );

    // Create the stream object that will hold vertex positions
    V_RETURN( pd3dDevice->CreateVertexBuffer( dwPositionSize,
                                              0,
                                              D3DFVF_XYZ,
                                              D3DPOOL_DEFAULT, &g_pVBs[ST_VERTEX_POSITION], NULL ) );


    // Create the stream object that will hold vertex normals
    V_RETURN( pd3dDevice->CreateVertexBuffer( dwNormalSize,
                                              0,
                                              D3DFVF_NORMAL,
                                              D3DPOOL_DEFAULT, &g_pVBs[ST_VERTEX_NORMAL], NULL ) );

    // Create the stream object that will hold vertex texture uv coordinates
    V_RETURN( pd3dDevice->CreateVertexBuffer( dwTextureUVSize,
                                              0,
                                              D3DFVF_TEX1,
                                              D3DPOOL_DEFAULT, &g_pVBs[ST_VERTEX_TEXTUREUV], NULL ) );

    // Create the stream object that will hold vertex texture uv2 coordinates
    V_RETURN( pd3dDevice->CreateVertexBuffer( dwTextureUVSize,
                                              0,
                                              D3DFVF_TEX1,
                                              D3DPOOL_DEFAULT, &g_pVBs[ST_VERTEX_TEXTUREUV2], NULL ) );

    // Create the index buffer
    V_RETURN( pd3dDevice->CreateIndexBuffer( dwIndexSize,
                                             0,
                                             D3DFMT_INDEX32,
                                             D3DPOOL_DEFAULT, &g_pIB, NULL ) );


    // Now we fill the position vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Multistream. This mechanism is required becuase vertex
    // buffers may be in device memory.
    VOID* pMultistream;
    V_RETURN( g_pVBs[ST_VERTEX_POSITION]->Lock( 0, dwPositionSize, ( void** )&pMultistream, 0 ) );
    CopyMemory( pMultistream, g_VertexPositions, dwPositionSize );
    g_pVBs[ST_VERTEX_POSITION]->Unlock();

    // Now we fill the normal vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Multistream. This mechanism is required becuase vertex
    // buffers may be in device memory.
    V_RETURN( g_pVBs[ST_VERTEX_NORMAL]->Lock( 0, dwNormalSize, ( void** )&pMultistream, 0 ) );
    CopyMemory( pMultistream, g_VertexNormals, dwNormalSize );
    g_pVBs[ST_VERTEX_NORMAL]->Unlock();

    // Now we fill the texture uv vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Multistream. This mechanism is required becuase vertex
    // buffers may be in device memory.
    V_RETURN( g_pVBs[ST_VERTEX_TEXTUREUV]->Lock( 0, dwTextureUVSize, ( void** )&pMultistream, 0 ) );
    CopyMemory( pMultistream, g_VertexTextureUV, dwTextureUVSize );
    g_pVBs[ST_VERTEX_TEXTUREUV]->Unlock();

    // Now we fill the texture uv2 vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the Multistream. This mechanism is required becuase vertex
    // buffers may be in device memory.
    V_RETURN( g_pVBs[ST_VERTEX_TEXTUREUV2]->Lock( 0, dwTextureUVSize, ( void** )&pMultistream, 0 ) );
    CopyMemory( pMultistream, g_VertexTextureUV2, dwTextureUVSize );
    g_pVBs[ST_VERTEX_TEXTUREUV2]->Unlock();

    // Fill the index buffer
    VOID* pIndices;
    V_RETURN( g_pIB->Lock( 0, dwIndexSize, ( void** )&pIndices, 0 ) );
    CopyMemory( pIndices, g_Indices, dwIndexSize );
    g_pIB->Unlock();

    // Create a Vertex Decl for the MultiStream data.
    // Notice that the first parameter is the stream index.  This indicates the
    // VB that the particular data comes from.  In this case, position data
    // comes from stream 0.  Normal data comes from stream 1.  Texture coordinate
    // data comes from stream 2.
    D3DVERTEXELEMENT9 declDesc[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
        {2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    pd3dDevice->CreateVertexDeclaration( declDesc, &g_pDecl );

    // Load a texture for the mesh
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\ball.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pMeshTex ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the Fixed Function Pipeline
//--------------------------------------------------------------------------------------
void RenderFixedFunction( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;

    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    // Set up the world matrix from the camera
    pd3dDevice->SetTransform( D3DTS_WORLD, &mWorld );

    // Set up the view matrix from the camera
    pd3dDevice->SetTransform( D3DTS_VIEW, &mView );

    // Set up the projection matrix from the camera
    pd3dDevice->SetTransform( D3DTS_PROJECTION, &mProj );

    // Enable Fixed Function Lighting
    pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    D3DLIGHT9 Light;
    ZeroMemory( &Light, sizeof( D3DLIGHT9 ) );
    Light.Type = D3DLIGHT_POINT;
    Light.Diffuse.r = 1.0f;
    Light.Diffuse.g = 1.0f;
    Light.Diffuse.b = 1.0f;
    Light.Diffuse.a = 1.0f;
    Light.Ambient.r = 1.0f;
    Light.Ambient.g = 1.0f;
    Light.Ambient.b = 1.0f;
    Light.Ambient.a = 1.0f;
    Light.Position.x = -5000.0f;
    Light.Position.y = 5000.0f;
    Light.Position.z = -5000.0f;
    Light.Attenuation0 = 1.0f;
    Light.Range = 10000.0f;
    pd3dDevice->SetLight( 0, &Light );
    pd3dDevice->LightEnable( 0, TRUE );

    // Setup a basic material
    D3DMATERIAL9 mat;
    ZeroMemory( &mat, sizeof( D3DMATERIAL9 ) );
    mat.Diffuse.r = 1.0f;
    mat.Diffuse.g = 1.0f;
    mat.Diffuse.b = 1.0f;
    mat.Diffuse.a = 1.0f;
    pd3dDevice->SetMaterial( &mat );

    // Setup texturing
    pd3dDevice->SetTexture( 0, g_pMeshTex );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    // Setup our multiple streams
    pd3dDevice->SetStreamSource( 0, g_pVBs[ST_VERTEX_POSITION], 0, sizeof( D3DXVECTOR3 ) );
    pd3dDevice->SetStreamSource( 1, g_pVBs[ST_VERTEX_NORMAL], 0, sizeof( D3DXVECTOR3 ) );
    if( g_bUseAltUV )
        pd3dDevice->SetStreamSource( 2, g_pVBs[ST_VERTEX_TEXTUREUV2], 0, sizeof( D3DXVECTOR2 ) );
    else
        pd3dDevice->SetStreamSource( 2, g_pVBs[ST_VERTEX_TEXTUREUV], 0, sizeof( D3DXVECTOR2 ) );

    // Set our index buffer as well
    pd3dDevice->SetIndices( g_pIB );

    // Set A Multistream Vertex Decl insted of FVF
    pd3dDevice->SetVertexDeclaration( g_pDecl );

    // Set the VS and PS to NULL, so that we know we're going through the Fixed Function Pipeline
    pd3dDevice->SetVertexShader( NULL );
    pd3dDevice->SetPixelShader( NULL );

    // Render the mesh through the Fixed Function Pipeline
    pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, g_dwNumVertices, 0, g_dwNumFaces );
}

//--------------------------------------------------------------------------------------
// Render the scene using the programmable pipeline
//--------------------------------------------------------------------------------------
void RenderProgrammable( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    mWorldViewProjection = mWorld * mView * mProj;

    D3DXVECTOR3 vLightDir( -1, 1, -1 );
    D3DXVec3Normalize( &vLightDir, &vLightDir );
    D3DXVECTOR4 vLightDiffuse( 1, 1, 1, 1 );

    // Update the effect's variables.  Instead of using strings, it would 
    // be more efficient to cache a handle to the parameter by calling 
    // ID3DXEffect::GetParameterByName
    V( g_pEffect9->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
    V( g_pEffect9->SetMatrix( g_hmWorld, &mWorld ) );
    V( g_pEffect9->SetFloatArray( g_hLightDir, vLightDir, 3 ) );
    V( g_pEffect9->SetFloatArray( g_hLightDiffuse, vLightDiffuse, 4 ) );

    // Setup texturing
    V( g_pEffect9->SetTexture( g_hMeshTexture, g_pMeshTex ) );

    // Setup our multiple streams
    pd3dDevice->SetStreamSource( 0, g_pVBs[ST_VERTEX_POSITION], 0, sizeof( D3DXVECTOR3 ) );
    pd3dDevice->SetStreamSource( 1, g_pVBs[ST_VERTEX_NORMAL], 0, sizeof( D3DXVECTOR3 ) );
    if( g_bUseAltUV )
        pd3dDevice->SetStreamSource( 2, g_pVBs[ST_VERTEX_TEXTUREUV2], 0, sizeof( D3DXVECTOR2 ) );
    else
        pd3dDevice->SetStreamSource( 2, g_pVBs[ST_VERTEX_TEXTUREUV], 0, sizeof( D3DXVECTOR2 ) );

    // Set our index buffer as well
    pd3dDevice->SetIndices( g_pIB );

    // Set A Multistream Vertex Decl insted of FVF
    pd3dDevice->SetVertexDeclaration( g_pDecl );

    // Render the scene with this technique 
    V( g_pEffect9->SetTechnique( g_hRenderScene ) );

    // Apply the technique contained in the effect 
    UINT cPasses = 0;
    V( g_pEffect9->Begin( &cPasses, 0 ) );

    for( UINT iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect9->BeginPass( iPass ) );

        // The effect interface queues up the changes and performs them 
        // with the CommitChanges call. You do not need to call CommitChanges if 
        // you are not setting any parameters between the BeginPass and EndPass.
        // V( g_pEffect->CommitChanges() );

        // Render the mesh with the applied technique
        pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, g_dwNumVertices, 0, g_dwNumFaces );

        V( g_pEffect9->EndPass() );
    }
    V( g_pEffect9->End() );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 160, 160, 250 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        if( g_bRenderFF )
            RenderFixedFunction( pd3dDevice, fTime, fElapsedTime, pUserContext );
        else
            RenderProgrammable( pd3dDevice, fTime, fElapsedTime, pUserContext );

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );
        DXUT_EndPerfEvent();

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

    for( int i = 0; i < ST_MAX_VB_STREAMS; i++ )
    {
        SAFE_RELEASE( g_pVBs[i] );
    }
    SAFE_RELEASE( g_pIB );
    SAFE_RELEASE( g_pDecl );
    SAFE_RELEASE( g_pMeshTex );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect9 );
    SAFE_RELEASE( g_pFont9 );
}
