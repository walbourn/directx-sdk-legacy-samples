//--------------------------------------------------------------------------------------
// File: ShadowVolume9.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "d3d9Mesh.h"
#include "resource.h"
#include "GenShadowMesh.h"


//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define MAX_NUM_LIGHTS 6
//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

enum RENDER_TYPE
{
    RENDERTYPE_SCENE,
    RENDERTYPE_SHADOWVOLUME,
    RENDERTYPE_COMPLEXITY
};

extern D3DXVECTOR4 g_vShadowColor[MAX_NUM_LIGHTS];

struct CLight
{
    D3DXVECTOR3 m_Position;  // Light position
    D3DXVECTOR4 m_Color;     // Light color
    D3DXMATRIX m_mWorld;  // World transform
};

#define IDC_RENDERTYPE          5
#define IDC_ENABLELIGHT         6
#define IDC_LUMINANCELABEL      7
#define IDC_LUMINANCE           8
#define IDC_BACKGROUND          9


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern bool                         g_bShowHelp;            // If true, it renders the UI control text
extern CFirstPersonCamera           g_Camera;               // A model viewing camera
extern CModelViewerCamera           g_MCamera;              // Camera for mesh control
extern CModelViewerCamera           g_LCamera;              // Camera for easy light movement control
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_D3DSettingsDlg;       // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // manages the 3D UI
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern D3DXMATRIX                   g_mWorldScaling;        // Scaling to apply to mesh
extern D3DXMATRIX g_mWorldBack[2];        // Background matrix
extern int                          g_nCurrBackground;
extern bool                         g_bShowShadowVolume;    // Whether the shadow volume is visibly shown.
extern RENDER_TYPE                  g_RenderType;           // Type of rendering to perform
extern int                          g_nNumLights;           // Number of active lights
extern CLight g_aLights[MAX_NUM_LIGHTS];  // Light objects
extern bool                         g_bLeftButtonDown;
extern bool                         g_bRightButtonDown;
extern bool                         g_bMiddleButtonDown;
extern D3DXVECTOR4                  g_vAmbient;

// Direct3D 9 resources
ID3DXFont*                          g_pFont = NULL;             // Font for drawing text
ID3DXSprite*                        g_pTextSprite = NULL;       // Sprite for batching draw text calls
ID3DXEffect*                        g_pEffect = NULL;           // D3DX effect interface
ID3DXMesh*                          g_pMesh = NULL;             // Mesh object
IDirect3DTexture9*                  g_pDefaultTex = NULL;       // Mesh texture
IDirect3DVertexDeclaration9*        g_pMeshDecl = NULL; // Vertex declaration for the meshes
IDirect3DVertexDeclaration9*        g_pShadowDecl = NULL;// Vertex declaration for the shadow volume
IDirect3DVertexDeclaration9*        g_pPProcDecl = NULL;// Vertex declaration for post-process quad rendering
D3DXHANDLE                          g_hRenderShadow;        // Technique handle for rendering shadow
D3DXHANDLE                          g_hShowShadow;          // Technique handle for showing shadow volume
D3DXHANDLE                          g_hRenderScene;         // Technique handle for rendering scene
D3DXHANDLE                          g_hfFarClip;
D3DXHANDLE                          g_hmProj;
D3DXHANDLE                          g_hRenderSceneAmbient;
D3DXHANDLE                          g_htxScene;
D3DXHANDLE                          g_hvMatColor;
D3DXHANDLE                          g_hmWorldView;
D3DXHANDLE                          g_hmWorldViewProjection;
D3DXHANDLE                          g_hvAmbient;
D3DXHANDLE                          g_hvLightView;
D3DXHANDLE                          g_hvLightColor;
D3DXHANDLE                          g_hRenderShadowVolumeComplexity;
D3DXHANDLE                          g_hvShadowColor;
D3DXHANDLE                          g_hRenderComplexity;

CD3D9Mesh g_Background[2];        // Background mesh
CD3D9Mesh                           g_LightMesh;            // Mesh to represent the light source
CD3D9Mesh                           g_Mesh;                 // The mesh object
ID3DXMesh*                          g_pShadowMesh = NULL;   // The shadow volume mesh


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

