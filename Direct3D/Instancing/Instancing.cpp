//--------------------------------------------------------------------------------------
// File: Instancing.cpp
//
// Demonstrates the geometry instancing capability of VS3.0-capable hardware by replicating
// a single box model into a pile of boxes all in one DrawIndexedPrimitive call.
// Also shows alternate ways of doing instancing on non-vs_3_0 HW
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*                      g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                    g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                    g_pEffect = NULL;       // D3DX effect interface
CModelViewerCamera              g_Camera;               // A model viewing camera
bool                            g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager      g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;          // Device settings dialog
CDXUTDialog                     g_HUD;                  // dialog for standard controls
CDXUTDialog                     g_SampleUI;             // dialog for sample specific controls
CDXUTDialog                     g_SampleUI2;             // dialog for sample specific controls
bool                            g_bAppIsDebug = false;  // The application build is debug
bool                            g_bRuntimeDebug = false;    // The D3D Debug Runtime was detected
const int                       g_nMaxBoxes = 1000;     // max number of boxes to render
int                             g_NumBoxes = 500;       // current number of boxes
const int                       g_nNumBatchInstance = 120;  // number of batched instances; must be the same size as g_nNumBatchInstance from .fx file
int                             g_iUpdateCPUUsageMessage = 0;   // controls when to update the CPU usage static control
double                          g_fBurnAmount = 0.0;    // time in seconds to burn for during each burn period
D3DXHANDLE                      g_HandleTechnique;      // handle to the rendering technique
D3DXHANDLE                      g_HandleWorld, g_HandleView, g_HandleProjection;    // handle to the world/view/projection matrixes
D3DXHANDLE                      g_HandleTexture;        // handle to the box texture
D3DXHANDLE                      g_HandleBoxInstance_Position;   // handle to the box instance position (array or the value itself)
D3DXHANDLE                      g_HandleBoxInstance_Color;      // handle to the box instance color (array or the value itself)
IDirect3DTexture9*              g_pBoxTexture;          // Texture covering the box
WORD                            g_iRenderTechnique = 1; // the currently selected rendering technique
D3DXVECTOR4 g_vBoxInstance_Position[ g_nMaxBoxes ]; // instance data used in shader instancing (position)
D3DXCOLOR g_vBoxInstance_Color[ g_nMaxBoxes ];    // instance data used in shader instancing (color)
double                          g_fLastTime = 0.0;
int                             g_nShowUI = 2;              // If 2, show UI.  If 1, show text only.  If 0, show no UI.

//--------------------------------------------------------------------------------------
//This VB holds float-valued normal, position and texture coordinates for the model (in this case, a box)
//This will be stream 0
IDirect3DVertexBuffer9*         g_pVBBox = 0;
IDirect3DIndexBuffer9*          g_pIBBox = 0;

// Format of the box vertices for HW instancing
struct BOX_VERTEX
{
    D3DXVECTOR3 pos;    // Position of the vertex
    D3DXVECTOR3 norm;   // Normal at this vertex
    float u, v;         // Texture coordinates u,v
};

// Format of the box vertices for shader instancing
struct BOX_VERTEX_INSTANCE
{
    D3DXVECTOR3 pos;    // Position of the vertex
    D3DXVECTOR3 norm;   // Normal at this vertex
    float u, v;         // Texture coordinates u,v
    float boxInstance;  // Box instance index
};


//--------------------------------------------------------------------------------------
//This VB holds the positions for copies of the basic box within the pile of boxes.
//The positions are expressed as bytes in a DWORD:
//  byte 0: x position
//  byte 1: y position
//  byte 3: height of box
//  byte 4: azimuthal rotation of box
//This will be stream 1
IDirect3DVertexBuffer9*         g_pVBInstanceData = 0;

//Here is the format of the box positions within the pile:
struct BOX_INSTANCEDATA_POS
{
    D3DCOLOR color;
    BYTE x;
    BYTE y;
    BYTE z;
    BYTE rotation;
};


//--------------------------------------------------------------------------------------
// Vertex Declaration for Hardware Instancing
IDirect3DVertexDeclaration9*    g_pVertexDeclHardware = NULL;
D3DVERTEXELEMENT9 g_VertexElemHardware[] =
{
    { 0, 0,     D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION,  0 },
    { 0, 3 * 4, D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,    0 },
    { 0, 6 * 4, D3DDECLTYPE_FLOAT2,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  0 },
    { 1, 0,     D3DDECLTYPE_D3DCOLOR,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,     0 },
    { 1, 4,     D3DDECLTYPE_D3DCOLOR,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,     1 },
    D3DDECL_END()
};

// Vertex Declaration for Shader Instancing
IDirect3DVertexDeclaration9*    g_pVertexDeclShader = NULL;
D3DVERTEXELEMENT9 g_VertexElemShader[] =
{
    { 0, 0,     D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION,  0 },
    { 0, 3 * 4, D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,    0 },
    { 0, 6 * 4, D3DDECLTYPE_FLOAT2,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  0 },
    { 0, 8 * 4, D3DDECLTYPE_FLOAT1,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  1 },
    D3DDECL_END()
};

