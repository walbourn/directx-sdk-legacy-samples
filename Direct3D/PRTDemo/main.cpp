//--------------------------------------------------------------------------------------
// File: main.cpp
//
// This sample demonstrates how use D3DXSHPRTSimulation(), a per vertex  
// precomputed radiance transfer (PRT) simulator that uses low-order 
// spherical harmonics (SH) and records the results to a file. The sample 
// then demonstrates how compress the results with clustered principal 
// component analysis (CPCA) and view the compressed results with arbitrary 
// lighting in real time with a vs_1_1 vertex shader
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include <msxml.h>
#include "PRTMesh.h"
#include "PRTSimulator.h"
#include "PRTOptionsDlg.h"
#include "lightprobe.h"
#include "SHFuncView.h"
#include "resource.h"
#include <shlobj.h>

// Enable extra D3D debugging in debug builds.  This makes D3D objects work well
// in the debugger watch window, but slows down performance slightly.
#if defined(DEBUG) | defined(_DEBUG)
#define D3D_DEBUG_INFO
#endif

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

// Standard stuff
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
CModelViewerCamera          g_Camera;               // A model viewing camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
WCHAR g_strLastError[MAX_PATH] = {0};

// PRT simulator
CPRTSimulator               g_Simulator;

// Mesh
CPRTMesh                    g_PRTMesh;

// UI
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_StartUpUI;            // dialog for startup
CDXUTDialog                 g_StartUpUI2;           // dialog for startup
CDXUTDialog                 g_SimulatorRunningUI;   // dialog for while simulator running
CDXUTDialog                 g_RenderingUI;          // dialog for while main render
CDXUTDialog                 g_RenderingUI2;         // dialog for while main render
CDXUTDialog                 g_RenderingUI3;         // dialog for while main render
CDXUTDialog                 g_CompressionUI;        // dialog for PRT compression settings
bool                        g_bRenderEnvMap = true;
bool                        g_bRenderUI = true;
bool                        g_bRenderArrows = true;
bool                        g_bRenderMesh = true;
bool                        g_bRenderText = true;
bool                        g_bRenderWithAlbedoTexture = true;
bool                        g_bRenderCompressionUI = false;
bool                        g_bRenderSHVisual = false;
bool                        g_bRenderSHProjection = false;
bool                        g_bRenderWireframe = false;

// Misc
float                       g_fCurObjectRadius = -1.0f;
float                       g_fLightScaleForSHTechs = 0.0f;
float                       g_fLightScaleForNdotL = 1.0f;

// SH transfer visualization
CSHFunctionViewer           g_SHFuncViewer;

// Light probe
#define NUM_SKY_BOXES 5  // Change this to 1 to make the sample load quickly
CLightProbe g_LightProbe[NUM_SKY_BOXES];
int                         g_dwLightProbeA = ( 0 % NUM_SKY_BOXES );
int                         g_dwLightProbeB = ( 2 % NUM_SKY_BOXES );
LPCWSTR g_LightProbeFiles[] =
{
    L"Light Probes\\rnl_cross.dds",
    L"Light Probes\\uffizi_cross.dds",
    L"Light Probes\\galileo_cross.dds",
    L"Light Probes\\grace_cross.dds",
    L"Light Probes\\stpeters_cross.dds"
};

WCHAR g_szAppFolder[] = L"\\PRT Demo\\";

// Directional lights 
#define MAX_LIGHTS 10
CDXUTDirectionWidget g_LightControl[MAX_LIGHTS];
int                         g_nNumActiveLights;
int                         g_nActiveLight;

// App technique
enum APP_TECHNIQUE
{
    APP_TECH_NDOTL = 0,
    APP_TECH_SHIRRADENVMAP,
    APP_TECH_LDPRT,
    APP_TECH_PRT,
    APP_TECH_NEAR_LOSSLESS_PRT
};

APP_TECHNIQUE               g_AppTechnique = APP_TECH_PRT; // Current technique of app

// App state 
enum APP_STATE
{
    APP_STATE_STARTUP = 0,
    APP_STATE_SIMULATOR_OPTIONS,
    APP_STATE_SIMULATOR_RUNNING,
    APP_STATE_LOAD_PRT_BUFFER,
    APP_STATE_RENDER_SCENE
};
APP_STATE                   g_AppState = APP_STATE_STARTUP; // Current state of app


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC              0
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4

#define IDC_NUM_LIGHTS          6
#define IDC_NUM_LIGHTS_STATIC   7
#define IDC_ACTIVE_LIGHT        8
#define IDC_LIGHT_SCALE         9
#define IDC_LIGHT_SCALE_STATIC  10

#define IDC_LOAD_PRTBUFFER      12
#define IDC_SIMULATOR           13
#define IDC_STOP_SIMULATOR      14

#define IDC_ENVIRONMENT_1_SCALER 15
#define IDC_ENVIRONMENT_2_SCALER 16
#define IDC_DIRECTIONAL_SCALER   17
#define IDC_ENVIRONMENT_BLEND_SCALER 18
#define IDC_ENVIRONMENT_A        19
#define IDC_ENVIRONMENT_B        20

#define IDC_RENDER_UI            21
#define IDC_RENDER_MAP           22
#define IDC_RENDER_ARROWS        23
#define IDC_RENDER_MESH          24
#define IDC_SIM_STATUS           25
#define IDC_SIM_STATUS_2         26

#define IDC_NUM_PCA              27
#define IDC_NUM_PCA_TEXT         28
#define IDC_NUM_CLUSTERS         29
#define IDC_NUM_CLUSTERS_TEXT    30
#define IDC_MAX_CONSTANTS        31
#define IDC_CUR_CONSTANTS        32
#define IDC_CUR_CONSTANTS_STATIC 34

#define IDC_APPLY                33
#define IDC_LIGHT_ANGLE_STATIC   35
#define IDC_LIGHT_ANGLE          36
#define IDC_RENDER_TEXTURE       37
#define IDC_WIREFRAME            38
#define IDC_RESTART              40
#define IDC_COMPRESSION          41
#define IDC_SH_PROJECTION        42
#define IDC_TRANSFER_VISUAL      52

#define IDC_TECHNIQUE_PRT        43
#define IDC_TECHNIQUE_SHIRRAD    44
#define IDC_TECHNIQUE_NDOTL      45
#define IDC_TECHNIQUE_LDPRT      51

#define IDC_SCENE_1              46
#define IDC_SCENE_2              47
#define IDC_SCENE_3              48
#define IDC_SCENE_4              49
#define IDC_SCENE_5              50

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
void RenderText();
void UpdateLightingEnvironment();
void UpdateConstText();
void ResetUI();
void LoadScene( IDirect3DDevice9* pd3dDevice, WCHAR* strInputMesh, WCHAR* strResultsFile );
void LoadSceneAndOptGenResults( IDirect3DDevice9* pd3dDevice, WCHAR* strInputMesh, WCHAR* strResultsFile, int nNumRays,
                                int nNumBounces, bool bSubSurface );
void GetSupportedTextureFormat( IDirect3D9* pD3D, D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT* pfmtTexture,
                                D3DFORMAT* pfmtCubeMap );
