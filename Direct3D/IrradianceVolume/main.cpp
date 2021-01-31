//--------------------------------------------------------------------------------------
// File: main.cpp
//
// This sample demonstrates irradiance volumes.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include <msxml.h>
#include "PRTMesh.h"
#include "SceneMesh.h"
#include "IrradianceCache.h"
#include "PRTSimulator.h"
#include "PRTOptionsDlg.h"
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

D3DXMATRIX                  g_mModelScale;
CFirstPersonCamera          g_VCamera;              // Viewer's camera
CModelViewerCamera          g_MCamera;              // Camera for mesh control
float                       g_fCameraDist = 3.0f;
bool                        g_bShowHelp = true;    // If true, renders the UI control text

// PRT simulator
CPRTSimulator               g_Simulator;

// PRT Mesh
CPRTMesh                    g_PRTMesh;

// Scene Mesh
CSceneMesh                  g_SceneMesh0;
CSceneMesh                  g_SceneMesh1;
CSceneMesh                  g_SceneMesh2;

// Irradiance Cache 
CIrradianceCache*           g_pIrradianceCache = NULL;

// Capturing Radiance/Irradiance for Cache
CIrradianceCacheGenerator   g_IrradianceCacheGenerator;
IDirect3DCubeTexture9*      g_pCapturedRadiance;
ID3DXRenderToEnvMap*        g_pRenderToEnvMap;
DWORD                       g_dwRadianceCubeSize = 32;
DWORD                       g_dwMaxOctreeSubdivision = 6;
DWORD                       g_dwMinOctreeSubdivision = 1;
float                       g_fOctreeAdaptiveSubdivisionHMDepthThreshold = 1.0f;
bool                        g_bOctreeAdaptiveSubdivision = true;

D3DXVECTOR3 g_CurrentVoxelLineList[8];

// Octree bounding boxes
#define MAX_SUBDIVISION 10
D3DXVECTOR3*                g_pOctreeLineList = NULL;
DWORD                       g_dwOctreeLineListSize = 0;
float                       g_fOctreeLineWidth = 1.0f;
float                       g_fPercentScenePreprocessing = 0.0f;


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
CDXUTDialog                 g_PreprocessOptionsUI;  // dialog for setting scene preprocessing options
CDXUTDialog                 g_PreprocessRunningUI;  // dialog for while scene preprocessor is running
bool                        g_bRenderUI = true;
bool                        g_bRenderMesh = true;
bool                        g_bRenderScene = true;
bool                        g_bRenderOctree = false;
bool                        g_bRenderOctreeSampleNode = true;
bool                        g_bRenderText = true;
bool                        g_bRenderCompressionUI = false;
bool                        g_bRenderSHVisual = false;
bool                        g_bRenderWireframe = false;
bool                        g_bChaseCamera = true;

// SH transfer visualization
CSHFunctionViewer           g_SHFuncViewer;

// Default Scene mesh
WCHAR*                      g_DefaultSceneMeshFile0 = TEXT( "acropolis.x" );
WCHAR*                      g_DefaultSceneMeshFile1 = TEXT( "acropolis_ground.x" );
WCHAR*                      g_DefaultSceneMeshFile2 = TEXT( "acropolis_sky.x" );
WCHAR*                      g_DefaultSceneTexFile0 = TEXT( "acropolis.jpg" );
WCHAR*                      g_DefaultSceneTexFile1 = TEXT( "acropolis_ground.jpg" );
WCHAR*                      g_DefaultSceneTexFile2 = TEXT( "acropolis_sky.jpg" );

// Default cache file
WCHAR*                      g_DefaultIrradianceCacheFile = TEXT( "acropolis.ivc" );

WCHAR g_szAppFolder[] = L"\\Irradiance Volume\\";

// App technique
enum APP_TECHNIQUE
{
    APP_TECH_SHIRRADENVMAP = 0,
    APP_TECH_LDPRT,
    APP_TECH_PRT,
    APP_TECH_NEAR_LOSSLESS_PRT
};

APP_TECHNIQUE               g_AppTechnique = APP_TECH_PRT; // Current technique of app

// App state 
enum APP_STATE
{
    APP_STATE_STARTUP = 0,
    APP_STATE_PREPROCESS_SCENE_OPTIONS,
    APP_STATE_PREPROCESS_SCENE_RUNNING,
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
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3

#define IDC_PREPROCESS_SCENE    9
#define IDC_LOAD_PRTBUFFER      10
#define IDC_SIMULATOR           11
#define IDC_STOP_SIMULATOR      12

#define IDC_RENDER_UI            19
#define IDC_RENDER_OCTREE_NODE   21
#define IDC_RENDER_MESH          22
#define IDC_RENDER_SCENE         23
#define IDC_RENDER_OCTREE        24
#define IDC_SIM_STATUS           25
#define IDC_SIM_STATUS_2         26
#define IDC_PREPROCESS_STATUS    27
#define IDC_PREPROCESS_STATUS_2  28
#define IDC_STOP_PREPROCESS      29

#define IDC_NUM_PCA              30
#define IDC_NUM_PCA_TEXT         31
#define IDC_NUM_CLUSTERS         32
#define IDC_NUM_CLUSTERS_TEXT    33
#define IDC_MAX_CONSTANTS        34
#define IDC_CUR_CONSTANTS        35
#define IDC_CUR_CONSTANTS_STATIC 36

#define IDC_APPLY                37
#define IDC_CANCEL               38
#define IDC_CHASECAMERA          39
#define IDC_WIREFRAME            41
#define IDC_RESTART              42
#define IDC_COMPRESSION          43
#define IDC_TRANSFER_VISUAL      45

#define IDC_TECHNIQUE_PRT        46
#define IDC_TECHNIQUE_SHIRRAD    47
#define IDC_TECHNIQUE_LDPRT      49

#define IDC_MAX_SUBDIVISION_STATIC 55
#define IDC_MAX_SUBDIVISION        56
#define IDC_ENABLE_ADAPTIVE_SUBDIVISION 57
#define IDC_MIN_SUBDIVISION_STATIC 58
#define IDC_MIN_SUBDIVISION        59
#define IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD_STATIC 60
#define IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD 61


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
void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down,
                         bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext );
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
                                D3DFORMAT* pfmtCubeMap, D3DFORMAT* pfmtIrrCubeMap );
