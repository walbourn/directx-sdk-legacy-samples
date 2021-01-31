//--------------------------------------------------------------------------------------
// File: LocalDeformablePRT.cpp
//
// Desc: This sample demonstrates a simple usage of Local-deformable precomputed 
//       radiance transfer (LDPRT). This implementation does not require an offline 
//       simulator for calulcating PRT coefficients; instead, the coefficients are 
//       calculated from a 'thickness' texture. This allows an artist to create and 
//       tweak sub-surface scattering PRT data in an intuitive way.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include "skybox.h"
#include "skinmesh.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
CModelViewerCamera          g_Camera;               // A model viewing camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
BOOL                        g_bWireFrame;
D3DFORMAT                   g_fmtCubeMap = D3DFMT_UNKNOWN;
D3DFORMAT                   g_fmtTexture = D3DFMT_UNKNOWN;
CSkybox                     g_Skybox;

// Skinning
D3DXFRAME*                  g_pFrameRoot = NULL;
ID3DXAnimationController*   g_pAnimController = NULL;

// Lights
CDXUTDirectionWidget        g_LightControl;
D3DXVECTOR3                 g_vLightDirection;
float                       g_fLightIntensity;
float                       g_fEnvIntensity;
float g_fSkyBoxLightSH[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
FLOAT m_fRLC[D3DXSH_MAXORDER*D3DXSH_MAXORDER];
FLOAT m_fGLC[D3DXSH_MAXORDER*D3DXSH_MAXORDER];
FLOAT m_fBLC[D3DXSH_MAXORDER*D3DXSH_MAXORDER];



//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN          1
#define IDC_TOGGLEREF                 2
#define IDC_CHANGEDEVICE              3
#define IDC_TECHNIQUE                 4
#define IDC_RED_TRANSMIT_SLIDER       5
#define IDC_GREEN_TRANSMIT_SLIDER     6
#define IDC_BLUE_TRANSMIT_SLIDER      7
#define IDC_RED_TRANSMIT_LABEL        8
#define IDC_GREEN_TRANSMIT_LABEL      9
#define IDC_BLUE_TRANSMIT_LABEL       10
#define IDC_LIGHT_SLIDER              11
#define IDC_ENV_SLIDER                12
#define IDC_ENV_LABEL                 13
#define IDC_ANIMATION_SPEED           14


//--------------------------------------------------------------------------------------
// struct SHCubeProj
// Used to generate lighting coefficients to match the skybox's light probe
//--------------------------------------------------------------------------------------
struct SHCubeProj
{
    float* pRed,*pGreen,*pBlue;
    int iOrderUse; // order to use
    float   fConvCoeffs[6]; // convolution coefficients

    void    InitDiffCubeMap( float* pR, float* pG, float* pB )
    {
        pRed = pR;
        pGreen = pG;
        pBlue = pB;

        iOrderUse = 3; // go to 5 is a bit more accurate...
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
void GetSupportedTextureFormat( IDirect3D9* pD3D, D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT* pfmtTexture,
                                D3DFORMAT* pfmtCubeMap );
HRESULT LoadLDPRTData( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName );
void    WINAPI SHCubeFill( D3DXVECTOR4* pOut, CONST D3DXVECTOR3* pTexCoord, CONST D3DXVECTOR3* pTexelSize, LPVOID pData );
HRESULT LoadTechniqueObjects( const char* szMedia );
void UpdateLightingEnvironment();
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame );
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase );
void RenderText();


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
    DXUTCreateWindow( L"LocalDeformablePRT" );
    DXUTCreateDevice( true, 800, 600 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    //g_LightControl.SetLightDirection( D3DXVECTOR3(-0.29f, 0.557f, 0.778f) );
    g_LightControl.SetLightDirection( D3DXVECTOR3( -0.789f, 0.527f, 0.316f ) );
    g_LightControl.SetButtonMask( MOUSE_MIDDLE_BUTTON );

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iX = 15; int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", iX, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", iX, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", iX, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iX = 15; iY = 10;

    // Title font for static
    g_SampleUI.SetFont( 1, L"Arial", 14, FW_NORMAL );
    CDXUTElement* pElement = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_RIGHT | DT_VCENTER;
    }

    // Technique
    g_SampleUI.AddStatic( -1, L"Technique", iX, iY += 24, 115, 22 );
    g_SampleUI.AddComboBox( IDC_TECHNIQUE, iX + 125, iY, 150, 22 );
    g_SampleUI.GetComboBox( IDC_TECHNIQUE )->SetScrollBarWidth( 0 );
    g_SampleUI.GetComboBox( IDC_TECHNIQUE )->AddItem( L"Local-deformable PRT", ( void* )"LDPRT" );
    g_SampleUI.GetComboBox( IDC_TECHNIQUE )->AddItem( L"N dot L lighting", ( void* )"NdotL" );

    // Animation speed
    iY += 10;
    g_SampleUI.AddStatic( -1, L"Animation Speed", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_ANIMATION_SPEED, iX + 125, iY, 125, 22, 0, 3000, 700 );

    // Light intensity
    iY += 10;
    g_SampleUI.AddStatic( -1, L"Light Intensity", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_LIGHT_SLIDER, iX + 125, iY, 125, 22, 0, 1000, 500 );
    g_SampleUI.AddStatic( IDC_ENV_LABEL, L"Env Intensity", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_ENV_SLIDER, iX + 125, iY, 125, 22, 0, 3000, 600 );

    // Color transmission
    iY += 10;
    g_SampleUI.AddStatic( IDC_RED_TRANSMIT_LABEL, L"Transmit Red", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_RED_TRANSMIT_SLIDER, iX + 125, iY, 125, 22, 0, 3000, 1200 );
    g_SampleUI.AddStatic( IDC_GREEN_TRANSMIT_LABEL, L"Transmit Green", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_GREEN_TRANSMIT_SLIDER, iX + 125, iY, 125, 22, 0, 3000, 800 );
    g_SampleUI.AddStatic( IDC_BLUE_TRANSMIT_LABEL, L"Transmit Blue", iX, iY += 24, 115, 22 );
    g_SampleUI.AddSlider( IDC_BLUE_TRANSMIT_SLIDER, iX + 125, iY, 125, 22, 0, 3000, 350 );
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

    // Determine texture support.  Fail if not good enough 
    D3DFORMAT fmtTexture, fmtCubeMap;
    GetSupportedTextureFormat( pD3D, pCaps, AdapterFormat, &fmtTexture, &fmtCubeMap );
    if( D3DFMT_UNKNOWN == fmtTexture || D3DFMT_UNKNOWN == fmtCubeMap )
        return false;

    // This sample requires pixel shader 2.0, but does showcase techniques which will 
    // perform well on shader model 1.1 hardware.
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
void GetSupportedTextureFormat( IDirect3D9* pD3D, D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT* pfmtTexture,
                                D3DFORMAT* pfmtCubeMap )
{
    D3DFORMAT fmtTexture = D3DFMT_UNKNOWN;
    D3DFORMAT fmtCubeMap = D3DFMT_UNKNOWN;

    // check for linear filtering support of signed formats
    fmtTexture = D3DFMT_UNKNOWN;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                            D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        fmtTexture = D3DFMT_A16B16G16R16F;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                                 D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_Q16W16V16U16 ) ) )
    {
        fmtTexture = D3DFMT_Q16W16V16U16;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                                 D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, D3DFMT_Q8W8V8U8 ) ) )
    {
        fmtTexture = D3DFMT_Q8W8V8U8;
    }
        // no support for linear filtering of signed, just checking for format support now
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        fmtTexture = D3DFMT_A16B16G16R16F;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_TEXTURE, D3DFMT_Q16W16V16U16 ) ) )
    {
        fmtTexture = D3DFMT_Q16W16V16U16;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_TEXTURE, D3DFMT_Q8W8V8U8 ) ) )
    {
        fmtTexture = D3DFMT_Q8W8V8U8;
    }

    // check for support linear filtering of signed format cubemaps
    fmtCubeMap = D3DFMT_UNKNOWN;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                            D3DUSAGE_QUERY_FILTER, D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        fmtCubeMap = D3DFMT_A16B16G16R16F;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                                 D3DUSAGE_QUERY_FILTER, D3DRTYPE_CUBETEXTURE, D3DFMT_Q16W16V16U16 ) ) )
    {
        fmtCubeMap = D3DFMT_Q16W16V16U16;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat,
                                                 D3DUSAGE_QUERY_FILTER, D3DRTYPE_CUBETEXTURE, D3DFMT_Q8W8V8U8 ) ) )
    {
        fmtCubeMap = D3DFMT_Q8W8V8U8;
    }
        // no support for linear filtering of signed formats, just checking for format support now
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        fmtCubeMap = D3DFMT_A16B16G16R16F;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_CUBETEXTURE, D3DFMT_Q16W16V16U16 ) ) )
    {
        fmtCubeMap = D3DFMT_Q16W16V16U16;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_CUBETEXTURE, D3DFMT_Q8W8V8U8 ) ) )
    {
        fmtCubeMap = D3DFMT_Q8W8V8U8;
    }

    if( pfmtTexture )
        *pfmtTexture = fmtTexture;
    if( pfmtCubeMap )
        *pfmtCubeMap = fmtCubeMap;
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

    // Turn vsync off
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // If device doesn't support HW T&L or doesn't support 2.0 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
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

    if( caps.MaxVertexBlendMatrices < 2 )
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

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
    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

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

    // Determine which LDPRT texture and SH coefficient cubemap formats are supported
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 Caps;
    pd3dDevice->GetDeviceCaps( &Caps );
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );

    GetSupportedTextureFormat( pD3D, &Caps, DisplayMode.Format, &g_fmtTexture, &g_fmtCubeMap );
    if( D3DFMT_UNKNOWN == g_fmtTexture || D3DFMT_UNKNOWN == g_fmtCubeMap )
        return E_FAIL;

    // Create the skybox
    g_Skybox.OnCreateDevice( pd3dDevice, 50, L"Light Probes\\rnl_cross.dds", L"SkyBox.fx" );
    V( D3DXSHProjectCubeMap( 6, g_Skybox.GetEnvironmentMap(), g_fSkyBoxLightSH[0], g_fSkyBoxLightSH[1],
                             g_fSkyBoxLightSH[2] ) );

    // Now compute the SH projection of the skybox...
    LPDIRECT3DCUBETEXTURE9 pSHCubeTex = NULL;
    V( D3DXCreateCubeTexture( pd3dDevice, 256, 1, 0, D3DFMT_A16B16G16R16F, D3DPOOL_MANAGED, &pSHCubeTex ) );

    SHCubeProj projData;
    projData.Init( g_fSkyBoxLightSH[0], g_fSkyBoxLightSH[1], g_fSkyBoxLightSH[2] );

    V( D3DXFillCubeTexture( pSHCubeTex, SHCubeFill, &projData ) );
    g_Skybox.InitSH( pSHCubeTex );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "LocalDeformablePRT.fx" ) ) );

    // If this fails, there should be debug output as to they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, NULL ) );

    V_RETURN( LoadTechniqueObjects( "bat" ) );

    V_RETURN( g_LightControl.StaticOnD3D9CreateDevice( pd3dDevice ) );
    g_LightControl.SetRadius( 2.0f );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Set the model's initial orientation
    D3DXQUATERNION quatRotation;
    D3DXQuaternionRotationYawPitchRoll( &quatRotation, -0.5f, 0.7f, 0.0f );
    g_Camera.SetWorldQuat( quatRotation );

    return hr;
}