HRESULT FindPRTMediaFile( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename, bool bCreatePath=false );

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
    DXUTCreateWindow( L"PRTDemo" );
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
    for( int i = 0; i < MAX_LIGHTS; i++ )
    {
        g_LightControl[i].SetLightDirection( D3DXVECTOR3( sinf( D3DX_PI * 2 * i / MAX_LIGHTS - D3DX_PI / 6 ),
                                                          0, -cosf( D3DX_PI * 2 * i / MAX_LIGHTS - D3DX_PI / 6 ) ) );
        g_LightControl[i].SetButtonMask( MOUSE_MIDDLE_BUTTON );
    }

    g_nActiveLight = 0;
    g_nNumActiveLights = 1;

    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_StartUpUI.Init( &g_DialogResourceManager );
    g_StartUpUI2.Init( &g_DialogResourceManager );
    g_SimulatorRunningUI.Init( &g_DialogResourceManager );
    g_RenderingUI.Init( &g_DialogResourceManager );
    g_RenderingUI2.Init( &g_DialogResourceManager );
    g_RenderingUI3.Init( &g_DialogResourceManager );
    g_CompressionUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    // Title font for comboboxes
    g_StartUpUI.SetFont( 1, L"Arial", 24, FW_BOLD );
    CDXUTElement* pElement = g_StartUpUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_CENTER | DT_BOTTOM;
    }

    g_StartUpUI.SetCallback( OnGUIEvent ); iY = 10;
    g_StartUpUI.AddStatic( -1, L"What would you like to do?", 0, 10, 300, 22 );
    g_StartUpUI.AddButton( IDC_SIMULATOR, L"Run (P)RT simulator", 90, 42, 125, 40, L'P' );
    g_StartUpUI.AddButton( IDC_LOAD_PRTBUFFER, L"View saved (r)esults", 90, 84, 125, 40, L'R' );

    g_StartUpUI2.SetCallback( OnGUIEvent ); iY = 10;
    g_StartUpUI2.AddButton( IDC_SCENE_1, L"Demo Scene 1 (Z)", 0, iY += 24, 125, 22 );
    g_StartUpUI2.AddButton( IDC_SCENE_2, L"Demo Scene 2 (X)", 0, iY += 24, 125, 22 );
    g_StartUpUI2.AddButton( IDC_SCENE_3, L"Demo Scene 3 (C)", 0, iY += 24, 125, 22 );
    g_StartUpUI2.AddButton( IDC_SCENE_4, L"Demo Scene 4 (V)", 0, iY += 24, 125, 22 );
    g_StartUpUI2.AddButton( IDC_SCENE_5, L"Demo Scene 5 (B)", 0, iY += 24, 125, 22 );

    g_SimulatorRunningUI.SetFont( 1, L"Arial", 24, FW_BOLD );
    pElement = g_SimulatorRunningUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_CENTER | DT_BOTTOM;
    }

    g_SimulatorRunningUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SimulatorRunningUI.AddStatic( IDC_SIM_STATUS, L"", 0, 0, 600, 22 );
    g_SimulatorRunningUI.AddStatic( IDC_SIM_STATUS_2, L"", 0, 30, 600, 22 );
    g_SimulatorRunningUI.AddButton( IDC_STOP_SIMULATOR, L"Stop PRT simulator", 240, 60, 125, 40 );

    // Title font for comboboxes
    g_RenderingUI2.SetFont( 1, L"Arial", 14, FW_BOLD );
    pElement = g_RenderingUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    g_RenderingUI2.SetCallback( OnGUIEvent ); iY = 10;
    g_RenderingUI2.AddCheckBox( IDC_RENDER_UI, L"Show UI (U)", 35, iY += 24, 125, 22, g_bRenderUI, 'U' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_MAP, L"Background (5)", 35, iY += 24, 125, 22, g_bRenderEnvMap, '5' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_ARROWS, L"Arrows (6)", 35, iY += 24, 125, 22, g_bRenderArrows, '6' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_MESH, L"Mesh (7)", 35, iY += 24, 125, 22, g_bRenderMesh, '7' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_TEXTURE, L"Texture (8)", 35, iY += 24, 125, 22, g_bRenderWithAlbedoTexture,
                                '8' );
    g_RenderingUI2.AddCheckBox( IDC_WIREFRAME, L"Wireframe (W)", 35, iY += 24, 125, 22, g_bRenderWireframe );
    g_RenderingUI2.AddCheckBox( IDC_SH_PROJECTION, L"SH Projection (H)", 35, iY += 24, 125, 22, g_bRenderSHProjection,
                                'H' );

    WCHAR sz[100];

    iY += 10;
    swprintf_s( sz, 100, L"Light scale: %0.2f", 0.0f ); sz[99] = 0;
    g_RenderingUI2.AddStatic( IDC_LIGHT_SCALE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_RenderingUI2.AddSlider( IDC_LIGHT_SCALE, 50, iY += 24, 100, 22, 0,
                              200, ( int )( g_fLightScaleForSHTechs * 100.0f ) );

    swprintf_s( sz, 100, L"# Lights: %d", g_nNumActiveLights ); sz[99] = 0;
    g_RenderingUI2.AddStatic( IDC_NUM_LIGHTS_STATIC, sz, 35, iY += 24, 125, 22 );
    g_RenderingUI2.AddSlider( IDC_NUM_LIGHTS, 50, iY += 24, 100, 22, 1, MAX_LIGHTS, g_nNumActiveLights );

    swprintf_s( sz, 100, L"Cone Angle: 1" ); sz[99] = 0;
    g_RenderingUI2.AddStatic( IDC_LIGHT_ANGLE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_RenderingUI2.AddSlider( IDC_LIGHT_ANGLE, 50, iY += 24, 100, 22, 1, 180, 1 );

    iY += 5;
    g_RenderingUI2.AddButton( IDC_ACTIVE_LIGHT, L"Change active light (K)", 35, iY += 24, 125, 22, 'K' );

    iY += 5;
    g_RenderingUI2.AddButton( IDC_COMPRESSION, L"Compression Settings", 35, iY += 24, 125, 22 );

    iY += 5;
    g_RenderingUI2.AddButton( IDC_RESTART, L"Restart", 35, iY += 24, 125, 22 );

    bool bEnable = ( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() != 0 );
    g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->SetEnabled( bEnable );
    g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->SetEnabled( bEnable );
    g_RenderingUI2.GetStatic( IDC_LIGHT_ANGLE_STATIC )->SetEnabled( bEnable );
    g_RenderingUI2.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetEnabled( bEnable );
    g_RenderingUI2.GetButton( IDC_ACTIVE_LIGHT )->SetEnabled( bEnable );

    // Title font for comboboxes
    g_RenderingUI3.SetFont( 1, L"Arial", 14, FW_BOLD );
    pElement = g_RenderingUI3.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    g_RenderingUI3.SetCallback( OnGUIEvent ); iY = 0;
    g_RenderingUI3.AddStatic( IDC_STATIC, L"(T)echnique", 0, iY, 150, 25 );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_NDOTL, 1, L"(1) N dot L", 0, iY += 24, 150, 22, false, '1' );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_SHIRRAD, 1, L"(2) SHIrradEnvMap", 0, iY += 24, 150, 22, false, '2' );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_LDPRT, 1, L"(3) LDPRT", 0, iY += 24, 150, 22, false, '3' );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_PRT, 1, L"(4) PRT", 0, iY += 24, 150, 22, true, '4' );
    // Title font for comboboxes
    g_RenderingUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    pElement = g_RenderingUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    g_RenderingUI.SetCallback( OnGUIEvent );

    int iStartY = 0;
    int iX = 10;
    iY = iStartY;
    g_RenderingUI.AddStatic( IDC_STATIC, L"(F)irst Light Probe", iX, iY += 24, 150, 25 );
    g_RenderingUI.AddComboBox( IDC_ENVIRONMENT_A, iX, iY += 24, 150, 22, 'F' );
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->SetDropHeight( 30 );
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->AddItem( L"Eucalyptus Grove", ( void* )0 );
#if NUM_SKY_BOXES > 1
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->AddItem( L"The Uffizi Gallery", ( void* )1 );
#endif
#if NUM_SKY_BOXES > 2
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->AddItem( L"Galileo's Tomb", ( void* )2 );
#endif
#if NUM_SKY_BOXES > 3
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->AddItem( L"Grace Cathedral", ( void* )3 );
#endif
#if NUM_SKY_BOXES > 4
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->AddItem( L"St. Peter's Basilica", ( void* )4 );
#endif
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->SetSelectedByData( IntToPtr( g_dwLightProbeA ) );

    g_RenderingUI.AddStatic( IDC_STATIC, L"First Light Probe Scaler", iX, iY += 24, 150, 25 );
    g_RenderingUI.AddSlider( IDC_ENVIRONMENT_1_SCALER, iX, iY += 24, 150, 22, 0, 100, 50 );

    iX += 175;
    iY = iStartY + 30;
    g_RenderingUI.AddStatic( IDC_STATIC, L"Light Probe Blend", iX, iY += 24, 100, 25 );
    g_RenderingUI.AddSlider( IDC_ENVIRONMENT_BLEND_SCALER, iX, iY += 24, 100, 22, 0, 100, 0 );

    iX += 125;
    iY = iStartY;
    g_RenderingUI.AddStatic( IDC_STATIC, L"(S)econd Light Probe", iX, iY += 24, 150, 25 );
    g_RenderingUI.AddComboBox( IDC_ENVIRONMENT_B, iX, iY += 24, 150, 22, L'S' );
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->SetDropHeight( 30 );
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->AddItem( L"Eucalyptus Grove", ( void* )0 );
#if NUM_SKY_BOXES > 1
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->AddItem( L"The Uffizi Gallery", ( void* )1 );
#endif
#if NUM_SKY_BOXES > 2
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->AddItem( L"Galileo's Tomb", ( void* )2 );
#endif
#if NUM_SKY_BOXES > 3
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->AddItem( L"Grace Cathedral", ( void* )3 );
#endif
#if NUM_SKY_BOXES > 4
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->AddItem( L"St. Peter's Basilica", ( void* )4 );
#endif
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->SetSelectedByData( IntToPtr( g_dwLightProbeB ) );

    g_RenderingUI.AddStatic( IDC_STATIC, L"Second Light Probe Scaler", iX, iY += 24, 150, 25 );
    g_RenderingUI.AddSlider( IDC_ENVIRONMENT_2_SCALER, iX, iY += 24, 150, 22, 0, 100, 50 );

    iX += 175;
    iY = iStartY + 30;
    g_RenderingUI.AddCheckBox( IDC_TRANSFER_VISUAL, L"Visualize Transfer (T)", iX, iY += 20, 150, 22,
                               g_bRenderSHVisual, 'T' );
    g_RenderingUI.AddStatic( IDC_STATIC, L"Use 'D' or drag middle mouse", iX, iY += 18, 190, 25 );
    g_RenderingUI.AddStatic( IDC_STATIC, L"button to pick vertex", iX, iY += 18, 190, 25 );

    g_CompressionUI.SetCallback( OnGUIEvent ); iY = 10;

    SIMULATOR_OPTIONS& options = GetGlobalOptions();

    g_CompressionUI.AddStatic( IDC_NUM_PCA_TEXT, L"Number of PCA: 24", 30, iY += 24, 120, 22 );
    g_CompressionUI.AddSlider( IDC_NUM_PCA, 30, iY += 24, 120, 22, 1, 6, options.dwNumPCA / 4 );
    g_CompressionUI.AddStatic( IDC_NUM_CLUSTERS_TEXT, L"Number of Clusters: 1", 30, iY += 24, 120, 22 );
    g_CompressionUI.AddSlider( IDC_NUM_CLUSTERS, 30, iY += 24, 120, 22, 1, 40, options.dwNumClusters );
    g_CompressionUI.AddStatic( IDC_MAX_CONSTANTS, L"Max VS Constants: 0", 30, iY += 24, 120, 22 );
    g_CompressionUI.AddStatic( IDC_CUR_CONSTANTS_STATIC, L"Cur VS Constants:", 30, iY += 24, 120, 22 );
    g_CompressionUI.AddStatic( IDC_CUR_CONSTANTS, L"0", -10, iY += 12, 200, 22 );
    g_CompressionUI.AddButton( IDC_APPLY, L"Apply", 30, iY += 24, 120, 24 );

    swprintf_s( sz, 100, L"Number of PCA: %d", options.dwNumPCA );
    g_CompressionUI.GetStatic( IDC_NUM_PCA_TEXT )->SetText( sz );
    swprintf_s( sz, 100, L"Number of Clusters: %d", options.dwNumClusters );
    g_CompressionUI.GetStatic( IDC_NUM_CLUSTERS_TEXT )->SetText( sz );
    UpdateConstText();

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
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

    // Turn vsync off
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support HW T&L or doesn't support 2.0 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // The PRT simulator runs on a seperate thread.  If you are just 
    // loading simulator results, then this isn't needed
    pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_MULTITHREADED;

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
    // Determine which LDPRT texture and SH coefficient cubemap formats are supported
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 Caps;
    pd3dDevice->GetDeviceCaps( &Caps );
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );

    D3DFORMAT fmtTexture, fmtCubeMap;
    GetSupportedTextureFormat( pD3D, &Caps, DisplayMode.Format, &fmtTexture, &fmtCubeMap );
    if( D3DFMT_UNKNOWN == fmtTexture || D3DFMT_UNKNOWN == fmtCubeMap )
        return E_FAIL;

    V_RETURN( g_PRTMesh.OnCreateDevice( pd3dDevice, fmtCubeMap ) );
    V_RETURN( g_SHFuncViewer.OnCreateDevice( pd3dDevice ) );

    // Load only the lightprobes that are needed.  The rest are loaded when needed
    g_LightProbe[g_dwLightProbeA].OnCreateDevice( DXUTGetD3D9Device(), g_LightProbeFiles[g_dwLightProbeA], true );
    g_LightProbe[g_dwLightProbeB].OnCreateDevice( DXUTGetD3D9Device(), g_LightProbeFiles[g_dwLightProbeB], true );

    g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, false );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );
    V_RETURN( CDXUTDirectionWidget::StaticOnD3D9CreateDevice( pd3dDevice ) );

    DWORD dwMaxVertexShaderConst = DXUTGetD3D9DeviceCaps()->MaxVertexShaderConst;
    WCHAR sz[256];
    int nMax = ( dwMaxVertexShaderConst - 4 ) / 2;
    g_CompressionUI.GetSlider( IDC_NUM_CLUSTERS )->SetRange( 1, nMax );

    swprintf_s( sz, 256, L"Max VS Constants: %d", dwMaxVertexShaderConst );
    g_CompressionUI.GetStatic( IDC_MAX_CONSTANTS )->SetText( sz );
    UpdateConstText();

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

    for( int i = 0; i < NUM_SKY_BOXES; ++i )
        g_LightProbe[i].OnResetDevice( pBackBufferSurfaceDesc );

    V( g_PRTMesh.OnResetDevice() );
    V( g_SHFuncViewer.OnResetDevice() );

    if( g_pFont )
        V( g_pFont->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    for( int i = 0; i < MAX_LIGHTS; i++ )
        g_LightControl[i].OnD3D9ResetDevice( pBackBufferSurfaceDesc );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.2f, 10000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON );
    g_Camera.SetAttachCameraToModel( true );

    g_SHFuncViewer.SetAspectRatio( fAspectRatio );

    if( g_PRTMesh.GetMesh() != NULL )
    {
        // Update camera's viewing radius based on the object radius
        float fObjectRadius = g_PRTMesh.GetObjectRadius();
        if( g_fCurObjectRadius != fObjectRadius )
        {
            g_fCurObjectRadius = fObjectRadius;
            g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 1.2f, fObjectRadius * 20.0f );
        }
        g_Camera.SetModelCenter( g_PRTMesh.GetObjectCenter() );
        for( int i = 0; i < MAX_LIGHTS; i++ )
            g_LightControl[i].SetRadius( fObjectRadius );
    }

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 100 );
    g_StartUpUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 300 ) / 2,
                             ( pBackBufferSurfaceDesc->Height - 200 ) / 2 );
    g_StartUpUI.SetSize( 300, 200 );
    g_StartUpUI2.SetLocation( 50, ( pBackBufferSurfaceDesc->Height - 200 ) );
    g_StartUpUI2.SetSize( 300, 200 );
    g_CompressionUI.SetLocation( 0, 150 );
    g_CompressionUI.SetSize( 200, 200 );
    g_SimulatorRunningUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 600 ) / 2,
                                      ( pBackBufferSurfaceDesc->Height - 100 ) / 2 );
    g_SimulatorRunningUI.SetSize( 600, 100 );
    g_RenderingUI.SetLocation( 0, pBackBufferSurfaceDesc->Height - 125 );
    g_RenderingUI.SetSize( pBackBufferSurfaceDesc->Width, 125 );
    g_RenderingUI2.SetLocation( pBackBufferSurfaceDesc->Width - 170, 100 );
    g_RenderingUI2.SetSize( 170, 400 );
    g_RenderingUI3.SetLocation( 10, 30 );
    g_RenderingUI3.SetSize( 200, 100 );

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
}


