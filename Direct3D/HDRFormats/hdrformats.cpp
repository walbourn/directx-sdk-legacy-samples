//-----------------------------------------------------------------------------
// File: HDRFormats.cpp
//
// Desc: This sample shows how to do a single pass motion blur effect using 
//       floating point textures and multiple render targets.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include <stdio.h>
#include <math.h>
#include "skybox.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


#define NUM_TONEMAP_TEXTURES  5       // Number of stages in the 3x3 down-scaling 
// of average luminance textures
#define NUM_BLOOM_TEXTURES    2
#define RGB16_MAX             100


enum ENCODING_MODE
{
    FP16,
    FP32,
    RGB16,
    RGBE8,
    NUM_ENCODING_MODES
};
enum RENDER_MODE
{
    DECODED,
    RGB_ENCODED,
    ALPHA_ENCODED,
    NUM_RENDER_MODES
};

struct TECH_HANDLES
{
    D3DXHANDLE Scene;
    D3DXHANDLE DownScale2x2_Lum;
    D3DXHANDLE DownScale3x3;
    D3DXHANDLE DownScale3x3_BrightPass;
    D3DXHANDLE FinalPass;
};

struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;

    static const DWORD FVF;
};
const DWORD                 SCREEN_VERTEX::FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;


//-----------------------------------------------------------------------------
// Globals variables and definitions
//-----------------------------------------------------------------------------
IDirect3DDevice9*           g_pd3dDevice;         // Direct3D device
ID3DXFont*                  g_pFont;              // Font for drawing text
CModelViewerCamera          g_Camera;
LPD3DXEFFECT                g_pEffect;
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;             // manages the 3D UI
CDXUTDialog                 g_SampleUI;        // Sample specific controls
CSkybox g_aSkybox[NUM_ENCODING_MODES];
PDIRECT3DSURFACE9           g_pMSRT = NULL;       // Multi-Sample float render target
PDIRECT3DSURFACE9           g_pMSDS = NULL;       // Depth Stencil surface for the float RT
LPDIRECT3DTEXTURE9          g_pTexRender;         // Render target texture
LPDIRECT3DTEXTURE9          g_pTexBrightPass;     // Bright pass filter
LPD3DXMESH                  g_pMesh;
LPDIRECT3DTEXTURE9 g_apTexToneMap[NUM_TONEMAP_TEXTURES]; // Tone mapping calculation textures
LPDIRECT3DTEXTURE9 g_apTexBloom[NUM_BLOOM_TEXTURES];     // Blooming effect intermediate texture
bool                        g_bBloom;             // Bloom effect on/off
ENCODING_MODE               g_eEncodingMode;
RENDER_MODE                 g_eRenderMode;
TECH_HANDLES g_aTechHandles[NUM_ENCODING_MODES];
TECH_HANDLES*               g_pCurTechnique;
bool                        g_bShowHelp;
bool                        g_bShowText;
double g_aPowsOfTwo[257]; // Lookup table for log calculations
bool                        g_bSupportsR16F = false;
bool                        g_bSupportsR32F = false;
bool                        g_bSupportsD16 = false;
bool                        g_bSupportsD32 = false;
bool                        g_bSupportsD24X8 = false;
bool                        g_bUseMultiSample = false; // True when using multisampling on a supported back buffer
D3DMULTISAMPLE_TYPE         g_MaxMultiSampleType = D3DMULTISAMPLE_NONE;  // Non-Zero when g_bUseMultiSample is true
DWORD                       g_dwMultiSampleQuality = 0; // Used when we have multisampling on a backbuffer



//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC           -1
#define IDC_TOGGLEFULLSCREEN 1
#define IDC_TOGGLEREF        3
#define IDC_CHANGEDEVICE     4
#define IDC_ENCODING_MODE    5
#define IDC_RENDER_MODE      6
#define IDC_BLOOM            7


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void RenderText( double fTime );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void AppInit();
void DrawFullScreenQuad( float fLeftU=0.0f, float fTopV=0.0f, float fRightU=1.0f, float fBottomV=1.0f );
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh );

HRESULT RenderModel();

HRESULT RetrieveTechHandles();
HRESULT GetSampleOffsets_DownScale3x3( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] );
HRESULT GetSampleOffsets_DownScale2x2_Lum( DWORD dwSrcWidth, DWORD dwSrcHeight, DWORD dwDestWidth, DWORD dwDestHeight,
                                           D3DXVECTOR2 avSampleOffsets[] );
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize, float afTexCoordOffset[15], D3DXVECTOR4* avColorWeight,
                                float fDeviation, float fMultiplier=1.0f );
HRESULT RenderBloom();
HRESULT MeasureLuminance();
HRESULT BrightPassFilter();

HRESULT CreateEncodedTexture( IDirect3DCubeTexture9* pTexSrc, IDirect3DCubeTexture9** ppTexDest,
                              ENCODING_MODE eTarget );


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

    // Initialize the application
    AppInit();

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

    DXUTCreateWindow( L"HDRFormats" );


    // Although multisampling is supported for render target surfaces, those surfaces
    // exist without a parent texture, and must therefore be copied to a texture 
    // surface if they're to be used as a source texture. This sample relies upon 
    // several render targets being used as source textures, and therefore it makes
    // sense to disable multisampling altogether.
    CGrowableArray <D3DMULTISAMPLE_TYPE>* pMultiSampleTypeList =
        DXUTGetD3D9Enumeration()->GetPossibleMultisampleTypeList();
    pMultiSampleTypeList->RemoveAll();
    pMultiSampleTypeList->Add( D3DMULTISAMPLE_NONE );
    DXUTGetD3D9Enumeration()->SetMultisampleQualityMax( 0 );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTCreateDevice( true, 640, 480 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

    return DXUTGetExitCode();
}


