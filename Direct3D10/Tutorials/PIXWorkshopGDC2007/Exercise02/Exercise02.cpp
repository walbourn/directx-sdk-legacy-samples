//--------------------------------------------------------------------------------------
// Exercise02.cpp
// PIX Workshop GDC2007
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#include "Terrain.h"
#include "Player.h"
#include "Sim.h"

#include "..\PIXWkshpPlugin\MemoryMapping.h"

#define MAX_SPRITES 200
#define SWAP( a, b ) { UINT _temp = a; a = b; b = _temp; }
#define NUM_PLAYERS 5000

//--------------------------------------------------------------------------------------
// DXUT Specific variables
//--------------------------------------------------------------------------------------
CModelViewerCamera              g_Camera;               // A model viewing camera
CDXUTDialogResourceManager      g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                g_pTxtHelper;
CDXUTDialog                     g_HUD;                  // dialog for standard controls
CDXUTDialog                     g_SampleUI;             // dialog for sample specific controls

// Direct3D 9 resources
ID3DXFont*                      g_pFont9 = NULL;
ID3DXSprite*                    g_pSprite9 = NULL;
ID3DXEffect*                    g_pEffect9 = NULL;
LPDIRECT3DVERTEXDECLARATION9    g_pBasicDecl = NULL;
LPDIRECT3DVERTEXDECLARATION9    g_pBallDecl = NULL;
LPDIRECT3DVERTEXDECLARATION9    g_pGrassDecl = NULL;
LPDIRECT3DTEXTURE9              g_pHeightTex = NULL;
LPDIRECT3DTEXTURE9              g_pNormalTex = NULL;
LPDIRECT3DTEXTURE9              g_pGrassTex = NULL;
LPDIRECT3DTEXTURE9              g_pDirtTex = NULL;
LPDIRECT3DTEXTURE9              g_pGroundGrassTex = NULL;
LPDIRECT3DTEXTURE9              g_pMaskTex = NULL;
LPDIRECT3DTEXTURE9              g_pShadeNormalTex = NULL;

CTerrain                        g_Terrain;
CPlayer*                        g_pPlayers = NULL;
CSim                            g_Sim;
LPDIRECT3DVERTEXBUFFER9         g_pStreamDataVB = NULL;
LPDIRECT3DVERTEXBUFFER9         g_pGrassDataVB = NULL;
UINT                            g_NumThreads = 0;
bool                            g_bWireframe = false;
bool                            g_bShowTiles = false;
bool                            g_bRenderBalls = true;
bool                            g_bRunSim = true;
bool                            g_bBallCam = true;
bool                            g_bRenderGrass = true;
bool                            g_bEnableCulling = true;
bool                            g_bDepthCullGrass = false;
float                           g_fFOV = 80.0f * ( D3DX_PI / 180.0f );
CGrowableArray <UINT>           g_VisibleTileArray;
CGrowableArray <float>          g_VisibleTileArrayDepth;
float                           g_fWorldScale = 400.0f;
float                           g_fHeightScale = 40.0f;
UINT                            g_SqrtNumTiles = 40;
UINT                            g_SidesPerTile = 20;
float                           g_fFadeStart = 40.0f;
float                           g_fFadeEnd = 60.0f;
UINT                            g_NumVisibleBalls = 0;
UINT                            g_NumGrassTiles = 0;
bool                            g_bUseFloat32Textures = false;

//--------------------------------------------------------------------------------------
// Effect variables
//--------------------------------------------------------------------------------------
WCHAR g_strEffect[MAX_PATH];
FILETIME                        g_fLastWriteTime;
double                          g_fLastFileCheck = 0.0;

D3DXHANDLE                      g_hRenderTerrain = NULL;
D3DXHANDLE                      g_hRenderBall = NULL;
D3DXHANDLE                      g_hRenderGrass = NULL;
D3DXHANDLE                      g_hRenderSky = NULL;
D3DXHANDLE                      g_hmWorldViewProj = NULL;
D3DXHANDLE                      g_hmViewProj = NULL;
D3DXHANDLE                      g_hmWorld = NULL;
D3DXHANDLE                      g_hvWorldLightDir = NULL;
D3DXHANDLE                      g_hfTime = NULL;
D3DXHANDLE                      g_hfElapsedTime = NULL;
D3DXHANDLE                      g_hvColor = NULL;
D3DXHANDLE                      g_hfWorldScale = NULL;
D3DXHANDLE                      g_hfHeightScale = NULL;
D3DXHANDLE                      g_hvEyePt = NULL;
D3DXHANDLE                      g_hfFadeStart = NULL;
D3DXHANDLE                      g_hfFadeEnd = NULL;
D3DXHANDLE                      g_htxDiffuse = NULL;
D3DXHANDLE                      g_htxNormal = NULL;
D3DXHANDLE                      g_htxHeight = NULL;
D3DXHANDLE                      g_htxDirt = NULL;
D3DXHANDLE                      g_htxGrass = NULL;
D3DXHANDLE                      g_htxMask = NULL;
D3DXHANDLE                      g_htxShadeNormals = NULL;