//--------------------------------------------------------------------------------------
void RenderStartup( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        RenderText();
        if( g_bRenderUI )
        {
            D3DXMATRIXA16 mWorld;
            D3DXMATRIXA16 mView;
            D3DXMATRIXA16 mProj;
            D3DXMATRIXA16 mWorldViewProjection;
            D3DXMATRIXA16 mViewProjection;
            mWorld = *g_Camera.GetWorldMatrix();
            mProj = *g_Camera.GetProjMatrix();
            mView = *g_Camera.GetViewMatrix();
            mViewProjection = mView * mProj;

            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_StartUpUI.OnRender( fElapsedTime ) );
            V( g_StartUpUI2.OnRender( fElapsedTime ) );
        }

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
void RenderSimulatorRunning( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        RenderText();
        V( g_SimulatorRunningUI.OnRender( fElapsedTime ) );

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
void RenderPRT( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mViewProjection;

    pd3dDevice->SetRenderState( D3DRS_FILLMODE, ( g_bRenderWireframe ) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        mWorldViewProjection = mWorld * mView * mProj;

        if( g_bRenderEnvMap && !g_bRenderWireframe )
        {
            float fEnv1Scaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_1_SCALER )->GetValue() / 100.0f );
            float fEnv2Scaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_2_SCALER )->GetValue() / 100.0f );

            float fEnvBlendScaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_BLEND_SCALER )->GetValue() / 100.0f );
            pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
            pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

            // Prevent projection zooming in light probe
            D3DXMATRIX mView2 = mView;
            mView2._43 = 3000.0f;
            mViewProjection = mView2 * mProj;

            g_LightProbe[g_dwLightProbeA].Render( &mViewProjection, ( 1.0f - fEnvBlendScaler ), fEnv1Scaler,
                                                  g_bRenderSHProjection );
            g_LightProbe[g_dwLightProbeB].Render( &mViewProjection, ( fEnvBlendScaler ), fEnv2Scaler,
                                                  g_bRenderSHProjection );

            pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        }

        float fLightScale = ( float )( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.01f );
        if( g_bRenderArrows && fLightScale > 0.0f )
        {
            // Render the light spheres so the user can 
            // visually see the light dir
            for( int i = 0; i < g_nNumActiveLights; i++ )
            {
                D3DXCOLOR arrowColor = ( i == g_nActiveLight ) ? D3DXCOLOR( 1, 1, 0, 1 ) : D3DXCOLOR( 1, 1, 1, 1 );
                V( g_LightControl[i].OnRender9( arrowColor, &mView, &mProj, g_Camera.GetEyePt() ) );
            }
        }

        if( g_bRenderMesh )
        {
            switch( g_AppTechnique )
            {
                case APP_TECH_NEAR_LOSSLESS_PRT:
                case APP_TECH_PRT:
                    g_PRTMesh.RenderWithPRT( pd3dDevice, &mWorldViewProjection, g_bRenderWithAlbedoTexture );
                    break;

                case APP_TECH_SHIRRADENVMAP:
                    g_PRTMesh.RenderWithSHIrradEnvMap( pd3dDevice, &mWorldViewProjection, g_bRenderWithAlbedoTexture );
                    break;

                case APP_TECH_NDOTL:
                {
                    D3DXMATRIX mWorldInv;
                    D3DXMatrixInverse( &mWorldInv, NULL, g_Camera.GetWorldMatrix() );
                    g_PRTMesh.RenderWithNdotL( pd3dDevice, &mWorldViewProjection, &mWorldInv,
                                               g_bRenderWithAlbedoTexture, g_LightControl, g_nNumActiveLights,
                                               fLightScale );
                    break;
                }

                case APP_TECH_LDPRT:
                {
                    g_PRTMesh.RenderWithLDPRT( pd3dDevice, &mWorldViewProjection, NULL, false,
                                               g_bRenderWithAlbedoTexture );
                    break;
                }
            }

            if( g_bRenderSHVisual && g_AppTechnique != APP_TECH_NDOTL ) // show visualization of transfer function
            {
                g_SHFuncViewer.Render( pd3dDevice, g_PRTMesh.GetObjectRadius(), mWorld, mView, mProj );
            }
        }

        RenderText();
        if( g_bRenderUI )
        {
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_RenderingUI.OnRender( fElapsedTime ) );
            V( g_RenderingUI2.OnRender( fElapsedTime ) );
            V( g_RenderingUI3.OnRender( fElapsedTime ) );
            if( g_bRenderCompressionUI )
                V( g_CompressionUI.OnRender( fElapsedTime ) );
        }

        V( pd3dDevice->EndScene() );
    }
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

    switch( g_AppState )
    {
        case APP_STATE_STARTUP:
            RenderStartup( pd3dDevice, fTime, fElapsedTime );
            break;

        case APP_STATE_SIMULATOR_OPTIONS:
        {
            CPRTOptionsDlg dlg;
            bool bFullscreenToggle = false;
            if( !DXUTIsWindowed() )
            {
                if( SUCCEEDED( DXUTToggleFullScreen() ) )
                    bFullscreenToggle = true;
            }

            bool bResult = dlg.Show();
            if( bFullscreenToggle )
                DXUTToggleFullScreen();

            if( bResult )
            {
                SIMULATOR_OPTIONS* pOptions = dlg.GetOptions();
                g_PRTMesh.LoadMesh( pd3dDevice, pOptions->strInputMesh );

                // Update camera's viewing radius based on the object radius
                float fObjectRadius = g_PRTMesh.GetObjectRadius();
                if( g_fCurObjectRadius != fObjectRadius )
                {
                    g_fCurObjectRadius = fObjectRadius;
                    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 1.2f, fObjectRadius * 20.0f );
                }
                g_Camera.SetModelCenter( g_PRTMesh.GetObjectCenter() );
                for( int i = 0; i < MAX_LIGHTS; i++ )
                    g_LightControl[i].SetRadius( fObjectRadius );

                g_Simulator.RunInWorkerThread( pd3dDevice, pOptions, &g_PRTMesh );
                g_AppState = APP_STATE_SIMULATOR_RUNNING;
            }
            else
            {
                g_AppState = APP_STATE_STARTUP;
            }
            break;
        }

        case APP_STATE_SIMULATOR_RUNNING:
        {
            WCHAR sz[256];
            if( g_Simulator.GetPercentComplete() >= 0.0f )
                swprintf_s( sz, 256, L"Step %d of %d: %0.1f%% done", g_Simulator.GetCurrentPass(),
                                 g_Simulator.GetNumPasses(), g_Simulator.GetPercentComplete() );
            else
                swprintf_s( sz, 256, L"Step %d of %d (progress n/a)", g_Simulator.GetCurrentPass(),
                                 g_Simulator.GetNumPasses() );
            g_SimulatorRunningUI.GetStatic( IDC_SIM_STATUS )->SetText( sz );
            g_SimulatorRunningUI.GetStatic( IDC_SIM_STATUS_2 )->SetText( g_Simulator.GetCurrentPassName() );

            RenderSimulatorRunning( pd3dDevice, fTime, fElapsedTime );
            Sleep( 50 ); // Yield time to simulator thread

            if( !g_Simulator.IsRunning() )
            {
                g_PRTMesh.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
                g_SHFuncViewer.ClearActiveVertex( &g_PRTMesh, ( int )g_AppTechnique );
                DXUTSetMultimonSettings( true );
                g_AppState = APP_STATE_RENDER_SCENE;
                g_bRenderCompressionUI = false;
            }
            break;
        }

        case APP_STATE_LOAD_PRT_BUFFER:
        {
            CPRTLoadDlg dlg;
            bool bFullscreenToggle = false;
            if( !DXUTIsWindowed() )
            {
                if( SUCCEEDED( DXUTToggleFullScreen() ) )
                    bFullscreenToggle = true;
            }

            bool bResult = dlg.Show();
            if( bFullscreenToggle )
                DXUTToggleFullScreen();

            if( bResult )
            {
                SIMULATOR_OPTIONS* pOptions = dlg.GetOptions();
                if( !pOptions->bAdaptive )
                    LoadScene( pd3dDevice, pOptions->strInputMesh, pOptions->strResultsFile );
                else
                    LoadScene( pd3dDevice, pOptions->strOutputMesh, pOptions->strResultsFile );
            }
            else
            {
                g_AppState = APP_STATE_STARTUP;
            }
            break;
        }

        case APP_STATE_RENDER_SCENE:
            UpdateLightingEnvironment();
            RenderPRT( pd3dDevice, fTime, fElapsedTime );
            break;

        default:
            assert( false );
            break;
    }
}