void RenderBoundingBoxLines( IDirect3DDevice9* pd3dDevice, bool bRenderOctree, bool bRenderOctreeSampleNode );
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
    DXUTSetCallbackMouse( MouseProc );
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
    DXUTCreateWindow( L"IrradianceVolume" );
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
    g_PreprocessOptionsUI.Init( &g_DialogResourceManager );
    g_PreprocessRunningUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22 );

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
    g_StartUpUI.AddButton( IDC_PREPROCESS_SCENE, L"Preprocess Scene", 90, 42, 125, 40 );
    g_StartUpUI.AddButton( IDC_SIMULATOR, L"Run PRT simulator", 90, 84, 125, 40 );
    g_StartUpUI.AddButton( IDC_LOAD_PRTBUFFER, L"View saved results", 90, 126, 125, 40 );

    WCHAR sz[100];

    // Scene preprocessing options
    g_PreprocessOptionsUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    pElement = g_PreprocessOptionsUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    g_PreprocessOptionsUI.SetCallback( OnGUIEvent ); iY = 10;
    swprintf_s( sz, 100, L"Max Octree Subdivision: %d", g_dwMaxOctreeSubdivision ); sz[99] = 0;
    g_PreprocessOptionsUI.AddStatic( IDC_MAX_SUBDIVISION_STATIC, sz, 0, iY += 24, 150, 22 );
    g_PreprocessOptionsUI.AddSlider( IDC_MAX_SUBDIVISION, 5, iY += 24, 100, 22, 0, MAX_SUBDIVISION,
                                     g_dwMaxOctreeSubdivision );
    g_PreprocessOptionsUI.AddCheckBox( IDC_ENABLE_ADAPTIVE_SUBDIVISION, L"Adaptive Subdivision", 0, iY += 24, 150, 22,
                                       g_bOctreeAdaptiveSubdivision );
    swprintf_s( sz, 100, L"Min Octree Subdivision: %d", g_dwMinOctreeSubdivision ); sz[99] = 0;
    g_PreprocessOptionsUI.AddStatic( IDC_MIN_SUBDIVISION_STATIC, sz, 0, iY += 24, 150, 22 );
    g_PreprocessOptionsUI.AddSlider( IDC_MIN_SUBDIVISION, 15, iY += 24, 100, 22, 0, g_dwMaxOctreeSubdivision,
                                     g_dwMinOctreeSubdivision );
    g_PreprocessOptionsUI.GetStatic( IDC_MIN_SUBDIVISION_STATIC )->SetEnabled( g_bOctreeAdaptiveSubdivision );
    g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->SetEnabled( g_bOctreeAdaptiveSubdivision );
    swprintf_s( sz, 100, L"HM Depth Threshold: %0.3f", g_fOctreeAdaptiveSubdivisionHMDepthThreshold ); sz[99] = 0;
    g_PreprocessOptionsUI.AddStatic( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD_STATIC, sz, 0, iY += 24, 150, 22 );
    g_PreprocessOptionsUI.AddSlider( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD, 15, iY += 24, 100, 22, 0,
                                     1000, ( int )( g_fOctreeAdaptiveSubdivisionHMDepthThreshold * 100.0f ) );
    g_PreprocessOptionsUI.GetStatic( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD_STATIC )->SetEnabled(
        g_bOctreeAdaptiveSubdivision );
    g_PreprocessOptionsUI.GetSlider( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD )->SetEnabled(
        g_bOctreeAdaptiveSubdivision );
    g_PreprocessOptionsUI.AddButton( IDC_APPLY, L"Apply", 0, iY += 40, 75, 25 );
    g_PreprocessOptionsUI.AddButton( IDC_CANCEL, L"Cancel", 90, iY, 75, 25 );

    // Scene preprocessor running
    g_PreprocessRunningUI.SetFont( 1, L"Arial", 24, FW_BOLD );
    pElement = g_PreprocessRunningUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_CENTER | DT_BOTTOM;
    }

    g_PreprocessRunningUI.SetCallback( OnGUIEvent ); iY = 10;
    g_PreprocessRunningUI.AddStatic( IDC_PREPROCESS_STATUS, L"", 0, 0, 600, 22 );
    g_PreprocessRunningUI.AddStatic( IDC_PREPROCESS_STATUS_2, L"", 0, 30, 600, 22 );
    g_PreprocessRunningUI.AddButton( IDC_STOP_PREPROCESS, L"Stop Preprocessor", 240, 60, 125, 40 );

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
    pElement->iFont = 1;
    pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;

    g_RenderingUI2.SetCallback( OnGUIEvent ); iY = 10;
    g_RenderingUI2.AddCheckBox( IDC_RENDER_UI, L"Show UI (U)", 35, iY += 24, 125, 22, g_bRenderUI, 'U' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_MESH, L"Mesh (6)", 35, iY += 24, 125, 22, g_bRenderMesh, '6' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_SCENE, L"Scene (7)", 35, iY += 24, 125, 22, g_bRenderScene, '7' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_OCTREE, L"Octree (8)", 35, iY += 24, 125, 22, g_bRenderOctree, '8' );
    g_RenderingUI2.AddCheckBox( IDC_RENDER_OCTREE_NODE, L"Voxel (9)", 35, iY += 24, 125, 22, g_bRenderOctreeSampleNode,
                                '9' );
    g_RenderingUI2.AddCheckBox( IDC_WIREFRAME, L"Wireframe (0)", 35, iY += 24, 125, 22, g_bRenderWireframe );
    g_RenderingUI2.AddCheckBox( IDC_CHASECAMERA, L"Chase camera (C)", 35, iY += 24, 125, 22, g_bChaseCamera, 'C' );

    iY += 5;
    g_RenderingUI2.AddButton( IDC_COMPRESSION, L"Compression Settings", 35, iY += 24, 125, 22 );

    iY += 5;
    g_RenderingUI2.AddButton( IDC_RESTART, L"Restart", 35, iY += 24, 125, 22 );

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
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_SHIRRAD, 1, L"(1) SHIrradEnvMap", 0, iY += 24, 150, 22, false, '1' );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_LDPRT, 1, L"(2) LDPRT", 0, iY += 24, 150, 22, false, '2' );
    g_RenderingUI3.AddRadioButton( IDC_TECHNIQUE_PRT, 1, L"(3) PRT", 0, iY += 24, 150, 22, true, '3' );
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
    g_RenderingUI.AddCheckBox( IDC_TRANSFER_VISUAL, L"Visualize Transfer (T)", iX, iY += 20, 150, 22,
                               g_bRenderSHVisual, 'T' );
    g_RenderingUI.AddStatic( IDC_STATIC, L"Use 'P' to pick vertex", iX, iY += 18, 190, 25 );

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

    g_VCamera.SetRotateButtons( false, false, true );
    g_VCamera.SetScalers( 0.01f, 10.0f );
    g_MCamera.SetScalers( 0.01f, 10.0f );
    g_MCamera.SetButtonMasks( MOUSE_LEFT_BUTTON, 0, 0 );

    D3DXVECTOR3 vEye( 8.3f, 7.5f, -17.5f );
    D3DXVECTOR3 vAt ( 3.5f, 3.6f, -10.0f );
    g_VCamera.SetViewParams( &vEye, &vAt );
    g_MCamera.SetViewParams( &vEye, &vAt );

    vEye = *g_VCamera.GetEyePt();
    vAt = *g_MCamera.GetLookAtPt();
    D3DXVECTOR3 v = vAt - vEye;
    g_fCameraDist = D3DXVec3Length( &v );
}

