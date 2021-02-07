//--------------------------------------------------------------------------------------
// File: PNTriangles11.cpp
//
// This code sample demonstrates the use of DX11 Hull & Domain shaders to implement the 
// PN-Triangles tessellation technique
//
// Contributed by the AMD Developer Relations Team
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <D3DX11tex.h>
#include <D3DX11.h>
#include <D3DX11core.h>
#include <D3DX11async.h>

// The different meshes
typedef enum _MESH_TYPE
{
    MESH_TYPE_TINY      = 0,
    MESH_TYPE_TIGER     = 1,
    MESH_TYPE_TEAPOT    = 2,
    MESH_TYPE_USER      = 3,
    MESH_TYPE_MAX       = 4,
}MESH_TYPE;

CDXUTDialogResourceManager  g_DialogResourceManager;    // Manager for shared resources of dialogs
CModelViewerCamera          g_Camera[MESH_TYPE_MAX];    // A model viewing camera for each mesh scene
CModelViewerCamera          g_LightCamera;              // A model viewing camera for the light
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // Dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = NULL;

// The scene meshes 
CDXUTSDKMesh                g_SceneMesh[MESH_TYPE_MAX];
static ID3D11InputLayout*   g_pSceneVertexLayout = NULL;
MESH_TYPE                   g_eMeshType = MESH_TYPE_TINY;
D3DXMATRIX                  g_m4x4MeshMatrix[MESH_TYPE_MAX];
D3DXVECTOR3                 g_v3AdaptiveTessParams[MESH_TYPE_MAX];

// Samplers
ID3D11SamplerState*         g_pSamplePoint = NULL;
ID3D11SamplerState*         g_pSampleLinear = NULL;

// Shaders
ID3D11VertexShader*         g_pSceneVS = NULL;
ID3D11VertexShader*         g_pSceneWithTessellationVS = NULL;
ID3D11HullShader*           g_pPNTrianglesHS = NULL;
ID3D11DomainShader*         g_pPNTrianglesDS = NULL;
ID3D11PixelShader*          g_pScenePS = NULL;
ID3D11PixelShader*          g_pTexturedScenePS = NULL;

// Constant buffer layout for transfering data to the PN-Triangles HLSL functions
struct CB_PNTRIANGLES
{
    D3DXMATRIX f4x4World;               // World matrix for object
    D3DXMATRIX f4x4ViewProjection;      // View * Projection matrix
    D3DXMATRIX f4x4WorldViewProjection; // World * View * Projection matrix  
    float fLightDir[4];                 // Light direction vector
    float fEye[4];                      // Eye
    float fViewVector[4];               // View Vector
    float fTessFactors[4];              // Tessellation factors ( x=Edge, y=Inside, z=MinDistance, w=Range )
    float fScreenParams[4];             // Screen params ( x=Current width, y=Current height )
    float fGUIParams1[4];               // GUI params1 ( x=BackFace Epsilon, y=Silhouette Epsilon, z=Range scale, w=Edge size )
    float fGUIParams2[4];               // GUI params2 ( x=Screen resolution scale, y=View Frustum Epsilon )
    D3DXVECTOR4 f4ViewFrustumPlanes[4]; // View frustum planes
};
UINT                    g_iPNTRIANGLESCBBind = 0;

// Various Constant buffers
static ID3D11Buffer*    g_pcbPNTriangles = NULL;                 

// State objects
ID3D11RasterizerState*  g_pRasterizerStateWireframe = NULL;
ID3D11RasterizerState*  g_pRasterizerStateSolid = NULL;

// Capture texture
static ID3D11Texture2D*    g_pCaptureTexture = NULL;

// User supplied data
static bool g_bUserMesh = false;
static ID3D11ShaderResourceView* g_pDiffuseTextureSRV = NULL;

// Tess factor
static unsigned int g_uTessFactor = 5;

// Back face culling epsilon
static float g_fBackFaceCullEpsilon = 0.5f;

// Silhoutte epsilon
static float g_fSilhoutteEpsilon = 0.25f;

// Range scale (for distance adaptive tessellation)
static float g_fRangeScale = 1.0f;

// Edge scale (for screen space adaptive tessellation)
static unsigned int g_uEdgeSize = 16; 

// Edge scale (for screen space adaptive tessellation)
static float g_fResolutionScale = 1.0f; 

// View frustum culling epsilon
static float g_fViewFrustumCullEpsilon = 0.5f;

// Cmd line params
typedef struct _CmdLineParams
{
    unsigned int uWidth;
    unsigned int uHeight;
    bool bCapture;
    WCHAR strCaptureFilename[256];
    int iExitFrame;
    bool bRenderHUD;
}CmdLineParams;
static CmdLineParams g_CmdLineParams;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------

// Standard device control
#define IDC_TOGGLEFULLSCREEN                    1
#define IDC_TOGGLEREF                           3
#define IDC_CHANGEDEVICE                        4

// Sample UI
#define IDC_STATIC_MESH                         5
#define IDC_COMBOBOX_MESH                       6
#define IDC_CHECKBOX_WIREFRAME                  7
#define IDC_CHECKBOX_TEXTURED                   8
#define IDC_CHECKBOX_TESSELLATION               9
#define IDC_CHECKBOX_DISTANCE_ADAPTIVE          10
#define IDC_CHECKBOX_ORIENTATION_ADAPTIVE       11
#define IDC_STATIC_TESS_FACTOR                  12
#define IDC_SLIDER_TESS_FACTOR                  13
#define IDC_CHECKBOX_BACK_FACE_CULL             14
#define IDC_CHECKBOX_VIEW_FRUSTUM_CULL          15
#define IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE      16
#define IDC_STATIC_BACK_FACE_CULL_EPSILON       17
#define IDC_SLIDER_BACK_FACE_CULL_EPSILON       18
#define IDC_STATIC_SILHOUTTE_EPSILON            19
#define IDC_SLIDER_SILHOUTTE_EPSILON            20
#define IDC_STATIC_CULLING_TECHNIQUES           21
#define IDC_STATIC_ADAPTIVE_TECHNIQUES          22
#define IDC_STATIC_RANGE_SCALE                  23
#define IDC_SLIDER_RANGE_SCALE                  24
#define IDC_STATIC_EDGE_SIZE                    25
#define IDC_SLIDER_EDGE_SIZE                    26
#define IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE 27
#define IDC_STATIC_SCREEN_RESOLUTION_SCALE      28
#define IDC_SLIDER_SCREEN_RESOLUTION_SCALE      29
#define IDC_STATIC_RENDER_SETTINGS              30
#define IDC_STATIC_VIEW_FRUSTUM_CULL_EPSILON    31
#define IDC_SLIDER_VIEW_FRUSTUM_CULL_EPSILON    32


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