// Vertex Declaration for Constants Instancing
IDirect3DVertexDeclaration9*    g_pVertexDeclConstants = NULL;
D3DVERTEXELEMENT9 g_VertexElemConstants[] =
{
    { 0, 0,     D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION,  0 },
    { 0, 3 * 4, D3DDECLTYPE_FLOAT3,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,    0 },
    { 0, 6 * 4, D3DDECLTYPE_FLOAT2,     D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_NUMBERBOXES_STATIC  5
#define IDC_NUMBERBOXES_SLIDER  6
#define IDC_RENDERMETHOD_LIST   7
#define IDC_BIND_CPU_STATIC     8
#define IDC_BIND_CPU_SLIDER     9
#define IDC_STATIC              10
#define IDC_CPUUSAGE_STATIC     11
#define IDC_TOGGLEUI            12


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
HRESULT OnCreateBuffers( IDirect3DDevice9* pd3dDevice );
void OnDestroyBuffers();


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
    DXUTCreateWindow( L"Instancing" );
    DXUTCreateDevice( true, 640, 480 );

    // Performance observations should not be compared against dis-similar builds (debug v retail)
#if defined(DEBUG) | defined(_DEBUG)
    g_bAppIsDebug = true;
#endif

    // Performance observations should not be compared against dis-similar d3d runtimes (debug v retail)
    if( GetModuleHandleW( L"d3d9d.dll" ) )
        g_bRuntimeDebug = true;

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
    g_SampleUI.Init( &g_DialogResourceManager );
    g_SampleUI2.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEUI, L"Toggle UI (U)", 35, iY += 24, 125, 22 );

    WCHAR szMessage[300];
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 0;
    g_SampleUI2.SetCallback( OnGUIEvent ); iY = 24;

    swprintf_s( szMessage, 300, L"# Boxes: %d", g_NumBoxes );
    g_SampleUI.AddStatic( IDC_NUMBERBOXES_STATIC, szMessage, 0, iY += 24, 200, 22 );
    g_SampleUI.AddSlider( IDC_NUMBERBOXES_SLIDER, 50, iY += 24, 100, 22, 1, g_nMaxBoxes, g_NumBoxes );

    iY += 24;
    wcscpy_s( szMessage, 300, L"Goal: 0ms/frame" );
    g_SampleUI.AddStatic( IDC_BIND_CPU_STATIC, szMessage, 0, iY += 12, 200, 22 );
    wcscpy_s( szMessage, 300, L"Remaining for logic: 0" );
    g_SampleUI.AddStatic( IDC_CPUUSAGE_STATIC, szMessage, 0, iY += 18, 200, 22 );
    g_SampleUI.AddSlider( IDC_BIND_CPU_SLIDER, 50, iY += 24, 100, 22, 0, 166, 0 );


    wcscpy_s( szMessage, 300, L"If rendering takes less\n"
                   L"time than goal, then remaining\n"
                   L"time is spent to represent\n"
                   L"game logic. More time\n"
                   L"remaining means technique\n"
                   L"takes less CPU time\n" );
    g_SampleUI.AddStatic( IDC_STATIC, szMessage, 10, iY += 12 * 3, 170, 100 );

    g_SampleUI2.AddComboBox( IDC_RENDERMETHOD_LIST, 0, 0, 166, 22 );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->SetDropHeight( 12 * 4 );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->AddItem( L"Hardware Instancing", NULL );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->AddItem( L"Shader Instancing", NULL );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->AddItem( L"Constants Instancing", NULL );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->AddItem( L"Stream Instancing", NULL );
    g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->SetSelectedByIndex( g_iRenderTechnique );

    g_SampleUI.EnableKeyboardInput( true );
    g_SampleUI.FocusDefaultControl();
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

    // Needs PS 2.0 support
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // Needs to be DDI9 (support vertex buffer offset)
    if( !( pCaps->DevCaps2 & D3DDEVCAPS2_STREAMOFFSET ) )
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
#ifdef DEBUG_PS
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif
#ifdef DEBUG_VS
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_MIXED_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif

    if( pDeviceSettings->d3d9.pp.Windowed == FALSE )
        pDeviceSettings->d3d9.pp.SwapEffect = D3DSWAPEFFECT_FLIP;
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
// Helper routine to fill faces of a cube
//--------------------------------------------------------------------------------------
void FillFace(
BOX_VERTEX* pVerts,
WORD* pIndices,
int iFace,
D3DXVECTOR3 vCenter,
D3DXVECTOR3 vNormal,
D3DXVECTOR3 vUp )
{
    D3DXVECTOR3 vRight;
    D3DXVec3Cross( &vRight, &vNormal, &vUp );

    pIndices[ iFace * 6 + 0 ] = (WORD)( iFace * 4 + 0 );
    pIndices[ iFace * 6 + 1 ] = (WORD)( iFace * 4 + 1 );
    pIndices[ iFace * 6 + 2 ] = (WORD)( iFace * 4 + 2 );
    pIndices[ iFace * 6 + 3 ] = (WORD)( iFace * 4 + 3 );
    pIndices[ iFace * 6 + 4 ] = (WORD)( iFace * 4 + 2 );
    pIndices[ iFace * 6 + 5 ] = (WORD)( iFace * 4 + 1 );

    pVerts[ iFace * 4 + 0 ].pos = vCenter + vRight + vUp;
    pVerts[ iFace * 4 + 1 ].pos = vCenter + vRight - vUp;
    pVerts[ iFace * 4 + 2 ].pos = vCenter - vRight + vUp;
    pVerts[ iFace * 4 + 3 ].pos = vCenter - vRight - vUp;

    for( int i = 0; i < 4; i++ )
    {
        pVerts[ iFace * 4 + i ].u = ( float )( ( i / 2 ) & 1 ) * 1.0f;
        pVerts[ iFace * 4 + i ].v = ( float )( ( i )&1 ) * 1.0f;
        pVerts[ iFace * 4 + i ].norm = vNormal;
    }
}


//--------------------------------------------------------------------------------------
// Helper routine to fill faces of a cube + instance data
//--------------------------------------------------------------------------------------
void FillFaceInstance(
BOX_VERTEX_INSTANCE* pVerts,
WORD* pIndices,
int iFace,
D3DXVECTOR3 vCenter,
D3DXVECTOR3 vNormal,
D3DXVECTOR3 vUp,
WORD iInstanceIndex )
{
    D3DXVECTOR3 vRight;
    D3DXVec3Cross( &vRight, &vNormal, &vUp );

    WORD offsetIndex = iInstanceIndex * 6 * 2 * 3;  // offset from the beginning of the index array
    WORD offsetVertex = iInstanceIndex * 4 * 6;     // offset from the beginning of the vertex array

    pIndices[ offsetIndex + iFace * 6 + 0 ] = (WORD)( offsetVertex + iFace * 4 + 0 );
    pIndices[ offsetIndex + iFace * 6 + 1 ] = (WORD)( offsetVertex + iFace * 4 + 1 );
    pIndices[ offsetIndex + iFace * 6 + 2 ] = (WORD)( offsetVertex + iFace * 4 + 2 );
    pIndices[ offsetIndex + iFace * 6 + 3 ] = (WORD)( offsetVertex + iFace * 4 + 3 );
    pIndices[ offsetIndex + iFace * 6 + 4 ] = (WORD)( offsetVertex + iFace * 4 + 2 );
    pIndices[ offsetIndex + iFace * 6 + 5 ] = (WORD)( offsetVertex + iFace * 4 + 1 );

    pVerts[ offsetVertex + iFace * 4 + 0 ].pos = vCenter + vRight + vUp;
    pVerts[ offsetVertex + iFace * 4 + 1 ].pos = vCenter + vRight - vUp;
    pVerts[ offsetVertex + iFace * 4 + 2 ].pos = vCenter - vRight + vUp;
    pVerts[ offsetVertex + iFace * 4 + 3 ].pos = vCenter - vRight - vUp;

    for( int i = 0; i < 4; i++ )
    {
        pVerts[ offsetVertex + iFace * 4 + i ].boxInstance = ( float )iInstanceIndex;
        pVerts[ offsetVertex + iFace * 4 + i ].u = ( float )( ( i / 2 ) & 1 ) * 1.0f;
        pVerts[ offsetVertex + iFace * 4 + i ].v = ( float )( ( i )&1 ) * 1.0f;
        pVerts[ offsetVertex + iFace * 4 + i ].norm = vNormal;
    }
}


//--------------------------------------------------------------------------------------
// Create the vertex and index buffers, based on the selected instancing method
// called from onCreateDevice or whenever user changes parameters or rendering method
//--------------------------------------------------------------------------------------
HRESULT OnCreateBuffers( IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;

    if( 0 == g_iRenderTechnique || 3 == g_iRenderTechnique )
    {
        g_HandleTechnique = g_pEffect->GetTechniqueByName( "THW_Instancing" );

        // Create the vertex declaration
        V_RETURN( pd3dDevice->CreateVertexDeclaration( g_VertexElemHardware, &g_pVertexDeclHardware ) );
    }

    if( 1 == g_iRenderTechnique )
    {
        g_HandleTechnique = g_pEffect->GetTechniqueByName( "TShader_Instancing" );
        g_HandleBoxInstance_Position = g_pEffect->GetParameterBySemantic( NULL, "BOXINSTANCEARRAY_POSITION" );
        g_HandleBoxInstance_Color = g_pEffect->GetParameterBySemantic( NULL, "BOXINSTANCEARRAY_COLOR" );

        // First create the vertex declaration we need
        V_RETURN( pd3dDevice->CreateVertexDeclaration( g_VertexElemShader, &g_pVertexDeclShader ) );

        // Build a VB to hold the position data. Four vertices on six faces of the box
        // Create g_nNumBatchInstance copies
        V_RETURN( pd3dDevice->CreateVertexBuffer( g_nNumBatchInstance * 4 * 6 * sizeof( BOX_VERTEX_INSTANCE ), 0, 0,
                                                  D3DPOOL_MANAGED, &g_pVBBox, 0 ) );

        // And an IB to go with it. We will be rendering
        // a list of independent tris, and each box face has two tris, so the box will have
        // 6 * 2 * 3 indices
        V_RETURN( pd3dDevice->CreateIndexBuffer( g_nNumBatchInstance * ( 6 * 2 * 3 ) * sizeof( WORD ), 0,
                                                 D3DFMT_INDEX16, D3DPOOL_MANAGED, &g_pIBBox, 0 ) );

        // Now, lock and fill the model VB and IB
        BOX_VERTEX_INSTANCE* pVerts;

        hr = g_pVBBox->Lock( 0, NULL, ( void** )&pVerts, 0 );

        if( SUCCEEDED( hr ) )
        {
            WORD* pIndices;
            hr = g_pIBBox->Lock( 0, NULL, ( void** )&pIndices, 0 );

            if( SUCCEEDED( hr ) )
            {
                for( WORD iInstance = 0; iInstance < g_nNumBatchInstance; iInstance++ )
                {
                    FillFaceInstance( pVerts, pIndices, 0,
                                      D3DXVECTOR3( 0.f, 0.f, -1.f ),
                                      D3DXVECTOR3( 0.f, 0.f, -1.f ),
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      iInstance );

                    FillFaceInstance( pVerts, pIndices, 1,
                                      D3DXVECTOR3( 0.f, 0.f, 1.f ),
                                      D3DXVECTOR3( 0.f, 0.f, 1.f ),
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      iInstance );

                    FillFaceInstance( pVerts, pIndices, 2,
                                      D3DXVECTOR3( 1.f, 0.f, 0.f ),
                                      D3DXVECTOR3( 1.f, 0.f, 0.f ),
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      iInstance );

                    FillFaceInstance( pVerts, pIndices, 3,
                                      D3DXVECTOR3( -1.f, 0.f, 0.f ),
                                      D3DXVECTOR3( -1.f, 0.f, 0.f ),
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      iInstance );

                    FillFaceInstance( pVerts, pIndices, 4,
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      D3DXVECTOR3( 0.f, 1.f, 0.f ),
                                      D3DXVECTOR3( 1.f, 0.f, 0.f ),
                                      iInstance );

                    FillFaceInstance( pVerts, pIndices, 5,
                                      D3DXVECTOR3( 0.f, -1.f, 0.f ),
                                      D3DXVECTOR3( 0.f, -1.f, 0.f ),
                                      D3DXVECTOR3( 1.f, 0.f, 0.f ),
                                      iInstance );
                }

                g_pIBBox->Unlock();
            }
            else
            {
                DXUT_ERR( L"Could not lock box model IB", hr );
            }
            g_pVBBox->Unlock();
        }
        else
        {
            DXUT_ERR( L"Could not lock box model VB", hr );
        }
    }

    if( 2 == g_iRenderTechnique )
    {
        g_HandleTechnique = g_pEffect->GetTechniqueByName( "TConstants_Instancing" );
        g_HandleBoxInstance_Position = g_pEffect->GetParameterBySemantic( NULL, "BOXINSTANCE_POSITION" );
        g_HandleBoxInstance_Color = g_pEffect->GetParameterBySemantic( NULL, "BOXINSTANCE_COLOR" );

        // Create the vertex declaration
        V_RETURN( pd3dDevice->CreateVertexDeclaration( g_VertexElemConstants, &g_pVertexDeclConstants ) );
    }

    if( 1 != g_iRenderTechnique )
    {
        // Build a VB to hold the position data. Four vertices on six faces of the box
        V_RETURN( pd3dDevice->CreateVertexBuffer( 4 * 6 * sizeof( BOX_VERTEX ), 0, 0, D3DPOOL_MANAGED, &g_pVBBox,
                                                  0 ) );

        // And an IB to go with it. We will be rendering
        // a list of independent tris, and each box face has two tris, so the box will have
        // 6 * 2 * 3 indices
        V_RETURN( pd3dDevice->CreateIndexBuffer( ( 6 * 2 * 3 ) * sizeof( WORD ), 0, D3DFMT_INDEX16,
                                                 D3DPOOL_MANAGED, &g_pIBBox, 0 ) );

        // Now, lock and fill the model VB and IB:
        BOX_VERTEX* pVerts;

        hr = g_pVBBox->Lock( 0, NULL, ( void** )&pVerts, 0 );

        if( SUCCEEDED( hr ) )
        {
            WORD* pIndices;
            hr = g_pIBBox->Lock( 0, NULL, ( void** )&pIndices, 0 );

            if( SUCCEEDED( hr ) )
            {
                FillFace( pVerts, pIndices, 0,
                          D3DXVECTOR3( 0.f, 0.f, -1.f ),
                          D3DXVECTOR3( 0.f, 0.f, -1.f ),
                          D3DXVECTOR3( 0.f, 1.f, 0.f ) );

                FillFace( pVerts, pIndices, 1,
                          D3DXVECTOR3( 0.f, 0.f, 1.f ),
                          D3DXVECTOR3( 0.f, 0.f, 1.f ),
                          D3DXVECTOR3( 0.f, 1.f, 0.f ) );

                FillFace( pVerts, pIndices, 2,
                          D3DXVECTOR3( 1.f, 0.f, 0.f ),
                          D3DXVECTOR3( 1.f, 0.f, 0.f ),
                          D3DXVECTOR3( 0.f, 1.f, 0.f ) );

                FillFace( pVerts, pIndices, 3,
                          D3DXVECTOR3( -1.f, 0.f, 0.f ),
                          D3DXVECTOR3( -1.f, 0.f, 0.f ),
                          D3DXVECTOR3( 0.f, 1.f, 0.f ) );

                FillFace( pVerts, pIndices, 4,
                          D3DXVECTOR3( 0.f, 1.f, 0.f ),
                          D3DXVECTOR3( 0.f, 1.f, 0.f ),
                          D3DXVECTOR3( 1.f, 0.f, 0.f ) );

                FillFace( pVerts, pIndices, 5,
                          D3DXVECTOR3( 0.f, -1.f, 0.f ),
                          D3DXVECTOR3( 0.f, -1.f, 0.f ),
                          D3DXVECTOR3( 1.f, 0.f, 0.f ) );

                g_pIBBox->Unlock();
            }
            else
            {
                DXUT_ERR( L"Could not lock box model IB", hr );
            }
            g_pVBBox->Unlock();
        }
        else
        {
            DXUT_ERR( L"Could not lock box model VB", hr );
        }
    }

    if( 0 == g_iRenderTechnique || 3 == g_iRenderTechnique )
    {
        // Create a  VB for the instancing data
        V_RETURN( pd3dDevice->CreateVertexBuffer( g_NumBoxes * sizeof( BOX_INSTANCEDATA_POS ), 0, 0,
                                                  D3DPOOL_MANAGED, &g_pVBInstanceData, 0 ) );

        // Lock and fill the instancing buffer
        BOX_INSTANCEDATA_POS* pIPos;
        hr = g_pVBInstanceData->Lock( 0, NULL, ( void** )&pIPos, 0 );

        if( SUCCEEDED( hr ) )
        {
            int nRemainingBoxes = g_NumBoxes;
            for( BYTE iY = 0; iY < 10; iY++ )
                for( BYTE iZ = 0; iZ < 10; iZ++ )
                    for( BYTE iX = 0; iX < 10 && nRemainingBoxes > 0; iX++, nRemainingBoxes-- )
                    {
                        // Scaled so 1 box is "32" wide, the intra-pos range is 8 box widths.
                        // The positins are scaled and biased.
                        // The rotations are not
                        BOX_INSTANCEDATA_POS instanceBox;
                        instanceBox.color = D3DCOLOR_ARGB( 0xff, 0x7f + 0x40 * ( ( g_NumBoxes - nRemainingBoxes +
                                                                                   iX ) % 3 ), 0x7f + 0x40 *
                                                           ( ( g_NumBoxes - nRemainingBoxes + iZ + 1 ) % 3 ),
                                                           0x7f + 0x40 * ( ( g_NumBoxes - nRemainingBoxes + iY +
                                                                             2 ) % 3 ) );
                        instanceBox.x = iZ * 24;
                        instanceBox.z = iX * 24;
                        instanceBox.y = iY * 24;
                        instanceBox.rotation = ( ( WORD )iX + ( WORD )iZ + ( WORD )iY ) * 24 / 3;
                        *pIPos = instanceBox, pIPos++;
                    }
            g_pVBInstanceData->Unlock();
        }
        else
        {
            DXUT_ERR( L"Could not lock box intra-pile-position VB", hr );
        }
    }

    if( 1 == g_iRenderTechnique || 2 == g_iRenderTechnique )
    {
        // Fill the instancing constants buffer
        int nRemainingBoxes = g_NumBoxes;
        for( BYTE iY = 0; iY < 10; iY++ )
            for( BYTE iZ = 0; iZ < 10; iZ++ )
                for( BYTE iX = 0; iX < 10 && nRemainingBoxes > 0; iX++, nRemainingBoxes-- )
                {
                    // Scaled so 1 box is "32" wide, the intra-pos range is 8 box widths.
                    // The positins are scaled and biased.
                    // The rotations are not
                    g_vBoxInstance_Color[g_NumBoxes - nRemainingBoxes] = D3DCOLOR_ARGB( 0xff, 0x7f + 0x40 *
                                                                                        ( ( g_NumBoxes -
                                                                                            nRemainingBoxes +
                                                                                            iX ) % 3 ), 0x7f + 0x40 *
                                                                                        ( ( g_NumBoxes -
                                                                                            nRemainingBoxes + iZ +
                                                                                            1 ) % 3 ), 0x7f + 0x40 *
                                                                                        ( ( g_NumBoxes -
                                                                                            nRemainingBoxes + iY +
                                                                                            2 ) % 3 ) );
                    g_vBoxInstance_Position[g_NumBoxes - nRemainingBoxes].x = iX * 24 / 255.0f;
                    g_vBoxInstance_Position[g_NumBoxes - nRemainingBoxes].z = iZ * 24 / 255.0f;
                    g_vBoxInstance_Position[g_NumBoxes - nRemainingBoxes].y = iY * 24 / 255.0f;
                    g_vBoxInstance_Position[g_NumBoxes - nRemainingBoxes].w = ( ( WORD )iX + ( WORD )iZ + ( WORD )
                                                                                iY ) * 8 / 255.0f;
                }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Release the vertex and index buffers, based on the selected instancing method
// called from onDestroyDevice or whenever user changes parameters or rendering method
//--------------------------------------------------------------------------------------
void OnDestroyBuffers()
{
    SAFE_RELEASE( g_pVBBox );
    SAFE_RELEASE( g_pIBBox );
    SAFE_RELEASE( g_pVBInstanceData );
    SAFE_RELEASE( g_pVertexDeclHardware );
    SAFE_RELEASE( g_pVertexDeclShader );
    SAFE_RELEASE( g_pVertexDeclConstants );
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
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
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
#ifdef D3DXFX_LARGEADDRESS_HANDLE
    dwShaderFlags |= D3DXFX_LARGEADDRESSAWARE;
#endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Instancing.fx" ) );

    // If this fails, there should be debug output as to 
    // why the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    g_HandleWorld = g_pEffect->GetParameterBySemantic( NULL, "WORLD" );
    g_HandleView = g_pEffect->GetParameterBySemantic( NULL, "VIEW" );
    g_HandleProjection = g_pEffect->GetParameterBySemantic( NULL, "PROJECTION" );
    g_HandleTexture = g_pEffect->GetParameterBySemantic( NULL, "TEXTURE" );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -24.0f, 36.0f, -36.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // create vertex, index buffers and vertex declaration
    V_RETURN( OnCreateBuffers( pd3dDevice ) );

    // Load the environment texture
    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Misc\\env2.bmp" );
    if( FAILED( hr ) )
        return DXTRACE_ERR( L"DXUTFindDXSDKMediaFileCch", hr );
    hr = D3DXCreateTextureFromFile( pd3dDevice, str, &g_pBoxTexture );
    if( FAILED( hr ) )
        return DXTRACE_ERR( L"D3DXCreateTextureFromFile", hr );

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
    g_Camera.SetProjParams( D3DX_PI / 3, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    // Set the camera speed
    g_Camera.SetScalers( 0.01f, 10.0f );

    // Constrain the camera to movement within the horizontal plane
    g_Camera.SetEnableYAxisMovement( false );
    g_Camera.SetEnablePositionMovement( true );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 400 );
    g_SampleUI.SetSize( 170, 400 );
    g_SampleUI2.SetLocation( 0, 100 );
    g_SampleUI2.SetSize( 200, 100 );

    g_fLastTime = DXUTGetGlobalTimer()->GetTime();

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
    // Burn CPU time       
    WCHAR szMessage[100];
    WCHAR szTmp[100];
    double fStartTime = DXUTGetGlobalTimer()->GetTime();
    double fCurTime = fStartTime;
    double fStopTime = g_fLastTime + g_fBurnAmount;
    double fCPUUsage;

    while( fCurTime < fStopTime )
    {
        fCurTime = DXUTGetGlobalTimer()->GetTime();
        double fTmp = fStartTime / g_fLastTime * 100.0f;
        swprintf_s( szTmp, 100, L"Test %d", ( int )( fTmp + 0.5f ) );
    }

    if( fCurTime - g_fLastTime > 0.00001f )
        fCPUUsage = ( fCurTime - fStartTime ) / ( fCurTime - g_fLastTime ) * 100.0f;
    else
        fCPUUsage = FLT_MAX;

    g_fLastTime = DXUTGetGlobalTimer()->GetTime();
    swprintf_s( szMessage, 100, L"Remaining for logic: %d%%", ( int )( fCPUUsage + 0.5f ) ); szMessage[99] = 0;

    // don't update the message every single time
    if( 0 == g_iUpdateCPUUsageMessage )
        g_SampleUI.GetStatic( IDC_CPUUSAGE_STATIC )->SetText( szMessage );
    g_iUpdateCPUUsageMessage = ( g_iUpdateCPUUsageMessage + 1 ) % 100;

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// This callback function is called by OnFrameRender
// It performs HW instancing
//--------------------------------------------------------------------------------------
void OnRenderHWInstancing( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;
    UINT iPass, cPasses;

    V( pd3dDevice->SetVertexDeclaration( g_pVertexDeclHardware ) );

    // Stream zero is our model, and its frequency is how we communicate the number of instances required,
    // which in this case is the total number of boxes
    V( pd3dDevice->SetStreamSource( 0, g_pVBBox, 0, sizeof( BOX_VERTEX ) ) );
    V( pd3dDevice->SetStreamSourceFreq( 0, D3DSTREAMSOURCE_INDEXEDDATA | g_NumBoxes ) );

    // Stream one is the instancing buffer, so this advances to the next value
    // after each box instance has been drawn, so the divider is 1.
    V( pd3dDevice->SetStreamSource( 1, g_pVBInstanceData, 0, sizeof( BOX_INSTANCEDATA_POS ) ) );
    V( pd3dDevice->SetStreamSourceFreq( 1, D3DSTREAMSOURCE_INSTANCEDATA | 1ul ) );

    V( pd3dDevice->SetIndices( g_pIBBox ) );

    // Render the scene with this technique
    // as defined in the .fx file
    V( g_pEffect->SetTechnique( g_HandleTechnique ) );

    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect->BeginPass( iPass ) );

        // Render the boxes with the applied technique
        V( g_pEffect->SetTexture( g_HandleTexture, g_pBoxTexture ) );

        // The effect interface queues up the changes and performs them 
        // with the CommitChanges call. You do not need to call CommitChanges if 
        // you are not setting any parameters between the BeginPass and EndPass.
        V( g_pEffect->CommitChanges() );

        V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 4 * 6, 0, 6 * 2 ) );

        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );

    V( pd3dDevice->SetStreamSourceFreq( 0, 1 ) );
    V( pd3dDevice->SetStreamSourceFreq( 1, 1 ) );
}