//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // Need this to sample scene
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                         D3DRTYPE_CUBETEXTURE, D3DFMT_A32B32G32R32F ) ) )
    {
        return false;
    }

    // Determine texture support.  Fail if not good enough 
    D3DFORMAT fmtTexture, fmtCubeMap, fmtIrrCubeMap;
    GetSupportedTextureFormat( pD3D, pCaps, AdapterFormat, &fmtTexture, &fmtCubeMap, &fmtIrrCubeMap );
    if( D3DFMT_UNKNOWN == fmtTexture || D3DFMT_UNKNOWN == fmtCubeMap || D3DFMT_UNKNOWN == fmtIrrCubeMap )
        return false;

    // This sample requires pixel shader 2.0, but does showcase techniques which will 
    // perform well on shader model 1.1 hardware.
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
void GetSupportedTextureFormat( IDirect3D9* pD3D, D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT* pfmtTexture,
                                D3DFORMAT* pfmtCubeMap, D3DFORMAT* pfmtIrrCubeMap )
{
    D3DFORMAT fmtTexture = D3DFMT_UNKNOWN;
    D3DFORMAT fmtCubeMap = D3DFMT_UNKNOWN;
    D3DFORMAT fmtIrrCubeMap = D3DFMT_UNKNOWN;

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

    // check for support linear filtering of signed format cubemaps
    fmtIrrCubeMap = D3DFMT_UNKNOWN;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                            D3DRTYPE_CUBETEXTURE, D3DFMT_A32B32G32R32F ) ) )
    {
        fmtIrrCubeMap = D3DFMT_A32B32G32R32F;
    }
    else if( SUCCEEDED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType, AdapterFormat, 0,
                                                 D3DRTYPE_CUBETEXTURE, D3DFMT_A16B16G16R16F ) ) )
    {
        fmtIrrCubeMap = D3DFMT_A16B16G16R16F;
    }

    if( pfmtTexture )
        *pfmtTexture = fmtTexture;
    if( pfmtCubeMap )
        *pfmtCubeMap = fmtCubeMap;
    if( pfmtIrrCubeMap )
        *pfmtIrrCubeMap = fmtIrrCubeMap;
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
    else
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
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

    D3DFORMAT fmtTexture, fmtCubeMap, fmtIrrCubeMap;
    GetSupportedTextureFormat( pD3D, &Caps, DisplayMode.Format, &fmtTexture, &fmtCubeMap, &fmtIrrCubeMap );
    if( D3DFMT_UNKNOWN == fmtTexture || D3DFMT_UNKNOWN == fmtCubeMap || D3DFMT_UNKNOWN == fmtIrrCubeMap )
        return E_FAIL;

    V_RETURN( g_SceneMesh0.OnCreateDevice( pd3dDevice ) );
    V_RETURN( g_SceneMesh1.OnCreateDevice( pd3dDevice ) );
    V_RETURN( g_SceneMesh2.OnCreateDevice( pd3dDevice ) );

    V_RETURN( g_PRTMesh.OnCreateDevice( pd3dDevice, fmtCubeMap ) );
    V_RETURN( g_SHFuncViewer.OnCreateDevice( pd3dDevice ) );

    // Create the cubemap render targets for capturing radiance & irradiance
    SAFE_RELEASE( g_pRenderToEnvMap );

    V_RETURN( D3DXCreateRenderToEnvMap( pd3dDevice, g_dwRadianceCubeSize, 0, fmtIrrCubeMap, true,
                                        D3DFMT_D24S8, &g_pRenderToEnvMap ) );
    V_RETURN( D3DXCreateCubeTexture( pd3dDevice, g_dwRadianceCubeSize, 0, 0, fmtIrrCubeMap,
                                     D3DPOOL_MANAGED, &g_pCapturedRadiance ) );

    g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, false );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

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
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    V( g_PRTMesh.OnResetDevice() );
    V( g_SHFuncViewer.OnResetDevice() );

    V( g_pRenderToEnvMap->OnResetDevice() );

    if( g_pFont )
        V( g_pFont->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;

    g_VCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, 1.0f, 300.0f );
    g_MCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_SHFuncViewer.SetAspectRatio( fAspectRatio );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 100 );
    g_StartUpUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 300 ) / 2,
                             ( pBackBufferSurfaceDesc->Height - 200 ) / 2 );
    g_StartUpUI.SetSize( 300, 200 );
    g_StartUpUI2.SetLocation( 50, ( pBackBufferSurfaceDesc->Height - 200 ) );
    g_StartUpUI2.SetSize( 300, 200 );
    g_CompressionUI.SetLocation( 0, 150 );
    g_CompressionUI.SetSize( 200, 200 );

    g_PreprocessOptionsUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 150 ) / 2,
                                       ( pBackBufferSurfaceDesc->Height - 300 ) / 2 );
    g_PreprocessOptionsUI.SetSize( 150, 200 );

    g_PreprocessRunningUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 600 ) / 2,
                                       ( pBackBufferSurfaceDesc->Height - 100 ) / 2 );
    g_PreprocessRunningUI.SetSize( 600, 100 );
    g_SimulatorRunningUI.SetLocation( ( pBackBufferSurfaceDesc->Width - 600 ) / 2,
                                      ( pBackBufferSurfaceDesc->Height - 100 ) / 2 );
    g_SimulatorRunningUI.SetSize( 600, 100 );
    g_RenderingUI.SetLocation( 0, pBackBufferSurfaceDesc->Height - 125 );
    g_RenderingUI.SetSize( pBackBufferSurfaceDesc->Width, 125 );
    g_RenderingUI2.SetLocation( pBackBufferSurfaceDesc->Width - 170, 100 );
    g_RenderingUI2.SetSize( 170, 400 );
    g_RenderingUI3.SetLocation( 10, 30 );
    g_RenderingUI3.SetSize( 200, 100 );

    g_IrradianceCacheGenerator.SetGPUResources( DXUTGetD3D9Device(), g_pRenderToEnvMap, g_pCapturedRadiance );

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
    g_MCamera.FrameMove( fElapsedTime );
    g_VCamera.FrameMove( fElapsedTime );

    D3DXVECTOR3 vEye = *g_VCamera.GetEyePt();
    D3DXVECTOR3 vAt = *g_MCamera.GetLookAtPt();
    D3DXVECTOR3 vD = D3DXVECTOR3( vAt.x - vEye.x, 0.0f, vAt.z - vEye.z );
    float fDist = D3DXVec3Length( &vD );

    if( g_bChaseCamera &&
        !DXUTIsMouseButtonDown( MK_RBUTTON ) && !DXUTIsMouseButtonDown( MK_LBUTTON ) && // don't chase if buttons are down
        fDist > 1.5f ) // don't chase if camera is directly above or below object or too close
    {
        D3DXVECTOR3 vN = vAt - vEye;
        D3DXVec3Normalize( &vN, &vN );
        vEye = vAt - g_fCameraDist * vN;

        g_VCamera.SetViewParams( &vEye, &vAt );

        vEye = *g_VCamera.GetEyePt();
        vEye.y = vAt.y;  // keep model controls level with ground
        g_MCamera.SetViewParams( &vEye, &vAt );
    }
    else
    {
        D3DXVECTOR3 v = vAt - vEye;
        g_fCameraDist = D3DXVec3Length( &v );
    }
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
void RenderPreprocessSceneOptions( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        RenderText();
        V( g_PreprocessOptionsUI.OnRender( fElapsedTime ) );

        V( pd3dDevice->EndScene() );
    }
}