CDXUTSDKMesh                    g_BallMesh;
CDXUTSDKMesh                    g_SkyMesh;

extern ID3D10Buffer*            g_pStreamDataVB10;
extern ID3D10Buffer*            g_pGrassDataVB10;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4

#define IDC_WIREFRAME			10
#define IDC_SHOWTILES			11
#define IDC_RENDERBALLS			12
#define IDC_RUNSIM				13
#define IDC_BALLCAM				14
#define IDC_RENDERGRASS			15
#define IDC_ENABLECULLING		16
#define IDC_DEPTHCULLGRASS		17
#define IDC_RESETBALLS			18

//--------------------------------------------------------------------------------------
// Callbacks
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

extern bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                              DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                             void* pUserContext );
extern HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                                 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
extern void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
extern void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime,
                                         void* pUserContext );

void InitApp();
void RenderText();
void AsyncKeyProc( float fElapsedTime );
void GetCameraData( D3DXMATRIX* pmWorld, D3DXMATRIX* pmView, D3DXMATRIX* pmProj, D3DXVECTOR3* pvEye,
                    D3DXVECTOR3* pvDir );
void GetCameraCullPlanes( D3DXVECTOR3* p1Normal, D3DXVECTOR3* p2Normal, float* p1D, float* p2D );
void VisibilityCullTiles();
FILETIME GetFileTime( WCHAR* strFile );
UINT GetSystemHWThreadCount();
HRESULT LoadEffect( LPDIRECT3DDEVICE9 pd3dDevice );
void ResetBalls();

void InitMemoryMapping();
void UninitMemoryMapping();
void SetNumVisibleTiles( UINT iVal );
void SetNumVisibleGrassTiles( UINT iVal );
void SetNumVisibleBalls( UINT iVal );
void SetMaxGridBalls( UINT iVal );

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10SwapChainResized );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10SwapChainReleasing );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitMemoryMapping();
    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"PIX Workship GDC 2007: Exercise02" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop
    UninitMemoryMapping();
    SAFE_DELETE_ARRAY( g_pPlayers );
    g_Sim.DestroySimThreads();
    g_Sim.DestroyCollisionGrid();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddCheckBox( IDC_WIREFRAME, L"Wireframe", 35, iY += 24, 125, 22, g_bWireframe );
    g_SampleUI.AddCheckBox( IDC_SHOWTILES, L"Show Tiles", 35, iY += 24, 125, 22, g_bShowTiles );
    g_SampleUI.AddCheckBox( IDC_RENDERBALLS, L"Render Balls", 35, iY += 24, 125, 22, g_bRenderBalls );
    g_SampleUI.AddCheckBox( IDC_RUNSIM, L"Run Physics", 35, iY += 24, 125, 22, g_bRunSim );
    g_SampleUI.AddCheckBox( IDC_BALLCAM, L"Ball Cam", 35, iY += 24, 125, 22, g_bBallCam );
    g_SampleUI.AddCheckBox( IDC_RENDERGRASS, L"Render Grass", 35, iY += 24, 125, 22, g_bRenderGrass );
    g_SampleUI.AddCheckBox( IDC_ENABLECULLING, L"Enable Culling", 35, iY += 24, 125, 22, g_bEnableCulling );
    //g_SampleUI.AddCheckBox( IDC_DEPTHCULLGRASS, L"Depth Cull Grass", 35, iY+=24, 125, 22, g_bDepthCullGrass );

    g_SampleUI.AddButton( IDC_RESETBALLS, L"Reset Balls", 35, iY += 24, 125, 22 );

    // Setup the sim
    g_pPlayers = new CPlayer[NUM_PLAYERS];
    g_NumThreads = GetSystemHWThreadCount();
    g_Sim.CreateSimThreads( g_NumThreads, g_pPlayers, NUM_PLAYERS );
    g_Sim.CreateCollisionGrid( 40, 40, g_fWorldScale );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

        // Turn VSync off
        pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( ( Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
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
    }
    else
    {
        // Turn VSync off
        pDeviceSettings->d3d10.SyncInterval = DXGI_SWAP_EFFECT_DISCARD;
        g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );
    }

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE ) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;

    // Handle keys
    AsyncKeyProc( fElapsedTime );

    g_Sim.InsertPlayersIntoGrid();

    if( g_bRunSim )
    {
        g_Sim.StartSim( fTime, fElapsedTime );
        g_Sim.WaitForSim();
    }

    SetMaxGridBalls( g_Sim.GetMaxBallsInACell() );

    // setup clip planes
    D3DXVECTOR3 vLeftNormal;
    D3DXVECTOR3 vRightNormal;
    float leftD;
    float rightD;
    GetCameraCullPlanes( &vLeftNormal, &vRightNormal, &leftD, &rightD );

    // Fill the stream data VB
    g_NumVisibleBalls = 0;
    D3DXVECTOR3* pPositions = NULL;
    if( DXUTIsAppRenderingWithD3D9() )
    {
        V( g_pStreamDataVB->Lock( 0, 0, ( void** )&pPositions, D3DLOCK_DISCARD ) );
    }
    else
    {
        V( g_pStreamDataVB10->Map( D3D10_MAP_WRITE_DISCARD, 0, ( void** )&pPositions ) );
    }

    for( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        g_pPlayers[i].UpdatePostPhysicsValues();
        D3DXVECTOR3 pos = g_pPlayers[i].GetPosition();
        float rad = g_pPlayers[i].GetRadius();

        if( !g_bEnableCulling ||
            ( D3DXVec3Dot( &pos, &vLeftNormal ) < leftD + rad &&
              D3DXVec3Dot( &pos, &vRightNormal ) < rightD + rad ) )
        {
            pPositions[g_NumVisibleBalls] = pos;
            g_NumVisibleBalls++;
        }
    }
    if( DXUTIsAppRenderingWithD3D9() )
        g_pStreamDataVB->Unlock();
    else
        g_pStreamDataVB10->Unmap();

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
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
// AsyncKeyProc
//--------------------------------------------------------------------------------------
void AsyncKeyProc( float fElapsedTime )
{
    static float fTurnSpeed = 90.0f * ( D3DX_PI / 180.0f );
    static float fSpeed = 0.5f;

    if( GetAsyncKeyState( VK_LEFT ) & 0x8000 )
    {
        g_pPlayers[0].Turn( -fTurnSpeed * fElapsedTime );
    }
    if( GetAsyncKeyState( VK_RIGHT ) & 0x8000 )
    {
        g_pPlayers[0].Turn( fTurnSpeed * fElapsedTime );
    }
    if( GetAsyncKeyState( VK_UP ) & 0x8000 )
    {
        g_pPlayers[0].MoveForward( fSpeed );
    }
    if( GetAsyncKeyState( VK_DOWN ) & 0x8000 )
    {
        g_pPlayers[0].MoveForward( -fSpeed );
    }
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
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
        case IDC_WIREFRAME:
        {
            g_bWireframe = !g_bWireframe;
            if( DXUTIsAppRenderingWithD3D9() )
                DXUTGetD3D9Device()->SetRenderState( D3DRS_FILLMODE,
                                                     g_bWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
        }
            break;
        case IDC_SHOWTILES:
        {
            g_bShowTiles = !g_bShowTiles;
        }
            break;
        case IDC_RENDERBALLS:
        {
            g_bRenderBalls = !g_bRenderBalls;
        }
            break;
        case IDC_RUNSIM:
        {
            g_bRunSim = !g_bRunSim;
        }
            break;
        case IDC_BALLCAM:
        {
            g_bBallCam = !g_bBallCam;
        }
            break;
        case IDC_RENDERGRASS:
        {
            g_bRenderGrass = !g_bRenderGrass;
        }
            break;
        case IDC_ENABLECULLING:
        {
            g_bEnableCulling = !g_bEnableCulling;
        }
            break;
        case IDC_DEPTHCULLGRASS:
        {
            g_bDepthCullGrass = !g_bDepthCullGrass;
        }
            break;
        case IDC_RESETBALLS:
        {
            ResetBalls();
        }
            break;
    }
}

