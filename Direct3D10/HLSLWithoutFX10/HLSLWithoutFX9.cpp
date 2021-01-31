//--------------------------------------------------------------------------------------
// File: HLSLWithoutFX9.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 
#define VERTS_PER_EDGE 64

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern bool                         g_bShowHelp;            // If true, it renders the UI control text
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // manages the 3D UI
extern DWORD                        g_dwNumVertices;
extern DWORD                        g_dwNumIndices;

// Direct3D 9 resources
ID3DXFont*                          g_pFont = NULL;
ID3DXSprite*                        g_pTextSprite = NULL;
LPDIRECT3DVERTEXBUFFER9             g_pVB = NULL;
LPDIRECT3DINDEXBUFFER9              g_pIB = NULL;
LPDIRECT3DVERTEXSHADER9             g_pVertexShader = NULL;
LPD3DXCONSTANTTABLE                 g_pConstantTable = NULL;
LPDIRECT3DVERTEXDECLARATION9        g_pVertexDeclaration = NULL;


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice,
                                            const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                           void* pUserContext );
extern void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime,
                                        void* pUserContext );
extern void CALLBACK OnD3D9LostDevice( void* pUserContext );
extern void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

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
                              L"Arial", &g_pFont ) );

    WCHAR strPath[MAX_PATH];
    LPD3DXBUFFER pCode;
    D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };
    V_RETURN( pd3dDevice->CreateVertexDeclaration( decl, &g_pVertexDeclaration ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"HLSLwithoutFX.vsh" ) );
    DWORD dwShaderFlags = 0;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif
    V_RETURN( D3DXCompileShaderFromFile( strPath, NULL, NULL, "Ripple",
                                             "vs_2_0", dwShaderFlags, &pCode,
                                             NULL, &g_pConstantTable ) );
    hr = pd3dDevice->CreateVertexShader( ( DWORD* )pCode->GetBufferPointer(), &g_pVertexShader );
    pCode->Release();
    if( FAILED( hr ) )
        return DXTRACE_ERR( TEXT( "CreateVertexShader" ), hr );

    g_Camera.SetViewQuat( D3DXQUATERNION( -0.275f, 0.3f, 0.0f, 0.7f ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont, g_pTextSprite, NULL, NULL, 15 );

    pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Create and initialize index buffer
    WORD* pIndices;
    V_RETURN( pd3dDevice->CreateIndexBuffer( g_dwNumIndices * sizeof( WORD ),
                                             0, D3DFMT_INDEX16,
                                             D3DPOOL_DEFAULT, &g_pIB, NULL ) );
    V_RETURN( g_pIB->Lock( 0, 0, ( void** )&pIndices, 0 ) );

    DWORD y;
    for( y = 1; y < VERTS_PER_EDGE; y++ )
    {
        for( DWORD x = 1; x < VERTS_PER_EDGE; x++ )
        {
            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 0 ) );

            *pIndices++ = ( WORD )( ( y - 1 ) * VERTS_PER_EDGE + ( x - 0 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 1 ) );
            *pIndices++ = ( WORD )( ( y - 0 ) * VERTS_PER_EDGE + ( x - 0 ) );
        }
    }
    V_RETURN( g_pIB->Unlock() );

    // Create and initialize vertex buffer
    V_RETURN( pd3dDevice->CreateVertexBuffer( g_dwNumVertices * sizeof( D3DXVECTOR2 ), 0, 0,
                                              D3DPOOL_DEFAULT, &g_pVB, NULL ) );

    D3DXVECTOR2* pVertices;
    V_RETURN( g_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) );
    for( y = 0; y < VERTS_PER_EDGE; y++ )
    {
        for( DWORD x = 0; x < VERTS_PER_EDGE; x++ )
        {
            *pVertices++ = D3DXVECTOR2( ( ( float )x / ( float )( VERTS_PER_EDGE - 1 ) - 0.5f ) * D3DX_PI,
                                        ( ( float )y / ( float )( VERTS_PER_EDGE - 1 ) - 0.5f ) * D3DX_PI );
        }
    }
    V_RETURN( hr = g_pVB->Unlock() );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );
        pd3dDevice->SetVertexShader( g_pVertexShader );
        pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( D3DXVECTOR2 ) );
        pd3dDevice->SetIndices( g_pIB );
        V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, g_dwNumVertices, 0, g_dwNumIndices / 3 ) );

        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
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
    if( g_pFont )
        g_pFont->OnLostDevice();

    SAFE_RELEASE( g_pIB );
    SAFE_RELEASE( g_pVB );
    SAFE_RELEASE( g_pTextSprite );
    SAFE_DELETE( g_pTxtHelper );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pConstantTable );
    SAFE_RELEASE( g_pVertexDeclaration );
}

