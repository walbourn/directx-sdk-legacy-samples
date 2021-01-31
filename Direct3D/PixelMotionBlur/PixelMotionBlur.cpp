//--------------------------------------------------------------------------------------
// File: PixelMotionBlur.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//-----------------------------------------------------------------------------
// Globals variables and definitions
//-----------------------------------------------------------------------------
#define NUM_OBJECTS 40
#define NUM_WALLS 250
#define MOVESTYLE_DEFAULT 0

struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    DWORD clr;
    D3DXVECTOR2 tex1;

    static const DWORD FVF;
};
const DWORD                 SCREEN_VERTEX::FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

#pragma warning( disable : 4324 )
struct OBJECT
{
    D3DXVECTOR3 g_vWorldPos;
    D3DXMATRIXA16 g_mWorld;
    D3DXMATRIXA16 g_mWorldLast;
    LPD3DXMESH g_pMesh;
    LPDIRECT3DTEXTURE9 g_pMeshTexture;
};


struct CRenderTargetSet
{
    IDirect3DSurface9* pRT[2][2];  // Two passes, two RTs
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
D3DFORMAT                   g_VelocityTexFormat;    // Texture format for velocity textures
CFirstPersonCamera          g_Camera;
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

SCREEN_VERTEX g_Vertex[4];

LPD3DXMESH                  g_pMesh1;
LPDIRECT3DTEXTURE9          g_pMeshTexture1;
LPD3DXMESH                  g_pMesh2;
LPDIRECT3DTEXTURE9          g_pMeshTexture2;
LPDIRECT3DTEXTURE9          g_pMeshTexture3;

LPDIRECT3DTEXTURE9          g_pFullScreenRenderTarget;
LPDIRECT3DSURFACE9          g_pFullScreenRenderTargetSurf;

LPDIRECT3DTEXTURE9          g_pPixelVelocityTexture1;
LPDIRECT3DSURFACE9          g_pPixelVelocitySurf1;
LPDIRECT3DTEXTURE9          g_pPixelVelocityTexture2;
LPDIRECT3DSURFACE9          g_pPixelVelocitySurf2;

LPDIRECT3DTEXTURE9          g_pLastFrameVelocityTexture;
LPDIRECT3DSURFACE9          g_pLastFrameVelocitySurf;
LPDIRECT3DTEXTURE9          g_pCurFrameVelocityTexture;
LPDIRECT3DSURFACE9          g_pCurFrameVelocitySurf;

FLOAT                       g_fChangeTime;
bool                        g_bShowBlurFactor;
bool                        g_bShowUnblurred;
DWORD                       g_dwBackgroundColor;

float                       g_fPixelBlurConst;
float                       g_fObjectSpeed;
float                       g_fCameraSpeed;

OBJECT*                     g_pScene1Object[NUM_OBJECTS];
OBJECT*                     g_pScene2Object[NUM_WALLS];
DWORD                       g_dwMoveSytle;
int                         g_nSleepTime;
D3DXMATRIX                  g_mViewProjectionLast;
int                         g_nCurrentScene;

D3DXHANDLE                  g_hWorld;
D3DXHANDLE                  g_hWorldLast;
D3DXHANDLE                  g_hMeshTexture;
D3DXHANDLE                  g_hWorldViewProjection;
D3DXHANDLE                  g_hWorldViewProjectionLast;
D3DXHANDLE                  g_hCurFrameVelocityTexture;
D3DXHANDLE                  g_hLastFrameVelocityTexture;
D3DXHANDLE                  g_hTechWorldWithVelocity;
D3DXHANDLE                  g_hPostProcessMotionBlur;

int                         g_nPasses = 0;          // Number of passes required to render
int                         g_nRtUsed = 0;          // Number of render targets used by each pass
CRenderTargetSet g_aRTSet[2];            // Two sets of render targets
CRenderTargetSet*           g_pCurFrameRTSet;      // Render target set for current frame
CRenderTargetSet*           g_pLastFrameRTSet;     // Render target set for last frame


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_CHANGE_SCENE        5
#define IDC_ENABLE_BLUR         6
#define IDC_FRAMERATE           7
#define IDC_FRAMERATE_STATIC    8
#define IDC_BLUR_FACTOR         9
#define IDC_BLUR_FACTOR_STATIC  10
#define IDC_OBJECT_SPEED        11
#define IDC_OBJECT_SPEED_STATIC 12
#define IDC_CAMERA_SPEED        13
#define IDC_CAMERA_SPEED_STATIC 14


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();
void SetupFullscreenQuad( const D3DSURFACE_DESC* pBackBufferSurfaceDesc );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys
    DXUTCreateWindow( L"PixelMotionBlur" );
    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.
    int iObject;
    for( iObject = 0; iObject < NUM_OBJECTS; iObject++ )
        SAFE_DELETE( g_pScene1Object[iObject] );

