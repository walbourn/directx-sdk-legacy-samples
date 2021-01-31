//--------------------------------------------------------------------------------------
// File: ShadowVolume10.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "d3d9Mesh.h"
#include "resource.h"
#include "GenShadowMesh.h"


//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define MAX_NUM_LIGHTS 6
struct QUAD_VERTEX
{
    D3DXVECTOR3 pos;
};
enum RENDER_TYPE
{
    RENDERTYPE_SCENE,
    RENDERTYPE_SHADOWVOLUME,
    RENDERTYPE_COMPLEXITY
};

D3DXVECTOR4 g_vShadowColor[MAX_NUM_LIGHTS] =
{
    D3DXVECTOR4( 0.0f, 1.0f, 0.0f, 0.2f ),
    D3DXVECTOR4( 1.0f, 1.0f, 0.0f, 0.2f ),
    D3DXVECTOR4( 1.0f, 0.0f, 0.0f, 0.2f ),
    D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 0.2f ),
    D3DXVECTOR4( 1.0f, 0.0f, 1.0f, 0.2f ),
    D3DXVECTOR4( 0.0f, 1.0f, 1.0f, 0.2f )
};

struct LIGHTINITDATA
{
    D3DXVECTOR3 Position;
    D3DXVECTOR4 Color;
public:
LIGHTINITDATA()
{
}
LIGHTINITDATA( D3DXVECTOR3 P, D3DXVECTOR4 C )
{
    Position = P;
    Color = C;
}
};

LIGHTINITDATA g_LightInit[MAX_NUM_LIGHTS] =
{
    LIGHTINITDATA( D3DXVECTOR3( -2.0f, 3.0f, -3.0f ), D3DXVECTOR4( 15.0f, 15.0f, 15.0f, 1.0f ) ),
    //LIGHTINITDATA( D3DXVECTOR3( 0.0f, 4.0f, 0.0f ), D3DXVECTOR4( 15.0f, 15.0f, 15.0f, 1.0f ) ),
#if MAX_NUM_LIGHTS > 1
    LIGHTINITDATA( D3DXVECTOR3( 2.0f, 3.0f, -3.0f ), D3DXVECTOR4( 15.0f, 15.0f, 15.0f, 1.0f ) ),
#endif
#if MAX_NUM_LIGHTS > 2
    LIGHTINITDATA( D3DXVECTOR3( -2.0f, 3.0f, 3.0f ), D3DXVECTOR4( 15.0f, 15.0f, 15.0f, 1.0f ) ),
#endif
#if MAX_NUM_LIGHTS > 3
    LIGHTINITDATA( D3DXVECTOR3( 2.0f, 3.0f, 3.0f ), D3DXVECTOR4( 15.0f, 15.0f, 15.0f, 1.0f ) ),
#endif
#if MAX_NUM_LIGHTS > 4
    LIGHTINITDATA( D3DXVECTOR3( -2.0f, 3.0f, 0.0f ), D3DXVECTOR4( 15.0f, 0.0f, 0.0f, 1.0f ) ),
#endif
#if MAX_NUM_LIGHTS > 5
    LIGHTINITDATA( D3DXVECTOR3( 2.0f, 3.0f, 0.0f ), D3DXVECTOR4( 0.0f, 0.0f, 15.0f, 1.0f ) )
#endif
};

struct CLight
{
    D3DXVECTOR3 m_Position;  // Light position
    D3DXVECTOR4 m_Color;     // Light color
    D3DXMATRIX m_mWorld;  // World transform
};

struct MESHVERTEX
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR3 Norm;
    D3DXVECTOR2 Tex;
};

const D3DVERTEXELEMENT9 POSTPROCVERT::Decl[2] =
{
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
    D3DDECL_END()
};

const D3DVERTEXELEMENT9 SHADOWVERT::Decl[3] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    D3DDECL_END()
};