//--------------------------------------------------------------------------------------
void RenderPreprocessSceneRunning( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;

    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        RenderText();
        V( g_PreprocessRunningUI.OnRender( fElapsedTime ) );

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
    D3DXMATRIXA16 mSceneWorldViewProjection;
    D3DXMATRIXA16 mModelWorldViewProjection;

    pd3dDevice->SetRenderState( D3DRS_FILLMODE, ( g_bRenderWireframe ) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        mWorld = *g_MCamera.GetWorldMatrix();
        mProj = *g_VCamera.GetProjMatrix();
        mView = *g_VCamera.GetViewMatrix();

        mSceneWorldViewProjection = mView * mProj;
        mModelWorldViewProjection = g_mModelScale * mWorld * mView * mProj;

        if( g_bRenderScene )
        {
            g_SceneMesh0.Render( pd3dDevice, &mSceneWorldViewProjection );
            g_SceneMesh1.Render( pd3dDevice, &mSceneWorldViewProjection );
            g_SceneMesh2.Render( pd3dDevice, &mSceneWorldViewProjection );
        }

        if( g_bRenderMesh )
        {
            switch( g_AppTechnique )
            {
                case APP_TECH_NEAR_LOSSLESS_PRT:
                case APP_TECH_PRT:
                    g_PRTMesh.RenderWithPRT( pd3dDevice, &mModelWorldViewProjection, true );
                    break;

                case APP_TECH_SHIRRADENVMAP:
                    g_PRTMesh.RenderWithSHIrradEnvMap( pd3dDevice, &mModelWorldViewProjection, true );
                    break;

                case APP_TECH_LDPRT:
                {
                    g_PRTMesh.RenderWithLDPRT( pd3dDevice, &mModelWorldViewProjection, NULL, false, true );
                    break;
                }
            }

            if( g_bRenderSHVisual ) // show visualization of transfer function
            {
                D3DXMATRIX mModelWorld = g_mModelScale * mWorld;
                g_SHFuncViewer.Render( pd3dDevice, g_PRTMesh.GetObjectRadius(), mModelWorld, mView, mProj );
            }
        }

        if( g_bRenderOctree || g_bRenderOctreeSampleNode )
            RenderBoundingBoxLines( pd3dDevice, g_bRenderOctree, g_bRenderOctreeSampleNode );

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
// Reads the irradiance cache file from disk if one is not already loaded in memory
//--------------------------------------------------------------------------------------
bool LoadCacheFile()
{
    if( NULL == g_pIrradianceCache )
    {
        SAFE_DELETE( g_pIrradianceCache );
        SAFE_DELETE_ARRAY( g_pOctreeLineList );
        g_IrradianceCacheGenerator.Reset();

        WCHAR str[MAX_PATH];
        FindPRTMediaFile( str, MAX_PATH, g_DefaultIrradianceCacheFile );

        g_IrradianceCacheGenerator.CreateCache( str, &g_pIrradianceCache );
    }
    else
    {
        // Already loaded
        return true;
    }

    if( g_pIrradianceCache )
    {
        SAFE_DELETE_ARRAY( g_pOctreeLineList );
        g_dwOctreeLineListSize = g_pIrradianceCache->GetNodeCount() * 24;
        g_pOctreeLineList = new D3DXVECTOR3 [g_dwOctreeLineListSize];
        if( NULL != g_pOctreeLineList )
        {
            g_pIrradianceCache->CreateOctreeLineList( g_pOctreeLineList );
            return true;
        }
    }

    return false;
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
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

        case APP_STATE_PREPROCESS_SCENE_OPTIONS:
            RenderPreprocessSceneOptions( pd3dDevice, fTime, fElapsedTime );
            break;

        case APP_STATE_PREPROCESS_SCENE_RUNNING:
        {
            WCHAR sz[256];
            swprintf_s( sz, 256, L"Step %d of %d: %0.1f%% done", 1, 1, g_fPercentScenePreprocessing );

            g_PreprocessRunningUI.GetStatic( IDC_PREPROCESS_STATUS )->SetText( sz );
            g_PreprocessRunningUI.GetStatic( IDC_PREPROCESS_STATUS_2 )->SetText( L"Sampling Scene Irradiance" );

            bool bDone = false;

            if( g_pIrradianceCache )
            {
                bool bResult = true;

                bResult = g_IrradianceCacheGenerator.ProgressiveCacheFill( g_pIrradianceCache, &bDone,
                                                                           &g_fPercentScenePreprocessing );

                if( !( bResult ) )
                {
                    SAFE_DELETE( g_pIrradianceCache );
                    DXUT_ERR_MSGBOX( L"Error calling g_IrradianceCacheGenerator.ProgressiveCacheFill", E_FAIL );
                    g_AppState = APP_STATE_STARTUP;
                    return;
                }

                if( bDone )
                {
                    g_fPercentScenePreprocessing = 100.0f;
                    g_AppState = APP_STATE_STARTUP;

                    if( g_pIrradianceCache )
                    {
                        SAFE_DELETE_ARRAY( g_pOctreeLineList );
                        g_dwOctreeLineListSize = g_pIrradianceCache->GetNodeCount() * 24;
                        g_pOctreeLineList = new D3DXVECTOR3 [g_dwOctreeLineListSize];
                        if( g_pOctreeLineList )
                        {
                            g_pIrradianceCache->CreateOctreeLineList( g_pOctreeLineList );
                        }
                        g_pIrradianceCache->SaveCache( g_DefaultIrradianceCacheFile );
                    }
                }
            }

            RenderPreprocessSceneRunning( pd3dDevice, fTime, fElapsedTime );

            break;
        }

        case APP_STATE_SIMULATOR_OPTIONS:
        {
            CPRTOptionsDlg dlg;
            bool bFullscreenToggle = false;
            if( !DXUTIsWindowed() )
            {
                if( SUCCEEDED( DXUTToggleFullScreen() ) )
                    bFullscreenToggle = true;
            }

            bool bResult = false;
            if( !LoadCacheFile() )
            {
                MessageBox( DXUTGetHWND(), L"Preprocess scene first", L"IrradianceVolume", MB_OK );
            }
            else
            {
                bResult = dlg.Show();
            }

            if( bFullscreenToggle )
                DXUTToggleFullScreen();

            if( bResult )
            {
                SIMULATOR_OPTIONS* pOptions = dlg.GetOptions();
                g_PRTMesh.LoadMesh( pd3dDevice, pOptions->strInputMesh );

                g_SceneMesh0.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile0, g_DefaultSceneTexFile0 );
                g_SceneMesh1.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile1, g_DefaultSceneTexFile1 );
                g_SceneMesh2.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile2, g_DefaultSceneTexFile2 );

                float fObjectRadius = g_PRTMesh.GetObjectRadius();
                D3DXMatrixScaling( &g_mModelScale, 1.0f / fObjectRadius, 1.0f / fObjectRadius, 1.0f / fObjectRadius );

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
                g_SceneMesh0.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
                g_SceneMesh1.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
                g_SceneMesh2.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );

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

            bool bResult = false;
            if( !LoadCacheFile() )
            {
                MessageBox( DXUTGetHWND(), L"Preprocess scene first", L"IrradianceVolume", MB_OK );
            }
            else
            {
                bResult = dlg.Show();
            }

            if( bFullscreenToggle )
                DXUTToggleFullScreen();

            if( bResult )
            {
                SIMULATOR_OPTIONS* pOptions = dlg.GetOptions();
                LoadScene( pd3dDevice, pOptions->strInputMesh, pOptions->strResultsFile );
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
    DWORD dwOrder = g_PRTMesh.GetPRTOrder();

    D3DXMATRIX mModelWorld = g_mModelScale * *g_MCamera.GetWorldMatrix();
    D3DXMATRIX mModelInv;
    D3DXMatrixTranspose( &mModelInv, g_MCamera.GetWorldMatrix() );

    float fSum[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    ZeroMemory( fSum, 3 * D3DXSH_MAXORDER * D3DXSH_MAXORDER * sizeof( float ) );

    if( g_pIrradianceCache )
    {
        CIrradianceCache::IrradianceSample sample;
        ZeroMemory( &sample, sizeof( CIrradianceCache::IrradianceSample ) );

        D3DXVECTOR3 vCentroidOS = g_PRTMesh.GetObjectCenter();
        D3DXVECTOR4 vCentroidWS;

        D3DXVec3Transform( &vCentroidWS, &vCentroidOS, &mModelWorld );

        D3DXVECTOR3 vDir = D3DXVECTOR3( vCentroidWS.x, vCentroidWS.y, vCentroidWS.z );
        g_pIrradianceCache->SampleTrilinear( &vDir, &sample, g_CurrentVoxelLineList );

        D3DXSHRotate( fSum[0], dwOrder, &mModelInv, sample.pRedCoefs );
        D3DXSHRotate( fSum[1], dwOrder, &mModelInv, sample.pGreenCoefs );
        D3DXSHRotate( fSum[2], dwOrder, &mModelInv, sample.pBlueCoefs );
    }

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
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    if( !g_bRenderUI )
        txtHelper.DrawFormattedTextLine( L"Press 'U' to show UI" );

    /*
      // Debug info to know where camera/model are
      D3DXVECTOR3 cvecEye = *g_VCamera.GetEyePt();
      D3DXVECTOR3 cvecAt = *g_VCamera.GetLookAtPt();
      D3DXVECTOR3 mvecEye = *g_MCamera.GetEyePt();
      D3DXVECTOR3 mvecAt = *g_MCamera.GetLookAtPt();
      txtHelper.DrawFormattedTextLine( L"camera: eye(%0.1f,%0.1f,%0.1f) at(%0.1f, %0.1f, %0.1f)", cvecEye.x,cvecEye.y,cvecEye.z,cvecAt.x,cvecAt.y,cvecAt.z);
      txtHelper.DrawFormattedTextLine( L"model: eye(%0.1f,%0.1f,%0.1f) at(%0.1f, %0.1f, %0.1f)", mvecEye.x,mvecEye.y,mvecEye.z,mvecAt.x,mvecAt.y,mvecAt.z);
     */

    if( g_bShowHelp && g_AppState == APP_STATE_RENDER_SCENE )
    {
        const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( pBackBufferSurfaceDesc->Width - 300, pBackBufferSurfaceDesc->Height - 100 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"To see the technique, move the model in and out" );
        txtHelper.DrawTextLine( L"of the building.  Press 8 and 9 to visualize the octree" );

        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        if( !( DXUTIsMouseButtonDown( MK_LBUTTON ) || DXUTIsMouseButtonDown( MK_MBUTTON ) ||
               DXUTIsMouseButtonDown( MK_RBUTTON ) ) )
        {
            if( g_bChaseCamera )
            {
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 110,
                                           DXUTGetD3D9BackBufferSurfaceDesc()->Height - 40 );
                txtHelper.DrawTextLine( L"W/S/A/D/Q/E to move model and camera." );
            }
            else
            {
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 170,
                                           DXUTGetD3D9BackBufferSurfaceDesc()->Height - 40 );
                txtHelper.DrawTextLine( L"W/S/A/D/Q/E to move model. Press C to enter camera chase mode" );
            }
            txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 130,
                                       DXUTGetD3D9BackBufferSurfaceDesc()->Height - 25 );
            txtHelper.DrawTextLine( L"Left click moves model, right click moves camera." );
        }
        else
        {
            txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 70,
                                       DXUTGetD3D9BackBufferSurfaceDesc()->Height - 40 );
            if( DXUTIsMouseButtonDown( MK_LBUTTON ) )
            {
                txtHelper.DrawTextLine( L"Model Control Mode" );
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 170,
                                           DXUTGetD3D9BackBufferSurfaceDesc()->Height - 25 );
                txtHelper.DrawTextLine( L"Move mouse to rotate camera. W/S/A/D/Q/E to move camera." );
            }
            else if( DXUTIsMouseButtonDown( MK_RBUTTON ) )
            {
                txtHelper.DrawTextLine( L"Camera Control Mode" );
                txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 170,
                                           DXUTGetD3D9BackBufferSurfaceDesc()->Height - 25 );
                txtHelper.DrawTextLine( L"Move mouse to rotate model. W/S/A/D/Q/E to move model." );
            }
        }
    }
    else if( g_bShowHelp && g_AppState == APP_STATE_PREPROCESS_SCENE_OPTIONS )
    {
        const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( ( pBackBufferSurfaceDesc->Width - 500 ) / 2, pBackBufferSurfaceDesc->Height - 150 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine(
            L"This preprocesses the scene to create a volume of radiance samples stored in an octree." );
        txtHelper.DrawTextLine(
            L"This allows a PRT object to use the local lighting environment as it moves through a scene." );
        txtHelper.DrawTextLine(
            L"The more subdivision done to the scene, the more accurate the octree will be, but it will" );
        txtHelper.DrawTextLine( L"take longer to preprocess and require more memory." );
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

        case APP_STATE_PREPROCESS_SCENE_OPTIONS:
            *pbNoFurtherProcessing = g_PreprocessOptionsUI.MsgProc( hWnd, uMsg, wParam, lParam );
            if( *pbNoFurtherProcessing )
                return 0;
            break;

        case APP_STATE_PREPROCESS_SCENE_RUNNING:
            *pbNoFurtherProcessing = g_PreprocessRunningUI.MsgProc( hWnd, uMsg, wParam, lParam );
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

            // Pass all remaining windows messages to camera so it can respond to user input
            g_MCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
            g_VCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
            break;

        default:
            break;
    }

    return 0;
}