// Helper functions
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );
void ParseCommandLine();
void CaptureFrame();
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, 
                 D3D11_PRIMITIVE_TOPOLOGY PrimType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED, 
                 UINT uDiffuseSlot = INVALID_SAMPLER_SLOT, UINT uNormalSlot = INVALID_SAMPLER_SLOT,
                 UINT uSpecularSlot = INVALID_SAMPLER_SLOT );
bool FileExists( WCHAR* pFileName );
HRESULT CreateHullShader();
void NormalizePlane( D3DXVECTOR4* pPlaneEquation );
void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix );

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

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    ParseCommandLine();

    InitApp();
    
    DXUTInit( true, true );                 // Use this line instead to try to create a hardware device

    DXUTSetCursorSettings( true, true );    // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"PN-Triangles Direct3D 11" );

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, g_CmdLineParams.uWidth, g_CmdLineParams.uHeight );
    DXUTMainLoop();                         // Enter into the DXUT render loop

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
    g_SampleUI.GetFont( 0 );

    g_HUD.SetCallback( OnGUIEvent ); 
    int iY = 30;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );
    

    g_SampleUI.SetCallback( OnGUIEvent );
    iY = 0;
    // Render Settings
    g_SampleUI.AddStatic( IDC_STATIC_RENDER_SETTINGS, L"-Render Settings-", 5, iY, 108, 24 );
    g_SampleUI.AddStatic( IDC_STATIC_MESH, L"Mesh:", 0, iY += 25, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_MESH, 55, iY, 110, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 45 );
        pCombo->AddItem( L"Tiny", NULL );
        pCombo->AddItem( L"Tiger", NULL );
        pCombo->AddItem( L"Teapot", NULL );
        pCombo->AddItem( L"User", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_WIREFRAME, L"Wireframe", 0, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_TEXTURED, L"Textured", 0, iY += 25, 140, 24, true );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_TESSELLATION, L"Tessellation", 0, iY += 25, 140, 24, true );
    WCHAR szTemp[256];
    
    // Tess factor
    swprintf_s( szTemp, L"%d", g_uTessFactor );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTOR, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTOR, 0, iY, 120, 24, 1, 5, 1 + ( g_uTessFactor - 1 ) / 2, false );
    
    // Culling Techniques
    g_SampleUI.AddStatic( IDC_STATIC_CULLING_TECHNIQUES, L"-Culling Techniques-", 5, iY += 50, 108, 24 );
    
    // Back face culling
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_BACK_FACE_CULL, L"Back Face Cull", 0, iY += 30, 140, 24, false );
    swprintf_s( szTemp, L"%.2f", g_fBackFaceCullEpsilon );
    g_SampleUI.AddStatic( IDC_STATIC_BACK_FACE_CULL_EPSILON, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_BACK_FACE_CULL_EPSILON, 0, iY, 120, 24, 0, 100, (unsigned int)( g_fBackFaceCullEpsilon * 100.0f ), false );
    
    // View frustum culling
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_VIEW_FRUSTUM_CULL, L"View Frustum Cull", 0, iY += 30, 140, 24, false );
    swprintf_s( szTemp, L"%.2f", g_fViewFrustumCullEpsilon );
    g_SampleUI.AddStatic( IDC_STATIC_VIEW_FRUSTUM_CULL_EPSILON, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_VIEW_FRUSTUM_CULL_EPSILON, 0, iY, 120, 24, 0, 100, (unsigned int)( g_fViewFrustumCullEpsilon * 100.0f ), false );
        
    // Adaptive Techniques
    g_SampleUI.AddStatic( IDC_STATIC_ADAPTIVE_TECHNIQUES, L"-Adaptive Techniques-", 5, iY += 50, 108, 24 );
    
    // Screen space adaptive
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE, L"Screen Space", 0, iY += 30, 140, 24, false ); 
    swprintf_s( szTemp, L"%d", g_uEdgeSize );
    g_SampleUI.AddStatic( IDC_STATIC_EDGE_SIZE, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_EDGE_SIZE, 0, iY, 120, 24, 0, 100, (unsigned int)( g_uEdgeSize ), false );
    
    // Distance adaptive
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE, L"Distance", 0, iY += 30, 140, 24, false ); 
    swprintf_s( szTemp, L"%.2f", g_fRangeScale );
    g_SampleUI.AddStatic( IDC_STATIC_RANGE_SCALE, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_RANGE_SCALE, 0, iY, 120, 24, 0, 100, (unsigned int)( g_fRangeScale * 50.0f ), false );
    
    // Screen resolution adaptive
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE, L"Screen Resolution", 0, iY += 30, 140, 24, false );
    swprintf_s( szTemp, L"%.2f", g_fResolutionScale );
    g_SampleUI.AddStatic( IDC_STATIC_SCREEN_RESOLUTION_SCALE, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_SCREEN_RESOLUTION_SCALE, 0, iY, 120, 24, 0, 100, (unsigned int)( g_fResolutionScale * 50.0f ), false );
    
    // Orientation adaptive
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_ORIENTATION_ADAPTIVE, L"Orientation", 0, iY += 30, 140, 24, false );
    swprintf_s( szTemp, L"%.2f", g_fSilhoutteEpsilon );
    g_SampleUI.AddStatic( IDC_STATIC_SILHOUTTE_EPSILON, szTemp, 140, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_SILHOUTTE_EPSILON, 0, iY, 120, 24, 0, 100, (unsigned int)( g_fSilhoutteEpsilon * 100.0f ), false );
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;

        if( pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
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
    g_Camera[g_eMeshType].FrameMove( fElapsedTime );
    g_LightCamera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render stats
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetInsertionPos( 2, DXUTGetDXGIBackBufferSurfaceDesc()->Height - 35 );
    g_pTxtHelper->DrawTextLine( L"Toggle GUI    : G" );
    g_pTxtHelper->DrawTextLine( L"Frame Capture : C" );

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
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

    // Pass all windows messages to camera so it can respond to user input
    g_Camera[g_eMeshType].HandleMessages( hWnd, uMsg, wParam, lParam );
    g_LightCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];
    bool bEnable;
    
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;

        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;

        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;

        case IDC_SLIDER_TESS_FACTOR:
            g_uTessFactor = ( (unsigned int)((CDXUTSlider*)pControl)->GetValue() - 1 ) * 2 + 1;
            swprintf_s( szTemp, L"%d", g_uTessFactor );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetText( szTemp );
            break;

        case IDC_COMBOBOX_MESH:
            g_eMeshType = (MESH_TYPE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
            if( MESH_TYPE_USER == g_eMeshType )
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( ( g_pDiffuseTextureSRV != NULL ) ? ( true ) : ( false ) );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetChecked( ( g_pDiffuseTextureSRV != NULL ) ? 
                    ( g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetChecked() ) : ( false ) );
            }
            else
            {
                if( MESH_TYPE_TEAPOT == g_eMeshType )
                {
                    g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( false );
                }
                else
                {
                    g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->SetEnabled( true );
                }
            }
            break;

        case IDC_CHECKBOX_TESSELLATION:
            bEnable = g_SampleUI.GetCheckBox( IDC_CHECKBOX_TESSELLATION )->GetChecked();
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->SetEnabled( bEnable );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_ORIENTATION_ADAPTIVE )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_TESS_FACTOR )->SetEnabled( bEnable );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_BACK_FACE_CULL )->SetEnabled( bEnable );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_FRUSTUM_CULL )->SetEnabled( bEnable );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_BACK_FACE_CULL_EPSILON )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_BACK_FACE_CULL_EPSILON )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_SILHOUTTE_EPSILON )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_SILHOUTTE_EPSILON )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_CULLING_TECHNIQUES )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_ADAPTIVE_TECHNIQUES )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_RANGE_SCALE )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_RANGE_SCALE )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_EDGE_SIZE )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_EDGE_SIZE )->SetEnabled( bEnable );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE )->SetEnabled( bEnable );
            g_SampleUI.GetStatic( IDC_STATIC_SCREEN_RESOLUTION_SCALE )->SetEnabled( bEnable );
            g_SampleUI.GetSlider( IDC_SLIDER_SCREEN_RESOLUTION_SCALE )->SetEnabled( bEnable );
            break;

        case IDC_CHECKBOX_BACK_FACE_CULL:
        case IDC_CHECKBOX_VIEW_FRUSTUM_CULL:
        case IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE:
        case IDC_CHECKBOX_ORIENTATION_ADAPTIVE:
            CreateHullShader();
            break;

        case IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE:
            if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->GetChecked() )
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->SetChecked( false );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE )->SetChecked( false );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->SetEnabled( false );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE )->SetEnabled( false );
                g_SampleUI.GetSlider( IDC_SLIDER_RANGE_SCALE )->SetEnabled( false );
                g_SampleUI.GetSlider( IDC_SLIDER_SCREEN_RESOLUTION_SCALE )->SetEnabled( false );
                g_SampleUI.GetStatic( IDC_STATIC_RANGE_SCALE )->SetEnabled( false );
                g_SampleUI.GetStatic( IDC_STATIC_SCREEN_RESOLUTION_SCALE )->SetEnabled( false );
            }
            else
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->SetEnabled( true );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE )->SetEnabled( true );
                g_SampleUI.GetSlider( IDC_SLIDER_RANGE_SCALE )->SetEnabled( true );
                g_SampleUI.GetSlider( IDC_SLIDER_SCREEN_RESOLUTION_SCALE )->SetEnabled( true );
                g_SampleUI.GetStatic( IDC_STATIC_RANGE_SCALE )->SetEnabled( true );
                g_SampleUI.GetStatic( IDC_STATIC_SCREEN_RESOLUTION_SCALE )->SetEnabled( true );
            }
            CreateHullShader();
            break;

        case IDC_CHECKBOX_DISTANCE_ADAPTIVE:
            if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->GetChecked() )
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->SetChecked( false );
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->SetEnabled( false );
                g_SampleUI.GetSlider( IDC_SLIDER_EDGE_SIZE )->SetEnabled( false );
                g_SampleUI.GetStatic( IDC_STATIC_EDGE_SIZE )->SetEnabled( false );
            }
            else
            {
                g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->SetEnabled( true );
                g_SampleUI.GetSlider( IDC_SLIDER_EDGE_SIZE )->SetEnabled( true );
                g_SampleUI.GetStatic( IDC_STATIC_EDGE_SIZE )->SetEnabled( true );
            }
            CreateHullShader();
            break;
    
        case IDC_SLIDER_BACK_FACE_CULL_EPSILON:
            g_fBackFaceCullEpsilon = (float)((CDXUTSlider*)pControl)->GetValue() / 100.0f;
            swprintf_s( szTemp, L"%.2f", g_fBackFaceCullEpsilon );
            g_SampleUI.GetStatic( IDC_STATIC_BACK_FACE_CULL_EPSILON )->SetText( szTemp );
            break;

        case IDC_SLIDER_SILHOUTTE_EPSILON:
            g_fSilhoutteEpsilon = (float)((CDXUTSlider*)pControl)->GetValue() / 100.0f;
            swprintf_s( szTemp, L"%.2f", g_fSilhoutteEpsilon );
            g_SampleUI.GetStatic( IDC_STATIC_SILHOUTTE_EPSILON )->SetText( szTemp );
            break;

        case IDC_SLIDER_RANGE_SCALE:
            g_fRangeScale = (float)((CDXUTSlider*)pControl)->GetValue() / 50.0f;
            swprintf_s( szTemp, L"%.2f", g_fRangeScale );
            g_SampleUI.GetStatic( IDC_STATIC_RANGE_SCALE )->SetText( szTemp );
            break;

        case IDC_SLIDER_EDGE_SIZE:
            g_uEdgeSize = ((CDXUTSlider*)pControl)->GetValue();
            swprintf_s( szTemp, L"%d", g_uEdgeSize );
            g_SampleUI.GetStatic( IDC_STATIC_EDGE_SIZE )->SetText( szTemp );
            break;

        case IDC_SLIDER_SCREEN_RESOLUTION_SCALE:
            g_fResolutionScale = (float)((CDXUTSlider*)pControl)->GetValue() / 50.0f;
            swprintf_s( szTemp, L"%.2f", g_fResolutionScale );
            g_SampleUI.GetStatic( IDC_STATIC_SCREEN_RESOLUTION_SCALE )->SetText( szTemp );
            break;

        case IDC_SLIDER_VIEW_FRUSTUM_CULL_EPSILON:
            g_fViewFrustumCullEpsilon = (float)((CDXUTSlider*)pControl)->GetValue() / 50.0f;
            swprintf_s( szTemp, L"%.2f", g_fViewFrustumCullEpsilon );
            g_SampleUI.GetStatic( IDC_STATIC_VIEW_FRUSTUM_CULL_EPSILON )->SetText( szTemp );
            break;
    }
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    static int iCaptureNumber = 0;
    #define VK_C (67)
    #define VK_G (71)

    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_C:    
                swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture%d.bmp", iCaptureNumber );
                CaptureFrame();
                iCaptureNumber++;
                break;

            case VK_G:
                g_CmdLineParams.bRenderHUD = !g_CmdLineParams.bRenderHUD;
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Create the shaders
    ID3DBlob* pBlob = NULL;
    D3D_SHADER_MACRO ShaderMacros[4];
    ZeroMemory( ShaderMacros, sizeof(ShaderMacros) );
    ShaderMacros[0].Definition = "1";
    ShaderMacros[1].Definition = "1";
    ShaderMacros[2].Definition = "1";
    ShaderMacros[3].Definition = "1";
        
    // Main scene VS (no tessellation)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", "VS_RenderScene", "vs_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneVS ) );
    DXUT_SetDebugName( g_pSceneVS, "VS_RenderScene" );
    
    // Define our scene vertex data layout
    const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( SceneLayout, ARRAYSIZE( SceneLayout ), pBlob->GetBufferPointer(),
                                                pBlob->GetBufferSize(), &g_pSceneVertexLayout ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( g_pSceneVertexLayout, "Primary" );

    // Main scene VS (with tessellation)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", "VS_RenderSceneWithTessellation", "vs_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneWithTessellationVS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( g_pSceneWithTessellationVS, "VS_RenderSceneWithTessellation" );

    // PNTriangles HS
    V_RETURN( CreateHullShader() );
            
    // PNTriangles DS
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", "DS_PNTriangles", "ds_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPNTrianglesDS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( g_pPNTrianglesDS, "DS_PNTriangles" );

    // Main scene PS (no textures)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", "PS_RenderScene", "ps_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pScenePS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( g_pScenePS, "PS_RenderScene" );
    
    // Main scene PS (textured)
    V_RETURN( CompileShaderFromFile( L"PNTriangles11.hlsl", "PS_RenderSceneTextured", "ps_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTexturedScenePS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( g_pTexturedScenePS, "PS_RenderSceneTextured" );
    
    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CB_PNTRIANGLES );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPNTriangles ) );
    DXUT_SetDebugName( g_pcbPNTriangles, "CB_PNTRIANGLES" );

    // Setup the camera for each scene   
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    // Tiny
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -700.0f;
    g_Camera[MESH_TYPE_TINY].SetViewParams( &vecEye, &vecAt );
    // Tiger
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TIGER].SetViewParams( &vecEye, &vecAt );
    // Teapot
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TEAPOT].SetViewParams( &vecEye, &vecAt );
    // User
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -3.0f;
    g_Camera[MESH_TYPE_USER].SetViewParams( &vecEye, &vecAt );

    // Setup the mesh params for adaptive tessellation
    g_v3AdaptiveTessParams[MESH_TYPE_TINY].x    = 1.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TINY].y    = 1000.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TINY].z    = 700.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TIGER].x   = 1.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TIGER].y   = 10.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TIGER].z    = 4.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TEAPOT].x  = 1.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TEAPOT].y  = 10.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_TEAPOT].z    = 4.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_USER].x    = 1.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_USER].y    = 10.0f;
    g_v3AdaptiveTessParams[MESH_TYPE_USER].z    = 3.0f;

    // Setup the matrix for each mesh
    D3DXMATRIXA16 mModelRotationX;
    D3DXMATRIXA16 mModelRotationY;
    D3DXMATRIXA16 mModelTranslation;
    // Tiny
    D3DXMatrixRotationX( &mModelRotationX, -D3DX_PI / 2 ); 
    D3DXMatrixRotationY( &mModelRotationY, D3DX_PI ); 
    g_m4x4MeshMatrix[MESH_TYPE_TINY] = mModelRotationX * mModelRotationY;
    // Tiger
    D3DXMatrixRotationX( &mModelRotationX, -D3DX_PI / 36 ); 
    D3DXMatrixRotationY( &mModelRotationY, D3DX_PI / 4 ); 
    g_m4x4MeshMatrix[MESH_TYPE_TIGER] = mModelRotationX * mModelRotationY;
    // Teapot
    D3DXMatrixIdentity( &g_m4x4MeshMatrix[MESH_TYPE_TEAPOT] );
    // User
    D3DXMatrixTranslation( &mModelTranslation, 0.0f, -1.0f, 0.0f );
    g_m4x4MeshMatrix[MESH_TYPE_USER] = mModelTranslation;

    // Setup the light camera
    vecEye.x = 0.0f; vecEye.y = -1.0f; vecEye.z = -1.0f;
    g_LightCamera.SetViewParams( &vecEye, &vecAt );

    // Load the standard scene meshes
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"tiny\\tiny.sdkmesh" ) );
    V_RETURN( g_SceneMesh[MESH_TYPE_TINY].Create( pd3dDevice, str ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH,  L"tiger\\tiger.sdkmesh" ) );
    V_RETURN( g_SceneMesh[MESH_TYPE_TIGER].Create( pd3dDevice, str ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"teapot\\teapot.sdkmesh" ) );
    V_RETURN( g_SceneMesh[MESH_TYPE_TEAPOT].Create( pd3dDevice, str ) );
    
    // Load a user mesh and textures if present
    g_bUserMesh = false;
    g_pDiffuseTextureSRV = NULL;
    // The mesh

    str[0] = 0;
    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"user.sdkmesh" );
    if( SUCCEEDED(hr) && FileExists( str ) )
    {
        V_RETURN( g_SceneMesh[MESH_TYPE_USER].Create( pd3dDevice, str ) )
        g_bUserMesh = true;
    }
    // The user textures
    if( FileExists( L"media\\user\\diffuse.dds" ) )
    {
        V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"media\\user\\diffuse.dds", NULL, NULL, &g_pDiffuseTextureSRV, NULL ) )
        DXUT_SetDebugName( g_pDiffuseTextureSRV, "user\\diffuse.dds" );
    }
                        
    // Create sampler states for point and linear
    // Point
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamplePoint ) );
    DXUT_SetDebugName( g_pSamplePoint, "Point" );

    // Linear
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSampleLinear ) );
    DXUT_SetDebugName( g_pSampleLinear, "Linear" );

    // Set the raster state
    // Wireframe
    D3D11_RASTERIZER_DESC RasterizerDesc;
    RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.FrontCounterClockwise = FALSE;
    RasterizerDesc.DepthBias = 0;
    RasterizerDesc.DepthBiasClamp = 0.0f;
    RasterizerDesc.SlopeScaledDepthBias = 0.0f;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.AntialiasedLineEnable = FALSE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &g_pRasterizerStateWireframe ) );
    DXUT_SetDebugName( g_pRasterizerStateWireframe, "Wireframe" );

    // Solid
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDesc, &g_pRasterizerStateSolid ) );
    DXUT_SetDebugName( g_pRasterizerStateSolid, "Solid" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resize
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters    
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    for( int iMeshType=0; iMeshType<MESH_TYPE_MAX; iMeshType++ )
    {
        g_Camera[iMeshType].SetProjParams( D3DX_PI / 4, fAspectRatio, 1.0f, 5000.0f );
        g_Camera[iMeshType].SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
        g_Camera[iMeshType].SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );
    }

    // Setup the light camera's projection params
    g_LightCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, 1.0f, 5000.0f );
    g_LightCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LightCamera.SetButtonMasks( MOUSE_RIGHT_BUTTON, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 180, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 180, pBackBufferSurfaceDesc->Height - 600 );
    g_SampleUI.SetSize( 150, 110 );

    // We need a screen-sized STAGING resource for frame capturing
    D3D11_TEXTURE2D_DESC TexDesc;
    DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
    TexDesc.Width = pBackBufferSurfaceDesc->Width;
    TexDesc.Height = pBackBufferSurfaceDesc->Height;
    TexDesc.Format = pBackBufferSurfaceDesc->Format;
    TexDesc.SampleDesc = SingleSample;
    TexDesc.MipLevels = 1;
    TexDesc.Usage = D3D11_USAGE_STAGING;
    TexDesc.MiscFlags = 0;
    TexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    TexDesc.BindFlags = 0;
    TexDesc.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateTexture2D( &TexDesc, NULL, &g_pCaptureTexture ) )
    DXUT_SetDebugName( g_pCaptureTexture, "Capture" );
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Debug function which copies a GPU buffer to a CPU readable buffer
//--------------------------------------------------------------------------------------
ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    pDevice->CreateBuffer(&desc, NULL, &debugbuf);
    DXUT_SetDebugName( debugbuf, "Debug" );

    pd3dImmediateContext->CopyResource( debugbuf, pBuffer );
    
    return debugbuf;
}