//--------------------------------------------------------------------------------------
// This callback function is called by OnFrameRender
// It performs Shader instancing
//--------------------------------------------------------------------------------------
void OnRenderShaderInstancing( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;
    UINT iPass, cPasses;

    V( pd3dDevice->SetVertexDeclaration( g_pVertexDeclShader ) );

    // Stream zero is our model
    V( pd3dDevice->SetStreamSource( 0, g_pVBBox, 0, sizeof( BOX_VERTEX_INSTANCE ) ) );
    V( pd3dDevice->SetIndices( g_pIBBox ) );

    // Render the scene with this technique
    // as defined in the .fx file
    V( g_pEffect->SetTechnique( g_HandleTechnique ) );

    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect->BeginPass( iPass ) );

        // Render the boxes with the applied technique
        V( g_pEffect->SetTexture( g_HandleTexture, g_pBoxTexture ) );

        int nRemainingBoxes = g_NumBoxes;
        while( nRemainingBoxes > 0 )
        {
            // determine how many instances are in this batch (up to g_nNumBatchInstance)           
            int nRenderBoxes = min( nRemainingBoxes, g_nNumBatchInstance );

            // set the box instancing array
            V( g_pEffect->SetVectorArray( g_HandleBoxInstance_Position, g_vBoxInstance_Position + g_NumBoxes -
                                          nRemainingBoxes, nRenderBoxes ) );
            V( g_pEffect->SetVectorArray( g_HandleBoxInstance_Color, ( D3DXVECTOR4* )
                                          ( g_vBoxInstance_Color + g_NumBoxes - nRemainingBoxes ), nRenderBoxes ) );

            // The effect interface queues up the changes and performs them 
            // with the CommitChanges call. You do not need to call CommitChanges if 
            // you are not setting any parameters between the BeginPass and EndPass.
            V( g_pEffect->CommitChanges() );

            V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, nRenderBoxes * 4 * 6, 0,
                                                 nRenderBoxes * 6 * 2 ) );

            // subtract the rendered boxes from the remaining boxes
            nRemainingBoxes -= nRenderBoxes;
        }

        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );
}