    for( iObject = 0; iObject < NUM_WALLS; iObject++ )
        SAFE_DELETE( g_pScene2Object[iObject] );

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_pFullScreenRenderTarget = NULL;
    g_pFullScreenRenderTargetSurf = NULL;
    g_pPixelVelocityTexture1 = NULL;
    g_pPixelVelocitySurf1 = NULL;
    g_pPixelVelocityTexture2 = NULL;
    g_pPixelVelocitySurf2 = NULL;
    g_pLastFrameVelocityTexture = NULL;
    g_pCurFrameVelocityTexture = NULL;
    g_pCurFrameVelocitySurf = NULL;
    g_pLastFrameVelocitySurf = NULL;
    g_pEffect = NULL;
    g_nSleepTime = 0;
    g_fPixelBlurConst = 1.0f;
    g_fObjectSpeed = 8.0f;
    g_fCameraSpeed = 20.0f;
    g_nCurrentScene = 1;

    g_fChangeTime = 0.0f;
    g_dwMoveSytle = MOVESTYLE_DEFAULT;
    D3DXMatrixIdentity( &g_mViewProjectionLast );

    g_hWorld = NULL;
    g_hWorldLast = NULL;
    g_hMeshTexture = NULL;
    g_hWorldViewProjection = NULL;
    g_hWorldViewProjectionLast = NULL;
    g_hCurFrameVelocityTexture = NULL;
    g_hLastFrameVelocityTexture = NULL;
    g_hTechWorldWithVelocity = NULL;
    g_hPostProcessMotionBlur = NULL;

    g_pMesh1 = NULL;
    g_pMeshTexture1 = NULL;
    g_pMesh2 = NULL;
    g_pMeshTexture2 = NULL;
    g_pMeshTexture3 = NULL;
    g_bShowBlurFactor = FALSE;
    g_bShowUnblurred = FALSE;
    g_bShowHelp = TRUE;
    g_dwBackgroundColor = 0x00003F3F;

    int iObject;
    for( iObject = 0; iObject < NUM_OBJECTS; iObject++ )
    {
        g_pScene1Object[iObject] = new OBJECT;
        if( g_pScene1Object[iObject] == NULL )
            return;
        ZeroMemory( g_pScene1Object[iObject], sizeof( OBJECT ) );
    }

    for( iObject = 0; iObject < NUM_WALLS; iObject++ )
    {
        g_pScene2Object[iObject] = new OBJECT;
        if( g_pScene2Object[iObject] == NULL )
            return;
        ZeroMemory( g_pScene2Object[iObject], sizeof( OBJECT ) );
    }

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddButton( IDC_CHANGE_SCENE, L"Change Scene", 35, iY += 24, 125, 22 );
    g_SampleUI.AddCheckBox( IDC_ENABLE_BLUR, L"Enable Blur", 35, iY += 24, 125, 22, true );

    WCHAR sz[100];
    iY += 10;
    swprintf_s( sz, 100, L"Sleep: %dms/frame", g_nSleepTime ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_FRAMERATE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_FRAMERATE, 50, iY += 24, 100, 22, 0, 100, g_nSleepTime );

    iY += 10;
    swprintf_s( sz, 100, L"Blur Factor: %0.2f", g_fPixelBlurConst ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_BLUR_FACTOR_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_BLUR_FACTOR, 50, iY += 24, 100, 22, 1, 200, ( int )( g_fPixelBlurConst * 100.0f ) );

    iY += 10;
    swprintf_s( sz, 100, L"Object Speed: %0.2f", g_fObjectSpeed ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_OBJECT_SPEED_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_OBJECT_SPEED, 50, iY += 24, 100, 22, 0, 30, ( int )g_fObjectSpeed );

    iY += 10;
    swprintf_s( sz, 100, L"Camera Speed: %0.2f", g_fCameraSpeed ); sz[99] = 0;
    g_SampleUI.AddStatic( IDC_CAMERA_SPEED_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_CAMERA_SPEED, 50, iY += 24, 100, 22, 0, 100, ( int )g_fCameraSpeed );
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // No fallback, so need ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // No fallback, so need to support render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
    {
        return false;
    }