//--------------------------------------------------------------------------------------
// Render
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Array of our samplers
    ID3D11SamplerState* ppSamplerStates[2] = { g_pSamplePoint, g_pSampleLinear };

    // Clear the render target & depth stencil
    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), ClearColor );
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );    
    
    // Get the projection & view matrix from the camera class
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mViewProjection;
    mWorld =  g_m4x4MeshMatrix[g_eMeshType] * *g_Camera[g_eMeshType].GetWorldMatrix();
    mView = *g_Camera[g_eMeshType].GetViewMatrix();
    mProj = *g_Camera[g_eMeshType].GetProjMatrix();
    mWorldViewProjection = mWorld * mView * mProj;
    mViewProjection = mView * mProj;
    
    // Get the direction of the light.
    D3DXVECTOR3 v3LightDir = *g_LightCamera.GetEyePt() - *g_LightCamera.GetLookAtPt();
    D3DXVec3Normalize( &v3LightDir, &v3LightDir );

    // Get the view vector.
    D3DXVECTOR3 v3ViewVector = *g_Camera[g_eMeshType].GetEyePt() - *g_Camera[g_eMeshType].GetLookAtPt();
    D3DXVec3Normalize( &v3ViewVector, &v3ViewVector );

    // Calculate the plane equations of the frustum in world space
    D3DXVECTOR4 f4ViewFrustumPlanes[6];
    ExtractPlanesFromFrustum( f4ViewFrustumPlanes, &mViewProjection );

    // Setup the constant buffer for the scene vertex shader
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbPNTriangles, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_PNTRIANGLES* pPNTrianglesCB = ( CB_PNTRIANGLES* )MappedResource.pData;
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4World, &mWorld );
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4ViewProjection, &mViewProjection );
    D3DXMatrixTranspose( &pPNTrianglesCB->f4x4WorldViewProjection, &mWorldViewProjection );
    pPNTrianglesCB->fLightDir[0] = v3LightDir.x; 
    pPNTrianglesCB->fLightDir[1] = v3LightDir.y; 
    pPNTrianglesCB->fLightDir[2] = v3LightDir.z; 
    pPNTrianglesCB->fLightDir[3] = 0.0f;
    pPNTrianglesCB->fEye[0] = g_Camera[g_eMeshType].GetEyePt()->x;
    pPNTrianglesCB->fEye[1] = g_Camera[g_eMeshType].GetEyePt()->y;
    pPNTrianglesCB->fEye[2] = g_Camera[g_eMeshType].GetEyePt()->z;
    pPNTrianglesCB->fViewVector[0] = v3ViewVector.x;
    pPNTrianglesCB->fViewVector[1] = v3ViewVector.y;
    pPNTrianglesCB->fViewVector[2] = v3ViewVector.z;
    pPNTrianglesCB->fTessFactors[0] = (float)g_uTessFactor;
    pPNTrianglesCB->fTessFactors[1] = (float)g_uTessFactor;
    pPNTrianglesCB->fTessFactors[2] = (float)g_v3AdaptiveTessParams[g_eMeshType].x;
    pPNTrianglesCB->fTessFactors[3] = (float)g_v3AdaptiveTessParams[g_eMeshType].y;
    pPNTrianglesCB->fScreenParams[0] = (float)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
    pPNTrianglesCB->fScreenParams[1] = (float)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
    pPNTrianglesCB->fGUIParams1[0] = g_fBackFaceCullEpsilon;
    pPNTrianglesCB->fGUIParams1[1] = ( g_fSilhoutteEpsilon > 0.99f ) ? ( 0.99f ) : ( g_fSilhoutteEpsilon );
    pPNTrianglesCB->fGUIParams1[2] = g_fRangeScale;
    pPNTrianglesCB->fGUIParams1[3] = (float)g_uEdgeSize;
    pPNTrianglesCB->fGUIParams2[0] = g_fResolutionScale;
    pPNTrianglesCB->fGUIParams2[1] = ( ( g_fViewFrustumCullEpsilon * 2.0f ) - 1.0f ) * g_v3AdaptiveTessParams[g_eMeshType].z;
    pPNTrianglesCB->f4ViewFrustumPlanes[0] = f4ViewFrustumPlanes[0]; 
    pPNTrianglesCB->f4ViewFrustumPlanes[1] = f4ViewFrustumPlanes[1]; 
    pPNTrianglesCB->f4ViewFrustumPlanes[2] = f4ViewFrustumPlanes[2]; 
    pPNTrianglesCB->f4ViewFrustumPlanes[3] = f4ViewFrustumPlanes[3]; 
    pd3dImmediateContext->Unmap( g_pcbPNTriangles, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
    pd3dImmediateContext->PSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );

    // Based on app and GUI settings set a bunch of bools that guide the render
    bool bTessellation = g_SampleUI.GetCheckBox( IDC_CHECKBOX_TESSELLATION )->GetChecked();
    bool bTextured = g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetChecked() && g_SampleUI.GetCheckBox( IDC_CHECKBOX_TEXTURED )->GetEnabled();
        
    // VS
    if( bTessellation )
    {
        pd3dImmediateContext->VSSetShader( g_pSceneWithTessellationVS, NULL, 0 );
    }
    else
    {
        pd3dImmediateContext->VSSetShader( g_pSceneVS, NULL, 0 );
    }
    pd3dImmediateContext->IASetInputLayout( g_pSceneVertexLayout );

    // HS
    ID3D11HullShader* pHS = NULL;
    if( bTessellation )
    {
        pd3dImmediateContext->HSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
        pHS = g_pPNTrianglesHS;
    }
    pd3dImmediateContext->HSSetShader( pHS, NULL, 0 );    
    
    // DS
    ID3D11DomainShader* pDS = NULL;
    if( bTessellation )
    {
        pd3dImmediateContext->DSSetConstantBuffers( g_iPNTRIANGLESCBBind, 1, &g_pcbPNTriangles );
        pDS = g_pPNTrianglesDS;
    }
    pd3dImmediateContext->DSSetShader( pDS, NULL, 0 );
    
    // GS
    pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );

    // PS
    ID3D11PixelShader* pPS = NULL;
    if( bTextured )
    {
        pd3dImmediateContext->PSSetSamplers( 0, 2, ppSamplerStates );
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &g_pDiffuseTextureSRV );
        pPS = g_pTexturedScenePS;
    }
    else
    {
        pPS = g_pScenePS;
    }
    pd3dImmediateContext->PSSetShader( pPS, NULL, 0 );

    // Set the rasterizer state
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_WIREFRAME )->GetChecked() )
    {
        pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe );
    }
    else
    {
        pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );
    }

    // Render the scene and optionally override the mesh topology and diffuse texture slot
    // Decide whether to use the user diffuse.dds
    UINT uDiffuseSlot = 0;
    if( ( g_eMeshType == MESH_TYPE_USER ) && ( NULL != g_pDiffuseTextureSRV ) )
    {
        uDiffuseSlot = INVALID_SAMPLER_SLOT;
    }
    // Decide which prim topology to use
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    if( bTessellation )
    {
        PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    }
    // Render the meshes    
    for( int iMesh = 0; iMesh < (int)g_SceneMesh[g_eMeshType].GetNumMeshes(); iMesh++ )
    {
        RenderMesh( &g_SceneMesh[g_eMeshType], (UINT)iMesh, PrimitiveTopology, uDiffuseSlot );
    }
    
    // Render GUI
    if( g_CmdLineParams.bRenderHUD )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );
        
        DXUT_EndPerfEvent();
    }

    // Always render text info 
    RenderText();

    // Decrement the exit frame counter
    g_CmdLineParams.iExitFrame--;

    // Exit on this frame
    if( g_CmdLineParams.iExitFrame == 0 )
    {
        if( g_CmdLineParams.bCapture )
        {
            CaptureFrame();
        }

        ::PostMessage( DXUTGetHWND(), WM_QUIT, 0, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    for( int iMeshType=0; iMeshType<MESH_TYPE_MAX; iMeshType++ )
    {
        g_SceneMesh[iMeshType].Destroy();
    }

    SAFE_RELEASE( g_pSceneVS );
    SAFE_RELEASE( g_pSceneWithTessellationVS );
    SAFE_RELEASE( g_pPNTrianglesHS );
    SAFE_RELEASE( g_pPNTrianglesDS );
    SAFE_RELEASE( g_pScenePS );
    SAFE_RELEASE( g_pTexturedScenePS );
        
    SAFE_RELEASE( g_pcbPNTriangles );

    SAFE_RELEASE( g_pCaptureTexture );

    SAFE_RELEASE( g_pSceneVertexLayout );
    
    SAFE_RELEASE( g_pSamplePoint );
    SAFE_RELEASE( g_pSampleLinear );

    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pRasterizerStateSolid );

    SAFE_RELEASE( g_pDiffuseTextureSRV );
}