//--------------------------------------------------------------------------------------
void UpdateLightingEnvironment()
{
    HRESULT hr;

    float fEnv1Scaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_1_SCALER )->GetValue() / 100.0f );
    float fEnv2Scaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_2_SCALER )->GetValue() / 100.0f );
    float fEnvBlendScaler = ( g_RenderingUI.GetSlider( IDC_ENVIRONMENT_BLEND_SCALER )->GetValue() / 100.0f );
    float fLightScale = ( float )( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.01f );
    float fConeRadius = ( float )( ( D3DX_PI * ( float )g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->GetValue() ) /
                                   180.0f );

    float fLight[MAX_LIGHTS][3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];

    D3DXCOLOR lightColor( 1.0f, 1.0f, 1.0f, 1.0f );
    lightColor *= fLightScale;

    DWORD dwOrder = g_PRTMesh.GetPRTOrder();

    // Pass in the light direction, the intensity of each channel, and it returns
    // the source radiance as an array of order^2 SH coefficients for each color channel.  
    D3DXVECTOR3 lightDirObjectSpace;
    D3DXMATRIX mWorldInv;
    D3DXMatrixInverse( &mWorldInv, NULL, g_Camera.GetWorldMatrix() );

    int i;
    for( i = 0; i < g_nNumActiveLights; i++ )
    {
        // Transform the world space light dir into object space
        // Note that if there's multiple objects using PRT in the scene, then
        // for each object you need to either evaulate the lights in object space
        // evaulate the lights in world and rotate the light coefficients 
        // into object space.
        D3DXVECTOR3 vLight = g_LightControl[i].GetLightDirection();
        D3DXVec3TransformNormal( &lightDirObjectSpace, &vLight, &mWorldInv );

        // This sample uses D3DXSHEvalDirectionalLight(), but there's other 
        // types of lights provided by D3DXSHEval*.  Pass in the 
        // order of SH, color of the light, and the direction of the light 
        // in object space.
        // The output is the source radiance coefficients for the SH basis functions.  
        // There are 3 outputs, one for each channel (R,G,B). 
        // Each output is an array of m_dwOrder^2 floats.  
        V( D3DXSHEvalConeLight( dwOrder, &lightDirObjectSpace, fConeRadius,
                                lightColor.r, lightColor.g, lightColor.b,
                                fLight[i][0], fLight[i][1], fLight[i][2] ) );
    }

    float fSum[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    ZeroMemory( fSum, 3 * D3DXSH_MAXORDER * D3DXSH_MAXORDER * sizeof( float ) );

    // For multiple lights, just them sum up using D3DXSHAdd().
    for( i = 0; i < g_nNumActiveLights; i++ )
    {
        // D3DXSHAdd will add Order^2 floats.  There are 3 color channels, 
        // so call it 3 times.
        D3DXSHAdd( fSum[0], dwOrder, fSum[0], fLight[i][0] );
        D3DXSHAdd( fSum[1], dwOrder, fSum[1], fLight[i][1] );
        D3DXSHAdd( fSum[2], dwOrder, fSum[2], fLight[i][2] );
    }

    float fLightProbe1[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    float fLightProbe1Rot[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    D3DXSHScale( fLightProbe1[0], dwOrder, g_LightProbe[g_dwLightProbeA].GetSHData( 0 ), fEnv1Scaler *
                 ( 1.0f - fEnvBlendScaler ) );
    D3DXSHScale( fLightProbe1[1], dwOrder, g_LightProbe[g_dwLightProbeA].GetSHData( 1 ), fEnv1Scaler *
                 ( 1.0f - fEnvBlendScaler ) );
    D3DXSHScale( fLightProbe1[2], dwOrder, g_LightProbe[g_dwLightProbeA].GetSHData( 2 ), fEnv1Scaler *
                 ( 1.0f - fEnvBlendScaler ) );
    D3DXSHRotate( fLightProbe1Rot[0], dwOrder, &mWorldInv, fLightProbe1[0] );
    D3DXSHRotate( fLightProbe1Rot[1], dwOrder, &mWorldInv, fLightProbe1[1] );
    D3DXSHRotate( fLightProbe1Rot[2], dwOrder, &mWorldInv, fLightProbe1[2] );
    D3DXSHAdd( fSum[0], dwOrder, fSum[0], fLightProbe1Rot[0] );
    D3DXSHAdd( fSum[1], dwOrder, fSum[1], fLightProbe1Rot[1] );
    D3DXSHAdd( fSum[2], dwOrder, fSum[2], fLightProbe1Rot[2] );

    float fLightProbe2[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    float fLightProbe2Rot[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    D3DXSHScale( fLightProbe2[0], dwOrder, g_LightProbe[g_dwLightProbeB].GetSHData( 0 ),
                 fEnv2Scaler * fEnvBlendScaler );
    D3DXSHScale( fLightProbe2[1], dwOrder, g_LightProbe[g_dwLightProbeB].GetSHData( 1 ),
                 fEnv2Scaler * fEnvBlendScaler );
    D3DXSHScale( fLightProbe2[2], dwOrder, g_LightProbe[g_dwLightProbeB].GetSHData( 2 ),
                 fEnv2Scaler * fEnvBlendScaler );
    D3DXSHRotate( fLightProbe2Rot[0], dwOrder, &mWorldInv, fLightProbe2[0] );
    D3DXSHRotate( fLightProbe2Rot[1], dwOrder, &mWorldInv, fLightProbe2[1] );
    D3DXSHRotate( fLightProbe2Rot[2], dwOrder, &mWorldInv, fLightProbe2[2] );
    D3DXSHAdd( fSum[0], dwOrder, fSum[0], fLightProbe2Rot[0] );
    D3DXSHAdd( fSum[1], dwOrder, fSum[1], fLightProbe2Rot[1] );
    D3DXSHAdd( fSum[2], dwOrder, fSum[2], fLightProbe2Rot[2] );

    g_PRTMesh.ComputeShaderConstants( fSum[0], fSum[1], fSum[2], dwOrder * dwOrder );
    g_PRTMesh.ComputeSHIrradEnvMapConstants( fSum[0], fSum[1], fSum[2] );
    g_PRTMesh.ComputeLDPRTConstants( fSum[0], fSum[1], fSum[2], dwOrder * dwOrder );
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    if( !g_bRenderText )
        return;

    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( g_pSprite, strMsg, -1, &rc, DT_NOCLIP, g_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) ); // Show FPS
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( g_strLastError );

    if( g_AppState == APP_STATE_RENDER_SCENE )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();

        // Draw help
        if( g_bShowHelp )
        {
            txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 55 * 3 );
            txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            txtHelper.DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate camera: Ctrl + Right mouse button\n"
                                    L"Rotate model and camera: Right Mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );
        }
        else
        {
            txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 40 * 3 );
            txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
            txtHelper.DrawTextLine( L"Press F1 for help" );
        }
    }

    if( !g_bRenderUI )
        txtHelper.DrawFormattedTextLine( L"Press 'U' to show UI" );

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


    if( g_AppState != APP_STATE_SIMULATOR_RUNNING )
    {
        // Give the dialogs a chance to handle the message first
        *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
    }

    switch( uMsg )
    {
        case WM_KEYDOWN:
        {
            if( wParam == VK_F8 )
                *pbNoFurtherProcessing = true;
            break;
        }

        case WM_MBUTTONDOWN:
        {
            KeyboardProc( 'D', true, false, NULL );
            break;
        }

        case WM_MOUSEMOVE:
        {
            if( ( wParam & MK_MBUTTON ) != 0 )
            {
                KeyboardProc( 'D', true, false, NULL );
            }
            break;
        }
    }

    switch( g_AppState )
    {
        case APP_STATE_STARTUP:
            *pbNoFurtherProcessing = g_StartUpUI.MsgProc( hWnd, uMsg, wParam, lParam );
            if( *pbNoFurtherProcessing )
                return 0;
            *pbNoFurtherProcessing = g_StartUpUI2.MsgProc( hWnd, uMsg, wParam, lParam );
            if( *pbNoFurtherProcessing )
                return 0;
            break;

        case APP_STATE_SIMULATOR_RUNNING:
            // Stop the framework from handling F2 & F3
            if( ( uMsg == WM_KEYDOWN || uMsg == WM_KEYUP ) && ( wParam == VK_F3 || wParam == VK_F2 ) )
            {
                *pbNoFurtherProcessing = TRUE;
                return 0;
            }

            *pbNoFurtherProcessing = g_SimulatorRunningUI.MsgProc( hWnd, uMsg, wParam, lParam );
            if( *pbNoFurtherProcessing )
                return 0;
            break;

        case APP_STATE_RENDER_SCENE:
            if( g_bRenderUI || uMsg == WM_KEYDOWN || uMsg == WM_KEYUP )
            {
                *pbNoFurtherProcessing = g_RenderingUI.MsgProc( hWnd, uMsg, wParam, lParam );
                if( *pbNoFurtherProcessing )
                    return 0;
                *pbNoFurtherProcessing = g_RenderingUI2.MsgProc( hWnd, uMsg, wParam, lParam );
                if( *pbNoFurtherProcessing )
                    return 0;
                *pbNoFurtherProcessing = g_RenderingUI3.MsgProc( hWnd, uMsg, wParam, lParam );
                if( *pbNoFurtherProcessing )
                    return 0;
                if( g_bRenderCompressionUI )
                {
                    *pbNoFurtherProcessing = g_CompressionUI.MsgProc( hWnd, uMsg, wParam, lParam );
                    if( *pbNoFurtherProcessing )
                        return 0;
                }
            }

            g_LightControl[g_nActiveLight].HandleMessages( hWnd, uMsg, wParam, lParam );

            // Pass all remaining windows messages to camera so it can respond to user input
            g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );
            break;

        default:
            break;
    }

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
                // Demo hotkeys
            case 'Z':
            {
                LoadSceneAndOptGenResults( DXUTGetD3D9Device(), L"PRT Demo\\cube_on_plane.x",
                                           L"PRT Demo\\cube_on_plane_1k_6b_prtresults.prt", 1024, 6, false );

                g_RenderingUI.EnableNonUserEvents( true );
                g_RenderingUI2.EnableNonUserEvents( true );
                g_RenderingUI3.EnableNonUserEvents( true );
                ResetUI();
                g_RenderingUI2.GetCheckBox( IDC_RENDER_TEXTURE )->SetChecked( false );
                g_RenderingUI.EnableNonUserEvents( false );
                g_RenderingUI2.EnableNonUserEvents( false );
                g_RenderingUI3.EnableNonUserEvents( false );
                break;
            }

            case 'X':
            {
                LoadSceneAndOptGenResults( DXUTGetD3D9Device(), L"PRT Demo\\LandShark.x",
                                           L"PRT Demo\\02_LandShark_1k_prtresults.prt", 1024, 1, false );

                g_RenderingUI.EnableNonUserEvents( true );
                g_RenderingUI2.EnableNonUserEvents( true );
                g_RenderingUI3.EnableNonUserEvents( true );
                ResetUI();
                g_RenderingUI.EnableNonUserEvents( false );
                g_RenderingUI2.EnableNonUserEvents( false );
                g_RenderingUI3.EnableNonUserEvents( false );
                break;
            }

            case 'C':
            {
                LoadSceneAndOptGenResults( DXUTGetD3D9Device(), L"PRT Demo\\wall_with_pillars.x",
                                           L"PRT Demo\\wall_with_pillars_1k_prtresults.prt", 1024, 1, false );

                g_RenderingUI.EnableNonUserEvents( true );
                g_RenderingUI2.EnableNonUserEvents( true );
                g_RenderingUI3.EnableNonUserEvents( true );
                ResetUI();
                g_RenderingUI.EnableNonUserEvents( false );
                g_RenderingUI2.EnableNonUserEvents( false );
                g_RenderingUI3.EnableNonUserEvents( false );
                break;
            }

            case 'V':
            {
                LoadSceneAndOptGenResults( DXUTGetD3D9Device(), L"PRT Demo\\Head_Sad.x",
                                           L"PRT Demo\\Head_Sad_1k_prtresults.prt", 1024, 1, false );

                g_RenderingUI.EnableNonUserEvents( true );
                g_RenderingUI2.EnableNonUserEvents( true );
                g_RenderingUI3.EnableNonUserEvents( true );
                ResetUI();
                g_RenderingUI.EnableNonUserEvents( false );
                g_RenderingUI2.EnableNonUserEvents( false );
                g_RenderingUI3.EnableNonUserEvents( false );
                break;
            }

            case 'B':
            {
                LoadSceneAndOptGenResults( DXUTGetD3D9Device(), L"PRT Demo\\Head_Big_Ears.x",
                                           L"PRT Demo\\Head_Big_Ears_1k_subsurf_prtresults.prt", 1024, 1, true );

                g_RenderingUI.EnableNonUserEvents( true );
                g_RenderingUI2.EnableNonUserEvents( true );
                g_RenderingUI3.EnableNonUserEvents( true );
                ResetUI();
                g_RenderingUI.GetSlider( IDC_ENVIRONMENT_1_SCALER )->SetValue( 0 );
                g_RenderingUI.GetSlider( IDC_ENVIRONMENT_2_SCALER )->SetValue( 0 );
                g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetRange( 0, 1000 );
                g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetValue( 200 );
                g_RenderingUI.EnableNonUserEvents( false );
                g_RenderingUI2.EnableNonUserEvents( false );
                g_RenderingUI3.EnableNonUserEvents( false );
                break;
            }

            case 'G':
            {
                if( g_AppTechnique == APP_TECH_PRT )
                    g_AppTechnique = APP_TECH_NEAR_LOSSLESS_PRT;
                else if( g_AppTechnique == APP_TECH_NEAR_LOSSLESS_PRT )
                    g_AppTechnique = APP_TECH_PRT;
                g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );
                break;
            }

            case 'D':
            {
                if( !g_bRenderSHVisual && !g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->GetEnabled() )
                {
                    g_RenderingUI.EnableNonUserEvents( true );
                    g_RenderingUI.GetCheckBox( IDC_TRANSFER_VISUAL )->SetChecked( true );
                    g_RenderingUI.EnableNonUserEvents( false );
                }

                // Don't update vertex faster than 0.1s/per to keep app responsive
                static double s_fLastTime = -1.0;
                if( fabs( DXUTGetTime() - s_fLastTime ) > 0.1f )
                {
                    s_fLastTime = DXUTGetTime();

                    const D3DXMATRIX* pmProj = g_Camera.GetProjMatrix();
                    const D3DXMATRIX* pmView = g_Camera.GetViewMatrix();
                    const D3DXMATRIX* pmWorld = g_Camera.GetWorldMatrix();
                    g_SHFuncViewer.GetVertexUnderMouse( &g_PRTMesh, ( int )g_AppTechnique, pmProj, pmView, pmWorld );
                }
                break;
            }

            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
            case VK_F8:
            case 'W':
                g_bRenderWireframe = !g_bRenderWireframe;
                g_RenderingUI2.GetCheckBox( IDC_WIREFRAME )->SetChecked( g_bRenderWireframe ); break;
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

        case IDC_SCENE_1:
            KeyboardProc( 'Z', true, false, NULL ); break;
        case IDC_SCENE_2:
            KeyboardProc( 'X', true, false, NULL ); break;
        case IDC_SCENE_3:
            KeyboardProc( 'C', true, false, NULL ); break;
        case IDC_SCENE_4:
            KeyboardProc( 'V', true, false, NULL ); break;
        case IDC_SCENE_5:
            KeyboardProc( 'B', true, false, NULL ); break;

        case IDC_ACTIVE_LIGHT:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                g_nActiveLight++;
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_NUM_LIGHTS:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                WCHAR sz[100];
                swprintf_s( sz, 100, L"# Lights: %d", g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->GetValue() );
                sz[99] = 0;
                g_RenderingUI2.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetText( sz );

                g_nNumActiveLights = g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->GetValue();
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_LIGHT_SCALE:
        {
            WCHAR sz[100];
            float fLightScale = ( float )( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.01f );
            swprintf_s( sz, 100, L"Light scale: %0.2f", fLightScale ); sz[99] = 0;
            g_RenderingUI2.GetStatic( IDC_LIGHT_SCALE_STATIC )->SetText( sz );

            bool bEnable = ( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() != 0 );
            g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->SetEnabled( bEnable );
            g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->SetEnabled( bEnable );
            g_RenderingUI2.GetStatic( IDC_LIGHT_ANGLE_STATIC )->SetEnabled( bEnable );
            g_RenderingUI2.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetEnabled( bEnable );
            g_RenderingUI2.GetButton( IDC_ACTIVE_LIGHT )->SetEnabled( bEnable );
            break;
        }

        case IDC_LIGHT_ANGLE:
        {
            WCHAR sz[100];
            int nLightAngle = g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->GetValue();
            swprintf_s( sz, 100, L"Cone Angle: %d", nLightAngle ); sz[99] = 0;
            g_RenderingUI2.GetStatic( IDC_LIGHT_ANGLE_STATIC )->SetText( sz );
            break;
        }

        case IDC_SIMULATOR:
            g_AppState = APP_STATE_SIMULATOR_OPTIONS;
            break;

        case IDC_LOAD_PRTBUFFER:
            g_AppState = APP_STATE_LOAD_PRT_BUFFER;
            break;

        case IDC_STOP_SIMULATOR:
            g_Simulator.Stop();
            if( g_Simulator.IsCompressing() )
                g_AppState = APP_STATE_SIMULATOR_RUNNING;
            else
                g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_ENVIRONMENT_A:
            g_dwLightProbeA = PtrToInt( g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->GetSelectedData() );
            if( !g_LightProbe[g_dwLightProbeA].IsCreated() && DXUTGetD3D9Device() )
            {
                g_LightProbe[g_dwLightProbeA].OnCreateDevice( DXUTGetD3D9Device(), g_LightProbeFiles[g_dwLightProbeA],
                                                              true );
                g_LightProbe[g_dwLightProbeA].OnResetDevice( DXUTGetD3D9BackBufferSurfaceDesc() );
            }
            break;

        case IDC_ENVIRONMENT_B:
            g_dwLightProbeB = PtrToInt( g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->GetSelectedData() );
            if( !g_LightProbe[g_dwLightProbeB].IsCreated() && DXUTGetD3D9Device() )
            {
                g_LightProbe[g_dwLightProbeB].OnCreateDevice( DXUTGetD3D9Device(), g_LightProbeFiles[g_dwLightProbeB],
                                                              true );
                g_LightProbe[g_dwLightProbeB].OnResetDevice( DXUTGetD3D9BackBufferSurfaceDesc() );
            }
            break;

        case IDC_TECHNIQUE_PRT:
        case IDC_TECHNIQUE_SHIRRAD:
        case IDC_TECHNIQUE_NDOTL:
        case IDC_TECHNIQUE_LDPRT:
            {
                float fCurLightScale = ( float )( g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.01f );

                // Save current light scale
                switch( g_AppTechnique )
                {
                    case APP_TECH_SHIRRADENVMAP:
                    case APP_TECH_LDPRT:
                    case APP_TECH_NEAR_LOSSLESS_PRT:
                    case APP_TECH_PRT:
                        g_fLightScaleForSHTechs = fCurLightScale;
                        break;
                    case APP_TECH_NDOTL:
                        g_fLightScaleForNdotL = fCurLightScale;
                        break;
                }

                // Switch to new technique
                switch( nControlID )
                {
                    case IDC_TECHNIQUE_NDOTL:
                        g_AppTechnique = APP_TECH_NDOTL; break;
                    case IDC_TECHNIQUE_SHIRRAD:
                        g_AppTechnique = APP_TECH_SHIRRADENVMAP; break;
                    case IDC_TECHNIQUE_LDPRT:
                        g_AppTechnique = APP_TECH_LDPRT; break;
                    case IDC_TECHNIQUE_PRT:
                        g_AppTechnique = APP_TECH_PRT; break;
                }

                if( g_bRenderSHVisual )
                    g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );

                g_bRenderEnvMap = ( g_AppTechnique == APP_TECH_NDOTL ) ? false : true;

                g_RenderingUI2.GetCheckBox( IDC_RENDER_MAP )->SetChecked( true );

                g_RenderingUI2.EnableNonUserEvents( true );
                float fOldLightScale = ( ( g_AppTechnique == APP_TECH_NDOTL ) ? g_fLightScaleForNdotL :
                                         g_fLightScaleForSHTechs );
                g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetValue( ( int )( fOldLightScale * 100.0f ) );
                g_RenderingUI2.EnableNonUserEvents( false );
                break;
            }

        case IDC_COMPRESSION:
            if( g_PRTMesh.IsPRTUncompressedBufferLoaded() )
            {
                g_bRenderCompressionUI = !g_bRenderCompressionUI;

                // Limit the max number of PCA vectors to # channels * # coeffs
                ID3DXPRTBuffer* pPRTBuffer = g_PRTMesh.GetPRTBuffer();
                DWORD dwNumPCA = ( min( 24, pPRTBuffer->GetNumChannels() * pPRTBuffer->GetNumCoeffs() ) / 4 ) * 4;
                g_CompressionUI.EnableNonUserEvents( true );
                g_CompressionUI.GetSlider( IDC_NUM_PCA )->SetRange( 1, dwNumPCA / 4 );
                DWORD dwCurNumPCA = dwNumPCA;
                if( g_PRTMesh.GetPRTCompBuffer() )
                    dwCurNumPCA = g_PRTMesh.GetPRTCompBuffer()->GetNumPCA();
                g_CompressionUI.GetSlider( IDC_NUM_PCA )->SetValue( dwCurNumPCA / 4 );
                g_CompressionUI.EnableNonUserEvents( false );
            }
            else
            {
                MessageBox( DXUTGetHWND(),
                            L"To change compression settings during rendering, please load an uncompressed buffer.  To make one use the simulator settings dialog to save an uncompressed buffer.", L"PRTDemo", MB_OK );
            }
            break;

        case IDC_RESTART:
            g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_RENDER_UI:
            g_bRenderUI = g_RenderingUI2.GetCheckBox( IDC_RENDER_UI )->GetChecked(); break;
        case IDC_RENDER_MAP:
            g_bRenderEnvMap = g_RenderingUI2.GetCheckBox( IDC_RENDER_MAP )->GetChecked(); break;
        case IDC_RENDER_ARROWS:
            g_bRenderArrows = g_RenderingUI2.GetCheckBox( IDC_RENDER_ARROWS )->GetChecked(); break;
        case IDC_RENDER_MESH:
            g_bRenderMesh = g_RenderingUI2.GetCheckBox( IDC_RENDER_MESH )->GetChecked(); break;
        case IDC_WIREFRAME:
            g_bRenderWireframe = g_RenderingUI2.GetCheckBox( IDC_WIREFRAME )->GetChecked(); break;
        case IDC_RENDER_TEXTURE:
            g_bRenderWithAlbedoTexture = g_RenderingUI2.GetCheckBox( IDC_RENDER_TEXTURE )->GetChecked(); break;
        case IDC_SH_PROJECTION:
            g_bRenderSHProjection = g_RenderingUI2.GetCheckBox( IDC_SH_PROJECTION )->GetChecked(); break;
        case IDC_TRANSFER_VISUAL:
        {
            g_bRenderSHVisual = g_RenderingUI.GetCheckBox( IDC_TRANSFER_VISUAL )->GetChecked();
            g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );

            for( int i = 0; i < MAX_LIGHTS; i++ )
            {
                g_LightControl[i].SetButtonMask( g_bRenderSHVisual ? 0 : MOUSE_MIDDLE_BUTTON );
            }
            break;
        }

        case IDC_NUM_PCA:
        {
            WCHAR sz[256];
            DWORD dwNumPCA = g_CompressionUI.GetSlider( IDC_NUM_PCA )->GetValue() * 4;
            swprintf_s( sz, 256, L"Number of PCA: %d", dwNumPCA );
            g_CompressionUI.GetStatic( IDC_NUM_PCA_TEXT )->SetText( sz );

            UpdateConstText();
            break;
        }

        case IDC_NUM_CLUSTERS:
        {
            WCHAR sz[256];
            DWORD dwNumClusters = g_CompressionUI.GetSlider( IDC_NUM_CLUSTERS )->GetValue();
            swprintf_s( sz, 256, L"Number of Clusters: %d", dwNumClusters );
            g_CompressionUI.GetStatic( IDC_NUM_CLUSTERS_TEXT )->SetText( sz );

            UpdateConstText();
            break;
        }

        case IDC_APPLY:
        {
            DWORD dwNumPCA = g_CompressionUI.GetSlider( IDC_NUM_PCA )->GetValue() * 4;
            DWORD dwNumClusters = g_CompressionUI.GetSlider( IDC_NUM_CLUSTERS )->GetValue();

            SIMULATOR_OPTIONS& options = GetGlobalOptions();
            options.dwNumClusters = dwNumClusters;
            options.dwNumPCA = dwNumPCA;
            GetGlobalOptionsFile().SaveOptions();

            g_Simulator.CompressInWorkerThread( DXUTGetD3D9Device(), &options, &g_PRTMesh );
            g_AppState = APP_STATE_SIMULATOR_RUNNING;
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Load a mesh and optionally generate the PRT results file if they aren't already cached
//--------------------------------------------------------------------------------------
void LoadSceneAndOptGenResults( IDirect3DDevice9* pd3dDevice, WCHAR* strInputMesh, WCHAR* strResultsFile,
                                int nNumRays, int nNumBounces, bool bSubSurface )
{
    HRESULT hr;
    WCHAR strResults[MAX_PATH];
    WCHAR strMesh[MAX_PATH];
    V( FindPRTMediaFile( strMesh, MAX_PATH, strInputMesh ) );

    hr = FindPRTMediaFile( strResults, MAX_PATH, strResultsFile );
    if( SUCCEEDED( hr ) )
    {
        LoadScene( pd3dDevice, strInputMesh, strResultsFile );
    }
    else
    {
        SIMULATOR_OPTIONS options;
        ZeroMemory( &options, sizeof( SIMULATOR_OPTIONS ) );

        wcscpy_s( options.strInputMesh, MAX_PATH, strInputMesh );
        FindPRTMediaFile( options.strInitialDir, MAX_PATH, options.strInputMesh, true );
        WCHAR* pLastSlash = wcsrchr( options.strInitialDir, L'\\' );
        if( pLastSlash )
            *pLastSlash = 0;
        pLastSlash = wcsrchr( strResultsFile, L'\\' );
        if( pLastSlash )
            wcscpy_s( options.strResultsFile, MAX_PATH, pLastSlash + 1 );
        else
            wcscpy_s( options.strResultsFile, MAX_PATH, strResultsFile );
        options.dwOrder = 6;
        options.dwNumRays = nNumRays;
        options.dwNumBounces = nNumBounces;
        options.bSubsurfaceScattering = bSubSurface;
        options.fLengthScale = 25.0f;
        options.dwNumChannels = 3;

        options.dwPredefinedMatIndex = 0;
        options.fRelativeIndexOfRefraction = 1.3f;
        options.Diffuse = D3DXCOLOR( 1.00f, 1.00f, 1.00f, 1.0f );
        options.Absorption = D3DXCOLOR( 0.0030f, 0.0030f, 0.0460f, 1.0f );
        options.ReducedScattering = D3DXCOLOR( 2.00f, 2.00f, 2.00f, 1.0f );

        options.bAdaptive = false;
        options.bRobustMeshRefine = true;
        options.fRobustMeshRefineMinEdgeLength = 0.0f;
        options.dwRobustMeshRefineMaxSubdiv = 2;
        options.bAdaptiveDL = true;
        options.fAdaptiveDLMinEdgeLength = 0.03f;
        options.fAdaptiveDLThreshold = 8e-5f;
        options.dwAdaptiveDLMaxSubdiv = 3;
        options.bAdaptiveBounce = false;
        options.fAdaptiveBounceMinEdgeLength = 0.03f;
        options.fAdaptiveBounceThreshold = 8e-5f;
        options.dwAdaptiveBounceMaxSubdiv = 3;
        wcscpy_s( options.strOutputMesh, MAX_PATH, L"shapes1_adaptive.x" );
        options.bBinaryOutputXFile = true;

        options.Quality = D3DXSHCQUAL_SLOWHIGHQUALITY;
        options.dwNumPCA = 24;
        options.dwNumClusters = 1;

        g_PRTMesh.LoadMesh( pd3dDevice, strInputMesh );

        // Update camera's viewing radius based on the object radius
        float fObjectRadius = g_PRTMesh.GetObjectRadius();
        if( g_fCurObjectRadius != fObjectRadius )
        {
            g_fCurObjectRadius = fObjectRadius;
            g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 1.2f, fObjectRadius * 20.0f );
        }
        g_Camera.SetModelCenter( g_PRTMesh.GetObjectCenter() );
        for( int i = 0; i < MAX_LIGHTS; i++ )
            g_LightControl[i].SetRadius( fObjectRadius );

        g_Simulator.RunInWorkerThread( pd3dDevice, &options, &g_PRTMesh );
        g_AppState = APP_STATE_SIMULATOR_RUNNING;
        DXUTSetMultimonSettings( false );
    }
}