const D3DVERTEXELEMENT9 MESHVERT::Decl[4] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
bool                                g_bShowHelp = true;     // If true, it renders the UI control text
CFirstPersonCamera                  g_Camera;               // A model viewing camera
CModelViewerCamera                  g_MCamera;              // Camera for mesh control
CModelViewerCamera                  g_LCamera;              // Camera for easy light movement control
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = NULL;
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
D3DXMATRIX                          g_mWorldScaling;        // Scaling to apply to mesh
D3DXMATRIX g_mWorldBack[2];        // Background matrix
int                                 g_nCurrBackground;
bool                                g_bShowShadowVolume = false;  // Whether the shadow volume is visibly shown.
RENDER_TYPE                         g_RenderType = RENDERTYPE_SCENE;  // Type of rendering to perform
int                                 g_nNumLights = 1;       // Number of active lights
CLight g_aLights[MAX_NUM_LIGHTS];  // Light objects
bool                                g_bLeftButtonDown = false;
bool                                g_bRightButtonDown = false;
bool                                g_bMiddleButtonDown = false;
D3DXVECTOR4                         g_vAmbient( 0.1f,0.1f,0.1f,1.0f );

// Direct3D 9 resources
extern ID3DXFont*                   g_pFont;             // Font for drawing text
extern ID3DXSprite*                 g_pTextSprite;       // Sprite for batching draw text calls
extern CD3D9Mesh                    g_Mesh;              // The mesh object
extern ID3DXMesh*                   g_pShadowMesh;       // The shadow volume mesh

// Direct3D10 resources
CDXUTSDKMesh g_Background10[2];      // Background mesh
CDXUTSDKMesh                        g_LightMesh10;          // Mesh to represent the light source
CDXUTSDKMesh                        g_Mesh10;               // The mesh object
ID3DX10Font*                        g_pFont10 = NULL;       // Font for drawing text
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10Buffer*                       g_pQuadVB = NULL;
ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10InputLayout*                  g_pQuadLayout = NULL;
ID3D10EffectTechnique*              g_pRenderSceneTextured = NULL;
ID3D10EffectTechnique*              g_pRenderSceneLit = NULL;
ID3D10EffectTechnique*              g_pRenderSceneAmbient = NULL;
ID3D10EffectTechnique*              g_pRenderShadowVolumeComplexity = NULL;
ID3D10EffectTechnique*              g_pRenderComplexity = NULL;
ID3D10EffectTechnique*              g_pShowShadow = NULL;
ID3D10EffectTechnique*              g_pRenderShadow = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectVectorVariable*         g_pvLightPos = NULL;
ID3D10EffectVectorVariable*         g_pvLightColor = NULL;
ID3D10EffectVectorVariable*         g_pvAmbient = NULL;
ID3D10EffectVectorVariable*         g_pvShadowColor = NULL;
ID3D10EffectScalarVariable*         g_pfExtrudeAmt = NULL;
ID3D10EffectScalarVariable*         g_pfExtrudeBias = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_RENDERTYPE          5
#define IDC_ENABLELIGHT         6
#define IDC_LUMINANCELABEL      7
#define IDC_LUMINANCE           8
#define IDC_BACKGROUND          9
#define IDC_TOGGLEWARP          10

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down,
                         bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

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

bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void InitApp();
void RenderText();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
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
    DXUTSetCallbackMouse( MouseProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

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

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"ShadowVolume" );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddComboBox( IDC_BACKGROUND, 0, iY += 24, 190, 22, 0, false );
    g_SampleUI.GetComboBox( IDC_BACKGROUND )->SetDropHeight( 40 );
    g_SampleUI.GetComboBox( IDC_BACKGROUND )->AddItem( L"Background: Cell", ( LPVOID )0 );
    g_SampleUI.GetComboBox( IDC_BACKGROUND )->AddItem( L"Background: Field", ( LPVOID )1 );
    g_SampleUI.AddComboBox( IDC_ENABLELIGHT, 0, iY += 24, 190, 22, 0, false );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->SetDropHeight( 90 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"1 light", ( LPVOID )1 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"2 lights", ( LPVOID )2 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"3 lights", ( LPVOID )3 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"4 lights", ( LPVOID )4 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"5 lights", ( LPVOID )5 );
    g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->AddItem( L"6 lights", ( LPVOID )6 );
    g_SampleUI.AddComboBox( IDC_RENDERTYPE, 0, iY += 24, 190, 22, 0, false );
    g_SampleUI.GetComboBox( IDC_RENDERTYPE )->SetDropHeight( 50 );
    g_SampleUI.GetComboBox( IDC_RENDERTYPE )->AddItem( L"Scene with shadow", ( LPVOID )RENDERTYPE_SCENE );
    g_SampleUI.GetComboBox( IDC_RENDERTYPE )->AddItem( L"Show shadow volume", ( LPVOID )RENDERTYPE_SHADOWVOLUME );
    g_SampleUI.GetComboBox( IDC_RENDERTYPE )->AddItem( L"Shadow volume complexity", ( LPVOID )RENDERTYPE_COMPLEXITY );
    g_SampleUI.AddStatic( IDC_LUMINANCELABEL, L"Luminance: 15.0", 10, iY += 30, 125, 22 );
    g_SampleUI.AddSlider( IDC_LUMINANCE, 20, iY += 20, 125, 22, 1, 40, 15 );

    g_Camera.SetRotateButtons( true, false, false );
    g_MCamera.SetButtonMasks( MOUSE_RIGHT_BUTTON, 0, 0 );
    g_LCamera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, 0, 0 );

    // Initialize the lights
    for( int L = 0; L < MAX_NUM_LIGHTS; ++L )
    {
        g_aLights[L].m_Position = g_LightInit[L].Position;
        g_aLights[L].m_Color = g_LightInit[L].Color;
    }

    // Initialize the scaling and translation for the background meshes
    // Hardcode the matrices since we only have two.
    D3DXMATRIX m;
    D3DXMatrixTranslation( &g_mWorldBack[0], 0.0f, 2.0f, 0.0f );
    D3DXMatrixScaling( &g_mWorldBack[1], 0.3f, 0.3f, 0.3f );
    D3DXMatrixTranslation( &m, 0.0f, 1.5f, 0.0f );
    D3DXMatrixMultiply( &g_mWorldBack[1], &g_mWorldBack[1], &m );

    D3DXMatrixIdentity( &g_mWorldScaling );
}


//--------------------------------------------------------------------------------------
// Compute a matrix that scales Mesh to a specified size and centers around origin
//--------------------------------------------------------------------------------------
void ComputeMeshScaling( CD3D9Mesh& Mesh, D3DXMATRIX* pmScalingCenter )
{
    LPVOID pVerts = NULL;
    D3DXMatrixIdentity( pmScalingCenter );
    if( SUCCEEDED( Mesh.GetMesh()->LockVertexBuffer( 0, &pVerts ) ) )
    {
        D3DXVECTOR3 vCtr;
        float fRadius;
        if( SUCCEEDED( D3DXComputeBoundingSphere( ( const D3DXVECTOR3* )pVerts,
                                                  Mesh.GetMesh()->GetNumVertices(),
                                                  Mesh.GetMesh()->GetNumBytesPerVertex(),
                                                  &vCtr, &fRadius ) ) )
        {
            D3DXMatrixTranslation( pmScalingCenter, -vCtr.x, -vCtr.y, -vCtr.z );
            D3DXMATRIXA16 m;
            D3DXMatrixScaling( &m, 1.0f / fRadius,
                               1.0f / fRadius,
                               1.0f / fRadius );
            D3DXMatrixMultiply( pmScalingCenter, pmScalingCenter, &m );
        }
        Mesh.GetMesh()->UnlockVertexBuffer();
    }
}