//--------------------------------------------------------------------------------------
// Release swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    SAFE_RELEASE( g_pCaptureTexture );
}


//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, pDefines, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
   int nArgLen = (int) wcslen(strArg);
   int nCmdLen = (int) wcslen(strCmdLine);

   if( nCmdLen >= nArgLen && 
      _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && 
      (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' || strCmdLine[nArgLen] == L'=' ) )
   {
      strCmdLine += nArgLen;
      return true;
   }

   return false;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
   if( *strCmdLine == L':' || *strCmdLine == L'=' )
   {       
      strCmdLine++; // Skip ':'

      // Place NULL terminator in strFlag after current token
      wcscpy_s( strFlag, 256, strCmdLine );
      WCHAR* strSpace = strFlag;
      while (*strSpace && (*strSpace > L' '))
         strSpace++;
      *strSpace = 0;

      // Update strCmdLine
      strCmdLine += wcslen(strFlag);
      return true;
   }
   else
   {
      strFlag[0] = 0;
      return false;
   }
}


//--------------------------------------------------------------------------------------
// Helper function to parse the command line
//--------------------------------------------------------------------------------------
void ParseCommandLine()
{
    // set some defaults
    g_CmdLineParams.uWidth = 1024;
    g_CmdLineParams.uHeight = 768;
    g_CmdLineParams.bCapture = false;
    swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture.bmp" );
    g_CmdLineParams.iExitFrame = -1;
    g_CmdLineParams.bRenderHUD = true;

    // Perform application-dependant command line processing
    WCHAR* strCmdLine = GetCommandLine();
    WCHAR strFlag[MAX_PATH];
    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( strCmdLine, &nNumArgs );
    for( int iArg=1; iArg<nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"width" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.uWidth = _wtoi(strFlag);
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"height" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.uHeight = _wtoi(strFlag);
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"capturefilename" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   swprintf_s( g_CmdLineParams.strCaptureFilename, L"%s", strFlag );
                   g_CmdLineParams.bCapture = true;
                }
                continue;
            }

            if( IsNextArg( strCmdLine, L"nogui" ) )
            {
                g_CmdLineParams.bRenderHUD = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"exitframe" ) )
            {
                if( GetCmdParam( strCmdLine, strFlag ) )
                {
                   g_CmdLineParams.iExitFrame = _wtoi(strFlag);
                }
                continue;
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Helper function to capture a frame and dump it to disk 
//--------------------------------------------------------------------------------------
void CaptureFrame()
{
    // Retrieve RT resource
    ID3D11Resource *pRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource(&pRTResource);

    // Retrieve a Texture2D interface from resource
    ID3D11Texture2D* RTTexture;
    pRTResource->QueryInterface( __uuidof( ID3D11Texture2D ), ( LPVOID* )&RTTexture);

    // Check if RT is multisampled or not
    D3D11_TEXTURE2D_DESC    TexDesc;
    RTTexture->GetDesc(&TexDesc);
    if (TexDesc.SampleDesc.Count>1)
    {
        // RT is multisampled, need resolving before dumping to disk

        // Create single-sample RT of the same type and dimensions
        DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
        TexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        TexDesc.MipLevels = 1;
        TexDesc.Usage = D3D11_USAGE_DEFAULT;
        TexDesc.CPUAccessFlags = 0;
        TexDesc.BindFlags = 0;
        TexDesc.SampleDesc = SingleSample;

        ID3D11Texture2D *pSingleSampleTexture;
        DXUTGetD3D11Device()->CreateTexture2D(&TexDesc, NULL, &pSingleSampleTexture );
        DXUT_SetDebugName( pSingleSampleTexture, "Single Sample" );

        DXUTGetD3D11DeviceContext()->ResolveSubresource(pSingleSampleTexture, 0, RTTexture, 0, TexDesc.Format );

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource(g_pCaptureTexture, pSingleSampleTexture);

        D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), g_pCaptureTexture, D3DX11_IFF_BMP, g_CmdLineParams.strCaptureFilename );

        SAFE_RELEASE(pSingleSampleTexture);
        
    }
    else
    {
        // Single sample case

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource(g_pCaptureTexture, pRTResource);

        D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), g_pCaptureTexture, D3DX11_IFF_BMP, g_CmdLineParams.strCaptureFilename );
    }

    SAFE_RELEASE(RTTexture);

    SAFE_RELEASE(pRTResource);
}