//--------------------------------------------------------------------------------------
void LoadScene( IDirect3DDevice9* pd3dDevice, WCHAR* strInputMesh, WCHAR* strResultsFile )
{
    HRESULT hr;
    WCHAR strResults[MAX_PATH];
    WCHAR strMesh[MAX_PATH];

    // Clear the error string
    ZeroMemory( g_strLastError, sizeof( g_strLastError ) );

    V( FindPRTMediaFile( strMesh, MAX_PATH, strInputMesh ) );
    V( FindPRTMediaFile( strResults, MAX_PATH, strResultsFile ) );

    hr = g_PRTMesh.LoadMesh( pd3dDevice, strMesh );
    if( FAILED( hr ) )
    {
        wcscpy_s( g_strLastError, MAX_PATH, L"Error loading mesh file" );
        g_AppState = APP_STATE_STARTUP;
        return;
    }

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.Reset();
    g_Camera.SetViewQuat( D3DXQUATERNION( 0, 0, 0, 1 ) );
    g_Camera.SetWorldQuat( D3DXQUATERNION( 0, 0, 0, 1 ) );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Update camera's viewing radius based on the object radius
    float fObjectRadius = g_PRTMesh.GetObjectRadius();
    g_fCurObjectRadius = fObjectRadius;
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 1.2f, fObjectRadius * 20.0f );
    g_Camera.SetModelCenter( g_PRTMesh.GetObjectCenter() );

    for( int i = 0; i < MAX_LIGHTS; i++ )
        g_LightControl[i].SetRadius( fObjectRadius );

    WCHAR* pLastDot = wcsrchr( strResults, L'.' );

    hr = g_PRTMesh.LoadPRTBufferFromFile( strResults );
    if( FAILED( hr ) )
    {
        wcscpy_s( g_strLastError, MAX_PATH, L"Error loading results file" );
        g_AppState = APP_STATE_STARTUP;
        return;
    }

    ID3DXPRTBuffer* pPRTBuffer = g_PRTMesh.GetPRTBuffer();
    DWORD dwNumPCA = ( min( 24, pPRTBuffer->GetNumChannels() * pPRTBuffer->GetNumCoeffs() ) / 4 ) * 4;
    g_PRTMesh.CompressPRTBuffer( D3DXSHCQUAL_FASTLOWQUALITY, 1, dwNumPCA );

    WCHAR szBaseFile[MAX_PATH];
    WCHAR szLDPRTFile[MAX_PATH];
    WCHAR szNormalsFile[MAX_PATH];

    // Save the LDPRT data for future sessions
    wcscpy_s( szBaseFile, MAX_PATH, strResults );
    pLastDot = wcsrchr( szBaseFile, L'.' );
    if( pLastDot )
        *pLastDot = 0;
    wcscpy_s( szLDPRTFile, MAX_PATH, szBaseFile );
    wcscat_s( szLDPRTFile, MAX_PATH, L".ldprt" );
    wcscpy_s( szNormalsFile, MAX_PATH, szBaseFile );
    wcscat_s( szNormalsFile, MAX_PATH, L".sn" );
    if( FAILED( g_PRTMesh.LoadLDPRTFromFiles( szLDPRTFile, szNormalsFile ) ) )
    {
        hr = g_PRTMesh.CreateLDPRTData();
        if( FAILED( hr ) )
        {
            wcscpy_s( g_strLastError, MAX_PATH, L"Error creating LDPRT data" );
            g_AppState = APP_STATE_STARTUP;
            return;
        }
    }

    g_PRTMesh.ExtractCompressedDataForPRTShader();
    g_PRTMesh.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );

    g_SHFuncViewer.ClearActiveVertex( &g_PRTMesh, ( int )g_AppTechnique );

    g_AppState = APP_STATE_RENDER_SCENE;
    g_bRenderCompressionUI = false;
}