//--------------------------------------------------------------------------------------
// Compute a matrix that scales Mesh to a specified size and centers around origin
//--------------------------------------------------------------------------------------
void ComputeMeshScaling10( CDXUTSDKMesh& Mesh, D3DXMATRIX* pmScalingCenter )
{
    D3DXVECTOR3 Max = Mesh.GetMeshBBoxCenter( 0 ) + Mesh.GetMeshBBoxExtents( 0 );
    D3DXVECTOR3 Min = Mesh.GetMeshBBoxCenter( 0 ) - Mesh.GetMeshBBoxExtents( 0 );
    D3DXVECTOR3 NewMax;
    D3DXVECTOR3 NewMin;
    for( UINT i = 0; i < Mesh.GetNumMeshes(); i++ )
    {
        NewMax = Mesh.GetMeshBBoxCenter( i ) + Mesh.GetMeshBBoxExtents( i );
        NewMin = Mesh.GetMeshBBoxCenter( i ) - Mesh.GetMeshBBoxExtents( i );

        if( NewMax.x > Max.x )
            Max.x = NewMax.x;
        if( NewMax.y > Max.y )
            Max.y = NewMax.y;
        if( NewMax.z > Max.z )
            Max.z = NewMax.z;

        if( NewMin.x > Min.x )
            Min.x = NewMin.x;
        if( NewMin.y > Min.y )
            Min.y = NewMin.y;
        if( NewMin.z > Min.z )
            Min.z = NewMin.z;
    }

    D3DXVECTOR3 vCtr = ( Max + Min ) / 2.0f;
    D3DXVECTOR3 vExtents = Max - vCtr;
    float fRadius = D3DXVec3Length( &vExtents );

    D3DXMatrixTranslation( pmScalingCenter, -vCtr.x, -vCtr.y, -vCtr.z );
    D3DXMATRIXA16 m;
    D3DXMatrixScaling( &m, 1.0f / fRadius,
                       1.0f / fRadius,
                       1.0f / fRadius );
    D3DXMatrixMultiply( pmScalingCenter, pmScalingCenter, &m );
}