//--------------------------------------------------------------------------------------
// Render text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 1.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"NumThreads: %d", g_NumThreads );
    g_pTxtHelper->DrawTextLine( str );

    g_pTxtHelper->End();
}

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
    // doesn't support at least ps3.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 3, 0 ) )
        return false;

    // See if we support VTF from a DXT texture (a D3D10-level card)
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_VERTEXTEXTURE,
                                         D3DRTYPE_TEXTURE, D3DFMT_DXT1 ) ) )
    {
        g_bUseFloat32Textures = true;

        // We must support at least 32bit float textures for VTF then
        if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                             AdapterFormat, D3DUSAGE_QUERY_VERTEXTEXTURE,
                                             D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) ) )
            return false;
    }

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

    g_SampleUI.GetCheckBox( IDC_WIREFRAME )->SetVisible( true );

    V_RETURN( LoadEffect( pd3dDevice ) );

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1.bmp" ) );
    V_RETURN( g_Terrain.LoadTerrain( str, g_SqrtNumTiles, g_SidesPerTile, g_fWorldScale, g_fHeightScale, 1000, 1.0f,
                                     2.0f ) );

    ResetBalls();

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 20.0f, -50.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 5.0f, 0.0f );
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
    g_Camera.SetProjParams( g_fFOV, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    // Create a Vertex Decl for the terrain and basic meshes
    D3DVERTEXELEMENT9 basicDesc[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    V_RETURN( pd3dDevice->CreateVertexDeclaration( basicDesc, &g_pBasicDecl ) );

    // Create a Vertex Decl for the ball
    D3DVERTEXELEMENT9 ballDesc[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {1, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    V_RETURN( pd3dDevice->CreateVertexDeclaration( ballDesc, &g_pBallDecl ) );

    // Create a Vertex Decl for the grass
    D3DVERTEXELEMENT9 grassDesc[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
        {1, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
        {0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
    };
    V_RETURN( pd3dDevice->CreateVertexDeclaration( grassDesc, &g_pGrassDecl ) );

    // Load terrain device objects
    V_RETURN( g_Terrain.OnResetDevice( pd3dDevice ) );

    // Load a mesh
    g_BallMesh.Create( pd3dDevice, L"PIXWorkshop\\lowpolysphere.sdkmesh" );
    g_SkyMesh.Create( pd3dDevice, L"PIXWorkshop\\desertsky.sdkmesh" );

    // Create a VB for the stream data
    V_RETURN( pd3dDevice->CreateVertexBuffer( NUM_PLAYERS * sizeof( D3DXVECTOR3 ),
                                              D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                              0,
                                              D3DPOOL_DEFAULT,
                                              &g_pStreamDataVB,
                                              NULL ) );

    // Create a VB for the grass instances
    V_RETURN( pd3dDevice->CreateVertexBuffer( g_SqrtNumTiles * g_SqrtNumTiles * sizeof( D3DXVECTOR3 ),
                                              D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                              0,
                                              D3DPOOL_DEFAULT,
                                              &g_pGrassDataVB,
                                              NULL ) );

    // Load a texture for the mesh
    WCHAR str[MAX_PATH];
    D3DFORMAT fmt = D3DFMT_FROM_FILE;
    if( g_bUseFloat32Textures )
    {
        fmt = D3DFMT_A32B32G32R32F;
    }

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1.bmp" ) );
    V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str,
                                           D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, fmt, D3DPOOL_DEFAULT,
                                           D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &g_pHeightTex ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_Mask.dds" ) );
    V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str,
                                           D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, fmt, D3DPOOL_DEFAULT,
                                           D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &g_pMaskTex ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_ShadeNormals.dds" ) );
    V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str,
                                           D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, fmt, D3DPOOL_DEFAULT,
                                           D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &g_pShadeNormalTex ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_Norm.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pNormalTex ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\grass_v3_dark_tex.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pGrassTex ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Dirt_Diff.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pDirtTex ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Grass_Diff.dds" ) );
    V_RETURN( D3DXCreateTextureFromFile( pd3dDevice, str, &g_pGroundGrassTex ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// GetCameraData
//--------------------------------------------------------------------------------------
void GetCameraData( D3DXMATRIX* pmWorld, D3DXMATRIX* pmView, D3DXMATRIX* pmProj, D3DXVECTOR3* pvEye,
                    D3DXVECTOR3* pvDir )
{
    D3DXVECTOR3 pos = g_pPlayers[0].GetPosition();
    *pvEye = pos + D3DXVECTOR3( 0, 2, 0 );
    *pvDir = g_pPlayers[0].GetDirection();
    *pvEye -= ( *pvDir ) * 5.0f;
    *pmProj = *g_Camera.GetProjMatrix();
    D3DXVec3Normalize( pvDir, pvDir );

    if( g_bBallCam )
    {
        float fHeight = g_Terrain.GetHeightOnMap( pvEye );
        if( fHeight > ( pvEye->y - 1.5f ) )
        {
            pvEye->y = fHeight + 1.5f;
        }

        D3DXVECTOR3 vAt = pos + D3DXVECTOR3( 0, 2, 0 );
        D3DXVECTOR3 vUp( 0,1,0 );
        D3DXMatrixIdentity( pmWorld );
        D3DXMatrixLookAtLH( pmView, pvEye, &vAt, &vUp );
    }
    else
    {
        *pmWorld = *g_Camera.GetWorldMatrix();
        *pmView = *g_Camera.GetViewMatrix();
    }
}


//--------------------------------------------------------------------------------------
// GetCameraCullPlanes
//--------------------------------------------------------------------------------------
void GetCameraCullPlanes( D3DXVECTOR3* p1Normal, D3DXVECTOR3* p2Normal, float* p1D, float* p2D )
{
    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );

    // setup clip planes
    D3DXVECTOR3 vLeftNormal;
    D3DXVECTOR3 vRightNormal;
    D3DXMATRIX mRotLeft;
    D3DXMATRIX mRotRight;
    float fAngle = D3DX_PI / 2.0f + ( g_fFOV / 2.0f ) * 1.3333f;
    D3DXMatrixRotationY( &mRotLeft, -fAngle );
    D3DXMatrixRotationY( &mRotRight, fAngle );
    D3DXVec3TransformNormal( &vLeftNormal, &vDir, &mRotLeft );
    D3DXVec3TransformNormal( &vRightNormal, &vDir, &mRotRight );
    *p1D = D3DXVec3Dot( &vLeftNormal, &vEye );
    *p2D = D3DXVec3Dot( &vRightNormal, &vEye );
    *p1Normal = vLeftNormal;
    *p2Normal = vRightNormal;
}

//--------------------------------------------------------------------------------------
// VisibilityCullTiles
//--------------------------------------------------------------------------------------
void VisibilityCullTiles()
{
    g_VisibleTileArray.Reset();
    g_VisibleTileArrayDepth.Reset();

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );

    // setup clip planes
    D3DXVECTOR3 vLeftNormal;
    D3DXVECTOR3 vRightNormal;
    float leftD;
    float rightD;
    GetCameraCullPlanes( &vLeftNormal, &vRightNormal, &leftD, &rightD );

    for( UINT i = 0; i < g_Terrain.GetNumTiles(); i++ )
    {
        TERRAIN_TILE* pTile = g_Terrain.GetTile( i );

        //-----------------------------------------------------------------------------------------
        // o/__   <-- BreakdancinBob TODO: We should probably do some actual culling.  So far we're  
        // |  (\						   just sorting the tiles for rendering front to back.  Use
        //								   the planes <vLeftNormal, leftD> <vRightNormal, rightD>
        //								   to test each of the boxes 4 corners for visibility.
        //                          
        //                           HINT: If dot( point, vLeftNormal ) > leftD, the point is 
        //								   beyond of the left viewing frustum plane of the camera.
        //								   Likewise, if dot( point, vRightNormal ) > rightD, the
        //								   point is beyond the right viewing frustum plane of the
        //								   camera.
        //-----------------------------------------------------------------------------------------

        // These are the 4 corners of the bounding square for this tile
        D3DXVECTOR3 c1( pTile->BBox.min.x, 0, pTile->BBox.min.z );
        D3DXVECTOR3 c2( pTile->BBox.max.x, 0, pTile->BBox.max.z );
        D3DXVECTOR3 c3( pTile->BBox.min.x, 0, pTile->BBox.max.z );
        D3DXVECTOR3 c4( pTile->BBox.max.x, 0, pTile->BBox.min.z );

        // Uncomment this to cull everything on the left of the field of view.
        // Don't forget the right!
        /*
          if( D3DXVec3Dot( &c1, &vLeftNormal ) > leftD &&
          D3DXVec3Dot( &c2, &vLeftNormal ) > leftD &&
          D3DXVec3Dot( &c3, &vLeftNormal ) > leftD &&
          D3DXVec3Dot( &c4, &vLeftNormal ) > leftD )
          continue;
         */
        //-----------------------------------------------------------------------------------------

        D3DXVECTOR3 middle = ( pTile->BBox.min + pTile->BBox.max ) / 2.0f;
        D3DXVECTOR3 toTile = middle - vEye;
        float fDot = D3DXVec3Dot( &toTile, &vDir );
        float fDist = D3DXVec3Length( &toTile );
        if( fDot < 0 )
            fDist = fDot;
        g_VisibleTileArray.Add( i );
        g_VisibleTileArrayDepth.Add( fDist );
    }

    UINT* pTiles = g_VisibleTileArray.GetData();
    float* pDepth = g_VisibleTileArrayDepth.GetData();

    QuickDepthSort( pTiles, pDepth, 0, g_VisibleTileArray.GetSize() - 1 );

    // Fill the grass instance VB
    HRESULT hr = S_OK;
    g_NumGrassTiles = 0;
    //float fTileRad = ( g_fWorldScale / g_SqrtNumTiles ) / 2.0f;
    D3DXVECTOR3* pPositions = NULL;

    if( DXUTIsAppRenderingWithD3D9() )
    {
        V( g_pGrassDataVB->Lock( 0, 0, ( void** )&pPositions, D3DLOCK_DISCARD ) );
    }
    else
    {
        V( g_pGrassDataVB10->Map( D3D10_MAP_WRITE_DISCARD, 0, ( void** )&pPositions ) );
    }

    UINT TileCount = g_VisibleTileArray.GetSize();
    for( int i = TileCount - 1; i >= 0; i-- )
    {
        TERRAIN_TILE* pTile = g_Terrain.GetTile( g_VisibleTileArray.GetAt( i ) );
        D3DXVECTOR3 middle = ( pTile->BBox.min + pTile->BBox.max ) / 2.0f;

        pPositions[ g_NumGrassTiles ] = middle;
        g_NumGrassTiles ++;
    }

    if( DXUTIsAppRenderingWithD3D9() )
        g_pGrassDataVB->Unlock();
    else
        g_pGrassDataVB10->Unmap();

}

//--------------------------------------------------------------------------------------
// RenderTerrain
//--------------------------------------------------------------------------------------
void RenderTerrain( LPDIRECT3DDEVICE9 pd3dDevice )
{
    HRESULT hr = S_OK;

    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    pd3dDevice->SetVertexDeclaration( g_pBasicDecl );
    g_pEffect9->SetTechnique( g_hRenderTerrain );

    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWVP );
    g_pEffect9->SetMatrix( g_hmWorld, &mWorld );

    g_pEffect9->SetTexture( g_htxNormal, g_pNormalTex );
    g_pEffect9->SetTexture( g_htxDirt, g_pDirtTex );
    g_pEffect9->SetTexture( g_htxGrass, g_pGroundGrassTex );
    g_pEffect9->SetTexture( g_htxMask, g_pMaskTex );


    if( !g_bShowTiles )
    {
        D3DXVECTOR4 color( 1,1,1,1 );
        g_pEffect9->SetVector( g_hvColor, &color );
    }

    UINT cPasses = 0;
    V( g_pEffect9->Begin( &cPasses, 0 ) );

    for( UINT iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect9->BeginPass( iPass ) );

        pd3dDevice->SetIndices( g_Terrain.GetTerrainIB() );

        // Render front to back
        UINT NumTiles = g_VisibleTileArray.GetSize();
        SetNumVisibleTiles( NumTiles );
        for( UINT i = 0; i < NumTiles; i++ )
        {
            TERRAIN_TILE* pTile = g_Terrain.GetTile( g_VisibleTileArray.GetAt( i ) );

            if( g_bShowTiles )
            {
                g_pEffect9->SetVector( g_hvColor, &pTile->Color );
                g_pEffect9->CommitChanges();
            }

            g_Terrain.RenderTile( pTile );
        }

        V( g_pEffect9->EndPass() );
    }
    V( g_pEffect9->End() );
}


//--------------------------------------------------------------------------------------
// RenderBalls
//--------------------------------------------------------------------------------------
void RenderBalls( LPDIRECT3DDEVICE9 pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWVP );
    g_pEffect9->SetMatrix( g_hmWorld, &mWorld );

    pd3dDevice->SetVertexDeclaration( g_pBallDecl );

    D3DXHANDLE hTechnique = g_hRenderBall;
    ID3DXEffect* pEffect = g_pEffect9;

    // set vb streams
    pd3dDevice->SetStreamSource( 0, g_BallMesh.GetVB9( 0, 0 ), 0, g_BallMesh.GetVertexStride( 0, 0 ) );
    pd3dDevice->SetStreamSource( 1, g_pStreamDataVB, 0, sizeof( D3DXVECTOR3 ) );
    pd3dDevice->SetStreamSourceFreq( 0, ( D3DSTREAMSOURCE_INDEXEDDATA | g_NumVisibleBalls ) );
    pd3dDevice->SetStreamSourceFreq( 1, ( D3DSTREAMSOURCE_INSTANCEDATA | 1ul ) );

    SetNumVisibleBalls( g_NumVisibleBalls );

    // Set our index buffer as well
    pd3dDevice->SetIndices( g_BallMesh.GetIB9( 0 ) );

    // Render the scene with this technique 
    pEffect->SetTechnique( hTechnique );

    SDKMESH_SUBSET* pSubset = NULL;
    D3DPRIMITIVETYPE PrimType;
    UINT cPasses = 0;
    pEffect->Begin( &cPasses, 0 );

    for( UINT p = 0; p < cPasses; ++p )
    {
        pEffect->BeginPass( p );

        for( UINT subset = 0; subset < g_BallMesh.GetNumSubsets( 0 ); subset++ )
        {
            pSubset = g_BallMesh.GetSubset( 0, subset );

            PrimType = g_BallMesh.GetPrimitiveType9( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );

            UINT PrimCount = ( UINT )pSubset->IndexCount;
            UINT IndexStart = ( UINT )pSubset->IndexStart;
            UINT VertexStart = ( UINT )pSubset->VertexStart;
            UINT VertexCount = ( UINT )pSubset->VertexCount;
            if( D3DPT_TRIANGLELIST == PrimType )
                PrimCount /= 3;
            if( D3DPT_LINELIST == PrimType )
                PrimCount /= 2;
            if( D3DPT_TRIANGLESTRIP == PrimType )
                PrimCount = ( PrimCount - 3 ) + 1;
            if( D3DPT_LINESTRIP == PrimType )
                PrimCount -= 1;

            pd3dDevice->DrawIndexedPrimitive( PrimType, VertexStart, 0, VertexCount, IndexStart, PrimCount );
        }

        pEffect->EndPass();
    }

    pEffect->End();

    pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    pd3dDevice->SetStreamSourceFreq( 0, 1 );
    pd3dDevice->SetStreamSourceFreq( 1, 1 );
}


//--------------------------------------------------------------------------------------
// RenderGrass
//--------------------------------------------------------------------------------------
void RenderGrass( LPDIRECT3DDEVICE9 pd3dDevice )
{
    HRESULT hr = S_OK;

    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    // set vb streams
    //pd3dDevice->SetStreamSource( 0, g_BallMesh.GetVB9(0,0), 0, g_BallMesh.GetVertexStride(0,0) );
    pd3dDevice->SetStreamSource( 1, g_pGrassDataVB, 0, sizeof( D3DXVECTOR3 ) );
    pd3dDevice->SetStreamSourceFreq( 0, ( D3DSTREAMSOURCE_INDEXEDDATA | g_NumGrassTiles ) );
    pd3dDevice->SetStreamSourceFreq( 1, ( D3DSTREAMSOURCE_INSTANCEDATA | 1ul ) );

    SetNumVisibleGrassTiles( g_NumGrassTiles );

    // set effect variables
    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWVP );
    g_pEffect9->SetFloat( g_hfWorldScale, g_fWorldScale );
    g_pEffect9->SetFloat( g_hfHeightScale, g_fHeightScale );
    g_pEffect9->SetRawValue( g_hvEyePt, &vEye, 0, sizeof( D3DXVECTOR3 ) );
    g_pEffect9->SetFloat( g_hfFadeStart, g_fFadeStart );
    g_pEffect9->SetFloat( g_hfFadeEnd, g_fFadeEnd );

    g_pEffect9->SetTexture( g_htxDiffuse, g_pGrassTex );
    g_pEffect9->SetTexture( g_htxHeight, g_pHeightTex );
    g_pEffect9->SetTexture( g_htxMask, g_pMaskTex );
    g_pEffect9->SetTexture( g_htxShadeNormals, g_pShadeNormalTex );

    g_pEffect9->SetTechnique( g_hRenderGrass );

    pd3dDevice->SetVertexDeclaration( g_pGrassDecl );

    UINT cPasses = 0;
    V( g_pEffect9->Begin( &cPasses, 0 ) );

    for( UINT iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect9->BeginPass( iPass ) );

        g_Terrain.RenderGrass( &vDir, g_NumGrassTiles );

        V( g_pEffect9->EndPass() );
    }
    V( g_pEffect9->End() );

    pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
    pd3dDevice->SetStreamSourceFreq( 0, 1 );
    pd3dDevice->SetStreamSourceFreq( 1, 1 );
}


//--------------------------------------------------------------------------------------
// RenderSky
//--------------------------------------------------------------------------------------
void RenderSky( LPDIRECT3DDEVICE9 pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    D3DXMatrixRotationY( &mWorld, -D3DX_PI / 2.5f );
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    mView._41 = mView._42 = mView._43 = 0.0f;
    D3DXMATRIX mWVP = mWorld * mCamWorld * mView * mProj;

    g_pEffect9->SetMatrix( g_hmWorldViewProj, &mWVP );
    g_pEffect9->SetMatrix( g_hmWorld, &mWorld );

    pd3dDevice->SetVertexDeclaration( g_pBasicDecl );
    g_SkyMesh.Render( pd3dDevice, g_pEffect9, g_hRenderSky, g_htxDiffuse );
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
        D3DXVECTOR4 vLightDir( -1,1,-1,1 );
        D3DXVec4Normalize( &vLightDir, &vLightDir );
        g_pEffect9->SetVector( g_hvWorldLightDir, &vLightDir );
        g_pEffect9->SetFloat( g_hfTime, ( float )fTime );
        g_pEffect9->SetFloat( g_hfElapsedTime, fElapsedTime );

        VisibilityCullTiles();

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Sky" );
        RenderSky( pd3dDevice );
        DXUT_EndPerfEvent();

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Terrain" );
        RenderTerrain( pd3dDevice );
        DXUT_EndPerfEvent();

        if( g_bRenderBalls )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Balls" );
            RenderBalls( pd3dDevice );
            DXUT_EndPerfEvent();
        }

        if( g_bRenderGrass )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Grass" );
            RenderGrass( pd3dDevice );
            DXUT_EndPerfEvent();
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
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pBasicDecl );
    SAFE_RELEASE( g_pBallDecl );
    SAFE_RELEASE( g_pGrassDecl );
    SAFE_RELEASE( g_pHeightTex );
    SAFE_RELEASE( g_pNormalTex );
    SAFE_RELEASE( g_pGrassTex );
    SAFE_RELEASE( g_pDirtTex );
    SAFE_RELEASE( g_pGroundGrassTex );
    SAFE_RELEASE( g_pMaskTex );
    SAFE_RELEASE( g_pShadeNormalTex );

    g_Terrain.OnLostDevice();
    g_BallMesh.Destroy();
    g_SkyMesh.Destroy();
    SAFE_RELEASE( g_pStreamDataVB );
    SAFE_RELEASE( g_pGrassDataVB );
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