//--------------------------------------------------------------------------------------
void ResetUI()
{
    g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetRange( 0, 200 );

    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_A )->SetSelectedByData( IntToPtr( 0 ) );
    g_RenderingUI.GetComboBox( IDC_ENVIRONMENT_B )->SetSelectedByData( IntToPtr( min( 2, NUM_SKY_BOXES ) ) );
    g_RenderingUI.GetSlider( IDC_ENVIRONMENT_1_SCALER )->SetValue( 50 );
    g_RenderingUI.GetSlider( IDC_ENVIRONMENT_2_SCALER )->SetValue( 50 );
    g_RenderingUI.GetSlider( IDC_ENVIRONMENT_BLEND_SCALER )->SetValue( 0 );

    g_RenderingUI2.GetCheckBox( IDC_RENDER_UI )->SetChecked( true );
    g_RenderingUI2.GetCheckBox( IDC_RENDER_MAP )->SetChecked( true );
    g_bRenderEnvMap = true;
    g_RenderingUI2.GetCheckBox( IDC_RENDER_ARROWS )->SetChecked( true );
    g_RenderingUI2.GetCheckBox( IDC_RENDER_MESH )->SetChecked( true );
    g_RenderingUI2.GetCheckBox( IDC_RENDER_TEXTURE )->SetChecked( true );
    g_RenderingUI2.GetCheckBox( IDC_WIREFRAME )->SetChecked( false );
    g_RenderingUI2.GetCheckBox( IDC_SH_PROJECTION )->SetChecked( false );

    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_NDOTL )->SetChecked( true, true );
    g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetValue( 100 );
    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_SHIRRAD )->SetChecked( true, true );
    g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetValue( 0 );
    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_PRT )->SetChecked( true, true );
    g_RenderingUI2.GetSlider( IDC_LIGHT_SCALE )->SetValue( 0 );
    g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->SetValue( 1 );

    WCHAR sz[100];
    swprintf_s( sz, 100, L"# Lights: %d", g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->GetValue() ); sz[99] = 0;
    g_RenderingUI2.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetText( sz );
    g_nNumActiveLights = g_RenderingUI2.GetSlider( IDC_NUM_LIGHTS )->GetValue();
    g_nActiveLight %= g_nNumActiveLights;

    g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->SetValue( 1 );
    int nLightAngle = g_RenderingUI2.GetSlider( IDC_LIGHT_ANGLE )->GetValue();
    swprintf_s( sz, 100, L"Cone Angle: %d", nLightAngle ); sz[99] = 0;
    g_RenderingUI2.GetStatic( IDC_LIGHT_ANGLE_STATIC )->SetText( sz );

    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_PRT )->SetChecked( true, true );
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
    for( int i = 0; i < NUM_SKY_BOXES; ++i )
        g_LightProbe[i].OnLostDevice();
    CDXUTDirectionWidget::StaticOnD3D9LostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    g_PRTMesh.OnLostDevice();
    g_SHFuncViewer.OnLostDevice();
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
    for( int i = 0; i < NUM_SKY_BOXES; ++i )
        g_LightProbe[i].OnDestroyDevice();
    g_PRTMesh.OnDestroyDevice();
    g_SHFuncViewer.OnDestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D9DestroyDevice();
    SAFE_RELEASE( g_pFont );
    g_Simulator.Stop();
}