//--------------------------------------------------------------------------------------
// Helper function that allows the app to render individual meshes of an sdkmesh
// and override the primitive topology
//--------------------------------------------------------------------------------------
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, D3D11_PRIMITIVE_TOPOLOGY PrimType, 
                UINT uDiffuseSlot, UINT uNormalSlot, UINT uSpecularSlot )
{
    #define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

    assert( NULL != pDXUTMesh );

    if( 0 < pDXUTMesh->GetOutstandingBufferResources() )
    {
        return;
    }

    SDKMESH_MESH* pMesh = pDXUTMesh->GetMesh( uMesh );

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if( pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS )
    {
        return;
    }

    for( UINT64 i = 0; i < pMesh->NumVertexBuffers; i++ )
    {
        pVB[i] = pDXUTMesh->GetVB11( uMesh, (UINT)i );
        Strides[i] = pDXUTMesh->GetVertexStride( uMesh, (UINT)i );
        Offsets[i] = 0;
    }

    ID3D11Buffer* pIB;
    pIB = pDXUTMesh->GetIB11( uMesh );
    DXGI_FORMAT ibFormat = pDXUTMesh->GetIBFormat11( uMesh );
    
    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    DXUTGetD3D11DeviceContext()->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;

    for( UINT uSubset = 0; uSubset < pMesh->NumSubsets; uSubset++ )
    {
        pSubset = pDXUTMesh->GetSubset( uMesh, uSubset );

        if( D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED == PrimType )
        {
            PrimType = pDXUTMesh->GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        }
        
        DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( PrimType );

        pMat = pDXUTMesh->GetMaterial( pSubset->MaterialID );
        if( uDiffuseSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pDiffuseRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uDiffuseSlot, 1, &pMat->pDiffuseRV11 );
        }

        if( uNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uNormalSlot, 1, &pMat->pNormalRV11 );
        }

        if( uSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uSpecularSlot, 1, &pMat->pSpecularRV11 );
        }

        UINT IndexCount = ( UINT )pSubset->IndexCount;
        UINT IndexStart = ( UINT )pSubset->IndexStart;
        UINT VertexStart = ( UINT )pSubset->VertexStart;
        
        DXUTGetD3D11DeviceContext()->DrawIndexed( IndexCount, IndexStart, VertexStart );
    }
}