//--------------------------------------------------------------------------------------
void WINAPI SHCubeFill( D3DXVECTOR4* pOut,
                       CONST D3DXVECTOR3* pTexCoord,
                       CONST D3DXVECTOR3* pTexelSize,
                       LPVOID pData )
 {
    SHCubeProj* pCP = ( SHCubeProj* ) pData;
    D3DXVECTOR3 vDir;

    D3DXVec3Normalize( &vDir,pTexCoord );

    float fVals[36];
    D3DXSHEvalDirection( fVals, pCP->iOrderUse, &vDir );

    ( *pOut ) = D3DXVECTOR4( 0,0,0,0 ); // just clear it out...

    int l, m, uIndex = 0;
    for( l=0; l<pCP->iOrderUse; l++ )
 {
        const float fConvUse = pCP->fConvCoeffs[l];
        for( m=0; m<2*l+1; m++ )
 {
            pOut->x += fConvUse*fVals[uIndex]*pCP->pRed[uIndex];
            pOut->y += fConvUse*fVals[uIndex]*pCP->pGreen[uIndex];
            pOut->z += fConvUse*fVals[uIndex]*pCP->pBlue[uIndex];
            pOut->w = 1;

            uIndex++;
        }
    }
}


//--------------------------------------------------------------------------------------
void WINAPI myFillBF( D3DXVECTOR4* pOut,
                     CONST D3DXVECTOR3* pTexCoord,
                     CONST D3DXVECTOR3* pTexelSize,
                     LPVOID pData )
 {
    D3DXVECTOR3 vDir;

    int iBase = ( int )( INT_PTR )pData;

    D3DXVec3Normalize( &vDir,pTexCoord );

    float fVals[16];
    D3DXSHEvalDirection( fVals, 4, &vDir );

    ( *pOut ) = D3DXVECTOR4( fVals[iBase+0],fVals[iBase+1],fVals[iBase+2],fVals[iBase+3] );
}