//-----------------------------------------------------------------------------
// update the dlg's text & controls
//-----------------------------------------------------------------------------
void UpdateConstText()
{
    WCHAR sz[256];
    DWORD dwNumClusters = g_CompressionUI.GetSlider( IDC_NUM_CLUSTERS )->GetValue();
    DWORD dwNumPCA = g_CompressionUI.GetSlider( IDC_NUM_PCA )->GetValue() * 4;
    DWORD dwMaxVertexShaderConst = DXUTGetD3D9DeviceCaps()->MaxVertexShaderConst;
    DWORD dwNumVConsts = dwNumClusters * ( 1 + 3 * dwNumPCA / 4 ) + 4;
    swprintf_s( sz, 256, L"%d * (1 + (3*%d/4)) + 4 = %d", dwNumClusters, dwNumPCA, dwNumVConsts );
    g_CompressionUI.GetStatic( IDC_CUR_CONSTANTS )->SetText( sz );

    bool bEnable = ( dwNumVConsts < dwMaxVertexShaderConst );
    g_CompressionUI.GetButton( IDC_APPLY )->SetEnabled( bEnable );
}


//-----------------------------------------------------------------------------
// Vista friendly file finder
//-----------------------------------------------------------------------------
HRESULT FindPRTMediaFile( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename, bool bCreatePath )
{
    WCHAR strFolder[MAX_PATH] = {0};
    WCHAR strPath[MAX_PATH] = {0};
    SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strFolder );
    wcscpy_s( strPath, MAX_PATH, strFolder );
    wcscat_s( strPath, MAX_PATH, L"\\" );
    wcscat_s( strFolder, MAX_PATH, g_szAppFolder );

    // Create the app directory if it's not already there
    if( 0xFFFFFFFF == GetFileAttributes( strFolder ) )
    {
        // create the directory
        if( !CreateDirectory( strFolder, NULL ) )
        {
            return E_FAIL;
        }
    }

    // Check for the file in the app directory first
    wcscat_s( strPath, MAX_PATH, strFilename );
    if( bCreatePath || ( 0xFFFFFFFF != GetFileAttributes( strPath ) ) )
    {
        wcscpy_s( strDestPath, cchDest, strPath );
        return S_OK;
    }

    // Check for the file in the normal DXUT directories
    return DXUTFindDXSDKMediaFileCch( strDestPath, cchDest, strFilename );
}