//-----------------------------------------------------------------------------
// Name: AppInit()
// Desc: Sets attributes for the app.
//-----------------------------------------------------------------------------
void AppInit()
{
    g_pFont = NULL;
    g_pEffect = NULL;
    g_bShowHelp = false;
    g_bShowText = true;

    g_pMesh = NULL;
    g_pTexRender = NULL;

    g_bBloom = TRUE;
    g_eEncodingMode = RGBE8;
    g_eRenderMode = DECODED;

    g_pCurTechnique = &g_aTechHandles[ g_eEncodingMode ];

    for( int i = 0; i <= 256; i++ )
    {
        g_aPowsOfTwo[i] = powf( 2.0f, ( float )( i - 128 ) );
    }

    ZeroMemory( g_apTexToneMap, sizeof( g_apTexToneMap ) );
    ZeroMemory( g_apTexBloom, sizeof( g_apTexBloom ) );
    ZeroMemory( g_aTechHandles, sizeof( g_aTechHandles ) );

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetFont( 0, L"Arial", 14, 400 );
    g_HUD.SetCallback( OnGUIEvent );

    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent );

    // Title font for comboboxes
    g_SampleUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    CDXUTElement* pElement = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    CDXUTComboBox* pComboBox = NULL;


    const WCHAR* RENDER_MODE_NAMES[] =
    {
        L"Decoded scene",
        L"RGB channels",
        L"Alpha channel",
    };

    g_SampleUI.AddStatic( IDC_STATIC, L"(E)ncoding mode", 0, 0, 105, 25 );
    g_SampleUI.AddComboBox( IDC_ENCODING_MODE, 0, 25, 140, 24, 'E', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 50 );

    g_SampleUI.AddStatic( IDC_STATIC, L"(R)ender mode", 0, 45, 105, 25 );
    g_SampleUI.AddComboBox( IDC_RENDER_MODE, 0, 70, 140, 24, 'R', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 30 );

    for( int i = 0; i < NUM_RENDER_MODES; i++ )
    {
        pComboBox->AddItem( RENDER_MODE_NAMES[i], IntToPtr( i ) );
    }

    g_SampleUI.AddCheckBox( IDC_BLOOM, L"Show (B)loom", 0, 110, 140, 18, g_bBloom, 'B' );
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning E_FAIL.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    UNREFERENCED_PARAMETER( bWindowed );

    // No fallback defined by this app, so reject any device that 
    // doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

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

    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;
    HRESULT hr;
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
    HRESULT hr = S_OK;
    WCHAR str[MAX_PATH] = {0};

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    g_pd3dDevice = pd3dDevice;

    // Initialize the font
    HDC hDC = GetDC( NULL );
    int nHeight = -MulDiv( 9, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );
    ReleaseDC( NULL, hDC );
    if( FAILED( hr = D3DXCreateFont( pd3dDevice, nHeight, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     TEXT( "Arial" ), &g_pFont ) ) )
        return hr;

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

    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDRFormats.fx" );
    if( FAILED( hr ) )
        return hr;

    hr = D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, NULL );

    // If this fails, there should be debug output
    if( FAILED( hr ) )
        return hr;

    RetrieveTechHandles();

    // Determine which encoding modes this device can support
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    DXUTDeviceSettings settings = DXUTGetDeviceSettings();

    bool bCreatedTexture = false;
    IDirect3DCubeTexture9* pCubeTexture = NULL;
    IDirect3DCubeTexture9* pEncodedTexture = NULL;


    WCHAR strPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"Light Probes\\uffizi_cross.dds" ) );

    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->RemoveAllItems();


    g_bSupportsR16F = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                            D3DRTYPE_TEXTURE, D3DFMT_R16F ) ) )
        g_bSupportsR16F = true;

    g_bSupportsR32F = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                            D3DRTYPE_TEXTURE, D3DFMT_R32F ) ) )
        g_bSupportsR32F = true;

    bool bSupports128FCube = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, 0,
                                            D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
        bSupports128FCube = true;

    // FP16
    if( g_bSupportsR16F && bSupports128FCube )
    {
        // Device supports floating-point textures. 
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );

        V_RETURN( g_aSkybox[FP16].OnCreateDevice( pd3dDevice, 50, pCubeTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"FP16", ( void* )FP16 );
    }

    // FP32
    if( g_bSupportsR32F && bSupports128FCube )
    {
        // Device supports floating-point textures. 
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_MANAGED, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );

        V_RETURN( g_aSkybox[FP32].OnCreateDevice( pd3dDevice, 50, pCubeTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"FP32", ( void* )FP32 );
    }

    if( ( !g_bSupportsR32F && !g_bSupportsR16F ) || !bSupports128FCube )
    {
        // Device doesn't support floating-point textures. Use the scratch pool to load it temporarily
        // in order to create encoded textures from it.
        bCreatedTexture = true;
        V_RETURN( D3DXCreateCubeTextureFromFileEx( pd3dDevice, strPath, D3DX_DEFAULT, 1, 0, D3DFMT_A16B16G16R16F,
                                                   D3DPOOL_SCRATCH, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
                                                   NULL, &pCubeTexture ) );
    }

    // RGB16
    if( D3D_OK == pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal,
                                           settings.d3d9.DeviceType,
                                           settings.d3d9.AdapterFormat, 0,
                                           D3DRTYPE_CUBETEXTURE,
                                           D3DFMT_A16B16G16R16 ) )
    {
        V_RETURN( CreateEncodedTexture( pCubeTexture, &pEncodedTexture, RGB16 ) );
        V_RETURN( g_aSkybox[RGB16].OnCreateDevice( pd3dDevice, 50, pEncodedTexture, L"skybox.fx" ) );

        g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"RGB16", ( void* )RGB16 );
    }


    // RGBE8
    V_RETURN( CreateEncodedTexture( pCubeTexture, &pEncodedTexture, RGBE8 ) );
    V_RETURN( g_aSkybox[RGBE8].OnCreateDevice( pd3dDevice, 50, pEncodedTexture, L"skybox.fx" ) );

    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( L"RGBE8", ( void* )RGBE8 );
    g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->SetSelectedByText( L"RGBE8" );

    if( bCreatedTexture )
        SAFE_RELEASE( pCubeTexture );

    hr = LoadMesh( L"misc\\teapot.x", &g_pMesh );
    if( FAILED( hr ) )
        return hr;

    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

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
    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( pd3dDevice == NULL )
        return S_OK;

    int i;
    for( i = 0; i < NUM_ENCODING_MODES; i++ )
    {
        g_aSkybox[i].OnResetDevice( pBackBufferSurfaceDesc );
    }

    if( g_pFont )
        g_pFont->OnResetDevice();

    if( g_pEffect )
        g_pEffect->OnResetDevice();

    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    switch( g_eEncodingMode )
    {
        case FP16:
            fmt = D3DFMT_A16B16G16R16F; break;
        case FP32:
            fmt = D3DFMT_A16B16G16R16F; break;
        case RGBE8:
            fmt = D3DFMT_A8R8G8B8; break;
        case RGB16:
            fmt = D3DFMT_A16B16G16R16; break;
    }

    hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height,
                                    1, D3DUSAGE_RENDERTARGET, fmt,
                                    D3DPOOL_DEFAULT, &g_pTexRender, NULL );
    if( FAILED( hr ) )
        return hr;

    hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width / 8, pBackBufferSurfaceDesc->Height / 8,
                                    1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                    D3DPOOL_DEFAULT, &g_pTexBrightPass, NULL );
    if( FAILED( hr ) )
        return hr;

    // Determine whether we can and should support a multisampling on the HDR render target
    g_bUseMultiSample = false;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( !pD3D )
        return E_FAIL;

    DXUTDeviceSettings settings = DXUTGetDeviceSettings();

    g_bSupportsD16 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D16 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D16 ) ) )
        {
            g_bSupportsD16 = true;
        }
    }
    g_bSupportsD32 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D32 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D32 ) ) )
        {
            g_bSupportsD32 = true;
        }
    }
    g_bSupportsD24X8 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, fmt,
                                                     D3DFMT_D24X8 ) ) )
        {
            g_bSupportsD24X8 = true;
        }
    }

    D3DFORMAT dfmt = D3DFMT_UNKNOWN;
    if( g_bSupportsD16 )
        dfmt = D3DFMT_D16;
    else if( g_bSupportsD32 )
        dfmt = D3DFMT_D32;
    else if( g_bSupportsD24X8 )
        dfmt = D3DFMT_D24X8;

    if( dfmt != D3DFMT_UNKNOWN )
    {
        D3DCAPS9 Caps;
        pd3dDevice->GetDeviceCaps( &Caps );

        g_MaxMultiSampleType = D3DMULTISAMPLE_NONE;
        for( D3DMULTISAMPLE_TYPE imst = D3DMULTISAMPLE_2_SAMPLES; imst <= D3DMULTISAMPLE_16_SAMPLES;
             imst = ( D3DMULTISAMPLE_TYPE )( imst + 1 ) )
        {
            DWORD msQuality = 0;
            if( SUCCEEDED( pD3D->CheckDeviceMultiSampleType( Caps.AdapterOrdinal,
                                                             Caps.DeviceType,
                                                             fmt,
                                                             settings.d3d9.pp.Windowed,
                                                             imst, &msQuality ) ) )
            {
                g_bUseMultiSample = true;
                g_MaxMultiSampleType = imst;
                if( msQuality > 0 )
                    g_dwMultiSampleQuality = msQuality - 1;
                else
                    g_dwMultiSampleQuality = msQuality;
            }
        }

        // Create the Multi-Sample floating point render target
        if( g_bUseMultiSample )
        {
            const D3DSURFACE_DESC* pBackBufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();
            hr = g_pd3dDevice->CreateRenderTarget( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                                   fmt,
                                                   g_MaxMultiSampleType, g_dwMultiSampleQuality,
                                                   FALSE, &g_pMSRT, NULL );
            if( FAILED( hr ) )
                g_bUseMultiSample = false;
            else
            {
                hr = g_pd3dDevice->CreateDepthStencilSurface( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                                              dfmt,
                                                              g_MaxMultiSampleType, g_dwMultiSampleQuality,
                                                              TRUE, &g_pMSDS, NULL );
                if( FAILED( hr ) )
                {
                    g_bUseMultiSample = false;
                    SAFE_RELEASE( g_pMSRT );
                }
            }
        }
    }

    // For each scale stage, create a texture to hold the intermediate results
    // of the luminance calculation
    int nSampleLen = 1;
    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        fmt = D3DFMT_UNKNOWN;
        switch( g_eEncodingMode )
        {
            case FP16:
                fmt = D3DFMT_R16F; break;
            case FP32:
                fmt = D3DFMT_R32F; break;
            case RGBE8:
                fmt = D3DFMT_A8R8G8B8; break;
            case RGB16:
                fmt = D3DFMT_A16B16G16R16; break;
        }

        hr = pd3dDevice->CreateTexture( nSampleLen, nSampleLen, 1, D3DUSAGE_RENDERTARGET,
                                        fmt, D3DPOOL_DEFAULT, &g_apTexToneMap[i], NULL );
        if( FAILED( hr ) )
            return hr;

        nSampleLen *= 3;
    }

    // Create the temporary blooming effect textures
    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        hr = pd3dDevice->CreateTexture( pBackBufferSurfaceDesc->Width / 8, pBackBufferSurfaceDesc->Height / 8,
                                        1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                        D3DPOOL_DEFAULT, &g_apTexBloom[i], NULL );
        if( FAILED( hr ) )
            return hr;
    }

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );

    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 150, pBackBufferSurfaceDesc->Height - 150 );
    g_SampleUI.SetSize( 150, 110 );

    CDXUTCheckBox* pCheckBox = g_SampleUI.GetCheckBox( IDC_BLOOM );
    pCheckBox->SetChecked( g_bBloom );

    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_ENCODING_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eEncodingMode );

    pComboBox = g_SampleUI.GetComboBox( IDC_RENDER_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eRenderMode );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    UNREFERENCED_PARAMETER( fTime );

    // Update the camera's postion based on user input 
    g_Camera.FrameMove( fElapsedTime );
    g_pEffect->SetValue( "g_vEyePt", g_Camera.GetEyePt(), sizeof( D3DXVECTOR3 ) );
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

    UNREFERENCED_PARAMETER( fElapsedTime );

    LPDIRECT3DSURFACE9 pSurfBackBuffer = NULL;
    PDIRECT3DSURFACE9 pSurfDS = NULL;
    pd3dDevice->GetRenderTarget( 0, &pSurfBackBuffer );
    pd3dDevice->GetDepthStencilSurface( &pSurfDS );

    LPDIRECT3DSURFACE9 pSurfRenderTarget = NULL;
    g_pTexRender->GetSurfaceLevel( 0, &pSurfRenderTarget );

    // Setup the HDR render target
    if( g_bUseMultiSample )
    {
        pd3dDevice->SetRenderTarget( 0, g_pMSRT );
        pd3dDevice->SetDepthStencilSurface( g_pMSDS );
    }
    else
    {
        pd3dDevice->SetRenderTarget( 0, pSurfRenderTarget );
    }

    // Clear the render target
    pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000FF, 1.0f, 0L );

    // Update matrices
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mWorldViewProj;

    D3DXMatrixMultiply( &mWorldViewProj, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix() );
    g_pEffect->SetMatrix( "g_mWorldViewProj", &mWorldViewProj );

    // For the first pass we'll draw the screen to the full screen render target
    // and to update the velocity render target with the velocity of each pixel
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Draw the skybox
        g_aSkybox[g_eEncodingMode].Render( &mWorldViewProj );

        RenderModel();

        // If using floating point multi sampling, stretchrect to the rendertarget
        if( g_bUseMultiSample )
        {
            pd3dDevice->StretchRect( g_pMSRT, NULL, pSurfRenderTarget, NULL, D3DTEXF_NONE );
            pd3dDevice->SetRenderTarget( 0, pSurfRenderTarget );
            pd3dDevice->SetDepthStencilSurface( pSurfDS );
            pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
        }

        MeasureLuminance();
        BrightPassFilter();

        if( g_bBloom )
            RenderBloom();

        //---------------------------------------------------------------------
        // Final pass
        pd3dDevice->SetRenderTarget( 0, pSurfBackBuffer );
        pd3dDevice->SetTexture( 0, g_pTexRender );
        pd3dDevice->SetTexture( 1, g_apTexToneMap[0] );
        pd3dDevice->SetTexture( 2, g_bBloom ? g_apTexBloom[0] : NULL );

        pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

        switch( g_eRenderMode )
        {
            case DECODED:
                g_pEffect->SetTechnique( g_pCurTechnique->FinalPass ); break;
            case RGB_ENCODED:
                g_pEffect->SetTechnique( "FinalPassEncoded_RGB" ); break;
            case ALPHA_ENCODED:
                g_pEffect->SetTechnique( "FinalPassEncoded_A" ); break;
        }

        UINT uiPass, uiNumPasses;
        g_pEffect->Begin( &uiNumPasses, 0 );

        for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
        {
            g_pEffect->BeginPass( uiPass );

            DrawFullScreenQuad();

            g_pEffect->EndPass();
        }

        g_pEffect->End();
        pd3dDevice->SetTexture( 0, NULL );
        pd3dDevice->SetTexture( 1, NULL );
        pd3dDevice->SetTexture( 2, NULL );

        if( g_bShowText )
            RenderText( fTime );

        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );

        pd3dDevice->EndScene();
    }

    SAFE_RELEASE( pSurfRenderTarget );
    SAFE_RELEASE( pSurfBackBuffer );
    SAFE_RELEASE( pSurfDS );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText( double fTime )
{
    UNREFERENCED_PARAMETER( fTime );

    const WCHAR* ENCODING_MODE_NAMES[] =
    {
        L"16-Bit floating-point (FP16)",
        L"32-Bit floating-point (FP32)",
        L"16-Bit integer (RGB16)",
        L"8-Bit integer w/ shared exponent (RGBE8)",
    };

    const WCHAR* RENDER_MODE_NAMES[] =
    {
        L"Decoded scene",
        L"RGB channels of encoded textures",
        L"Alpha channel of encoded textures",
    };


    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work fine however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves perf.
    // TODO: Add sprite
    CDXUTTextHelper txtHelper( g_pFont, NULL, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 2, 0 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 0.90f, 0.90f, 1.0f, 1.0f ) );
    txtHelper.DrawFormattedTextLine( ENCODING_MODE_NAMES[ g_eEncodingMode ] );
    txtHelper.DrawFormattedTextLine( RENDER_MODE_NAMES[ g_eRenderMode ] );

    if( g_bUseMultiSample )
    {
        txtHelper.DrawTextLine( L"Using MultiSample Render Target" );
        txtHelper.DrawFormattedTextLine( L"Number of Samples: %d", ( int )g_MaxMultiSampleType );
        txtHelper.DrawFormattedTextLine( L"Quality: %d", g_dwMultiSampleQuality );
    }

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls:" );

        txtHelper.SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate model: Left mouse button\n"
                                L"Rotate camera: Right mouse button\n"
                                L"Zoom camera: Mouse wheel scroll\n"
                                L"Quit: ESC" );

        txtHelper.SetInsertionPos( 250, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Cycle encoding: E\n"
                                L"Cycle render mode: R\n"
                                L"Toggle bloom: B\n"
                                L"Hide text: T\n" );

        txtHelper.SetInsertionPos( 410, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Hide help: F1\n"
                                L"Change Device: F2\n"
                                L"Toggle HAL/REF: F3\n"
                                L"View readme: F9\n" );
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

    UNREFERENCED_PARAMETER( pbNoFurtherProcessing );

    // Give on screen UI a chance to handle this message
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
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
    UNREFERENCED_PARAMETER( bAltDown );

    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
            case 'T':
                g_bShowText = !g_bShowText; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    CDXUTComboBox* pComboBox = NULL;
    CDXUTCheckBox* pCheckBox = NULL;


    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_BLOOM:
            g_bBloom = !g_bBloom; break;

        case IDC_RENDER_MODE:
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eRenderMode = ( RENDER_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );
            break;

        case IDC_ENCODING_MODE:
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eEncodingMode = ( ENCODING_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );
            g_pCurTechnique = &g_aTechHandles[ g_eEncodingMode ];

            // Refresh resources
            const D3DSURFACE_DESC* pBackBufDesc = DXUTGetD3D9BackBufferSurfaceDesc();
            OnLostDevice( NULL );
            OnResetDevice( g_pd3dDevice, pBackBufDesc, NULL );
            break;

    }

    // Update the bloom checkbox based on new state
    pCheckBox = g_SampleUI.GetCheckBox( IDC_BLOOM );
    pCheckBox->SetEnabled( g_eRenderMode == DECODED );
    pCheckBox->SetChecked( g_eRenderMode == DECODED && g_bBloom );
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
    int i = 0;

    for( i = 0; i < NUM_ENCODING_MODES; i++ )
    {
        g_aSkybox[i].OnLostDevice();
    }

    if( g_pFont )
        g_pFont->OnLostDevice();

    if( g_pEffect )
        g_pEffect->OnLostDevice();

    SAFE_RELEASE( g_pMSRT );
    SAFE_RELEASE( g_pMSDS );

    SAFE_RELEASE( g_pTexRender );
    SAFE_RELEASE( g_pTexBrightPass );

    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexToneMap[i] );
    }

    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexBloom[i] );
    }
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
    for( int i = 0; i < NUM_ENCODING_MODES; i++ )
    {
        g_aSkybox[i].OnDestroyDevice();
    }

    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pMesh );
}