//--------------------------------------------------------------------------------------
// This function loads a new technique and all device objects it requires.
//--------------------------------------------------------------------------------------
HRESULT LoadTechniqueObjects( const char* szMedia )
{
    HRESULT hr = S_OK;

    if( NULL == g_pEffect )
        return D3DERR_INVALIDCALL;

    IDirect3DTexture9* pTexture = NULL;
    IDirect3DCubeTexture9* pCubeTexture = NULL;

    IDirect3DDevice9* pDevice = DXUTGetD3D9Device();

    WCHAR strFileName[MAX_PATH+1] = {0};
    WCHAR strPath[MAX_PATH+1] = {0};
    char strTechnique[MAX_PATH] = {0};

    // Make sure the technique works
    char* strComboTech = ( char* )g_SampleUI.GetComboBox( IDC_TECHNIQUE )->GetSelectedData();
    strcpy_s( strTechnique, MAX_PATH, strComboTech );
    bool bLDPRT = ( strTechnique && ( 0 == strcmp( strTechnique, "LDPRT" ) ) );

    // If we're not a signed format, make sure we use a technnique that will unbias
    if( D3DFMT_Q16W16V16U16 != g_fmtTexture && D3DFMT_Q8W8V8U8 != g_fmtTexture )
        strcat_s( strTechnique, MAX_PATH, "_Unbias" );

    D3DXHANDLE hTechnique = g_pEffect->GetTechniqueByName( strTechnique );
    V_RETURN( g_pEffect->SetTechnique( hTechnique ) );

    // Enable/disable LDPRT-only items
    g_SampleUI.GetStatic( IDC_ENV_LABEL )->SetEnabled( bLDPRT );
    g_SampleUI.GetSlider( IDC_ENV_SLIDER )->SetEnabled( bLDPRT );
    g_SampleUI.GetSlider( IDC_RED_TRANSMIT_SLIDER )->SetEnabled( bLDPRT );
    g_SampleUI.GetSlider( IDC_GREEN_TRANSMIT_SLIDER )->SetEnabled( bLDPRT );
    g_SampleUI.GetSlider( IDC_BLUE_TRANSMIT_SLIDER )->SetEnabled( bLDPRT );
    g_SampleUI.GetStatic( IDC_RED_TRANSMIT_LABEL )->SetEnabled( bLDPRT );
    g_SampleUI.GetStatic( IDC_GREEN_TRANSMIT_LABEL )->SetEnabled( bLDPRT );
    g_SampleUI.GetStatic( IDC_BLUE_TRANSMIT_LABEL )->SetEnabled( bLDPRT );

    // Load the mesh
    swprintf_s( strFileName, MAX_PATH, TEXT( "media\\%S" ), szMedia );
    V_RETURN( LoadLDPRTData( pDevice, strFileName ) );

    // Albedo texture
    swprintf_s( strFileName, MAX_PATH, TEXT( "media\\%SAlbedo.dds" ), szMedia );
    DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, strFileName );
    V( D3DXCreateTextureFromFile( pDevice, strPath, &pTexture ) );
    g_pEffect->SetTexture( "Albedo", pTexture );
    SAFE_RELEASE( pTexture );

    // Normal map 
    swprintf_s( strFileName, MAX_PATH, TEXT( "media\\%SNormalMap.dds" ), szMedia );
    DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, strFileName );
    V( D3DXCreateTextureFromFileEx( pDevice, strPath, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                    g_fmtTexture, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                    NULL, NULL, &pTexture ) );
    g_pEffect->SetTexture( "NormalMap", pTexture );
    SAFE_RELEASE( pTexture );

    // Spherical harmonic basic functions
    char* pNames[4] = {"YlmCoeff0","YlmCoeff4","YlmCoeff8","YlmCoeff12"};
    for( int i = 0; i < 4; i++ )
    {
        D3DXCreateCubeTexture( pDevice, 32, 1, 0, g_fmtCubeMap, D3DPOOL_MANAGED, &pCubeTexture );
        D3DXFillCubeTexture( pCubeTexture, myFillBF, ( LPVOID )( INT_PTR )( i * 4 ) );
        g_pEffect->SetTexture( pNames[i], pCubeTexture );
        SAFE_RELEASE( pCubeTexture );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and LDPRT data.  It also centers and optimizes the 
// mesh for the graphics card's vertex cache.
//--------------------------------------------------------------------------------------
HRESULT LoadLDPRTData( IDirect3DDevice9* pd3dDevice, WCHAR* strFilePrefixIn )
{
    WCHAR str[MAX_PATH];
    WCHAR strFileName[MAX_PATH];
    WCHAR strFilePrefix[MAX_PATH];
    HRESULT hr;

    // Load the mesh with D3DX and get back a ID3DXMesh*.  For this
    // sample we'll ignore the X file's embedded materials since we know 
    // exactly the model we're loading.  See the mesh samples such as
    // "OptimizedMesh" for a more generic mesh loading example.
    swprintf_s( strFilePrefix, MAX_PATH, TEXT( "%s" ), strFilePrefixIn ); strFilePrefix[MAX_PATH - 1] = 0;
    swprintf_s( strFileName, MAX_PATH, TEXT( "%s.x" ), strFilePrefix ); strFileName[MAX_PATH - 1] = 0;
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );

    CAllocateHierarchy Alloc;

    // Delete existing resources
    if( g_pFrameRoot )
    {
        D3DXFrameDestroy( g_pFrameRoot, &Alloc );
        g_pFrameRoot = NULL;
    }

    // Create hierarchy
    V_RETURN( D3DXLoadMeshHierarchyFromX( str, D3DXMESH_MANAGED, pd3dDevice,
                                          &Alloc, NULL, &g_pFrameRoot, &g_pAnimController ) );

    SetupBoneMatrixPointers( g_pFrameRoot, g_pFrameRoot );


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

    g_LightControl.OnD3D9ResetDevice( pBackBufferSurfaceDesc );
    g_Skybox.OnResetDevice( pBackBufferSurfaceDesc );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.001f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON );
    g_Camera.SetAttachCameraToModel( true );
    g_Camera.SetRadius( 5.0f, 0.1f, 20.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 300, pBackBufferSurfaceDesc->Height - 245 );
    g_SampleUI.SetSize( 300, 300 );

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
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    UpdateLightingEnvironment();

    if( g_pAnimController != NULL )
    {
        g_pAnimController->SetTrackSpeed( 0, g_SampleUI.GetSlider( IDC_ANIMATION_SPEED )->GetValue() / 1000.0f );
        g_pAnimController->AdvanceTime( fElapsedTime, NULL );
    }

    UpdateFrameMatrices( g_pFrameRoot, g_Camera.GetWorldMatrix() );
}