void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down,
                         bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext )
{
    if( bRightButtonDown )
    {
        g_MCamera.SetEnablePositionMovement( false );
        g_VCamera.SetEnablePositionMovement( true );

        g_RenderingUI2.EnableNonUserEvents( true );
        g_RenderingUI2.GetCheckBox( IDC_CHASECAMERA )->SetChecked( false );
        g_RenderingUI2.EnableNonUserEvents( false );
    }
    else
    {
        g_MCamera.SetEnablePositionMovement( true );
        g_VCamera.SetEnablePositionMovement( false );
    }

    D3DXVECTOR3 vEye = *g_VCamera.GetEyePt();
    D3DXVECTOR3 vAt = *g_MCamera.GetLookAtPt();
    vEye.y = vAt.y;  // keep model controls level with ground
    g_MCamera.SetViewParams( &vEye, &vAt );
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
            case 'G':
            {
                if( g_AppTechnique == APP_TECH_PRT )
                    g_AppTechnique = APP_TECH_NEAR_LOSSLESS_PRT;
                else if( g_AppTechnique == APP_TECH_NEAR_LOSSLESS_PRT )
                    g_AppTechnique = APP_TECH_PRT;
                g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );
                break;
            }

            case 'P':
            {
                if( !g_bRenderSHVisual )
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

                    const D3DXMATRIX* pmProj = g_VCamera.GetProjMatrix();
                    const D3DXMATRIX* pmView = g_VCamera.GetViewMatrix();
                    const D3DXMATRIX* pmWorld = g_MCamera.GetWorldMatrix();
                    D3DXMATRIX mWorld = g_mModelScale * *pmWorld;

                    g_SHFuncViewer.GetVertexUnderMouse( &g_PRTMesh, ( int )g_AppTechnique, pmProj, pmView, &mWorld );
                }
                break;
            }

            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
            case VK_F8:
            case '0':
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

        case IDC_MAX_SUBDIVISION:
        case IDC_MIN_SUBDIVISION:
            {
                WCHAR sz[100];
                g_dwMaxOctreeSubdivision = g_PreprocessOptionsUI.GetSlider( IDC_MAX_SUBDIVISION )->GetValue();
                swprintf_s( sz, 100, L"Max Octree Subdivision: %d", g_dwMaxOctreeSubdivision ); sz[99] = 0;
                g_PreprocessOptionsUI.GetStatic( IDC_MAX_SUBDIVISION_STATIC )->SetText( sz );
                g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->SetRange( 0, g_dwMaxOctreeSubdivision );

                g_dwMinOctreeSubdivision = g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->GetValue();
                g_dwMinOctreeSubdivision = g_dwMinOctreeSubdivision <= g_dwMaxOctreeSubdivision ?
                    g_dwMinOctreeSubdivision : g_dwMaxOctreeSubdivision;
                g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->SetValue( 0 );
                g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->SetValue( g_dwMinOctreeSubdivision );

                swprintf_s( sz, 100, L"Min Octree Subdivision: %d", g_dwMinOctreeSubdivision ); sz[99] = 0;
                g_PreprocessOptionsUI.GetStatic( IDC_MIN_SUBDIVISION_STATIC )->SetText( sz );

            }
            break;

        case IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD:
        {
            WCHAR sz[100];
            g_fOctreeAdaptiveSubdivisionHMDepthThreshold = ( float )g_PreprocessOptionsUI.GetSlider
                ( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD )->GetValue() / 100.0f;
            swprintf_s( sz, 100, L"HM Depth Threshold: %0.3f", g_fOctreeAdaptiveSubdivisionHMDepthThreshold );
            sz[99] = 0;
            g_PreprocessOptionsUI.GetStatic( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD_STATIC )->SetText( sz );
        }
            break;

        case IDC_ENABLE_ADAPTIVE_SUBDIVISION:
        {
            g_bOctreeAdaptiveSubdivision = g_PreprocessOptionsUI.GetCheckBox( IDC_ENABLE_ADAPTIVE_SUBDIVISION
                                                                              )->GetChecked();
            g_PreprocessOptionsUI.GetStatic( IDC_MIN_SUBDIVISION_STATIC )->SetEnabled( g_bOctreeAdaptiveSubdivision );
            g_PreprocessOptionsUI.GetSlider( IDC_MIN_SUBDIVISION )->SetEnabled( g_bOctreeAdaptiveSubdivision );
            g_PreprocessOptionsUI.GetStatic( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD_STATIC )->SetEnabled(
                g_bOctreeAdaptiveSubdivision );
            g_PreprocessOptionsUI.GetSlider( IDC_ADAPTIVE_SUBDIVISION_HMDEPTH_THRESHOLD )->SetEnabled(
                g_bOctreeAdaptiveSubdivision );
        }
            break;

        case IDC_SIMULATOR:
            g_AppState = APP_STATE_SIMULATOR_OPTIONS;
            break;

        case IDC_LOAD_PRTBUFFER:
            g_AppState = APP_STATE_LOAD_PRT_BUFFER;
            break;

        case IDC_STOP_PREPROCESS:
            SAFE_DELETE( g_pIrradianceCache );
            g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_PREPROCESS_SCENE:
            g_AppState = APP_STATE_PREPROCESS_SCENE_OPTIONS;
            break;

        case IDC_STOP_SIMULATOR:
            g_Simulator.Stop();
            if( g_Simulator.IsCompressing() )
                g_AppState = APP_STATE_SIMULATOR_RUNNING;
            else
                g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_TECHNIQUE_PRT:
        case IDC_TECHNIQUE_SHIRRAD:
        case IDC_TECHNIQUE_LDPRT:
            {
                // Switch to new technique
                switch( nControlID )
                {
                    case IDC_TECHNIQUE_SHIRRAD:
                        g_AppTechnique = APP_TECH_SHIRRADENVMAP; break;
                    case IDC_TECHNIQUE_LDPRT:
                        g_AppTechnique = APP_TECH_LDPRT; break;
                    case IDC_TECHNIQUE_PRT:
                        g_AppTechnique = APP_TECH_PRT; break;
                }

                if( g_bRenderSHVisual )
                    g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );

                g_RenderingUI2.EnableNonUserEvents( true );
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
                            L"To change compression settings during rendering, please load an uncompressed buffer.  To make one use the simulator settings dialog to save an uncompressed buffer.", L"IrradianceVolume", MB_OK );
            }
            break;

        case IDC_RESTART:
            g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_RENDER_UI:
            g_bRenderUI = g_RenderingUI2.GetCheckBox( IDC_RENDER_UI )->GetChecked();
            break;

        case IDC_RENDER_MESH:
            g_bRenderMesh = g_RenderingUI2.GetCheckBox( IDC_RENDER_MESH )->GetChecked();
            break;

        case IDC_RENDER_SCENE:
            g_bRenderScene = g_RenderingUI2.GetCheckBox( IDC_RENDER_SCENE )->GetChecked();
            break;

        case IDC_RENDER_OCTREE:
            g_bRenderOctree = g_RenderingUI2.GetCheckBox( IDC_RENDER_OCTREE )->GetChecked();
            break;

        case IDC_RENDER_OCTREE_NODE:
            g_bRenderOctreeSampleNode = g_RenderingUI2.GetCheckBox( IDC_RENDER_OCTREE_NODE )->GetChecked();
            break;

        case IDC_WIREFRAME:
            g_bRenderWireframe = g_RenderingUI2.GetCheckBox( IDC_WIREFRAME )->GetChecked();
            break;

        case IDC_CHASECAMERA:
            g_bChaseCamera = g_RenderingUI2.GetCheckBox( IDC_CHASECAMERA )->GetChecked();
            break;

        case IDC_TRANSFER_VISUAL:
        {
            g_bRenderSHVisual = g_RenderingUI.GetCheckBox( IDC_TRANSFER_VISUAL )->GetChecked();
            g_SHFuncViewer.UpdateDataForActiveVertex( &g_PRTMesh, ( int )g_AppTechnique, true );
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

        case IDC_CANCEL:
            g_AppState = APP_STATE_STARTUP;
            break;

        case IDC_APPLY:
        {
            if( APP_STATE_PREPROCESS_SCENE_OPTIONS == g_AppState )
            {
                g_SceneMesh0.LoadMesh( DXUTGetD3D9Device(), g_DefaultSceneMeshFile0, g_DefaultSceneTexFile0 );
                g_SceneMesh1.LoadMesh( DXUTGetD3D9Device(), g_DefaultSceneMeshFile1, g_DefaultSceneTexFile1 );
                g_SceneMesh2.LoadMesh( DXUTGetD3D9Device(), g_DefaultSceneMeshFile2, g_DefaultSceneTexFile2 );

                g_SceneMesh0.LoadEffects( DXUTGetD3D9Device(), DXUTGetD3D9DeviceCaps() );
                g_SceneMesh1.LoadEffects( DXUTGetD3D9Device(), DXUTGetD3D9DeviceCaps() );
                g_SceneMesh2.LoadEffects( DXUTGetD3D9Device(), DXUTGetD3D9DeviceCaps() );

                g_AppState = APP_STATE_PREPROCESS_SCENE_RUNNING;

                SAFE_DELETE( g_pIrradianceCache );

                // start radiance and irradiance sampling
                g_IrradianceCacheGenerator.Reset();
                g_IrradianceCacheGenerator.AddSceneMesh( &g_SceneMesh0, true );
                g_IrradianceCacheGenerator.AddSceneMesh( &g_SceneMesh1, true );
                g_IrradianceCacheGenerator.AddSceneMesh( &g_SceneMesh2, false );

                if( !( g_IrradianceCacheGenerator.SetSamplingInfo( g_dwMaxOctreeSubdivision,
                                                                   g_bOctreeAdaptiveSubdivision,
                                                                   g_dwMinOctreeSubdivision,
                                                                   g_fOctreeAdaptiveSubdivisionHMDepthThreshold ) ) )
                {
                    DXUT_ERR_MSGBOX( L"Error calling g_IrradianceCacheGenerator.SetSamplingInfo", E_FAIL );
                    g_AppState = APP_STATE_STARTUP;
                    break;
                }

                if( !( g_IrradianceCacheGenerator.SetGPUResources( DXUTGetD3D9Device(), g_pRenderToEnvMap,
                                                                   g_pCapturedRadiance ) ) )
                {
                    DXUT_ERR_MSGBOX( L"Error calling g_IrradianceCacheGenerator.SetGPUResources", E_FAIL );
                    g_AppState = APP_STATE_STARTUP;
                    break;
                }

                if( !( g_IrradianceCacheGenerator.CreateCache( &g_pIrradianceCache, false ) ) )
                {
                    DXUT_ERR_MSGBOX( L"Error calling g_IrradianceCacheGenerator.CreateCache", E_FAIL );
                    g_AppState = APP_STATE_STARTUP;
                    break;
                }

                g_fPercentScenePreprocessing = 0.0f;
            }
            else
            {
                DWORD dwNumPCA = g_CompressionUI.GetSlider( IDC_NUM_PCA )->GetValue() * 4;
                DWORD dwNumClusters = g_CompressionUI.GetSlider( IDC_NUM_CLUSTERS )->GetValue();

                SIMULATOR_OPTIONS& options = GetGlobalOptions();
                options.dwNumClusters = dwNumClusters;
                options.dwNumPCA = dwNumPCA;
                GetGlobalOptionsFile().SaveOptions();

                g_Simulator.CompressInWorkerThread( DXUTGetD3D9Device(), &options, &g_PRTMesh );
                g_AppState = APP_STATE_SIMULATOR_RUNNING;
            }
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
        wcscat_s( options.strInitialDir, MAX_PATH, L"" );
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
        g_SceneMesh0.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile0, g_DefaultSceneTexFile0 );
        g_SceneMesh1.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile1, g_DefaultSceneTexFile1 );
        g_SceneMesh2.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile2, g_DefaultSceneTexFile2 );

        g_Simulator.RunInWorkerThread( pd3dDevice, &options, &g_PRTMesh );
        g_AppState = APP_STATE_SIMULATOR_RUNNING;
        DXUTSetMultimonSettings( false );
    }
}