//--------------------------------------------------------------------------------------
// This callback function is called by OnFrameRender
// It performs Constants instancing
//--------------------------------------------------------------------------------------
void OnRenderConstantsInstancing( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;
    UINT iPass, cPasses;

    V( pd3dDevice->SetVertexDeclaration( g_pVertexDeclConstants ) );

    // Stream zero is our model
    V( pd3dDevice->SetStreamSource( 0, g_pVBBox, 0, sizeof( BOX_VERTEX ) ) );
    V( pd3dDevice->SetIndices( g_pIBBox ) );

    // Render the scene with this technique
    // as defined in the .fx file
    V( g_pEffect->SetTechnique( g_HandleTechnique ) );

    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect->BeginPass( iPass ) );

        // Render the boxes with the applied technique
        V( g_pEffect->SetTexture( g_HandleTexture, g_pBoxTexture ) );

        for( int nRemainingBoxes = 0; nRemainingBoxes < g_NumBoxes; nRemainingBoxes++ )
        {
            // set the box instancing array
            V( g_pEffect->SetVector( g_HandleBoxInstance_Position, &g_vBoxInstance_Position[nRemainingBoxes] ) );
            V( g_pEffect->SetVector( g_HandleBoxInstance_Color,
                                     ( D3DXVECTOR4* )&g_vBoxInstance_Color[nRemainingBoxes] ) );

            // The effect interface queues up the changes and performs them 
            // with the CommitChanges call. You do not need to call CommitChanges if 
            // you are not setting any parameters between the BeginPass and EndPass.
            V( g_pEffect->CommitChanges() );

            V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 4 * 6, 0, 6 * 2 ) );
        }

        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );
}