//--------------------------------------------------------------------------------------
// Helper function to check for file existance
//--------------------------------------------------------------------------------------
bool FileExists( WCHAR* pFileName )
{
    DWORD fileAttr;    
    fileAttr = GetFileAttributes(pFileName);    
    if (0xFFFFFFFF == fileAttr)        
        return false;    
    return true;
}


//--------------------------------------------------------------------------------------
// Creates the appropriate HS based upon current GUI settings
//--------------------------------------------------------------------------------------
HRESULT CreateHullShader()
{
    HRESULT hr;

    // Release any existing shader
    SAFE_RELEASE( g_pPNTrianglesHS );
    
    // Create the shaders
    ID3DBlob* pBlob = NULL;
    D3D_SHADER_MACRO ShaderMacros[7];
    ZeroMemory( ShaderMacros, sizeof(ShaderMacros) );
    ShaderMacros[0].Name = NULL;
    ShaderMacros[0].Definition = "1";
    ShaderMacros[1].Name = NULL;
    ShaderMacros[1].Definition = "1";
    ShaderMacros[2].Name = NULL;
    ShaderMacros[2].Definition = "1";
    ShaderMacros[3].Name = NULL;
    ShaderMacros[3].Definition = "1";
    ShaderMacros[4].Name = NULL;
    ShaderMacros[4].Definition = "1";
    ShaderMacros[5].Name = NULL;
    ShaderMacros[5].Definition = "1";
    ShaderMacros[6].Name = NULL;
    ShaderMacros[6].Definition = "1";
    
    int i = 0;

    DWORD switches = 0;

    // Back face culling
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_BACK_FACE_CULL )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_BACK_FACE_CULLING";
        i++;
        switches |= 0x1;
    }

    // View frustum culling
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_FRUSTUM_CULL )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_VIEW_FRUSTUM_CULLING";
        i++;
        switches |= 0x2;
    }

    // Screen space adaptive tessellation
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_SCREEN_SPACE_ADAPTIVE_TESSELLATION";
        i++;
        switches |= 0x4;
    }

    // Distance adaptive tessellation (with screen resolution scaling)
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_DISTANCE_ADAPTIVE )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_DISTANCE_ADAPTIVE_TESSELLATION";
        i++;
        switches |= 0x8;
    }

    // Screen resolution adaptive tessellation
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_SCREEN_RESOLUTION_ADAPTIVE )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_SCREEN_RESOLUTION_ADAPTIVE_TESSELLATION";
        i++;
        switches |= 0x10;
    }

    // Orientation adaptive tessellation
    if( g_SampleUI.GetCheckBox( IDC_CHECKBOX_ORIENTATION_ADAPTIVE )->GetChecked() )
    {
        ShaderMacros[i].Name = "USE_ORIENTATION_ADAPTIVE_TESSELLATION";
        i++;
        switches |= 0x20;
    }

    // Create the shader
    hr = CompileShaderFromFile( L"PNTriangles11.hlsl", "HS_PNTriangles", "hs_5_0", &pBlob, ShaderMacros ); 
    if ( FAILED(hr) )
        return hr;

    hr = DXUTGetD3D11Device()->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPNTrianglesHS );
    if ( FAILED(hr) )
        return hr;
    SAFE_RELEASE( pBlob );