    // No fallback, so need to support D3DFMT_G16R16F or D3DFMT_A16B16G16R16F render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_G16R16F ) ) )
    {
        if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                             AdapterFormat, D3DUSAGE_RENDERTARGET,
                                             D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
        {
            return false;
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif
#ifdef DEBUG_PS
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnDestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;


    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    // Query multiple RT setting and set the num of passes required
    D3DCAPS9 Caps;
    pd3dDevice->GetDeviceCaps( &Caps );
    if( Caps.NumSimultaneousRTs < 2 )
    {
        g_nPasses = 2;
        g_nRtUsed = 1;
    }
    else
    {
        g_nPasses = 1;
        g_nRtUsed = 2;
    }

    // Determine which of D3DFMT_G16R16F or D3DFMT_A16B16G16R16F to use for velocity texture
    IDirect3D9* pD3D;
    pd3dDevice->GetDirect3D( &pD3D );
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );

    if( FAILED( pD3D->CheckDeviceFormat( Caps.AdapterOrdinal, Caps.DeviceType,
                                         DisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_G16R16F ) ) )
        g_VelocityTexFormat = D3DFMT_A16B16G16R16F;
    else
        g_VelocityTexFormat = D3DFMT_G16R16F;

    SAFE_RELEASE( pD3D );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    V_RETURN( LoadMesh( pd3dDevice, L"misc\\sphere.x", &g_pMesh1 ) );
    V_RETURN( LoadMesh( pd3dDevice, L"quad.x", &g_pMesh2 ) );

    // Create the mesh texture from a file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"earth\\earth.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pMeshTexture1 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\env2.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pMeshTexture2 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\seafloor.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pMeshTexture3 ) );

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the 
    // shader debugger. Debugging vertex shaders requires either REF or software vertex 
    // processing, and debugging pixel shaders requires REF.  The 
    // D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug experience in the 
    // shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile 
    // against the next higher available software target, which ensures that the 
    // unoptimized shaders do not exceed the shader model limitations.  Setting these 
    // flags will cause slower rendering since the shaders will be unoptimized and 
    // forced into software.  See the DirectX documentation for more information about 
    // using the shader debugger.
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PixelMotionBlur.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    int iObject;
    for( iObject = 0; iObject < NUM_OBJECTS; iObject++ )
    {
        g_pScene1Object[iObject]->g_pMesh = g_pMesh1;
        g_pScene1Object[iObject]->g_pMeshTexture = g_pMeshTexture1;
    }

    for( iObject = 0; iObject < NUM_WALLS; iObject++ )
    {
        g_pScene2Object[iObject]->g_pMesh = g_pMesh2;

        D3DXVECTOR3 vPos;
        D3DXMATRIX mRot;
        D3DXMATRIX mPos;
        D3DXMATRIX mScale;

        if( iObject < NUM_WALLS / 5 * 1 )
        {
            g_pScene2Object[iObject]->g_pMeshTexture = g_pMeshTexture3;

            // Center floor
            vPos.x = 0.0f;
            vPos.y = 0.0f;
            vPos.z = ( iObject - NUM_WALLS / 5 * 0 ) * 1.0f + 10.0f;

            D3DXMatrixRotationX( &mRot, -D3DX_PI / 2.0f );
            D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );
        }
        else if( iObject < NUM_WALLS / 5 * 2 )
        {
            g_pScene2Object[iObject]->g_pMeshTexture = g_pMeshTexture3;

            // Right floor
            vPos.x = 1.0f;
            vPos.y = 0.0f;
            vPos.z = ( iObject - NUM_WALLS / 5 * 1 ) * 1.0f + 10.0f;

            D3DXMatrixRotationX( &mRot, -D3DX_PI / 2.0f );
            D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );
        }
        else if( iObject < NUM_WALLS / 5 * 3 )
        {
            g_pScene2Object[iObject]->g_pMeshTexture = g_pMeshTexture3;

            // Left floor
            vPos.x = -1.0f;
            vPos.y = 0.0f;
            vPos.z = ( iObject - NUM_WALLS / 5 * 2 ) * 1.0f + 10.0f;

            D3DXMatrixRotationX( &mRot, -D3DX_PI / 2.0f );
            D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );
        }
        else if( iObject < NUM_WALLS / 5 * 4 )
        {
            g_pScene2Object[iObject]->g_pMeshTexture = g_pMeshTexture2;

            // Right wall 
            vPos.x = 1.5f;
            vPos.y = 0.5f;
            vPos.z = ( iObject - NUM_WALLS / 5 * 3 ) * 1.0f + 10.0f;

            D3DXMatrixRotationY( &mRot, -D3DX_PI / 2.0f );
            D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );
        }
        else if( iObject < NUM_WALLS / 5 * 5 )
        {
            g_pScene2Object[iObject]->g_pMeshTexture = g_pMeshTexture2;

            // Left wall 
            vPos.x = -1.5f;
            vPos.y = 0.5f;
            vPos.z = ( iObject - NUM_WALLS / 5 * 4 ) * 1.0f + 10.0f;

            D3DXMatrixRotationY( &mRot, D3DX_PI / 2.0f );
            D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );
        }

        // Update the current world matrix for this object
        D3DXMatrixTranslation( &mPos, vPos.x, vPos.y, vPos.z );
        g_pScene2Object[iObject]->g_mWorld = mRot * mPos;
        g_pScene2Object[iObject]->g_mWorld = mScale * g_pScene2Object[iObject]->g_mWorld;

        // The walls don't move so just copy the current world matrix
        g_pScene2Object[iObject]->g_mWorldLast = g_pScene2Object[iObject]->g_mWorld;
    }

    // Setup the camera
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 1.0f, 1000.0f );

    D3DXVECTOR3 vecEye( 40.0f, 0.0f, -15.0f );
    D3DXVECTOR3 vecAt ( 4.0f, 4.0f, -15.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    g_Camera.SetScalers( 0.01f, g_fCameraSpeed );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also optimizes the 
// mesh for the graphics card's vertex cache, which improves performance by organizing 
// the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh )
{
    ID3DXMesh* pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    // Load the mesh with D3DX and get back a ID3DXMesh*.  For this
    // sample we'll ignore the X file's embedded materials since we know 
    // exactly the model we're loading.  See the mesh samples such as
    // "OptimizedMesh" for a more generic mesh loading example.
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );

    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL, NULL, NULL, NULL, &pMesh ) );

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        ID3DXMesh* pTempMesh;
        V( pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                pMesh->GetFVF() | D3DFVF_NORMAL,
                                pd3dDevice, &pTempMesh ) );
        V( D3DXComputeNormals( pTempMesh, NULL ) );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimize the mesh for this graphics card's vertex cache 
    // so when rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader 
    // on those vertices so it will improve perf.     
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// reset, which will happen after a lost device scenario. This is the best location to 
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever 
// the device is lost. Resources created here should be released in the OnLostDevice 
// callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pEffect )
        V_RETURN( g_pEffect->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    // Create a A8R8G8B8 render target texture.  This will be used to render 
    // the full screen and then rendered to the backbuffer using a quad.
    V_RETURN( D3DXCreateTexture( pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                 D3DPOOL_DEFAULT, &g_pFullScreenRenderTarget ) );

    // Create two floating-point render targets with at least 2 channels.  These will be used to store 
    // velocity of each pixel (one for the current frame, and one for last frame).
    V_RETURN( D3DXCreateTexture( pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                 1, D3DUSAGE_RENDERTARGET, g_VelocityTexFormat,
                                 D3DPOOL_DEFAULT, &g_pPixelVelocityTexture1 ) );
    V_RETURN( D3DXCreateTexture( pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                 1, D3DUSAGE_RENDERTARGET, g_VelocityTexFormat,
                                 D3DPOOL_DEFAULT, &g_pPixelVelocityTexture2 ) );

    // Store pointers to surfaces so we can call SetRenderTarget() later
    V_RETURN( g_pFullScreenRenderTarget->GetSurfaceLevel( 0, &g_pFullScreenRenderTargetSurf ) );
    V_RETURN( g_pPixelVelocityTexture1->GetSurfaceLevel( 0, &g_pPixelVelocitySurf1 ) );
    V_RETURN( g_pPixelVelocityTexture2->GetSurfaceLevel( 0, &g_pPixelVelocitySurf2 ) );

    // Setup render target sets
    if( 1 == g_nPasses )
    {
        // Multiple RTs

        // First frame
        g_aRTSet[0].pRT[0][0] = g_pFullScreenRenderTargetSurf;
        g_aRTSet[0].pRT[0][1] = g_pPixelVelocitySurf1;
        g_aRTSet[0].pRT[1][0] = NULL;  // 2nd pass is not needed
        g_aRTSet[0].pRT[1][1] = NULL;  // 2nd pass is not needed

        // Second frame
        g_aRTSet[1].pRT[0][0] = g_pFullScreenRenderTargetSurf;
        g_aRTSet[1].pRT[0][1] = g_pPixelVelocitySurf2;
        g_aRTSet[1].pRT[1][0] = NULL;  // 2nd pass is not needed
        g_aRTSet[1].pRT[1][1] = NULL;  // 2nd pass is not needed
    }
    else
    {
        // Single RT, multiple passes

        // First frame
        g_aRTSet[0].pRT[0][0] = g_pFullScreenRenderTargetSurf;
        g_aRTSet[0].pRT[0][1] = NULL;  // 2nd RT is not needed
        g_aRTSet[0].pRT[1][0] = g_pPixelVelocitySurf1;
        g_aRTSet[0].pRT[1][1] = NULL;  // 2nd RT is not needed

        // Second frame
        g_aRTSet[1].pRT[0][0] = g_pFullScreenRenderTargetSurf;
        g_aRTSet[1].pRT[0][1] = NULL;  // 2nd RT is not needed
        g_aRTSet[1].pRT[1][0] = g_pPixelVelocitySurf2;
        g_aRTSet[1].pRT[1][1] = NULL;  // 2nd RT is not needed
    }

    // Setup the current & last pointers that are swapped every frame.
    g_pCurFrameVelocityTexture = g_pPixelVelocityTexture1;
    g_pLastFrameVelocityTexture = g_pPixelVelocityTexture2;
    g_pCurFrameVelocitySurf = g_pPixelVelocitySurf1;
    g_pLastFrameVelocitySurf = g_pPixelVelocitySurf2;
    g_pCurFrameRTSet = &g_aRTSet[0];
    g_pLastFrameRTSet = &g_aRTSet[1];

    SetupFullscreenQuad( pBackBufferSurfaceDesc );

    D3DXCOLOR colorWhite( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXCOLOR colorBlack( 0.0f, 0.0f, 0.0f, 1.0f );
    D3DXCOLOR colorAmbient( 0.35f, 0.35f, 0.35f, 0 );
    D3DXCOLOR colorSpecular( 0.5f, 0.5f, 0.5f, 1.0f );
    V_RETURN( g_pEffect->SetVector( "MaterialAmbientColor", ( D3DXVECTOR4* )&colorAmbient ) );
    V_RETURN( g_pEffect->SetVector( "MaterialDiffuseColor", ( D3DXVECTOR4* )&colorWhite ) );
    V_RETURN( g_pEffect->SetTexture( "RenderTargetTexture", g_pFullScreenRenderTarget ) );

    D3DSURFACE_DESC desc;
    V_RETURN( g_pFullScreenRenderTargetSurf->GetDesc( &desc ) );
    V_RETURN( g_pEffect->SetFloat( "RenderTargetWidth", ( FLOAT )desc.Width ) );
    V_RETURN( g_pEffect->SetFloat( "RenderTargetHeight", ( FLOAT )desc.Height ) );

    // 12 is the number of samples in our post-process pass, so we don't want 
    // pixel velocity of more than 12 pixels or else we'll see artifacts
    float fVelocityCapInPixels = 3.0f;
    float fVelocityCapNonHomogeneous = fVelocityCapInPixels * 2 / pBackBufferSurfaceDesc->Width;
    float fVelocityCapSqNonHomogeneous = fVelocityCapNonHomogeneous * fVelocityCapNonHomogeneous;

    V_RETURN( g_pEffect->SetFloat( "VelocityCapSq", fVelocityCapSqNonHomogeneous ) );
    V_RETURN( g_pEffect->SetFloat( "ConvertToNonHomogeneous", 1.0f / pBackBufferSurfaceDesc->Width ) );

    // Determine the technique to use when rendering world based on # of passes.
    if( 1 == g_nPasses )
        g_hTechWorldWithVelocity = g_pEffect->GetTechniqueByName( "WorldWithVelocityMRT" );
    else
        g_hTechWorldWithVelocity = g_pEffect->GetTechniqueByName( "WorldWithVelocityTwoPasses" );

    g_hPostProcessMotionBlur = g_pEffect->GetTechniqueByName( "PostProcessMotionBlur" );
    g_hWorld = g_pEffect->GetParameterByName( NULL, "mWorld" );
    g_hWorldLast = g_pEffect->GetParameterByName( NULL, "mWorldLast" );
    g_hWorldViewProjection = g_pEffect->GetParameterByName( NULL, "mWorldViewProjection" );
    g_hWorldViewProjectionLast = g_pEffect->GetParameterByName( NULL, "mWorldViewProjectionLast" );
    g_hMeshTexture = g_pEffect->GetParameterByName( NULL, "MeshTexture" );
    g_hCurFrameVelocityTexture = g_pEffect->GetParameterByName( NULL, "CurFrameVelocityTexture" );
    g_hLastFrameVelocityTexture = g_pEffect->GetParameterByName( NULL, "LastFrameVelocityTexture" );

    // Turn off lighting since its done in the shaders 
    V_RETURN( pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE ) );

    // Save a pointer to the orignal render target to restore it later
    LPDIRECT3DSURFACE9 pOriginalRenderTarget;
    V_RETURN( pd3dDevice->GetRenderTarget( 0, &pOriginalRenderTarget ) );

    // Clear each RT
    V_RETURN( pd3dDevice->SetRenderTarget( 0, g_pFullScreenRenderTargetSurf ) );
    V_RETURN( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 ) );
    V_RETURN( pd3dDevice->SetRenderTarget( 0, g_pLastFrameVelocitySurf ) );
    V_RETURN( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 ) );
    V_RETURN( pd3dDevice->SetRenderTarget( 0, g_pCurFrameVelocitySurf ) );
    V_RETURN( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 ) );

    // Restore the orignal RT
    V_RETURN( pd3dDevice->SetRenderTarget( 0, pOriginalRenderTarget ) );
    SAFE_RELEASE( pOriginalRenderTarget );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: SetupFullscreenQuad()