extern BOOL IsDepthFormatOk( D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat );
extern void ComputeMeshScaling( CD3D9Mesh& Mesh, D3DXMATRIX* pmScalingCenter );


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

    // Must support pixel shader 2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // Must support stencil buffer
    if( !IsDepthFormatOk( D3DFMT_D24S8,
                          AdapterFormat,
                          BackBufferFormat ) &&
        !IsDepthFormatOk( D3DFMT_D24X4S4,
                          AdapterFormat,
                          BackBufferFormat ) &&
        !IsDepthFormatOk( D3DFMT_D15S1,
                          AdapterFormat,
                          BackBufferFormat ) &&
        !IsDepthFormatOk( D3DFMT_D24FS8,
                          AdapterFormat,
                          BackBufferFormat ) )
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
    V_RETURN( g_D3DSettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( pd3dDevice->CreateVertexDeclaration( MESHVERT::Decl, &g_pMeshDecl ) );
    V_RETURN( pd3dDevice->CreateVertexDeclaration( SHADOWVERT::Decl, &g_pShadowDecl ) );
    V_RETURN( pd3dDevice->CreateVertexDeclaration( POSTPROCVERT::Decl, &g_pPProcDecl ) );

    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ShadowVolume.fx" ) );
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, NULL ) );

    // Determine the rendering techniques to use based on device caps
    D3DCAPS9 Caps;
    V_RETURN( pd3dDevice->GetDeviceCaps( &Caps ) );
    g_hRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );

    // If 2-sided stencil is supported, use it.
    if( Caps.StencilCaps & D3DSTENCILCAPS_TWOSIDED )
    {
        g_hRenderShadow = g_pEffect->GetTechniqueByName( "RenderShadowVolume2Sided" );
        g_hShowShadow = g_pEffect->GetTechniqueByName( "ShowShadowVolume2Sided" );
    }
    else
    {
        g_hRenderShadow = g_pEffect->GetTechniqueByName( "RenderShadowVolume" );
        g_hShowShadow = g_pEffect->GetTechniqueByName( "ShowShadowVolume" );
    }

    g_hfFarClip = g_pEffect->GetParameterByName( NULL, "g_fFarClip" );
    g_hmProj = g_pEffect->GetParameterByName( NULL, "g_mProj" );
    g_hRenderSceneAmbient = g_pEffect->GetTechniqueByName( "RenderSceneAmbient" );
    g_htxScene = g_pEffect->GetParameterByName( NULL, "g_txScene" );
    g_hvMatColor = g_pEffect->GetParameterByName( NULL, "g_vMatColor" );
    g_hmWorldView = g_pEffect->GetParameterByName( NULL, "g_mWorldView" );
    g_hmWorldViewProjection = g_pEffect->GetParameterByName( NULL, "g_mWorldViewProjection" );
    g_hvAmbient = g_pEffect->GetParameterByName( NULL, "g_vAmbient" );
    g_hvLightView = g_pEffect->GetParameterByName( NULL, "g_vLightView" );
    g_hvLightColor = g_pEffect->GetParameterByName( NULL, "g_vLightColor" );
    g_hRenderShadowVolumeComplexity = g_pEffect->GetTechniqueByName( "RenderShadowVolumeComplexity" );
    g_hvShadowColor = g_pEffect->GetParameterByName( NULL, "g_vShadowColor" );
    g_hRenderComplexity = g_pEffect->GetTechniqueByName( "RenderComplexity" );

    // Load the meshes
    V_RETURN( g_Background[0].Create( pd3dDevice, L"misc\\cell.x" ) );
    g_Background[0].SetVertexDecl( pd3dDevice, MESHVERT::Decl );
    V_RETURN( g_Background[1].Create( pd3dDevice, L"misc\\seafloor.x" ) );
    g_Background[1].SetVertexDecl( pd3dDevice, MESHVERT::Decl );
    V_RETURN( g_LightMesh.Create( pd3dDevice, L"misc\\sphere0.x" ) );
    g_LightMesh.SetVertexDecl( pd3dDevice, MESHVERT::Decl );
    V_RETURN( g_Mesh.Create( pd3dDevice, L"dwarf\\dwarf.x" ) );
    g_Mesh.SetVertexDecl( pd3dDevice, MESHVERT::Decl );

    // Compute the scaling matrix for the mesh so that the size of the mesh
    // that shows on the screen is consistent.
    ComputeMeshScaling( g_Mesh, &g_mWorldScaling );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_LCamera.SetViewParams( &vecEye, &vecAt );
    g_MCamera.SetViewParams( &vecEye, &vecAt );

    // Create the 1x1 white default texture
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, 0, D3DFMT_A8R8G8B8,
                                         D3DPOOL_MANAGED, &g_pDefaultTex, NULL ) );
    D3DLOCKED_RECT lr;
    V_RETURN( g_pDefaultTex->LockRect( 0, &lr, NULL, 0 ) );
    *( LPDWORD )lr.pBits = D3DCOLOR_RGBA( 255, 255, 255, 255 );
    V_RETURN( g_pDefaultTex->UnlockRect( 0 ) );

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
    V_RETURN( g_D3DSettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont ) V_RETURN( g_pFont->OnResetDevice() );
    if( g_pEffect ) V_RETURN( g_pEffect->OnResetDevice() );

    g_Background[0].RestoreDeviceObjects( pd3dDevice );
    g_Background[1].RestoreDeviceObjects( pd3dDevice );
    g_LightMesh.RestoreDeviceObjects( pd3dDevice );
    g_Mesh.RestoreDeviceObjects( pd3dDevice );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont, g_pTextSprite, NULL, NULL, 15 );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 20.0f );
    g_MCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_pEffect->SetFloat( g_hfFarClip, 20.0f - EXTRUDE_EPSILON );
    V( g_pEffect->SetMatrix( g_hmProj, g_Camera.GetProjMatrix() ) );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( 0, pBackBufferSurfaceDesc->Height - 200 );
    g_SampleUI.SetSize( pBackBufferSurfaceDesc->Width, 150 );

    int iY = 10;
    g_SampleUI.GetControl( IDC_BACKGROUND )->SetLocation( pBackBufferSurfaceDesc->Width - 200, iY += 24 );
    g_SampleUI.GetControl( IDC_ENABLELIGHT )->SetLocation( pBackBufferSurfaceDesc->Width - 200, iY += 24 );
    g_SampleUI.GetControl( IDC_RENDERTYPE )->SetLocation( pBackBufferSurfaceDesc->Width - 200, iY += 24 );
    g_SampleUI.GetControl( IDC_LUMINANCELABEL )->SetLocation( pBackBufferSurfaceDesc->Width - 145, iY += 30 );
    g_SampleUI.GetControl( IDC_LUMINANCE )->SetLocation( pBackBufferSurfaceDesc->Width - 145, iY += 20 );

    // Generate the shadow volume mesh
    GenerateShadowMesh( pd3dDevice, g_Mesh.GetMesh(), &g_pShadowMesh );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Simply renders the entire scene without any shadow handling.