//--------------------------------------------------------------------------------------
void LoadScene( IDirect3DDevice9* pd3dDevice, WCHAR* strInputMesh, WCHAR* strResultsFile )
{
    WCHAR strResults[MAX_PATH];
    WCHAR strMesh[MAX_PATH];

    FindPRTMediaFile( strMesh, MAX_PATH, strInputMesh );
    FindPRTMediaFile( strResults, MAX_PATH, strResultsFile );

    g_PRTMesh.LoadMesh( pd3dDevice, strMesh );
    g_SceneMesh0.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile0, g_DefaultSceneTexFile0 );
    g_SceneMesh1.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile1, g_DefaultSceneTexFile1 );
    g_SceneMesh2.LoadMesh( pd3dDevice, g_DefaultSceneMeshFile2, g_DefaultSceneTexFile2 );

    float fObjectRadius = g_PRTMesh.GetObjectRadius();
    D3DXMatrixScaling( &g_mModelScale, 1.0f / fObjectRadius, 1.0f / fObjectRadius, 1.0f / fObjectRadius );

    WCHAR* pLastDot = wcsrchr( strResults, L'.' );
    g_PRTMesh.LoadPRTBufferFromFile( strResults );
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
    g_PRTMesh.LoadLDPRTFromFiles( szLDPRTFile, szNormalsFile );

    g_PRTMesh.ExtractCompressedDataForPRTShader();
    g_PRTMesh.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
    g_SceneMesh0.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
    g_SceneMesh1.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );
    g_SceneMesh2.LoadEffects( pd3dDevice, DXUTGetD3D9DeviceCaps() );

    g_SHFuncViewer.ClearActiveVertex( &g_PRTMesh, ( int )g_AppTechnique );

    g_AppState = APP_STATE_RENDER_SCENE;
    g_bRenderCompressionUI = false;
}