//--------------------------------------------------------------------------------------
// Get the last time the file was modified
//--------------------------------------------------------------------------------------
FILETIME GetFileTime( WCHAR* strFile )
{
    WIN32_FIND_DATA FindData;
    HANDLE hFile = FindFirstFile( strFile, &FindData );
    FindClose( hFile );
    return FindData.ftLastWriteTime;
}


//--------------------------------------------------------------------------------------
// Quick and Dirty, return the number of hardware threads
//--------------------------------------------------------------------------------------
UINT GetSystemHWThreadCount()
{
    DWORD_PTR dwProcessMask;
    DWORD_PTR dwSystemMask;

    if( !GetProcessAffinityMask( GetCurrentProcess(), &dwProcessMask, &dwSystemMask ) )
        return 1;

    UINT NumProc = 0;
    for( UINT64 i = 0; i < sizeof( DWORD_PTR ) * 8; i++ )
    {
        if( dwProcessMask & 1ull << i )
            NumProc ++;
    }

    return NumProc;
}


//--------------------------------------------------------------------------------------
// Load an effect file
//--------------------------------------------------------------------------------------
HRESULT LoadEffect( LPDIRECT3DDEVICE9 pd3dDevice )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pEffect9 );

    g_fLastWriteTime = GetFileTime( g_strEffect );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXFX_LARGEADDRESSAWARE;
    dwShaderFlags |= D3DXSHADER_DEBUG;

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Exercise02.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect9, NULL ) );

    // Get the effect variable handles
    g_hRenderTerrain = g_pEffect9->GetTechniqueByName( "RenderTerrain" );
    g_hRenderBall = g_pEffect9->GetTechniqueByName( "RenderBall" );
    g_hRenderGrass = g_pEffect9->GetTechniqueByName( "RenderGrass" );
    g_hRenderSky = g_pEffect9->GetTechniqueByName( "RenderSky" );

    g_hmWorldViewProj = g_pEffect9->GetParameterByName( NULL, "g_mWorldViewProj" );
    g_hmViewProj = g_pEffect9->GetParameterByName( NULL, "g_mViewProj" );
    g_hmWorld = g_pEffect9->GetParameterByName( NULL, "g_mWorld" );
    g_hvWorldLightDir = g_pEffect9->GetParameterByName( NULL, "g_vWorldLightDir" );
    g_hfTime = g_pEffect9->GetParameterByName( NULL, "g_fTime" );
    g_hfElapsedTime = g_pEffect9->GetParameterByName( NULL, "g_fElapsedTime" );
    g_hvColor = g_pEffect9->GetParameterByName( NULL, "g_vColor" );
    g_hfWorldScale = g_pEffect9->GetParameterByName( NULL, "g_fWorldScale" );
    g_hfHeightScale = g_pEffect9->GetParameterByName( NULL, "g_fHeightScale" );
    g_hvEyePt = g_pEffect9->GetParameterByName( NULL, "g_vEyePt" );
    g_hfFadeStart = g_pEffect9->GetParameterByName( NULL, "g_fFadeStart" );
    g_hfFadeEnd = g_pEffect9->GetParameterByName( NULL, "g_fFadeEnd" );

    g_htxDiffuse = g_pEffect9->GetParameterByName( NULL, "g_txDiffuse" );
    g_htxNormal = g_pEffect9->GetParameterByName( NULL, "g_txNormal" );
    g_htxHeight = g_pEffect9->GetParameterByName( NULL, "g_txHeight" );
    g_htxDirt = g_pEffect9->GetParameterByName( NULL, "g_txDirt" );
    g_htxGrass = g_pEffect9->GetParameterByName( NULL, "g_txGrass" );
    g_htxMask = g_pEffect9->GetParameterByName( NULL, "g_txMask" );
    g_htxShadeNormals = g_pEffect9->GetParameterByName( NULL, "g_txShadeNormals" );

    return hr;
}