//--------------------------------------------------------------------------------------
void D3D9RenderScene( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, bool bRenderLight )
{
    HRESULT hr;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mViewProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Get the projection & view matrix from the camera class
    D3DXMatrixMultiply( &mViewProj, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix() );

    // Render the lights if requested
    if( bRenderLight )
    {
        D3DXHANDLE hCurrTech;
        hCurrTech = g_pEffect->GetCurrentTechnique();  // Save the current technique
        V( g_pEffect->SetTechnique( g_hRenderSceneAmbient ) );
        V( g_pEffect->SetTexture( g_htxScene, g_pDefaultTex ) );
        D3DXVECTOR4 vLightMat( 1.0f, 1.0f, 1.0f, 1.0f );
        V( g_pEffect->SetVector( g_hvMatColor, &vLightMat ) );
        UINT cPasses;
        ID3DXMesh* pMesh = g_LightMesh.GetMesh();
        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( UINT p = 0; p < cPasses; ++p )
        {
            V( g_pEffect->BeginPass( p ) );
            for( int i = 0; i < g_nNumLights; ++i )
            {
                for( UINT m = 0; m < g_LightMesh.m_dwNumMaterials; ++m )
                {
                    mWorldView = g_aLights[i].m_mWorld * *g_LCamera.GetWorldMatrix() * *g_Camera.GetViewMatrix();
                    mWorldViewProjection = mWorldView * *g_Camera.GetProjMatrix();
                    V( g_pEffect->SetMatrix( g_hmWorldView, &mWorldView ) );
                    V( g_pEffect->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
                    V( g_pEffect->SetVector( g_hvAmbient, &g_aLights[i].m_Color ) );

                    // The effect interface queues up the changes and performs them 
                    // with the CommitChanges call. You do not need to call CommitChanges if 
                    // you are not setting any parameters between the BeginPass and EndPass.
                    V( g_pEffect->CommitChanges() );

                    V( pMesh->DrawSubset( m ) );
                }
            }
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );
        V( g_pEffect->SetTechnique( hCurrTech ) ); // Restore the old technique
        V( g_pEffect->SetVector( g_hvAmbient, &g_vAmbient ) );
    }

    // Render the background mesh
    V( pd3dDevice->SetVertexDeclaration( g_pMeshDecl ) );
    mWorldView = g_mWorldBack[g_nCurrBackground] * *g_Camera.GetViewMatrix();
    mWorldViewProjection = g_mWorldBack[g_nCurrBackground] * mViewProj;
    V( g_pEffect->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
    V( g_pEffect->SetMatrix( g_hmWorldView, &mWorldView ) );
    UINT cPasses;
    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( UINT p = 0; p < cPasses; ++p )
    {
        V( g_pEffect->BeginPass( p ) );
        ID3DXMesh* pMesh = g_Background[g_nCurrBackground].GetMesh();
        for( UINT i = 0; i < g_Background[g_nCurrBackground].m_dwNumMaterials; ++i )
        {
            V( g_pEffect->SetVector( g_hvMatColor,
                                     ( D3DXVECTOR4* )&g_Background[g_nCurrBackground].m_pMaterials[i].Diffuse ) );
            if( g_Background[g_nCurrBackground].m_pTextures[i] )
                V( g_pEffect->SetTexture( g_htxScene, g_Background[g_nCurrBackground].m_pTextures[i] ) )
            else
            V( g_pEffect->SetTexture( g_htxScene, g_pDefaultTex ) );
            // The effect interface queues up the changes and performs them 
            // with the CommitChanges call. You do not need to call CommitChanges if 
            // you are not setting any parameters between the BeginPass and EndPass.
            V( g_pEffect->CommitChanges() );
            V( pMesh->DrawSubset( i ) );
        }
        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );

    // Render the mesh
    V( pd3dDevice->SetVertexDeclaration( g_pMeshDecl ) );
    mWorldView = g_mWorldScaling * *g_MCamera.GetWorldMatrix() * *g_Camera.GetViewMatrix();
    mWorldViewProjection = mWorldView * *g_Camera.GetProjMatrix();
    V( g_pEffect->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
    V( g_pEffect->SetMatrix( g_hmWorldView, &mWorldView ) );
    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( UINT p = 0; p < cPasses; ++p )
    {
        V( g_pEffect->BeginPass( p ) );
        ID3DXMesh* pMesh = g_Mesh.GetMesh();
        for( UINT i = 0; i < g_Mesh.m_dwNumMaterials; ++i )
        {
            V( g_pEffect->SetVector( g_hvMatColor, ( D3DXVECTOR4* )&g_Mesh.m_pMaterials[i].Diffuse ) );
            if( g_Mesh.m_pTextures[i] )
                V( g_pEffect->SetTexture( g_htxScene, g_Mesh.m_pTextures[i] ) )
            else
            V( g_pEffect->SetTexture( g_htxScene, g_pDefaultTex ) );
            // The effect interface queues up the changes and performs them 
            // with the CommitChanges call. You do not need to call CommitChanges if 
            // you are not setting any parameters between the BeginPass and EndPass.
            V( g_pEffect->CommitChanges() );
            V( pMesh->DrawSubset( i ) );
        }
        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;

    // Clear the render target and the zbuffer
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 66, 75, 121 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Demonstration Code" );

        // First, render the scene with only ambient lighting
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Scene Ambient" );

            g_pEffect->SetTechnique( g_hRenderSceneAmbient );
            V( g_pEffect->SetVector( g_hvAmbient, &g_vAmbient ) );
            D3D9RenderScene( pd3dDevice, fTime, fElapsedTime, true );
        }

        // Now process the lights.  For each light in the scene,
        // render the shadow volume, then render the scene with
        // stencil enabled.

        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Shadow" );

            for( int L = 0; L < g_nNumLights; ++L )
            {
                // Clear the stencil buffer
                if( g_RenderType != RENDERTYPE_COMPLEXITY )
                    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_ARGB( 0, 170, 170, 170 ), 1.0f, 0 ) );

                D3DXVECTOR4 vLight( g_aLights[L].m_Position.x, g_aLights[L].m_Position.y, g_aLights[L].m_Position.z,
                                    1.0f );
                D3DXVec4Transform( &vLight, &vLight, g_LCamera.GetWorldMatrix() );
                D3DXVec4Transform( &vLight, &vLight, g_Camera.GetViewMatrix() );
                V( g_pEffect->SetVector( g_hvLightView, &vLight ) );
                V( g_pEffect->SetVector( g_hvLightColor, &g_aLights[L].m_Color ) );

                // Render the shadow volume
                switch( g_RenderType )
                {
                    case RENDERTYPE_COMPLEXITY:
                        g_pEffect->SetTechnique( g_hRenderShadowVolumeComplexity );
                        break;
                    case RENDERTYPE_SHADOWVOLUME:
                        g_pEffect->SetTechnique( g_hShowShadow );
                        break;
                    default:
                        g_pEffect->SetTechnique( g_hRenderShadow );
                }

                g_pEffect->SetVector( g_hvShadowColor, &g_vShadowColor[L] );

                // If there was an error generating the shadow volume,
                // skip rendering the shadow mesh.  The scene will show
                // up without shadow.
                if( g_pShadowMesh )
                {
                    V( pd3dDevice->SetVertexDeclaration( g_pShadowDecl ) );
                    UINT cPasses;
                    V( g_pEffect->Begin( &cPasses, 0 ) );
                    for( UINT i = 0; i < cPasses; ++i )
                    {
                        V( g_pEffect->BeginPass( i ) );
                        V( g_pEffect->CommitChanges() );
                        V( g_pShadowMesh->DrawSubset( 0 ) );
                        V( g_pEffect->EndPass() );
                    }
                    V( g_pEffect->End() );
                }

                //
                // Render the scene with stencil and lighting enabled.
                //
                if( g_RenderType != RENDERTYPE_COMPLEXITY )
                {
                    g_pEffect->SetTechnique( g_hRenderScene );
                    D3D9RenderScene( pd3dDevice, fTime, fElapsedTime, false );
                }
            }
        }

        if( g_RenderType == RENDERTYPE_COMPLEXITY )
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Complexity" );

            // Clear the render target
            V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0 ) );

            // Render scene complexity visualization
            const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
            POSTPROCVERT quad[4] =
            {
                { -0.5f,                                                -0.5f, 0.5f, 1.0f },
                { pd3dsdBackBuffer->Width - 0.5f,                         -0.5f, 0.5f, 1.0f },
                { -0.5f,                        pd3dsdBackBuffer->Height - 0.5f, 0.5f, 1.0f },
                { pd3dsdBackBuffer->Width - 0.5f, pd3dsdBackBuffer->Height - 0.5f, 0.5f, 1.0f }
            };

            pd3dDevice->SetVertexDeclaration( g_pPProcDecl );
            g_pEffect->SetTechnique( g_hRenderComplexity );
            UINT cPasses;
            g_pEffect->Begin( &cPasses, 0 );
            for( UINT p = 0; p < cPasses; ++p )
            {
                g_pEffect->BeginPass( p );
                pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, quad, sizeof( POSTPROCVERT ) );
                g_pEffect->EndPass();
            }
            g_pEffect->End();
        }

        DXUT_EndPerfEvent(); // end of draw code

        // Miscellaneous rendering
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

            RenderText();
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );
        }

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_D3DSettingsDlg.OnD3D9LostDevice();
    g_Background[0].InvalidateDeviceObjects();
    g_Background[1].InvalidateDeviceObjects();
    g_LightMesh.InvalidateDeviceObjects();
    g_Mesh.InvalidateDeviceObjects();
    SAFE_RELEASE( g_pShadowMesh );

    if( g_pFont ) g_pFont->OnLostDevice();
    if( g_pEffect ) g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    SAFE_DELETE( g_pTxtHelper );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_D3DSettingsDlg.OnD3D9DestroyDevice();
    g_Background[0].Destroy();
    g_Background[1].Destroy();
    g_LightMesh.Destroy();
    g_Mesh.Destroy();

    SAFE_RELEASE( g_pDefaultTex );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pMeshDecl );
    SAFE_RELEASE( g_pShadowDecl );
    SAFE_RELEASE( g_pPProcDecl );
}