//--------------------------------------------------------------------------------------
void ResetUI()
{
    g_RenderingUI2.GetCheckBox( IDC_RENDER_UI )->SetChecked( true );

    g_RenderingUI2.GetCheckBox( IDC_RENDER_MESH )->SetChecked( true );
    g_RenderingUI2.GetCheckBox( IDC_WIREFRAME )->SetChecked( false );

    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_SHIRRAD )->SetChecked( true, true );
    g_RenderingUI3.GetRadioButton( IDC_TECHNIQUE_LDPRT )->SetChecked( true, true );
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
    if( g_pFont )
        g_pFont->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );
    g_PRTMesh.OnLostDevice();
    g_SceneMesh0.OnLostDevice();
    g_SceneMesh1.OnLostDevice();
    g_SceneMesh2.OnLostDevice();
    g_SHFuncViewer.OnLostDevice();
    if( g_pRenderToEnvMap )
        g_pRenderToEnvMap->OnLostDevice();

    g_IrradianceCacheGenerator.Reset();
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

    g_PRTMesh.OnDestroyDevice();
    g_SceneMesh0.OnDestroyDevice();
    g_SceneMesh1.OnDestroyDevice();
    g_SceneMesh2.OnDestroyDevice();
    g_SHFuncViewer.OnDestroyDevice();

    SAFE_RELEASE( g_pFont );
    g_Simulator.Stop();

    g_IrradianceCacheGenerator.Reset();
    SAFE_RELEASE( g_pRenderToEnvMap );
    SAFE_RELEASE( g_pCapturedRadiance );

    SAFE_DELETE_ARRAY( g_pOctreeLineList );
    SAFE_DELETE( g_pIrradianceCache );
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
// Render bounding box debug info
//-----------------------------------------------------------------------------
void RenderBoundingBoxLines( IDirect3DDevice9* pd3dDevice, bool bRenderOctree, bool bRenderOctreeSampleNode )
{
    if( NULL == g_pIrradianceCache || NULL == g_pOctreeLineList )
        return;

    if( !bRenderOctree && !bRenderOctreeSampleNode )
        return;


    D3DXMATRIXA16 mView = *g_VCamera.GetViewMatrix();
    D3DXMATRIXA16 mProj = *g_VCamera.GetProjMatrix();

    // Set device state
    D3DXMATRIX mat;
    D3DXMatrixIdentity( &mat );
    pd3dDevice->SetTransform( D3DTS_WORLD, &mat );
    pd3dDevice->SetTransform( D3DTS_VIEW, &mView );
    pd3dDevice->SetTransform( D3DTS_PROJECTION, &mProj );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, ( DWORD )FALSE );
    pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    pd3dDevice->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_FLAT );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    pd3dDevice->SetRenderState( D3DRS_WRAP0, 0 );
    pd3dDevice->SetRenderState( D3DRS_CLIPPING, TRUE );
    pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, FALSE );
    pd3dDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_FOGENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_RANGEFOGENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_TWOSIDEDSTENCILMODE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    pd3dDevice->SetRenderState( D3DRS_COLORVERTEX, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
    pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_ALWAYS );
    pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x00 );
    pd3dDevice->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetTexture( 0, NULL );
    pd3dDevice->SetFVF( D3DFVF_XYZ );

    if( bRenderOctree )
    {
        // Not very efficent rendering but this is just debug info
        pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
        pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, g_dwOctreeLineListSize / 2, g_pOctreeLineList, sizeof
                                     ( D3DXVECTOR3 ) );
    }

    if( bRenderOctreeSampleNode )
    {
        D3DXVECTOR3 list[24];

        // bottom
        list[0] = g_CurrentVoxelLineList[ 0];
        list[1] = g_CurrentVoxelLineList[ 1];
        list[2] = g_CurrentVoxelLineList[ 4];
        list[3] = g_CurrentVoxelLineList[ 5];
        list[4] = g_CurrentVoxelLineList[ 0];
        list[5] = g_CurrentVoxelLineList[ 4];
        list[6] = g_CurrentVoxelLineList[ 1];
        list[7] = g_CurrentVoxelLineList[ 5];

        // top
        list[8] = g_CurrentVoxelLineList[ 2];
        list[9] = g_CurrentVoxelLineList[ 3];
        list[10] = g_CurrentVoxelLineList[ 6];
        list[11] = g_CurrentVoxelLineList[ 7];
        list[12] = g_CurrentVoxelLineList[ 2];
        list[13] = g_CurrentVoxelLineList[ 6];
        list[14] = g_CurrentVoxelLineList[ 3];
        list[15] = g_CurrentVoxelLineList[ 7];

        // sides
        list[16] = g_CurrentVoxelLineList[ 0];
        list[17] = g_CurrentVoxelLineList[ 2];
        list[18] = g_CurrentVoxelLineList[ 4];
        list[19] = g_CurrentVoxelLineList[ 6];
        list[20] = g_CurrentVoxelLineList[ 1];
        list[21] = g_CurrentVoxelLineList[ 3];
        list[22] = g_CurrentVoxelLineList[ 5];
        list[23] = g_CurrentVoxelLineList[ 7];

        // Not very efficent rendering but this is just debug info
        pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFF0000 );
        pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 24 / 2, list, sizeof( D3DXVECTOR3 ) );
    }

    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, ( DWORD )TRUE );
    pd3dDevice->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_CURRENT );
}


//-----------------------------------------------------------------------------
// Vista friendly file finder
//-----------------------------------------------------------------------------
HRESULT FindPRTMediaFile( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename, bool bCreatePath )
{
    WCHAR strPath[MAX_PATH] = {0};
    SHGetFolderPath( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strPath );
    wcscat_s( strPath, MAX_PATH, g_szAppFolder );

    // Create the app directory if it's not already there
    if( 0xFFFFFFFF == GetFileAttributes( strPath ) )
    {
        // create the directory
        if( !CreateDirectory( strPath, NULL ) )
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