//--------------------------------------------------------------------------------------
void UpdateLightingEnvironment()
{
    // Gather lighting options from the HUD
    g_vLightDirection = g_LightControl.GetLightDirection();
    g_fLightIntensity = g_SampleUI.GetSlider( IDC_LIGHT_SLIDER )->GetValue() / 100.0f;
    g_fEnvIntensity = g_SampleUI.GetSlider( IDC_ENV_SLIDER )->GetValue() / 1000.0f;

    // Create the spotlight
    D3DXSHEvalConeLight( D3DXSH_MAXORDER, &g_vLightDirection, D3DX_PI / 8.0f,
                         g_fLightIntensity, g_fLightIntensity, g_fLightIntensity,
                         m_fRLC, m_fGLC, m_fBLC );

    float fSkybox[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];

    // Scale the light probe environment contribution based on input options    
    D3DXSHScale( fSkybox[0], D3DXSH_MAXORDER, g_fSkyBoxLightSH[0], g_fEnvIntensity );
    D3DXSHScale( fSkybox[1], D3DXSH_MAXORDER, g_fSkyBoxLightSH[1], g_fEnvIntensity );
    D3DXSHScale( fSkybox[2], D3DXSH_MAXORDER, g_fSkyBoxLightSH[2], g_fEnvIntensity );

    // Combine the environment and the spotlight
    D3DXSHAdd( m_fRLC, D3DXSH_MAXORDER, m_fRLC, fSkybox[0] );
    D3DXSHAdd( m_fGLC, D3DXSH_MAXORDER, m_fGLC, fSkybox[1] );
    D3DXSHAdd( m_fBLC, D3DXSH_MAXORDER, m_fBLC, fSkybox[2] );
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

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 50, 50, 50 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        D3DXMATRIXA16 mViewProjection = ( *g_Camera.GetViewMatrix() ) * ( *g_Camera.GetProjMatrix() );

        g_Skybox.SetDrawSH( false );
        g_Skybox.Render( &mViewProjection, 1.0f, 1.0f );

        V( g_pEffect->SetMatrix( "g_mViewProjection", &mViewProjection ) );
        V( g_pEffect->SetFloat( "g_fTime", ( float )fTime ) );

        // Set the amount of transmitted light per color channel
        D3DXVECTOR3 vColorTransmit;
        vColorTransmit.x = g_SampleUI.GetSlider( IDC_RED_TRANSMIT_SLIDER )->GetValue() / 1000.0f;
        vColorTransmit.y = g_SampleUI.GetSlider( IDC_GREEN_TRANSMIT_SLIDER )->GetValue() / 1000.0f;
        vColorTransmit.z = g_SampleUI.GetSlider( IDC_BLUE_TRANSMIT_SLIDER )->GetValue() / 1000.0f;
        V( g_pEffect->SetFloatArray( "g_vColorTransmit", vColorTransmit, 3 ) );

        // for Cubic degree rendering
        V( g_pEffect->SetFloat( "g_fLightIntensity", g_fLightIntensity ) );
        V( g_pEffect->SetFloatArray( "g_vLightDirection", g_vLightDirection, 3 * sizeof( float ) ) );
        V( g_pEffect->SetFloatArray( "g_vLightCoeffsR", m_fRLC, 4 * sizeof( float ) ) );
        V( g_pEffect->SetFloatArray( "g_vLightCoeffsG", m_fGLC, 4 * sizeof( float ) ) );
        V( g_pEffect->SetFloatArray( "g_vLightCoeffsB", m_fBLC, 4 * sizeof( float ) ) );

        pd3dDevice->SetRenderState( D3DRS_FILLMODE, g_bWireFrame ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

        DrawFrame( pd3dDevice, g_pFrameRoot );

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );
        V( g_LightControl.OnRender9( D3DXCOLOR( 1, 1, 1, 1 ), ( D3DXMATRIX* )g_Camera.GetViewMatrix(),
                                     ( D3DXMATRIX* )g_Camera.GetProjMatrix(), g_Camera.GetEyePt() ) );
        DXUT_EndPerfEvent();

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Called to render a frame in the hierarchy
//--------------------------------------------------------------------------------------
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame )
{
    LPD3DXMESHCONTAINER pMeshContainer;

    pMeshContainer = pFrame->pMeshContainer;
    while( pMeshContainer != NULL )
    {
        DrawMeshContainer( pd3dDevice, pMeshContainer, pFrame );

        pMeshContainer = pMeshContainer->pNextMeshContainer;
    }

    if( pFrame->pFrameSibling != NULL )
    {
        DrawFrame( pd3dDevice, pFrame->pFrameSibling );
    }

    if( pFrame->pFrameFirstChild != NULL )
    {
        DrawFrame( pd3dDevice, pFrame->pFrameFirstChild );
    }
}