//--------------------------------------------------------------------------------------
void ResetBalls()
{
    for( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        D3DXVECTOR3 randPos( RPercent() * g_fWorldScale / 2.0f, 0.0f, RPercent() * g_fWorldScale / 2.0f );
        g_pPlayers[i].SetTerrain( &g_Terrain );
        g_pPlayers[i].SetRadius( 0.5f );
        g_pPlayers[i].SetID( i );
        g_pPlayers[i].SetPosition( &randPos );
    }
    D3DXVECTOR3 zero( 0,0,0 );
    g_pPlayers[0].SetPosition( &zero );
}


//--------------------------------------------------------------------------------------
void InitMemoryMapping()
{
    g_hMapFile = CreateFileMapping( INVALID_HANDLE_VALUE,
                                    NULL,
                                    PAGE_READWRITE,
                                    0,
                                    sizeof( SHARED_MEMORY_DATA ),
                                    g_szSharedName );
    if( NULL == g_hMapFile )
        return;

    g_pSharedData = ( SHARED_MEMORY_DATA* )MapViewOfFile( g_hMapFile,
                                                          FILE_MAP_WRITE,
                                                          0,
                                                          0,
                                                          sizeof( SHARED_MEMORY_DATA ) );
    if( NULL == g_pSharedData )
        return;
}


//--------------------------------------------------------------------------------------
void UninitMemoryMapping()
{
    UnmapViewOfFile( g_pSharedData );
    CloseHandle( g_hMapFile );
}


//--------------------------------------------------------------------------------------
void SetNumVisibleTiles( UINT iVal )
{
    if( g_pSharedData )
        g_pSharedData->NumTerrainTiles = iVal;
}


//--------------------------------------------------------------------------------------
void SetNumVisibleGrassTiles( UINT iVal )
{
    if( g_pSharedData )
        g_pSharedData->NumGrassTiles = iVal;
}


//--------------------------------------------------------------------------------------
void SetNumVisibleBalls( UINT iVal )
{
    if( g_pSharedData )
        g_pSharedData->NumBalls = iVal;
}


//--------------------------------------------------------------------------------------
void SetMaxGridBalls( UINT iVal )
{
    if( g_pSharedData )
        g_pSharedData->MaxBalls = iVal;
}