#if defined(DEBUG) || defined(PROFILE)
    char swstr[64];
    sprintf_s( swstr, sizeof(swstr), "Hull (GUI settings %x)\n", switches );
    DXUT_SetDebugName( g_pPNTrianglesHS, swstr );
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper function to normalize a plane
//--------------------------------------------------------------------------------------
void NormalizePlane( D3DXVECTOR4* pPlaneEquation )
{
    float mag;
    
    mag = sqrt( pPlaneEquation->x * pPlaneEquation->x + 
                pPlaneEquation->y * pPlaneEquation->y + 
                pPlaneEquation->z * pPlaneEquation->z );
    
    pPlaneEquation->x = pPlaneEquation->x / mag;
    pPlaneEquation->y = pPlaneEquation->y / mag;
    pPlaneEquation->z = pPlaneEquation->z / mag;
    pPlaneEquation->w = pPlaneEquation->w / mag;
}


//--------------------------------------------------------------------------------------
// Extract all 6 plane equations from frustum denoted by supplied matrix
//--------------------------------------------------------------------------------------
void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix )
{
    // Left clipping plane
    pPlaneEquation[0].x = pMatrix->_14 + pMatrix->_11;
    pPlaneEquation[0].y = pMatrix->_24 + pMatrix->_21;
    pPlaneEquation[0].z = pMatrix->_34 + pMatrix->_31;
    pPlaneEquation[0].w = pMatrix->_44 + pMatrix->_41;
    
    // Right clipping plane
    pPlaneEquation[1].x = pMatrix->_14 - pMatrix->_11;
    pPlaneEquation[1].y = pMatrix->_24 - pMatrix->_21;
    pPlaneEquation[1].z = pMatrix->_34 - pMatrix->_31;
    pPlaneEquation[1].w = pMatrix->_44 - pMatrix->_41;
    
    // Top clipping plane
    pPlaneEquation[2].x = pMatrix->_14 - pMatrix->_12;
    pPlaneEquation[2].y = pMatrix->_24 - pMatrix->_22;
    pPlaneEquation[2].z = pMatrix->_34 - pMatrix->_32;
    pPlaneEquation[2].w = pMatrix->_44 - pMatrix->_42;
    
    // Bottom clipping plane
    pPlaneEquation[3].x = pMatrix->_14 + pMatrix->_12;
    pPlaneEquation[3].y = pMatrix->_24 + pMatrix->_22;
    pPlaneEquation[3].z = pMatrix->_34 + pMatrix->_32;
    pPlaneEquation[3].w = pMatrix->_44 + pMatrix->_42;
    
    // Near clipping plane
    pPlaneEquation[4].x = pMatrix->_13;
    pPlaneEquation[4].y = pMatrix->_23;
    pPlaneEquation[4].z = pMatrix->_33;
    pPlaneEquation[4].w = pMatrix->_43;
    
    // Far clipping plane
    pPlaneEquation[5].x = pMatrix->_14 - pMatrix->_13;
    pPlaneEquation[5].y = pMatrix->_24 - pMatrix->_23;
    pPlaneEquation[5].z = pMatrix->_34 - pMatrix->_33;
    pPlaneEquation[5].w = pMatrix->_44 - pMatrix->_43;
    
    // Normalize the plane equations, if requested
    NormalizePlane( &pPlaneEquation[0] );
    NormalizePlane( &pPlaneEquation[1] );
    NormalizePlane( &pPlaneEquation[2] );
    NormalizePlane( &pPlaneEquation[3] );
    NormalizePlane( &pPlaneEquation[4] );
    NormalizePlane( &pPlaneEquation[5] );
}


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