//--------------------------------------------------------------------------------------
inline float GaussianDistribution( float x, float y, float rho )
{
    float g = 1.0f / sqrtf( 2.0f * D3DX_PI * rho * rho );
    g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

    return g;
}


//--------------------------------------------------------------------------------------
inline int log2_ceiling( float val )
{
    int iMax = 256;
    int iMin = 0;

    while( iMax - iMin > 1 )
    {
        int iMiddle = ( iMax + iMin ) / 2;

        if( val > g_aPowsOfTwo[iMiddle] )
            iMin = iMiddle;
        else
            iMax = iMiddle;
    }

    return iMax - 128;
}


//--------------------------------------------------------------------------------------
inline VOID EncodeRGBE8( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Determine the largest color component
    float maxComponent = max( max( r, g ), b );

    // Round to the nearest integer exponent
    int nExp = log2_ceiling( maxComponent );

    // Divide the components by the shared exponent
    FLOAT fDivisor = ( FLOAT )g_aPowsOfTwo[ nExp + 128 ];

    r /= fDivisor;
    g /= fDivisor;
    b /= fDivisor;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store the shared exponent in the alpha channel
    D3DCOLOR* pDestColor = ( D3DCOLOR* )*ppDest;
    *pDestColor = D3DCOLOR_RGBA( ( BYTE )( r * 255 ), ( BYTE )( g * 255 ), ( BYTE )( b * 255 ), nExp + 128 );
    *ppDest += sizeof( D3DCOLOR );
}