//--------------------------------------------------------------------------------------
// Called to render a mesh in the hierarchy
//--------------------------------------------------------------------------------------
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase )
{
    HRESULT hr = S_OK;

    D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

    // If there's no skinning information just draw the mesh
    if( NULL == pMeshContainer->pSkinInfo )
    {
        for( UINT iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
        {
            V( pMeshContainer->MeshData.pMesh->DrawSubset( iMaterial ) );
        }

        return;
    }

    LPD3DXBONECOMBINATION pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>(
        pMeshContainer->pBoneCombinationBuf->GetBufferPointer() );
    for( UINT iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
    {
        D3DXMATRIXA16 BoneMatrices[MAX_BONES];

        // first calculate all the world matrices
        for( UINT iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
        {

            UINT iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
            if( iMatrixIndex != UINT_MAX )
                D3DXMatrixMultiply( &BoneMatrices[iPaletteEntry], &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
                                    pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
        }

        V( g_pEffect->SetMatrixArray( "g_mWorldMatrixArray", BoneMatrices, pMeshContainer->NumPaletteEntries ) );

        // Set CurNumBones to select the correct vertex shader for the number of bones
        V( g_pEffect->SetInt( "g_NumBones", pMeshContainer->NumInfl - 1 ) );

        // Start the effect now all parameters have been updated
        UINT numPasses;
        V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
        for( UINT iPass = 0; iPass < numPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );

            // Draw the subset with the current world matrix palette and material state
            V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );

            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );
    }
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 8 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 7 );
        txtHelper.DrawTextLine( L"Rotate Model: Left Mouse" );
        txtHelper.DrawTextLine( L"Rotate Light: Middle Mouse" );
        txtHelper.DrawTextLine( L"Rotate Camera and Model: Right Mouse" );
        txtHelper.DrawTextLine( L"Rotate Camera: Ctrl + Right Mouse" );
        txtHelper.DrawTextLine( L"Wireframe: W" );
        txtHelper.DrawTextLine( L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 2 );
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
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

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
                g_bShowHelp = !g_bShowHelp;
                break;

            case 'W':
                g_bWireFrame = !g_bWireFrame;
                break;
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
        case IDC_TECHNIQUE:
            LoadTechniqueObjects( "bat" ); break;
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

    g_LightControl.StaticOnD3D9LostDevice();
    g_Skybox.OnLostDevice();

    SAFE_RELEASE( g_pTextSprite );
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

    g_LightControl.StaticOnD3D9DestroyDevice();
    g_Skybox.OnDestroyDevice();

    if( g_pFrameRoot )
    {
        CAllocateHierarchy Alloc;
        D3DXFrameDestroy( g_pFrameRoot, &Alloc );
        g_pFrameRoot = NULL;
    }

    SAFE_RELEASE( g_pAnimController );
}