//--------------------------------------------------------------------------------------
// This callback function is called by OnFrameRender
// It performs Stream instancing
//--------------------------------------------------------------------------------------
void OnRenderStreamInstancing( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    HRESULT hr;
    UINT iPass, cPasses;

    V( pd3dDevice->SetVertexDeclaration( g_pVertexDeclHardware ) );

    // Stream zero is our model
    V( pd3dDevice->SetStreamSource( 0, g_pVBBox, 0, sizeof( BOX_VERTEX ) ) );

    V( pd3dDevice->SetIndices( g_pIBBox ) );

    // Render the scene with this technique
    // as defined in the .fx file
    V( g_pEffect->SetTechnique( g_HandleTechnique ) );

    V( g_pEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( g_pEffect->BeginPass( iPass ) );

        // Render the boxes with the applied technique
        V( g_pEffect->SetTexture( g_HandleTexture, g_pBoxTexture ) );

        // The effect interface queues up the changes and performs them 
        // with the CommitChanges call. You do not need to call CommitChanges if 
        // you are not setting any parameters between the BeginPass and EndPass.
        V( g_pEffect->CommitChanges() );

        for( int nRemainingBoxes = 0; nRemainingBoxes < g_NumBoxes; nRemainingBoxes++ )
        {
            // Stream one is the instancing buffer
            V( pd3dDevice->SetStreamSource( 1, g_pVBInstanceData, nRemainingBoxes * sizeof( BOX_INSTANCEDATA_POS ),
                                            0 ) );

            V( pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 4 * 6, 0, 6 * 2 ) );
        }

        V( g_pEffect->EndPass() );
    }
    V( g_pEffect->End() );
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
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 45, 50, 170 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        // Update the effect's variables
        V( g_pEffect->SetMatrix( g_HandleWorld, &mWorld ) );
        V( g_pEffect->SetMatrix( g_HandleView, &mView ) );
        V( g_pEffect->SetMatrix( g_HandleProjection, &mProj ) );

        switch( g_iRenderTechnique )
        {
            case 0:
                if( DXUTGetD3D9DeviceCaps()->VertexShaderVersion >= D3DVS_VERSION( 3, 0 ) )
                    OnRenderHWInstancing( pd3dDevice, fTime, fElapsedTime );
                break;
            case 1:
                OnRenderShaderInstancing( pd3dDevice, fTime, fElapsedTime );
                break;
            case 2:
                OnRenderConstantsInstancing( pd3dDevice, fTime, fElapsedTime );
                break;
            case 3:
                OnRenderStreamInstancing( pd3dDevice, fTime, fElapsedTime );
        }

        if( g_nShowUI > 0 )
            RenderText();
        if( g_nShowUI > 1 )
        {
            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );
            V( g_SampleUI2.OnRender( fElapsedTime ) );
        }

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
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    if( g_nShowUI < 2 )
        txtHelper.DrawTextLine( L"Press 'U' to toggle UI" );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    if( g_bRuntimeDebug )
        txtHelper.DrawTextLine( L"WARNING (perf): DEBUG D3D runtime detected!" );
    if( g_bAppIsDebug )
        txtHelper.DrawTextLine( L"WARNING (perf): DEBUG application build detected!" );
    if( g_iRenderTechnique == 0 && DXUTGetD3D9DeviceCaps()->VertexShaderVersion < D3DVS_VERSION( 3, 0 ) )
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"ERROR: Hardware instancing is not supported on non vs_3_0 HW" );
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

    if( g_nShowUI > 1 )
    {
        // Give the dialogs a chance to handle the message first
        *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
        *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
        *pbNoFurtherProcessing = g_SampleUI2.MsgProc( hWnd, uMsg, wParam, lParam );
        if( *pbNoFurtherProcessing )
            return 0;
    }

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
            case 'U':
                g_nShowUI--; if( g_nShowUI < 0 ) g_nShowUI = 2;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szMessage[100];
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        case IDC_TOGGLEUI:
            KeyboardProc( 'U', true, false, NULL );
            break;
        case IDC_NUMBERBOXES_SLIDER:
            g_NumBoxes = g_SampleUI.GetSlider( IDC_NUMBERBOXES_SLIDER )->GetValue();
            swprintf_s( szMessage, 100, L"# Boxes: %d", g_NumBoxes ); szMessage[99] = 0;
            g_SampleUI.GetStatic( IDC_NUMBERBOXES_STATIC )->SetText( szMessage );
            OnDestroyBuffers();
            OnCreateBuffers( DXUTGetD3D9Device() );
            break;
        case IDC_BIND_CPU_SLIDER:
            g_fBurnAmount = 0.0001f * g_SampleUI.GetSlider( IDC_BIND_CPU_SLIDER )->GetValue();
            swprintf_s( szMessage, 100, L"Goal: %0.1fms/frame", g_fBurnAmount * 1000.0f ); szMessage[99] = 0;
            g_SampleUI.GetStatic( IDC_BIND_CPU_STATIC )->SetText( szMessage );
            break;
        case IDC_RENDERMETHOD_LIST:
        {
            DXUTComboBoxItem* pCBItem = g_SampleUI2.GetComboBox( IDC_RENDERMETHOD_LIST )->GetSelectedItem();
            g_iRenderTechnique = wcscmp( pCBItem->strText, L"Hardware Instancing" ) == 0? 0 :
                wcscmp( pCBItem->strText, L"Shader Instancing" ) == 0? 1 :
                wcscmp( pCBItem->strText, L"Constants Instancing" ) == 0? 2 :
                wcscmp( pCBItem->strText, L"Stream Instancing" ) == 0? 3 : -1;

            // Note hardware instancing is not supported on non vs_3_0 HW
            // This sample allows it to be set, but displays an error to the user

            // recreate the buffers
            OnDestroyBuffers();
            OnCreateBuffers( DXUTGetD3D9Device() );
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
    SAFE_RELEASE( g_pTextSprite );
    SAFE_RELEASE( g_pBoxTexture );

    // destroy vertex/index buffers, vertex declaration
    OnDestroyBuffers();
}