//--------------------------------------------------------------------------------------
inline VOID EncodeRGB16( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Divide the components by the multiplier
    r /= RGB16_MAX;
    g /= RGB16_MAX;
    b /= RGB16_MAX;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store
    USHORT* pDestColor = ( USHORT* )*ppDest;
    *pDestColor++ = ( USHORT )( r * 65535 );
    *pDestColor++ = ( USHORT )( g * 65535 );
    *pDestColor++ = ( USHORT )( b * 65535 );

    *ppDest += sizeof( UINT64 );
}


//-----------------------------------------------------------------------------
// Name: RetrieveTechHandles()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT RetrieveTechHandles()
{
    CHAR* modes[] = { "FP16", "FP16", "RGB16", "RGBE8" };
    CHAR* techniques[] = { "Scene", "DownScale2x2_Lum", "DownScale3x3", "DownScale3x3_BrightPass", "FinalPass" };
    DWORD dwNumTechniques = sizeof( TECH_HANDLES ) / sizeof( D3DXHANDLE );

    CHAR strBuffer[MAX_PATH] = {0};

    D3DXHANDLE* pHandle = ( D3DXHANDLE* )g_aTechHandles;

    for( UINT m = 0; m < ( UINT )NUM_ENCODING_MODES; m++ )
    {
        for( UINT t = 0; t < dwNumTechniques; t++ )
        {
            sprintf_s( strBuffer, MAX_PATH - 1, "%s_%s", techniques[t], modes[m] );

            *pHandle++ = g_pEffect->GetTechniqueByName( strBuffer );
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: LoadMesh()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT LoadMesh( WCHAR* strFileName, LPD3DXMESH* ppMesh )
{
    LPD3DXMESH pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    if( ppMesh == NULL )
        return E_INVALIDARG;

    DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName );
    hr = D3DXLoadMeshFromX( str, D3DXMESH_MANAGED,
                            g_pd3dDevice, NULL, NULL, NULL, NULL, &pMesh );
    if( FAILED( hr ) || ( pMesh == NULL ) )
        return hr;

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        LPD3DXMESH pTempMesh;
        hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                  pMesh->GetFVF() | D3DFVF_NORMAL,
                                  g_pd3dDevice, &pTempMesh );
        if( FAILED( hr ) )
            return hr;

        D3DXComputeNormals( pTempMesh, NULL );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimze the mesh to make it fast for the user's graphics card
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: MeasureLuminance()
// Desc: Measure the average log luminance in the scene.
//-----------------------------------------------------------------------------
HRESULT MeasureLuminance()
{
    HRESULT hr = S_OK;
    UINT uiNumPasses, uiPass;
    D3DXVECTOR2 avSampleOffsets[16];

    LPDIRECT3DTEXTURE9 pTexSrc = NULL;
    LPDIRECT3DTEXTURE9 pTexDest = NULL;
    LPDIRECT3DSURFACE9 pSurfDest = NULL;

    //-------------------------------------------------------------------------
    // Initial sampling pass to convert the image to the log of the grayscale
    //-------------------------------------------------------------------------
    pTexSrc = g_pTexRender;
    pTexDest = g_apTexToneMap[NUM_TONEMAP_TEXTURES - 1];

    D3DSURFACE_DESC descSrc;
    pTexSrc->GetLevelDesc( 0, &descSrc );

    D3DSURFACE_DESC descDest;
    pTexDest->GetLevelDesc( 0, &descDest );

    GetSampleOffsets_DownScale2x2_Lum( descSrc.Width, descSrc.Height, descDest.Width, descDest.Height,
                                       avSampleOffsets );

    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetTechnique( g_pCurTechnique->DownScale2x2_Lum );

    hr = pTexDest->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

    pd3dDevice->SetRenderTarget( 0, pSurfDest );
    pd3dDevice->SetTexture( 0, pTexSrc );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

    hr = g_pEffect->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    pd3dDevice->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );

    //-------------------------------------------------------------------------
    // Iterate through the remaining tone map textures
    //------------------------------------------------------------------------- 
    for( int i = NUM_TONEMAP_TEXTURES - 1; i > 0; i-- )
    {
        // Cycle the textures
        pTexSrc = g_apTexToneMap[i];
        pTexDest = g_apTexToneMap[i - 1];

        hr = pTexDest->GetSurfaceLevel( 0, &pSurfDest );
        if( FAILED( hr ) )
            return hr;

        D3DSURFACE_DESC desc;
        pTexSrc->GetLevelDesc( 0, &desc );
        GetSampleOffsets_DownScale3x3( desc.Width, desc.Height, avSampleOffsets );

        g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
        g_pEffect->SetTechnique( g_pCurTechnique->DownScale3x3 );

        pd3dDevice->SetRenderTarget( 0, pSurfDest );
        pd3dDevice->SetTexture( 0, pTexSrc );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

        hr = g_pEffect->Begin( &uiNumPasses, 0 );
        if( FAILED( hr ) )
            return hr;

        for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
        {
            g_pEffect->BeginPass( uiPass );

            // Draw a fullscreen quad to sample the RT
            DrawFullScreenQuad();

            g_pEffect->EndPass();
        }

        g_pEffect->End();
        pd3dDevice->SetTexture( 0, NULL );

        SAFE_RELEASE( pSurfDest );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale3x3
// Desc: Get the texture coordinate offsets to be used inside the DownScale3x3
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale3x3( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwWidth;
    float tV = 1.0f / dwHeight;

    // Sample from the 9 surrounding points. 
    int index = 0;
    for( int y = -1; y <= 1; y++ )
    {
        for( int x = -1; x <= 1; x++ )
        {
            avSampleOffsets[ index ].x = x * tU;
            avSampleOffsets[ index ].y = y * tV;

            index++;
        }
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale2x2_Lum
// Desc: Get the texture coordinate offsets to be used inside the DownScale2x2_Lum
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale2x2_Lum( DWORD dwSrcWidth, DWORD dwSrcHeight, DWORD dwDestWidth, DWORD dwDestHeight,
                                           D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwSrcWidth;
    float tV = 1.0f / dwSrcHeight;

    float deltaU = ( float )dwSrcWidth / dwDestWidth / 2.0f;
    float deltaV = ( float )dwSrcHeight / dwDestHeight / 2.0f;

    // Sample from 4 surrounding points. 
    int index = 0;
    for( int y = 0; y < 2; y++ )
    {
        for( int x = 0; x <= 2; x++ )
        {
            avSampleOffsets[ index ].x = ( x - 0.5f ) * deltaU * tU;
            avSampleOffsets[ index ].y = ( y - 0.5f ) * deltaV * tV;

            index++;
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize,
                                float afTexCoordOffset[15],
                                D3DXVECTOR4* avColorWeight,
                                float fDeviation, float fMultiplier )
{
    int i = 0;
    float tu = 1.0f / ( float )dwD3DTexSize;

    // Fill the center texel
    float weight = 1.0f * GaussianDistribution( 0, 0, fDeviation );
    avColorWeight[0] = D3DXVECTOR4( weight, weight, weight, 1.0f );

    afTexCoordOffset[0] = 0.0f;

    // Fill the right side
    for( i = 1; i < 8; i++ )
    {
        weight = fMultiplier * GaussianDistribution( ( float )i, 0, fDeviation );
        afTexCoordOffset[i] = i * tu;

        avColorWeight[i] = D3DXVECTOR4( weight, weight, weight, 1.0f );
    }

    // Copy to the left side
    for( i = 8; i < 15; i++ )
    {
        avColorWeight[i] = avColorWeight[i - 7];
        afTexCoordOffset[i] = -afTexCoordOffset[i - 7];
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Render the model
//-----------------------------------------------------------------------------
HRESULT RenderModel()
{
    HRESULT hr = S_OK;

    // Set the transforms
    D3DXMATRIXA16 mWorldViewProj;
    D3DXMatrixMultiply( &mWorldViewProj, g_Camera.GetWorldMatrix(), g_Camera.GetViewMatrix() );
    D3DXMatrixMultiply( &mWorldViewProj, &mWorldViewProj, g_Camera.GetProjMatrix() );
    g_pEffect->SetMatrix( "g_mWorld", g_Camera.GetWorldMatrix() );
    g_pEffect->SetMatrix( "g_mWorldViewProj", &mWorldViewProj );

    // Draw the mesh
    g_pEffect->SetTechnique( g_pCurTechnique->Scene );
    g_pEffect->SetTexture( "g_tCube", g_aSkybox[g_eEncodingMode].GetEnvironmentMap() );

    UINT uiPass, uiNumPasses;
    hr = g_pEffect->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        g_pMesh->DrawSubset( 0 );

        g_pEffect->EndPass();
    }

    g_pEffect->End();

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: BrightPassFilter
// Desc: Prepare for the bloom pass by removing dark information from the scene
//-----------------------------------------------------------------------------
HRESULT BrightPassFilter()
{
    HRESULT hr = S_OK;

    const D3DSURFACE_DESC* pBackBufDesc = DXUTGetD3D9BackBufferSurfaceDesc();

    D3DXVECTOR2 avSampleOffsets[16];
    GetSampleOffsets_DownScale3x3( pBackBufDesc->Width / 2, pBackBufDesc->Height / 2, avSampleOffsets );

    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

    LPDIRECT3DSURFACE9 pSurfBrightPass = NULL;
    hr = g_pTexBrightPass->GetSurfaceLevel( 0, &pSurfBrightPass );
    if( FAILED( hr ) )
        return hr;

    g_pEffect->SetTechnique( g_pCurTechnique->DownScale3x3_BrightPass );
    g_pd3dDevice->SetRenderTarget( 0, pSurfBrightPass );
    g_pd3dDevice->SetTexture( 0, g_pTexRender );
    g_pd3dDevice->SetTexture( 1, g_apTexToneMap[0] );

    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

    UINT uiPass, uiNumPasses;
    hr = g_pEffect->Begin( &uiNumPasses, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiNumPasses; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    g_pd3dDevice->SetTexture( 0, NULL );
    g_pd3dDevice->SetTexture( 1, NULL );

    SAFE_RELEASE( pSurfBrightPass );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderBloom()
// Desc: Render the blooming effect
//-----------------------------------------------------------------------------
HRESULT RenderBloom()
{
    HRESULT hr = S_OK;
    UINT uiPassCount, uiPass;
    int i = 0;

    D3DXVECTOR2 avSampleOffsets[16];
    float afSampleOffsets[16];
    D3DXVECTOR4 avSampleWeights[16];

    LPDIRECT3DSURFACE9 pSurfDest = NULL;
    hr = g_apTexBloom[1]->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    D3DSURFACE_DESC desc;
    hr = g_pTexBrightPass->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Width, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 16; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( afSampleOffsets[i], 0.0f );
    }

    g_pEffect->SetTechnique( "Bloom" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfDest );
    g_pd3dDevice->SetTexture( 0, g_pTexBrightPass );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    g_pEffect->Begin( &uiPassCount, 0 );
    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect->EndPass();
    }
    g_pEffect->End();
    g_pd3dDevice->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );
    hr = g_apTexBloom[0]->GetSurfaceLevel( 0, &pSurfDest );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Height, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 16; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( 0.0f, afSampleOffsets[i] );
    }

    g_pEffect->SetTechnique( "Bloom" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfDest );
    g_pd3dDevice->SetTexture( 0, g_apTexBloom[1] );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    g_pEffect->Begin( &uiPassCount, 0 );

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad();

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    g_pd3dDevice->SetTexture( 0, NULL );

    SAFE_RELEASE( pSurfDest );

    return hr;
}


//-----------------------------------------------------------------------------
// Name: DrawFullScreenQuad
// Desc: Draw a properly aligned quad covering the entire render target
//-----------------------------------------------------------------------------
void DrawFullScreenQuad( float fLeftU, float fTopV, float fRightU, float fBottomV )
{
    D3DSURFACE_DESC dtdsdRT;
    PDIRECT3DSURFACE9 pSurfRT;

    // Acquire render target width and height
    g_pd3dDevice->GetRenderTarget( 0, &pSurfRT );
    pSurfRT->GetDesc( &dtdsdRT );
    pSurfRT->Release();

    // Ensure that we're directly mapping texels to pixels by offset by 0.5
    // For more info see the doc page titled "Directly Mapping Texels to Pixels"
    FLOAT fWidth5 = ( FLOAT )dtdsdRT.Width - 0.5f;
    FLOAT fHeight5 = ( FLOAT )dtdsdRT.Height - 0.5f;

    // Draw the quad
    SCREEN_VERTEX svQuad[4];

    svQuad[0].pos = D3DXVECTOR4( -0.5f, -0.5f, 0.5f, 1.0f );
    svQuad[0].tex = D3DXVECTOR2( fLeftU, fTopV );

    svQuad[1].pos = D3DXVECTOR4( fWidth5, -0.5f, 0.5f, 1.0f );
    svQuad[1].tex = D3DXVECTOR2( fRightU, fTopV );

    svQuad[2].pos = D3DXVECTOR4( -0.5f, fHeight5, 0.5f, 1.0f );
    svQuad[2].tex = D3DXVECTOR2( fLeftU, fBottomV );

    svQuad[3].pos = D3DXVECTOR4( fWidth5, fHeight5, 0.5f, 1.0f );
    svQuad[3].tex = D3DXVECTOR2( fRightU, fBottomV );

    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    g_pd3dDevice->SetFVF( SCREEN_VERTEX::FVF );
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, svQuad, sizeof( SCREEN_VERTEX ) );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
}


//-----------------------------------------------------------------------------
// Name: CreateEncodedTexture
// Desc: Create a copy of the input floating-point texture with RGBE8 or RGB16 
//       encoding
//-----------------------------------------------------------------------------
HRESULT CreateEncodedTexture( IDirect3DCubeTexture9* pTexSrc, IDirect3DCubeTexture9** ppTexDest,
                              ENCODING_MODE eTarget )
{
    HRESULT hr = S_OK;

    D3DSURFACE_DESC desc;
    V_RETURN( pTexSrc->GetLevelDesc( 0, &desc ) );

    // Create a texture with equal dimensions to store the encoded texture
    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    switch( eTarget )
    {
        case RGBE8:
            fmt = D3DFMT_A8R8G8B8; break;
        case RGB16:
            fmt = D3DFMT_A16B16G16R16; break;
    }

    V_RETURN( g_pd3dDevice->CreateCubeTexture( desc.Width, 1, 0,
                                               fmt, D3DPOOL_MANAGED,
                                               ppTexDest, NULL ) );

    for( UINT iFace = 0; iFace < 6; iFace++ )
    {
        // Lock the source texture for reading
        D3DLOCKED_RECT rcSrc;
        V_RETURN( pTexSrc->LockRect( ( D3DCUBEMAP_FACES )iFace, 0, &rcSrc, NULL, D3DLOCK_READONLY ) );

        // Lock the destination texture for writing
        D3DLOCKED_RECT rcDest;
        V_RETURN( ( *ppTexDest )->LockRect( ( D3DCUBEMAP_FACES )iFace, 0, &rcDest, NULL, 0 ) );

        BYTE* pSrcBytes = ( BYTE* )rcSrc.pBits;
        BYTE* pDestBytes = ( BYTE* )rcDest.pBits;


        for( UINT y = 0; y < desc.Height; y++ )
        {
            D3DXFLOAT16* pSrc = ( D3DXFLOAT16* )pSrcBytes;
            BYTE* pDest = pDestBytes;

            for( UINT x = 0; x < desc.Width; x++ )
            {
                switch( eTarget )
                {
                    case RGBE8:
                        EncodeRGBE8( pSrc, &pDest ); break;
                    case RGB16:
                        EncodeRGB16( pSrc, &pDest ); break;
                    default:
                        return E_FAIL;
                }

                pSrc += 4;
            }

            pSrcBytes += rcSrc.Pitch;
            pDestBytes += rcDest.Pitch;
        }

        // Release the locks
        ( *ppTexDest )->UnlockRect( ( D3DCUBEMAP_FACES )iFace, 0 );
        pTexSrc->UnlockRect( ( D3DCUBEMAP_FACES )iFace, 0 );
    }

    return S_OK;
}