// Desc: Sets up a full screen quad.  First we render to a fullscreen render 
//       target texture, and then we render that texture using this quad to 
//       apply a pixel shader on every pixel of the scene.
//-----------------------------------------------------------------------------
void SetupFullscreenQuad( const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    D3DSURFACE_DESC desc;

    g_pFullScreenRenderTargetSurf->GetDesc( &desc );

    // Ensure that we're directly mapping texels to pixels by offset by 0.5
    // For more info see the doc page titled "Directly Mapping Texels to Pixels"
    FLOAT fWidth5 = ( FLOAT )pBackBufferSurfaceDesc->Width - 0.5f;
    FLOAT fHeight5 = ( FLOAT )pBackBufferSurfaceDesc->Height - 0.5f;

    FLOAT fTexWidth1 = ( FLOAT )pBackBufferSurfaceDesc->Width / ( FLOAT )desc.Width;
    FLOAT fTexHeight1 = ( FLOAT )pBackBufferSurfaceDesc->Height / ( FLOAT )desc.Height;

    // Fill in the vertex values
    g_Vertex[0].pos = D3DXVECTOR4( fWidth5, -0.5f, 0.0f, 1.0f );
    g_Vertex[0].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[0].tex1 = D3DXVECTOR2( fTexWidth1, 0.0f );

    g_Vertex[1].pos = D3DXVECTOR4( fWidth5, fHeight5, 0.0f, 1.0f );
    g_Vertex[1].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[1].tex1 = D3DXVECTOR2( fTexWidth1, fTexHeight1 );

    g_Vertex[2].pos = D3DXVECTOR4( -0.5f, -0.5f, 0.0f, 1.0f );
    g_Vertex[2].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[2].tex1 = D3DXVECTOR2( 0.0f, 0.0f );

    g_Vertex[3].pos = D3DXVECTOR4( -0.5f, fHeight5, 0.0f, 1.0f );
    g_Vertex[3].clr = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 0.66666f );
    g_Vertex[3].tex1 = D3DXVECTOR2( 0.0f, fTexHeight1 );
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double dTime, float fElapsedTime, void* pUserContext )
{
    float fTime = ( float )dTime;

    // Update the camera's postion based on user input 
    g_Camera.FrameMove( fElapsedTime );

    if( g_nCurrentScene == 1 )
    {
        // Move the objects around based on some simple function
        int iObject;
        for( iObject = 0; iObject < NUM_OBJECTS; iObject++ )
        {
            D3DXVECTOR3 vPos;

            float fRadius = 7.0f;
            if( iObject >= 30 && iObject < 41 )
            {
                vPos.x = cosf( g_fObjectSpeed * 0.125f * fTime + 2 * D3DX_PI / 10 * ( iObject - 30 ) ) * fRadius;
                vPos.y = 10.0f;
                vPos.z = sinf( g_fObjectSpeed * 0.125f * fTime + 2 * D3DX_PI / 10 * ( iObject - 30 ) ) * fRadius -
                    25.0f;
            }
            else if( iObject >= 20 && iObject < 31 )
            {
                vPos.x = cosf( g_fObjectSpeed * 0.25f * fTime + 2 * D3DX_PI / 10 * ( iObject - 20 ) ) * fRadius;
                vPos.y = 10.0f;
                vPos.z = sinf( g_fObjectSpeed * 0.25f * fTime + 2 * D3DX_PI / 10 * ( iObject - 20 ) ) * fRadius - 5.0f;
            }
            else if( iObject >= 10 && iObject < 21 )
            {
                vPos.x = cosf( g_fObjectSpeed * 0.5f * fTime + 2 * D3DX_PI / 10 * ( iObject - 10 ) ) * fRadius;
                vPos.y = 0.0f;
                vPos.z = sinf( g_fObjectSpeed * 0.5f * fTime + 2 * D3DX_PI / 10 * ( iObject - 10 ) ) * fRadius - 25.0f;
            }
            else
            {
                vPos.x = cosf( g_fObjectSpeed * fTime + 2 * D3DX_PI / 10 * iObject ) * fRadius;
                vPos.y = 0.0f;
                vPos.z = sinf( g_fObjectSpeed * fTime + 2 * D3DX_PI / 10 * iObject ) * fRadius - 5.0f;
            }

            g_pScene1Object[iObject]->g_vWorldPos = vPos;

            // Store the last world matrix so that we can tell the effect file
            // what it was when we render this object
            g_pScene1Object[iObject]->g_mWorldLast = g_pScene1Object[iObject]->g_mWorld;

            // Update the current world matrix for this object
            D3DXMatrixTranslation( &g_pScene1Object[iObject]->g_mWorld, g_pScene1Object[iObject]->g_vWorldPos.x,
                                   g_pScene1Object[iObject]->g_vWorldPos.y, g_pScene1Object[iObject]->g_vWorldPos.z );
        }
    }

    if( g_nSleepTime > 0 )
        Sleep( g_nSleepTime );
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
    LPDIRECT3DSURFACE9 apOriginalRenderTarget[2] = { NULL, NULL };
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mViewProjection;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mWorldViewProjectionLast;
    D3DXMATRIXA16 mWorld;
    D3DXVECTOR4 vEyePt;
    UINT iPass, cPasses;

    // Swap the current frame's per-pixel velocity texture with  
    // last frame's per-pixel velocity texture
    LPDIRECT3DTEXTURE9 pTempTex = g_pCurFrameVelocityTexture;
    g_pCurFrameVelocityTexture = g_pLastFrameVelocityTexture;
    g_pLastFrameVelocityTexture = pTempTex;

    LPDIRECT3DSURFACE9 pTempSurf = g_pCurFrameVelocitySurf;
    g_pCurFrameVelocitySurf = g_pLastFrameVelocitySurf;
    g_pLastFrameVelocitySurf = pTempSurf;

    CRenderTargetSet* pTempRTSet = g_pCurFrameRTSet;
    g_pCurFrameRTSet = g_pLastFrameRTSet;
    g_pLastFrameRTSet = pTempRTSet;

    // Save a pointer to the current render target in the swap chain
    V( pd3dDevice->GetRenderTarget( 0, &apOriginalRenderTarget[0] ) );

    V( g_pEffect->SetFloat( "PixelBlurConst", g_fPixelBlurConst ) );

    V( g_pEffect->SetTexture( g_hCurFrameVelocityTexture, g_pCurFrameVelocityTexture ) );
    V( g_pEffect->SetTexture( g_hLastFrameVelocityTexture, g_pLastFrameVelocityTexture ) );

    // Clear the velocity render target to 0
    V( pd3dDevice->SetRenderTarget( 0, g_pCurFrameVelocitySurf ) );
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 ) );

    // Clear the full screen render target to the background color
    V( pd3dDevice->SetRenderTarget( 0, g_pFullScreenRenderTargetSurf ) );
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, g_dwBackgroundColor, 1.0f, 0 ) );

    // Turn on Z for this pass
    V( pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE ) );

    // For the first pass we'll draw the screen to the full screen render target
    // and to update the velocity render target with the velocity of each pixel
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Set world drawing technique
        V( g_pEffect->SetTechnique( g_hTechWorldWithVelocity ) );

        // Get the projection & view matrix from the camera class
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        mViewProjection = mView * mProj;

        if( g_nCurrentScene == 1 )
        {
            // For each object, tell the effect about the object's current world matrix
            // and its last frame's world matrix and render the object. 
            // The vertex shader can then use both these matricies to calculate how 
            // much each vertex has moved.  The pixel shader then interpolates this 
            // vertex velocity for each pixel
            for( int iObject = 0; iObject < NUM_OBJECTS; iObject++ )
            {
                mWorldViewProjection = g_pScene1Object[iObject]->g_mWorld * mViewProjection;
                mWorldViewProjectionLast = g_pScene1Object[iObject]->g_mWorldLast * g_mViewProjectionLast;

                // Tell the effect where the camera is now
                V( g_pEffect->SetMatrix( g_hWorldViewProjection, &mWorldViewProjection ) );
                V( g_pEffect->SetMatrix( g_hWorld, &g_pScene1Object[iObject]->g_mWorld ) );

                // Tell the effect where the camera was last frame
                V( g_pEffect->SetMatrix( g_hWorldViewProjectionLast, &mWorldViewProjectionLast ) );

                // Tell the effect the current mesh's texture
                V( g_pEffect->SetTexture( g_hMeshTexture, g_pScene1Object[iObject]->g_pMeshTexture ) );

                V( g_pEffect->Begin( &cPasses, 0 ) );
                for( iPass = 0; iPass < cPasses; iPass++ )
                {
                    // Set the render targets here.  If multiple render targets are
                    // supported, render target 1 is set to be the velocity surface.
                    // If multiple render targets are not supported, the velocity
                    // surface will be rendered in the 2nd pass.
                    for( int rt = 0; rt < g_nRtUsed; ++rt )
                        V( pd3dDevice->SetRenderTarget( rt, g_pCurFrameRTSet->pRT[iPass][rt] ) );

                    V( g_pEffect->BeginPass( iPass ) );
                    V( g_pScene1Object[iObject]->g_pMesh->DrawSubset( 0 ) );
                    V( g_pEffect->EndPass() );
                }
                V( g_pEffect->End() );
            }
        }
        else if( g_nCurrentScene == 2 )
        {
            for( int iObject = 0; iObject < NUM_WALLS; iObject++ )
            {
                mWorldViewProjection = g_pScene2Object[iObject]->g_mWorld * mViewProjection;
                mWorldViewProjectionLast = g_pScene2Object[iObject]->g_mWorldLast * g_mViewProjectionLast;

                // Tell the effect where the camera is now
                V( g_pEffect->SetMatrix( g_hWorldViewProjection, &mWorldViewProjection ) );
                V( g_pEffect->SetMatrix( g_hWorld, &g_pScene2Object[iObject]->g_mWorld ) );

                // Tell the effect where the camera was last frame
                V( g_pEffect->SetMatrix( g_hWorldViewProjectionLast, &mWorldViewProjectionLast ) );

                // Tell the effect the current mesh's texture
                V( g_pEffect->SetTexture( g_hMeshTexture, g_pScene2Object[iObject]->g_pMeshTexture ) );

                V( g_pEffect->Begin( &cPasses, 0 ) );
                for( iPass = 0; iPass < cPasses; iPass++ )
                {
                    // Set the render targets here.  If multiple render targets are
                    // supported, render target 1 is set to be the velocity surface.
                    // If multiple render targets are not supported, the velocity
                    // surface will be rendered in the 2nd pass.
                    for( int rt = 0; rt < g_nRtUsed; ++rt )
                        V( pd3dDevice->SetRenderTarget( rt, g_pCurFrameRTSet->pRT[iPass][rt] ) );

                    V( g_pEffect->BeginPass( iPass ) );
                    V( g_pScene2Object[iObject]->g_pMesh->DrawSubset( 0 ) );
                    V( g_pEffect->EndPass() );
                }
                V( g_pEffect->End() );
            }
        }

        V( pd3dDevice->EndScene() );
    }

    // Remember the current view projection matrix for next frame
    g_mViewProjectionLast = mViewProjection;

    // Now that we have the scene rendered into g_pFullScreenRenderTargetSurf
    // and the pixel velocity rendered into g_pCurFrameVelocitySurf 
    // we can do a final pass to render this into the backbuffer and use
    // a pixel shader to blur the pixels based on the last frame's & current frame's 
    // per pixel velocity to achieve a motion blur effect
    for( int rt = 0; rt < g_nRtUsed; ++rt )
        V( pd3dDevice->SetRenderTarget( rt, apOriginalRenderTarget[rt] ) );
    SAFE_RELEASE( apOriginalRenderTarget[0] );
    V( pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ) );

    // Clear the render target
    V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L ) );

    // Above we rendered to a fullscreen render target texture, and now we 
    // render that texture using a quad to apply a pixel shader on every pixel of the scene.
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        V( g_pEffect->SetTechnique( g_hPostProcessMotionBlur ) );

        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( iPass = 0; iPass < cPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( pd3dDevice->SetFVF( SCREEN_VERTEX::FVF ) );
            V( pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, g_Vertex, sizeof( SCREEN_VERTEX ) ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );

        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );
        RenderText();

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( g_pSprite, strMsg, -1, &rc, DT_NOCLIP, g_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.DrawFormattedTextLine( L"Blur=%0.2f Object Speed=%0.1f Camera Speed=%0.1f", g_fPixelBlurConst,
                                     g_fObjectSpeed, g_fCameraSpeed );

    if( g_nSleepTime == 0 )
        txtHelper.DrawTextLine( L"Not sleeping between frames" );
    else
        txtHelper.DrawFormattedTextLine( L"Sleeping %dms per frame", g_nSleepTime );

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Look: Left drag mouse\n"
                                L"Move: A,W,S,D or Arrow Keys\n"
                                L"Move up/down: Q,E or PgUp,PgDn\n"
                                L"Reset camera: Home\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke 
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;

        case IDC_CHANGE_SCENE:
        {
            g_nCurrentScene %= 2;
            g_nCurrentScene++;

            switch( g_nCurrentScene )
            {
                case 1:
                {
                    D3DXVECTOR3 vecEye( 40.0f, 0.0f, -15.0f );
                    D3DXVECTOR3 vecAt ( 4.0f, 4.0f, -15.0f );
                    g_Camera.SetViewParams( &vecEye, &vecAt );
                    break;
                }

                case 2:
                {
                    D3DXVECTOR3 vecEye( 0.125f, 1.25f, 3.0f );
                    D3DXVECTOR3 vecAt ( 0.125f, 1.25f, 4.0f );
                    g_Camera.SetViewParams( &vecEye, &vecAt );
                    break;
                }
            }
            break;
        }

        case IDC_ENABLE_BLUR:
        {
            static float s_fRemember = 1.0f;
            if( g_SampleUI.GetCheckBox( IDC_ENABLE_BLUR )->GetChecked() )
            {
                g_fPixelBlurConst = s_fRemember;
                g_SampleUI.GetStatic( IDC_BLUR_FACTOR_STATIC )->SetEnabled( true );
                g_SampleUI.GetSlider( IDC_BLUR_FACTOR )->SetEnabled( true );
            }
            else
            {
                s_fRemember = g_fPixelBlurConst;
                g_fPixelBlurConst = 0.0f;
                g_SampleUI.GetStatic( IDC_BLUR_FACTOR_STATIC )->SetEnabled( false );
                g_SampleUI.GetSlider( IDC_BLUR_FACTOR )->SetEnabled( false );
            }
            break;
        }

        case IDC_FRAMERATE:
        {
            WCHAR sz[100];
            g_nSleepTime = g_SampleUI.GetSlider( IDC_FRAMERATE )->GetValue();
            swprintf_s( sz, 100, L"Sleep: %dms/frame", g_nSleepTime ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_FRAMERATE_STATIC )->SetText( sz );
            break;
        }

        case IDC_BLUR_FACTOR:
        {
            WCHAR sz[100];
            g_fPixelBlurConst = ( float )g_SampleUI.GetSlider( IDC_BLUR_FACTOR )->GetValue() / 100.0f;
            swprintf_s( sz, 100, L"Blur Factor: %0.2f", g_fPixelBlurConst ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_BLUR_FACTOR_STATIC )->SetText( sz );
            break;
        }

        case IDC_OBJECT_SPEED:
        {
            WCHAR sz[100];
            g_fObjectSpeed = ( float )g_SampleUI.GetSlider( IDC_OBJECT_SPEED )->GetValue();
            swprintf_s( sz, 100, L"Object Speed: %0.2f", g_fObjectSpeed ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_OBJECT_SPEED_STATIC )->SetText( sz );
            break;
        }

        case IDC_CAMERA_SPEED:
        {
            WCHAR sz[100];
            g_fCameraSpeed = ( float )g_SampleUI.GetSlider( IDC_CAMERA_SPEED )->GetValue();
            swprintf_s( sz, 100, L"Camera Speed: %0.2f", g_fCameraSpeed ); sz[99] = 0;
            g_SampleUI.GetStatic( IDC_CAMERA_SPEED_STATIC )->SetText( sz );
            g_Camera.SetScalers( 0.01f, g_fCameraSpeed );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all 
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for 
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    SAFE_RELEASE( g_pFullScreenRenderTargetSurf );
    SAFE_RELEASE( g_pFullScreenRenderTarget );
    SAFE_RELEASE( g_pPixelVelocitySurf1 );
    SAFE_RELEASE( g_pPixelVelocityTexture1 );
    SAFE_RELEASE( g_pPixelVelocitySurf2 );
    SAFE_RELEASE( g_pPixelVelocityTexture2 );
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );

    SAFE_RELEASE( g_pFullScreenRenderTargetSurf );
    SAFE_RELEASE( g_pFullScreenRenderTarget );

    SAFE_RELEASE( g_pMesh1 );
    SAFE_RELEASE( g_pMeshTexture1 );
    SAFE_RELEASE( g_pMesh2 );
    SAFE_RELEASE( g_pMeshTexture2 );
    SAFE_RELEASE( g_pMeshTexture3 );
}