//--------------------------------------------------------------------------------------
// Returns true if a particular depth-stencil format can be created and used with
// an adapter format and backbuffer format combination.
//--------------------------------------------------------------------------------------
BOOL IsDepthFormatOk( D3DFORMAT DepthFormat,
                      D3DFORMAT AdapterFormat,
                      D3DFORMAT BackBufferFormat )
{
    // Verify that the depth format exists
    HRESULT hr = DXUTGetD3D9Object()->CheckDeviceFormat( D3DADAPTER_DEFAULT,
                                                         D3DDEVTYPE_HAL,
                                                         AdapterFormat,
                                                         D3DUSAGE_DEPTHSTENCIL,
                                                         D3DRTYPE_SURFACE,
                                                         DepthFormat );
    if( FAILED( hr ) ) return FALSE;

    // Verify that the backbuffer format is valid
    hr = DXUTGetD3D9Object()->CheckDeviceFormat( D3DADAPTER_DEFAULT,
                                                 D3DDEVTYPE_HAL,
                                                 AdapterFormat,
                                                 D3DUSAGE_RENDERTARGET,
                                                 D3DRTYPE_SURFACE,
                                                 BackBufferFormat );
    if( FAILED( hr ) ) return FALSE;

    // Verify that the depth format is compatible
    hr = DXUTGetD3D9Object()->CheckDepthStencilMatch( D3DADAPTER_DEFAULT,
                                                      D3DDEVTYPE_HAL,
                                                      AdapterFormat,
                                                      BackBufferFormat,
                                                      DepthFormat );

    return SUCCEEDED( hr );
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

        // This sample requires a stencil buffer.
        if( IsDepthFormatOk( D3DFMT_D24S8,
                             pDeviceSettings->d3d9.AdapterFormat,
                             pDeviceSettings->d3d9.pp.BackBufferFormat ) )
            pDeviceSettings->d3d9.pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        else if( IsDepthFormatOk( D3DFMT_D24X4S4,
                                  pDeviceSettings->d3d9.AdapterFormat,
                                  pDeviceSettings->d3d9.pp.BackBufferFormat ) )
            pDeviceSettings->d3d9.pp.AutoDepthStencilFormat = D3DFMT_D24X4S4;
        else if( IsDepthFormatOk( D3DFMT_D24FS8,
                                  pDeviceSettings->d3d9.AdapterFormat,
                                  pDeviceSettings->d3d9.pp.BackBufferFormat ) )
            pDeviceSettings->d3d9.pp.AutoDepthStencilFormat = D3DFMT_D24FS8;
        else if( IsDepthFormatOk( D3DFMT_D15S1,
                                  pDeviceSettings->d3d9.AdapterFormat,
                                  pDeviceSettings->d3d9.pp.BackBufferFormat ) )
            pDeviceSettings->d3d9.pp.AutoDepthStencilFormat = D3DFMT_D15S1;
    }
    else
    {
        pDeviceSettings->d3d10.AutoDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
        DXUTGetDXGIBackBufferSurfaceDesc()->Height;
    UINT nBackBufferWidth = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Width :
        DXUTGetDXGIBackBufferSurfaceDesc()->Width;

    // Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        g_pTxtHelper->SetInsertionPos( 10, nBackBufferHeight - 80 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls (F1 to hide):" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 65 );
        g_pTxtHelper->DrawTextLine( L"Camera control: Left mouse\n"
                                    L"Mesh control: Right mouse\n"
                                    L"Light control: Middle mouse\n"
                                    L"Quit: ESC" );
    }
    else
    {
        g_pTxtHelper->SetInsertionPos( 10, nBackBufferHeight - 25 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    if( !( g_bLeftButtonDown || g_bMiddleButtonDown || g_bRightButtonDown ) )
    {
        g_pTxtHelper->SetInsertionPos( nBackBufferWidth / 2 - 90, nBackBufferHeight - 40 );
        g_pTxtHelper->DrawTextLine( L"\nW/S/A/D/Q/E to move camera." );
    }
    else
    {
        g_pTxtHelper->SetInsertionPos( nBackBufferWidth / 2 - 70, nBackBufferHeight - 40 );
        if( g_bLeftButtonDown )
        {
            g_pTxtHelper->DrawTextLine( L"Camera Control Mode" );
        }
        else if( g_bMiddleButtonDown )
        {
            g_pTxtHelper->DrawTextLine( L"Light Control Mode" );
        }
        if( g_bRightButtonDown )
        {
            g_pTxtHelper->DrawTextLine( L"Model Control Mode" );
        }
        g_pTxtHelper->SetInsertionPos( nBackBufferWidth / 2 - 130, nBackBufferHeight - 25 );
        g_pTxtHelper->DrawTextLine( L"Move mouse to rotate. W/S/A/D/Q/E to move." );
    }

    if( g_RenderType == RENDERTYPE_COMPLEXITY )
    {
        g_pTxtHelper->SetInsertionPos( 5, 70 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Shadow volume complexity:" );
        g_pTxtHelper->SetInsertionPos( 5, 90 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"1 to 5 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"6 to 10 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"11 to 20 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"21 to 30 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"31 to 40 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"41 to 50 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"51 to 70 fills\n" );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"71 or more fills\n" );
    }

    // Display an error message if unable to generate shadow mesh
    if( !g_pShadowMesh && DXUTIsAppRenderingWithD3D9() )
    {
        g_pTxtHelper->SetInsertionPos( 5, 35 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Unable to generate closed shadow volume for this mesh\n" );
        g_pTxtHelper->DrawTextLine( L"No shadow will be rendered" );
    }

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    g_Camera.FrameMove( fElapsedTime );
    g_MCamera.FrameMove( fElapsedTime );
    g_LCamera.FrameMove( fElapsedTime );
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
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
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
    g_MCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
    g_LCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
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
// Handle mouse buttons
//--------------------------------------------------------------------------------------
void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down,
                         bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext )
{
    bool bOldLeftButtonDown = g_bLeftButtonDown;
    bool bOldRightButtonDown = g_bRightButtonDown;
    bool bOldMiddleButtonDown = g_bMiddleButtonDown;
    g_bLeftButtonDown = bLeftButtonDown;
    g_bMiddleButtonDown = bMiddleButtonDown;
    g_bRightButtonDown = bRightButtonDown;

    if( bOldLeftButtonDown && !g_bLeftButtonDown )
        g_Camera.SetEnablePositionMovement( false );
    else if( !bOldLeftButtonDown && g_bLeftButtonDown )
        g_Camera.SetEnablePositionMovement( true );

    if( bOldRightButtonDown && !g_bRightButtonDown )
    {
        g_MCamera.SetEnablePositionMovement( false );
    }
    else if( !bOldRightButtonDown && g_bRightButtonDown )
    {
        g_MCamera.SetEnablePositionMovement( true );
        g_Camera.SetEnablePositionMovement( false );
    }

    if( bOldMiddleButtonDown && !g_bMiddleButtonDown )
    {
        g_LCamera.SetEnablePositionMovement( false );
    }
    else if( !bOldMiddleButtonDown && g_bMiddleButtonDown )
    {
        g_LCamera.SetEnablePositionMovement( true );
        g_Camera.SetEnablePositionMovement( false );
    }

    // If no mouse button is down at all, enable camera movement.
    if( !g_bLeftButtonDown && !g_bRightButtonDown && !g_bMiddleButtonDown )
        g_Camera.SetEnablePositionMovement( true );
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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_RENDERTYPE:
            g_RenderType = ( RENDER_TYPE )( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData();
            break;

        case IDC_ENABLELIGHT:
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_nNumLights = ( int )( size_t )g_SampleUI.GetComboBox( IDC_ENABLELIGHT )->GetSelectedData();
            }
            break;

        case IDC_LUMINANCE:
            if( nEvent == EVENT_SLIDER_VALUE_CHANGED )
            {
                float fLuminance = float( ( ( CDXUTSlider* )pControl )->GetValue() );
                WCHAR wszText[50];
                swprintf_s( wszText, 50, L"Luminance: %.1f", fLuminance );
                g_SampleUI.GetStatic( IDC_LUMINANCELABEL )->SetText( wszText );

                for( int i = 0; i < MAX_NUM_LIGHTS; ++i )
                {
                    if( g_aLights[i].m_Color.x > 0.5f )
                        g_aLights[i].m_Color.x = fLuminance;
                    if( g_aLights[i].m_Color.y > 0.5f )
                        g_aLights[i].m_Color.y = fLuminance;
                    if( g_aLights[i].m_Color.z > 0.5f )
                        g_aLights[i].m_Color.z = fLuminance;
                }
            }

        case IDC_BACKGROUND:
            if( nEvent == EVENT_COMBOBOX_SELECTION_CHANGED )
            {
                g_nCurrBackground = ( int )( size_t )g_SampleUI.GetComboBox( IDC_BACKGROUND )->GetSelectedData();
            }
            break;
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ShadowVolume10.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderSceneTextured = g_pEffect10->GetTechniqueByName( "RenderSceneTextured" );
    g_pRenderSceneLit = g_pEffect10->GetTechniqueByName( "RenderSceneLit" );
    g_pRenderSceneAmbient = g_pEffect10->GetTechniqueByName( "RenderSceneAmbient" );
    g_pRenderShadow = g_pEffect10->GetTechniqueByName( "RenderShadow" );
    g_pRenderShadowVolumeComplexity = g_pEffect10->GetTechniqueByName( "RenderShadowVolumeComplexity" );
    g_pRenderComplexity = g_pEffect10->GetTechniqueByName( "RenderComplexity" );
    g_pShowShadow = g_pEffect10->GetTechniqueByName( "ShowShadow" );

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pRenderSceneLit->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Create our quad vertex input layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderComplexity->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    // Load the meshes
    V_RETURN( g_Background10[0].Create( pd3dDevice, L"misc\\cell.sdkmesh", true ) );
    V_RETURN( g_Background10[1].Create( pd3dDevice, L"misc\\seafloor.sdkmesh", true ) );
    V_RETURN( g_Mesh10.Create( pd3dDevice, L"dwarf\\dwarf.sdkmesh", true ) );

    ComputeMeshScaling10( g_Mesh10, &g_mWorldScaling );

    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmViewProj = g_pEffect10->GetVariableByName( "g_mViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pvLightPos = g_pEffect10->GetVariableByName( "g_vLightPos" )->AsVector();
    g_pvLightColor = g_pEffect10->GetVariableByName( "g_vLightColor" )->AsVector();
    g_pvAmbient = g_pEffect10->GetVariableByName( "g_vAmbient" )->AsVector();
    g_pvShadowColor = g_pEffect10->GetVariableByName( "g_vShadowColor" )->AsVector();
    g_pfExtrudeAmt = g_pEffect10->GetVariableByName( "g_fExtrudeAmt" )->AsScalar();
    g_pfExtrudeBias = g_pEffect10->GetVariableByName( "g_fExtrudeBias" )->AsScalar();

    // Create a screen quad for render-to-texture
    UINT uiVertBufSize = 4 * sizeof( QUAD_VERTEX );
    QUAD_VERTEX pVerts[4];
    pVerts[0].pos = D3DXVECTOR3( -1, -1, 0.5f );
    pVerts[1].pos = D3DXVECTOR3( -1, 1, 0.5f );
    pVerts[2].pos = D3DXVECTOR3( 1, -1, 0.5f );
    pVerts[3].pos = D3DXVECTOR3( 1, 1, 0.5f );

    D3D10_BUFFER_DESC vbdesc =
    {
        uiVertBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVerts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pQuadVB ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 1.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_LCamera.SetViewParams( &vecEye, &vecAt );
    g_MCamera.SetViewParams( &vecEye, &vecAt );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;
    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 500.0f );
    g_MCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

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

    return S_OK;
}


//--------------------------------------------------------------------------------------
void UpdateLights( int iLight )
{
    D3DXVECTOR4 vLight( g_aLights[iLight].m_Position.x, g_aLights[iLight].m_Position.y, g_aLights[iLight].m_Position.z,
                        1.0f );
    D3DXVec4Transform( &vLight, &vLight, g_LCamera.GetWorldMatrix() );

    g_pvLightPos->SetFloatVector( ( float* )&vLight );
    g_pvLightColor->SetFloatVector( ( float* )g_aLights[iLight].m_Color );
}


//--------------------------------------------------------------------------------------
void RenderScene( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, bool bAdjacent )
{
    // Setup the view matrices
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldViewProj;
    D3DXMATRIX mViewProj;
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    mViewProj = mView * mProj;
    g_pmViewProj->SetMatrix( ( float* )&mViewProj );

    // Render background
    if( !bAdjacent )
    {
        mWorldViewProj = g_mWorldBack[ g_nCurrBackground ] * mView * mProj;
        g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
        g_pmWorld->SetMatrix( ( float* )&g_mWorldBack[ g_nCurrBackground ] );
        g_Background10[ g_nCurrBackground ].Render( pd3dDevice, pTech, g_pDiffuseTex );
    }

    // Render object
    mWorld = g_mWorldScaling * ( *g_MCamera.GetWorldMatrix() );
    mWorldViewProj = mWorld * mView * mProj;
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    if( bAdjacent )
        g_Mesh10.RenderAdjacent( pd3dDevice, pTech, g_pDiffuseTex );
    else
        g_Mesh10.Render( pd3dDevice, pTech, g_pDiffuseTex );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0f, 0 );

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Render the scene ambient
    g_pvAmbient->SetFloatVector( ( float* )&g_vAmbient );
    RenderScene( pd3dDevice, g_pRenderSceneAmbient, false );
    g_pfExtrudeAmt->SetFloat( 120.0f - EXTRUDE_EPSILON );
    g_pfExtrudeBias->SetFloat( EXTRUDE_EPSILON );

    for( int l = 0; l < g_nNumLights; l++ )
    {
        UpdateLights( l );

        // Clear the stencil
        ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
        if( g_RenderType != RENDERTYPE_COMPLEXITY )
            pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_STENCIL, 1.0, 0 );

        // Render the shadow volume
        ID3D10EffectTechnique* pTech = g_pRenderShadow;
        switch( g_RenderType )
        {
            case RENDERTYPE_COMPLEXITY:
                pTech = g_pRenderShadowVolumeComplexity;
                break;
            case RENDERTYPE_SHADOWVOLUME:
                pTech = g_pShowShadow;
                g_pvShadowColor->SetFloatVector( ( float* )&g_vShadowColor[l] );
                break;
            default:
                pTech = g_pRenderShadow;
        }

        RenderScene( pd3dDevice, pTech, true );

        // Render the lit scene
        if( g_RenderType != RENDERTYPE_COMPLEXITY )
            RenderScene( pd3dDevice, g_pRenderSceneLit, false );
    }

    if( g_RenderType == RENDERTYPE_COMPLEXITY )
    {
        // Clear the render target
        pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

        // Setup IA params
        pd3dDevice->IASetInputLayout( g_pQuadLayout );
        UINT stride = sizeof( QUAD_VERTEX );
        UINT offset = 0;
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVB, &stride, &offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

        D3D10_TECHNIQUE_DESC techDesc;
        g_pRenderComplexity->GetDesc( &techDesc );
        for( UINT p = 0; p < techDesc.Passes; ++p )
        {
            g_pRenderComplexity->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->Draw( 4, 0 );
        }
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pQuadVB );
    SAFE_RELEASE( g_pQuadLayout );

    g_Background10[0].Destroy();
    g_Background10[1].Destroy();
    g_Mesh10.Destroy();
    g_LightMesh10.Destroy();
}


