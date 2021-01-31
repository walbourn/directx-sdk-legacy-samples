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

#define NUM_EFFECTS 3
struct EFFECT_DATA9
{
    ID3DXEffect* pEffect;
    D3DXHANDLE hRenderScene;
    D3DXHANDLE hmWorld;
    D3DXHANDLE hMaterialDiffuseColor;
};

extern D3DXVECTOR4 g_vDiffuseColors[];

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // dialog for standard controls
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern CDXUTSDKMesh                 g_Mesh;

// Direct3D 9 resources
ID3DXFont*                          g_pFont9 = NULL;
ID3DXSprite*                        g_pSprite9 = NULL;
IDirect3DVertexDeclaration9*        g_pDecl = NULL;

// Shared effect variables
ID3DXEffectPool*                    g_pEffectPool = NULL;
D3DXHANDLE                          g_hmViewProj = NULL;
D3DXHANDLE                          g_hLightDir = NULL;
D3DXHANDLE                          g_hLightDiffuse = NULL;
D3DXHANDLE                          g_hTexMove = NULL;
D3DXHANDLE                          g_htxDiffuse = NULL;

// Individual effect data
EFFECT_DATA9 g_EffectData[NUM_EFFECTS];

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

    // Create an effect pool
    V_RETURN( D3DXCreateEffectPool( &g_pEffectPool ) );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // Create the individual effects
    for( UINT i = 0; i < NUM_EFFECTS; i++ )
    {
        // Load the effect as a child effect
        WCHAR strEffect[MAX_PATH];
        if( i == 2 )
            swprintf_s( strEffect, MAX_PATH, L"EffectPools%d.fxo9", i + 1 );
        else
            swprintf_s( strEffect, MAX_PATH, L"EffectPools%d.fx", i + 1 );
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strEffect ) );
        V_RETURN( D3DXCreateEffectFromFile( pd3dDevice,
                                            str,
                                            NULL,
                                            NULL,
                                            dwShaderFlags,
                                            g_pEffectPool,
                                            &g_EffectData[i].pEffect,
                                            NULL ) );

        // Get effect-specific variables
        g_EffectData[i].hRenderScene = g_EffectData[i].pEffect->GetTechniqueByName( "RenderScene" );
        g_EffectData[i].hmWorld = g_EffectData[i].pEffect->GetParameterByName( NULL, "g_mWorld" );
        g_EffectData[i].hMaterialDiffuseColor = g_EffectData[i].pEffect->GetParameterByName( NULL,
                                                                                             "g_MaterialDiffuseColor"
                                                                                             );
    }

    // Get the shared effect variable handles
    g_hmViewProj = g_EffectData[0].pEffect->GetParameterByName( NULL, "g_mViewProj" );
    g_hLightDir = g_EffectData[0].pEffect->GetParameterByName( NULL, "g_LightDir" );
    g_hLightDiffuse = g_EffectData[0].pEffect->GetParameterByName( NULL, "g_LightDiffuse" );
    g_hTexMove = g_EffectData[0].pEffect->GetParameterByName( NULL, "g_TexMove" );
    g_htxDiffuse = g_EffectData[0].pEffect->GetParameterByName( NULL, "g_txDiffuse" );

    // Create a mesh to render
    g_Mesh.Create( pd3dDevice, L"misc\\ball.sdkmesh" );

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

    for( UINT i = 0; i < NUM_EFFECTS; i++ )
    {
        if( g_EffectData[i].pEffect ) V_RETURN( g_EffectData[i].pEffect->OnResetDevice() );
    }

    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    // Create a Vertex Decl
    D3DVERTEXELEMENT9 declDesc[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    pd3dDevice->CreateVertexDeclaration( declDesc, &g_pDecl );

    return S_OK;
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
        D3DXMATRIX mWorld;
        D3DXMATRIX mProj;
        D3DXMATRIX mView;
        D3DXMATRIX mViewProj;

        // Get the projection & view matrix from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();
        mViewProj = mView * mProj;

        // Update the shared variables.
        g_EffectData[0].pEffect->SetMatrix( g_hmViewProj, &mViewProj );
        D3DXVECTOR4 vLightDir( -1, 1, -1, 1 );
        D3DXVec3Normalize( ( D3DXVECTOR3* )&vLightDir, ( D3DXVECTOR3* )&vLightDir );
        D3DXVECTOR4 vLightDiffuse( 1, 1, 1, 1 );
        g_EffectData[0].pEffect->SetVector( g_hLightDir, &vLightDir );
        g_EffectData[0].pEffect->SetVector( g_hLightDiffuse, &vLightDiffuse );
        g_EffectData[0].pEffect->SetFloat( g_hTexMove, ( float )fTime * 0.5f );
        g_EffectData[0].pEffect->SetTexture( g_htxDiffuse, g_Mesh.GetMaterial( 0 )->pDiffuseTexture9 );

        // Set the input layout
        pd3dDevice->SetVertexDeclaration( g_pDecl );

        // Draw the meshes for each effect
        for( int i = 0; i < NUM_EFFECTS; i++ )
        {
            D3DXMATRIX mWorld;
            D3DXMatrixTranslation( &mWorld, ( i - 1 ) * 2.0f, 0.0f, 0.0f );

            g_EffectData[i].pEffect->SetMatrix( g_EffectData[i].hmWorld, &mWorld );
            g_EffectData[i].pEffect->SetVector( g_EffectData[i].hMaterialDiffuseColor, &g_vDiffuseColors[i] );
            g_Mesh.Render( pd3dDevice, g_EffectData[i].pEffect, g_EffectData[i].hRenderScene );
        }

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
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

    for( UINT i = 0; i < NUM_EFFECTS; i++ )
    {
        g_EffectData[i].pEffect->OnLostDevice();
    }

    SAFE_RELEASE( g_pDecl );
    g_Mesh.Destroy();

}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();

    for( UINT i = 0; i < NUM_EFFECTS; i++ )
    {
        SAFE_RELEASE( g_EffectData[i].pEffect );
    }

    SAFE_RELEASE( g_pFont9 );
}
