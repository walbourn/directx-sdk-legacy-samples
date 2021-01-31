//--------------------------------------------------------------------------------------
// File: HDAO10.1.cpp
//
// This code sample demonstrates the use of the DX10.1 Gather instruction to accelerate the
// HDAO technique  
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

#include "Sprite.h"
#include "Magnify.h"
#include "MagnifyTool.h"

//#define DEBUG_VS   // Uncomment this line to debug D3D9 vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug D3D9 pixel shaders 

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct SpriteVertex
{
    D3DXVECTOR3 v3Pos;
    D3DXVECTOR2 v2TexCoord;
};

typedef struct _HDAOParams
{
    float fRejectRadiusMax;
    float fRejectRadius;
    float fAcceptRadiusMax;
    float fAcceptRadius;
    float fIntensityMax;
    float fIntensity;
    float fNormalScaleMax;
    float fNormalScale;
    float fAcceptAngleMax;
    float fAcceptAngle;
}HDAOParams;

typedef enum _MESH_TYPE
{
    MESH_TYPE_DESERT_TANK        = 0,
    MESH_TYPE_MAX                = 1,
}MESH_TYPE;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera[MESH_TYPE_MAX]; // A model viewing camera
CModelViewerCamera          g_LightCamera;             // The light
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;           // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTDialog                 g_HUD;                   // dialog for standard controls
CDXUTDialog                 g_SampleUI;              // dialog for sample specific controls

// Direct3D 10 resources
static ID3DX10Font*                g_pFont10 = NULL;
static ID3DX10Sprite*              g_pSprite10 = NULL;
static ID3D10Effect*               g_pEffect10 = NULL;
static ID3D10InputLayout*          g_pVertexLayout = NULL;
static ID3D10InputLayout*          g_pHQVertexLayout = NULL;

// Effect variables
static ID3D10EffectMatrixVariable* g_pmWorldViewProj = NULL;
static ID3D10EffectMatrixVariable* g_pmInvProj = NULL;
static ID3D10EffectMatrixVariable* g_pmWorld = NULL;
static ID3D10EffectMatrixVariable* g_pmView = NULL;
static ID3D10EffectScalarVariable* g_pfTime = NULL;
static ID3D10EffectScalarVariable* g_pfQ = NULL;
static ID3D10EffectScalarVariable* g_pfQTimesZNear = NULL;
static ID3D10EffectScalarVariable* g_pfZFar = NULL;
static ID3D10EffectScalarVariable* g_pfZNear = NULL;
static ID3D10EffectVectorVariable* g_pLightDir = NULL;
static ID3D10EffectVectorVariable* g_pEyePt = NULL;
static ID3D10EffectVectorVariable* g_pRTSize = NULL;
static ID3D10EffectScalarVariable* g_pHDAORejectRadius = NULL;
static ID3D10EffectScalarVariable* g_pHDAOAcceptRadius = NULL;
static ID3D10EffectScalarVariable* g_pHDAOIntensity = NULL;
static ID3D10EffectScalarVariable* g_pHDAOKernelScale = NULL;
static ID3D10EffectScalarVariable* g_pHDAONormalScale = NULL;
static ID3D10EffectScalarVariable* g_pHDAOAcceptAngle = NULL;
static ID3D10EffectVectorVariable* g_pMaterialDiffuse = NULL;
static ID3D10EffectVectorVariable* g_pMaterialSpecular = NULL;
static ID3D10EffectScalarVariable* g_pTanH = NULL;
static ID3D10EffectScalarVariable* g_pTanV = NULL;

// Effect texture variables
static ID3D10EffectShaderResourceVariable* g_pSceneTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pNormalsTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pNormalsZTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pNormalsXYTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pHDAOTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pDepthTextureVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pDiffuseTexVar = NULL;
static ID3D10EffectShaderResourceVariable* g_pNormalTexVar = NULL;

// Techniques
static ID3D10EffectTechnique*      g_pRenderScene_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderScene_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderTexturedScene_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderTexturedScene_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderHQTexturedScene_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderHQTexturedScene_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderHDAO_Normals_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderHDAO_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderHDAO_Normals_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderHDAO_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderCombined = NULL;
static ID3D10EffectTechnique*      g_pRenderHDAOBuffer = NULL;
static ID3D10EffectTechnique*      g_pRenderCameraZ = NULL;
static ID3D10EffectTechnique*      g_pRenderNormalsBuffer_10_0 = NULL;
static ID3D10EffectTechnique*      g_pRenderNormalsBuffer_10_1 = NULL;
static ID3D10EffectTechnique*      g_pRenderUnCombined = NULL;

// Meshes
CDXUTSDKMesh    g_TankMesh;

// HDAO texture data
static ID3D10Texture2D*             g_pHDAOTexture = NULL;
static ID3D10ShaderResourceView*    g_pHDAOTextureSRV = NULL;
static ID3D10RenderTargetView*      g_pHDAOTextureRTV = NULL;

// Scene texture data
static ID3D10Texture2D*             g_pSceneTexture = NULL;
static ID3D10ShaderResourceView*    g_pSceneTextureSRV = NULL;
static ID3D10RenderTargetView*      g_pSceneTextureRTV = NULL;

// Normal texture data
static ID3D10Texture2D*             g_pNormalsTexture = NULL;
static ID3D10ShaderResourceView*    g_pNormalsTextureSRV = NULL;
static ID3D10RenderTargetView*      g_pNormalsTextureRTV = NULL;
static ID3D10Texture2D*             g_pNormalsZTexture = NULL;
static ID3D10ShaderResourceView*    g_pNormalsZTextureSRV = NULL;
static ID3D10RenderTargetView*      g_pNormalsZTextureRTV = NULL;
static ID3D10Texture2D*             g_pNormalsXYTexture = NULL;
static ID3D10ShaderResourceView*    g_pNormalsXYTextureSRV = NULL;
static ID3D10RenderTargetView*      g_pNormalsXYTextureRTV = NULL;

// Depth Texture view
static ID3D10ShaderResourceView*    g_pDepthTextureSRV = NULL;

// Fullscreen quad data
static ID3D10Buffer*                g_pQuadVertexBuffer = NULL;
static ID3D10InputLayout*           g_pQuadVertexLayout = NULL;

// GUI variables
static bool g_b10_1 = false;
static bool g_bHDAO = true;
static bool g_bViewHDAOBuffer = false;
static bool g_bUseNormals = true;
static bool g_bViewNormalsBuffer = false;
static bool g_bViewCameraZ = false;

// HDAO params for the different meshes
static MESH_TYPE g_MeshType = MESH_TYPE_DESERT_TANK;
static HDAOParams g_HDAOParams[MESH_TYPE_MAX];

// Magnify class
static Magnify g_Magnify;
static MagnifyTool g_MagnifyTool;

// Used by RenderTankMesh helper function
#define MAX_D3D10_VERTEX_STREAMS D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                1
#define IDC_TOGGLEREF                       2
#define IDC_CHANGEDEVICE                    3

#define IDC_STATIC_MESH                     ( 10 )
#define IDC_COMBOBOX_MESH                   ( 11 )
#define IDC_CHECKBOX_HDAO_ENABLE            ( 14 )
#define IDC_STATIC_REJECT_RADIUS            ( 15 )
#define IDC_SLIDER_REJECT_RADIUS            ( 16 )
#define IDC_STATIC_ACCEPT_RADIUS            ( 17 )
#define IDC_SLIDER_ACCEPT_RADIUS            ( 18 )
#define IDC_STATIC_INTENSITY                ( 19 )
#define IDC_SLIDER_INTENSITY                ( 20 )
#define IDC_CHECKBOX_VIEW_HDAO_BUFFER       ( 21 )
#define IDC_CHECKBOX_USE_NORMALS            ( 24 )
#define IDC_STATIC_NORMAL_SCALE             ( 25 )
#define IDC_SLIDER_NORMAL_SCALE             ( 26 )
#define IDC_STATIC_ACCEPT_ANGLE             ( 27 )
#define IDC_SLIDER_ACCEPT_ANGLE             ( 28 )
#define IDC_CHECKBOX_VIEW_NORMALS_BUFFER    ( 29 )
#define IDC_CHECKBOX_VIEW_CAMERA_Z          ( 30 )
#define IDC_CHECKBOX_10_1_ENABLE            ( 31 )


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void InitApp();
void RenderText();

HRESULT CreateSurface( ID3D10Texture2D** ppTexture, ID3D10ShaderResourceView** ppTextureSRV,
                       ID3D10RenderTargetView** ppTextureRTV, DXGI_FORMAT Format, unsigned int uWidth,
                       unsigned int uHeight );


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
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"High Definition Ambient Occlusion - Direct3D 10.1" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

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

    D3DCOLOR DlgColor = 0x88888888;

    // Disable MSAA in this app
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled( false );
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled( false );

    // D3D UI
    g_HUD.SetCallback( OnGUIEvent ); int iY = 0; 
    g_HUD.SetBackgroundColors( DlgColor );
    g_HUD.EnableCaption( true );
    g_HUD.SetCaptionText( L"-- Direct3D --" );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 10, iY, 150, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 10, iY += 24, 150, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 10, iY += 24, 150, 22, VK_F2 );

    // Add the sample UI controls
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 0;
    g_SampleUI.SetBackgroundColors( DlgColor );
    g_SampleUI.EnableCaption( true );
    g_SampleUI.SetCaptionText( L"-- Rendering Settings --" );
    g_SampleUI.AddStatic( IDC_STATIC_MESH, L"Mesh:", 5, iY, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_MESH, 5, iY += 20, 160, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 15 );
        pCombo->AddItem( L"Desert Tank", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_10_1_ENABLE, L"Direct3D 10.1", 5, iY += 28, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_HDAO_ENABLE, L"HDAO Enable", 5, iY += 25, 140, 24, true );
    g_SampleUI.AddStatic( IDC_STATIC_REJECT_RADIUS, L"Reject Radius : 0.070", 5, iY += 20, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_REJECT_RADIUS, 15, iY += 25, 140, 24, 0, 100, 50, false );
    g_SampleUI.AddStatic( IDC_STATIC_ACCEPT_RADIUS, L"Accept Radius : 0.00312", 5, iY += 20, 130, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_ACCEPT_RADIUS, 15, iY += 25, 140, 24, 0, 100, 50, false );
    g_SampleUI.AddStatic( IDC_STATIC_INTENSITY, L"Intensity : 2.14", 5, iY += 20, 85, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_INTENSITY, 15, iY += 25, 140, 24, 0, 100, 50, false );
    g_SampleUI.AddStatic( IDC_STATIC_NORMAL_SCALE, L"Normal Scale : 0.3", 5, iY += 20, 115, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_NORMAL_SCALE, 15, iY += 25, 140, 24, 0, 100, 50, false );
    g_SampleUI.AddStatic( IDC_STATIC_ACCEPT_ANGLE, L"Accept Angle : 0.98", 5, iY += 20, 115, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_ACCEPT_ANGLE, 15, iY += 25, 140, 24, 0, 100, 50, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_VIEW_HDAO_BUFFER, L"View HDAO Buffer", 5, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_VIEW_CAMERA_Z, L"View Camera Z", 5, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_VIEW_NORMALS_BUFFER, L"View Normals Buffer", 5, iY += 25, 140, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_USE_NORMALS, L"Use Normals", 5, iY += 25, 140, 24, true );
    
    // Init the magnify tool UI
    g_MagnifyTool.InitApp( &g_DialogResourceManager );
    g_MagnifyTool.GetMagnifyUI()->SetCallback( OnGUIEvent );

    // This sample doesn't need all functionality from the MagnifyTool
    g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetVisible( false );        
    g_MagnifyTool.GetMagnifyUI()->GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetVisible( false );
    g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetVisible( false );

    // Setup the default HDAO params for the mesh
    WCHAR szTemp[256];
    
    // Mesh HDAO params
    for( int iMesh=0; iMesh<MESH_TYPE_MAX; iMesh++ )
    {
        g_HDAOParams[iMesh].fRejectRadius = 0.43f;
        g_HDAOParams[iMesh].fAcceptRadius = 0.00312f;
        g_HDAOParams[iMesh].fIntensity = 2.14f;
        g_HDAOParams[iMesh].fNormalScale = 0.3f;
        g_HDAOParams[iMesh].fAcceptAngle = 0.98f;
        g_HDAOParams[iMesh].fRejectRadiusMax = g_HDAOParams[iMesh].fRejectRadius * 2.0f;
        g_HDAOParams[iMesh].fAcceptRadiusMax = g_HDAOParams[iMesh].fAcceptRadius * 2.0f;
        g_HDAOParams[iMesh].fIntensityMax = g_HDAOParams[iMesh].fIntensity * 2.0f;
        g_HDAOParams[iMesh].fNormalScaleMax = g_HDAOParams[iMesh].fNormalScale * 2.0f;
        g_HDAOParams[iMesh].fAcceptAngleMax = g_HDAOParams[iMesh].fAcceptAngle * 2.0f;
    }

    // Update the UI depending on the initial mesh choice
    swprintf_s( szTemp, L"Reject Radius : %.2f", g_HDAOParams[g_MeshType].fRejectRadius );
    g_SampleUI.GetStatic( IDC_STATIC_REJECT_RADIUS )->SetText( szTemp );
    swprintf_s( szTemp, L"Accept Radius : %.5f", g_HDAOParams[g_MeshType].fAcceptRadius );
    g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_RADIUS )->SetText( szTemp );
    swprintf_s( szTemp, L"Intensity : %.2f", g_HDAOParams[g_MeshType].fIntensity );
    g_SampleUI.GetStatic( IDC_STATIC_INTENSITY )->SetText( szTemp );
    swprintf_s( szTemp, L"Normal Scale : %.2f", g_HDAOParams[g_MeshType].fNormalScale );
    g_SampleUI.GetStatic( IDC_STATIC_NORMAL_SCALE )->SetText( szTemp );
    swprintf_s( szTemp, L"Accept Angle : %.2f", g_HDAOParams[g_MeshType].fAcceptAngle );
    g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_ANGLE )->SetText( szTemp );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );

    if( NULL != DXUTGetD3D10Device1() ) 
    {
        g_pTxtHelper->DrawTextLine( L"Direct3D 10.1 Device Detected" );  
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Please Run On Direct3D 10.1 Hardware To Access All Features Of This Sample!!" );  
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    }

    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
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
    
    LPCSTR pszTarget;
    D3D10_SHADER_MACRO* pShaderMacros = NULL;
    D3D10_SHADER_MACRO ShaderMacros[2];
    
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
        // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
        // Setting this flag improves the shader debugging experience, but still allows 
        // the shaders to be optimized and to run exactly the way they will run in 
        // the release configuration of this program.
        dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
        
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 500, &g_pSprite10 ) );
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDAO10.1.fx" ) );
   
    // Check to see if we have a DX10.1 device
    if( NULL == DXUTGetD3D10Device1() ) 
    {
        pszTarget = "fx_4_0";
        pShaderMacros = NULL;
    }
    else
    {
        pszTarget = "fx_4_1";
        ShaderMacros[0].Name = "DX10_1_ENABLED";
        ShaderMacros[0].Definition = "1";
        ShaderMacros[1].Name = NULL;
        pShaderMacros = ShaderMacros;
    }

    ID3D10Blob *pErrors = NULL;
    hr = D3DX10CreateEffectFromFile( str, pShaderMacros, NULL, pszTarget, dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &g_pEffect10, &pErrors, NULL );
    assert( D3D_OK == hr );
    if( pErrors )
    {
        char *pErrorStr = (char*)pErrors->GetBufferPointer();
        if( pErrorStr )
        {
            OutputDebugStringA( pErrorStr );
        }
    }
    SAFE_RELEASE( pErrors );    
    

    // Get effects variables
    g_pRenderScene_10_0 = g_pEffect10->GetTechniqueByName( "RenderScene_10_0" );
    g_pRenderScene_10_1 = g_pEffect10->GetTechniqueByName( "RenderScene_10_1" );
    g_pRenderTexturedScene_10_0 = g_pEffect10->GetTechniqueByName( "RenderTexturedScene_10_0" );
    g_pRenderTexturedScene_10_1 = g_pEffect10->GetTechniqueByName( "RenderTexturedScene_10_1" );
    g_pRenderHQTexturedScene_10_0 = g_pEffect10->GetTechniqueByName( "RenderHQTexturedScene_10_0" );
    g_pRenderHQTexturedScene_10_1 = g_pEffect10->GetTechniqueByName( "RenderHQTexturedScene_10_1" );
    g_pRenderCombined = g_pEffect10->GetTechniqueByName( "RenderCombined" );
    g_pRenderHDAOBuffer = g_pEffect10->GetTechniqueByName( "RenderHDAOBuffer" );
    g_pRenderCameraZ = g_pEffect10->GetTechniqueByName( "RenderCameraZ" );
    g_pRenderNormalsBuffer_10_0 = g_pEffect10->GetTechniqueByName( "RenderNormalsBuffer_10_0" );
    g_pRenderNormalsBuffer_10_1 = g_pEffect10->GetTechniqueByName( "RenderNormalsBuffer_10_1" );
    g_pRenderUnCombined = g_pEffect10->GetTechniqueByName( "RenderUnCombined" );
    g_pRenderHDAO_Normals_10_1 = g_pEffect10->GetTechniqueByName( "RenderHDAO_Normals_10_1" );
    g_pRenderHDAO_10_1 = g_pEffect10->GetTechniqueByName( "RenderHDAO_10_1" );
    g_pRenderHDAO_Normals_10_0 = g_pEffect10->GetTechniqueByName( "RenderHDAO_Normals_10_0" );
    g_pRenderHDAO_10_0 = g_pEffect10->GetTechniqueByName( "RenderHDAO_10_0" );
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_f4x4WorldViewProjection" )->AsMatrix();
    g_pmInvProj = g_pEffect10->GetVariableByName( "g_f4x4InvProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_f4x4World" )->AsMatrix();
    g_pmView = g_pEffect10->GetVariableByName( "g_f4x4View" )->AsMatrix();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pfQ = g_pEffect10->GetVariableByName( "g_fQ" )->AsScalar();
    g_pfQTimesZNear = g_pEffect10->GetVariableByName( "g_fQTimesZNear" )->AsScalar();
    g_pfZFar = g_pEffect10->GetVariableByName( "g_fZFar" )->AsScalar();
    g_pfZNear = g_pEffect10->GetVariableByName( "g_fZNear" )->AsScalar();
    g_pLightDir = g_pEffect10->GetVariableByName( "g_f3LightDir" )->AsVector();
    g_pEyePt = g_pEffect10->GetVariableByName( "g_f3EyePt" )->AsVector();
    g_pSceneTextureVar = g_pEffect10->GetVariableByName( "g_txScene" )->AsShaderResource();
    g_pNormalsTextureVar = g_pEffect10->GetVariableByName( "g_txNormals" )->AsShaderResource();
    g_pNormalsZTextureVar = g_pEffect10->GetVariableByName( "g_txNormalsZ" )->AsShaderResource();
    g_pNormalsXYTextureVar = g_pEffect10->GetVariableByName( "g_txNormalsXY" )->AsShaderResource();
    g_pHDAOTextureVar = g_pEffect10->GetVariableByName( "g_txHDAO" )->AsShaderResource();
    g_pDepthTextureVar = g_pEffect10->GetVariableByName( "g_txDepth" )->AsShaderResource();
    g_pDiffuseTexVar = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pNormalTexVar = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();
    g_pRTSize = g_pEffect10->GetVariableByName( "g_f2RTSize" )->AsVector();
    g_pHDAORejectRadius = g_pEffect10->GetVariableByName( "g_fHDAORejectRadius" )->AsScalar();
    g_pHDAOAcceptRadius = g_pEffect10->GetVariableByName( "g_fHDAOAcceptRadius" )->AsScalar();
    g_pHDAOIntensity = g_pEffect10->GetVariableByName( "g_fHDAOIntensity" )->AsScalar();
    g_pHDAONormalScale = g_pEffect10->GetVariableByName( "g_fNormalScale" )->AsScalar();
    g_pHDAOAcceptAngle = g_pEffect10->GetVariableByName( "g_fAcceptAngle" )->AsScalar();
    g_pMaterialDiffuse = g_pEffect10->GetVariableByName( "g_f4MaterialDiffuse" )->AsVector();
    g_pMaterialSpecular = g_pEffect10->GetVariableByName( "g_f4MaterialSpecular" )->AsVector();
    g_pTanH = g_pEffect10->GetVariableByName( "g_fTanH" )->AsScalar();
    g_pTanV = g_pEffect10->GetVariableByName( "g_fTanV" )->AsScalar();
    
    // Define our HQ vertex data layout
    const D3D10_INPUT_ELEMENT_DESC HQlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC HQPassDesc;
    g_pRenderHQTexturedScene_10_0->GetPassByIndex( 0 )->GetDesc( &HQPassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( HQlayout, sizeof( HQlayout ) / sizeof( HQlayout[0] ), HQPassDesc.pIAInputSignature,
                                             HQPassDesc.IAInputSignatureSize, &g_pHQVertexLayout ) );
    
    // Define our LQ vertex data layout
    const D3D10_INPUT_ELEMENT_DESC layout1[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc1;
    g_pRenderTexturedScene_10_0->GetPassByIndex( 0 )->GetDesc( &PassDesc1 );
    V_RETURN( pd3dDevice->CreateInputLayout( layout1, 3, PassDesc1.pIAInputSignature,
                                             PassDesc1.IAInputSignatureSize, &g_pVertexLayout ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 3.0f, -3.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_LightCamera.SetViewParams( &vecEye, &vecAt );
    vecEye.x = 0.0f; vecEye.y = 3.0f; vecEye.z = -15.0f;
    g_Camera[MESH_TYPE_DESERT_TANK].SetViewParams( &vecEye, &vecAt );
        
    // Load the Meshes
    g_TankMesh.Create( pd3dDevice, L"softparticles\\tankscene.sdkmesh", false );
        
    // Setup input layout and VB for fullscreen quad
    // Define the input layout
    D3D10_INPUT_ELEMENT_DESC FSlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof(FSlayout) / sizeof(FSlayout[0]);

    // Create the input layout
    D3D10_PASS_DESC PassDesc3;
    g_pRenderCombined->GetPassByIndex( 0 )->GetDesc( &PassDesc3 );
    hr = pd3dDevice->CreateInputLayout( FSlayout, numElements, PassDesc3.pIAInputSignature, 
        PassDesc3.IAInputSignatureSize, &g_pQuadVertexLayout );
    assert( D3D_OK == hr );

    // Fill out a unit quad
    SpriteVertex QuadVertices[6];
    QuadVertices[0].v3Pos = D3DXVECTOR3( -1.0f, -1.0f, 0.5f );
    QuadVertices[0].v2TexCoord = D3DXVECTOR2( 0.0f, 1.0f );
    QuadVertices[1].v3Pos = D3DXVECTOR3( -1.0f, 1.0f, 0.5f );
    QuadVertices[1].v2TexCoord = D3DXVECTOR2( 0.0f, 0.0f );
    QuadVertices[2].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[2].v2TexCoord = D3DXVECTOR2( 1.0f, 1.0f );
    QuadVertices[3].v3Pos = D3DXVECTOR3( -1.0f, 1.0f, 0.5f );
    QuadVertices[3].v2TexCoord = D3DXVECTOR2( 0.0f, 0.0f );
    QuadVertices[4].v3Pos = D3DXVECTOR3( 1.0f, 1.0f, 0.5f );
    QuadVertices[4].v2TexCoord = D3DXVECTOR2( 1.0f, 0.0f );
    QuadVertices[5].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.5f );
    QuadVertices[5].v2TexCoord = D3DXVECTOR2( 1.0f, 1.0f );

    // Create the vertex buffer
    D3D10_BUFFER_DESC BD;
    BD.Usage = D3D10_USAGE_DYNAMIC;
    BD.ByteWidth = sizeof( SpriteVertex ) * 6;
    BD.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    BD.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = QuadVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &g_pQuadVertexBuffer );
    assert( D3D_OK == hr );
    
    // Call the magnify tool hook function
    g_MagnifyTool.OnCreateDevice( pd3dDevice );
    
    // Only add the 10.1 path if we have capable HW
    if( DXUTGetD3D10Device1() )
    {
        g_SampleUI.GetCheckBox( IDC_CHECKBOX_10_1_ENABLE )->SetEnabled( true );
    }
    else
    {
        g_SampleUI.GetCheckBox( IDC_CHECKBOX_10_1_ENABLE )->SetEnabled( false );
    }

    return S_OK;
}

    
//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    for( int iMesh=0; iMesh<MESH_TYPE_MAX; iMesh++ )
    {
        g_Camera[iMesh].SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
        g_Camera[iMesh].SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
        g_Camera[iMesh].SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );
    }

    g_LightCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LightCamera.SetButtonMasks( MOUSE_RIGHT_BUTTON, 0, 0 );
    g_LightCamera.SetEnablePositionMovement( true );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width-180, 10 );
    g_HUD.SetSize( 170, 100 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width-180, 115 );
    g_SampleUI.SetSize( 170, 450 );

    // Call the MagnifyTool hook function 
    g_MagnifyTool.OnResizedSwapChain( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext, 
        pBackBufferSurfaceDesc->Width-180, 570 );
        
    // HDAO buffer creation
    hr = CreateSurface( &g_pHDAOTexture, &g_pHDAOTextureSRV, &g_pHDAOTextureRTV, 
                        DXGI_FORMAT_R16_FLOAT, pBackBufferSurfaceDesc->Width,
                        pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );
    
    // Scene buffer creation 
    hr = CreateSurface( &g_pSceneTexture, &g_pSceneTextureSRV, &g_pSceneTextureRTV, 
                        DXGI_FORMAT_R8G8B8A8_UNORM, pBackBufferSurfaceDesc->Width,
                        pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );

    // Normal buffer(s) creation
    // This is used for the 10.0 path
    hr = CreateSurface( &g_pNormalsTexture, &g_pNormalsTextureSRV, &g_pNormalsTextureRTV, 
                        DXGI_FORMAT_R16G16B16A16_FLOAT, pBackBufferSurfaceDesc->Width,
                        pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );

    // These 2 are used for the 10.1 path. Z needs to be in a single channel surface for
    // gather4 instruction
    hr = CreateSurface( &g_pNormalsZTexture, &g_pNormalsZTextureSRV, &g_pNormalsZTextureRTV, 
                        DXGI_FORMAT_R16_FLOAT, pBackBufferSurfaceDesc->Width,
                        pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );
    hr = CreateSurface( &g_pNormalsXYTexture, &g_pNormalsXYTextureSRV, &g_pNormalsXYTextureRTV, 
                        DXGI_FORMAT_R16G16_FLOAT, pBackBufferSurfaceDesc->Width,
                        pBackBufferSurfaceDesc->Height );
    assert( D3D_OK == hr );

    // We need a shader resource view of the main depth buffer
    ID3D10Resource* pDepthResource;
    g_MagnifyTool.GetDepthStencilView()->GetResource( &pDepthResource );

    D3D10_SHADER_RESOURCE_VIEW_DESC SRDesc;
    SRDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRDesc.Texture2D.MostDetailedMip = 0;
    SRDesc.Texture2D.MipLevels = 1;
    hr = DXUTGetD3D10Device()->CreateShaderResourceView( pDepthResource, &SRDesc, &g_pDepthTextureSRV );
    assert( D3D_OK == hr );

    SAFE_RELEASE( pDepthResource );

    // We need to set various shader constants
    float RTSize[2];
    RTSize[0] = (float)pBackBufferSurfaceDesc->Width;
    RTSize[1] = (float)pBackBufferSurfaceDesc->Height;
    g_pRTSize->SetFloatVector( RTSize );
    g_pHDAORejectRadius->SetFloat( g_HDAOParams[g_MeshType].fRejectRadius );
    g_pHDAOAcceptRadius->SetFloat( g_HDAOParams[g_MeshType].fAcceptRadius );
    g_pHDAOIntensity->SetFloat( g_HDAOParams[g_MeshType].fIntensity );
    g_pHDAONormalScale->SetFloat( g_HDAOParams[g_MeshType].fNormalScale );
    g_pHDAOAcceptAngle->SetFloat( g_HDAOParams[g_MeshType].fAcceptAngle );

    // Pass these camera space calculations 
    float fHFOV = ( D3DX_PI / 4 ) * fAspectRatio;
    float fVFOV = ( D3DX_PI / 4 );
    float fTanH = tanf( ( D3DX_PI / 2 ) - ( fHFOV / 2 ) );
    float fTanV = tanf( ( D3DX_PI / 2 ) - ( fVFOV / 2 ) );
    g_pTanH->SetFloat( fTanH );
    g_pTanV->SetFloat( fTanV );
    
    // Pass the back buffer and primary depth buffer to the MagnifyTool,
    // this is a once only operation, as the tool uses the hook functions to 
    // keep up with changing devices, and back buffers etc.
    static bool s_bFirstPass = true;
    if( s_bFirstPass )
    {
        D3D10_RENDER_TARGET_VIEW_DESC RTDesc;
        ID3D10Resource* pTempRTResource;
        DXUTGetD3D10RenderTargetView()->GetResource( &pTempRTResource );
        DXUTGetD3D10RenderTargetView()->GetDesc( &RTDesc );
        
        D3D10_DEPTH_STENCIL_VIEW_DESC DSDesc;
        ID3D10Resource* pTempDepthResource;
        g_MagnifyTool.GetDepthStencilView()->GetResource( &pTempDepthResource );
        g_MagnifyTool.GetDepthStencilView()->GetDesc( &DSDesc );

        // Both
        g_MagnifyTool.SetSourceResources( pTempRTResource, RTDesc.Format, pTempDepthResource, DSDesc.Format,
                    DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height,
                    DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count );
                
        g_MagnifyTool.SetPixelRegion( 128 );
        g_MagnifyTool.SetScale( 5 );

        s_bFirstPass = false;
    }
        
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIX mWorldViewProjection;
    D3DXMATRIX mTran;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mInvProj;
    ID3D10EffectTechnique* pTechnique = NULL;
    UINT Stride = sizeof( SpriteVertex );
    UINT Offset = 0;
    
    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    float NormalColor[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    float NormalZColor[1] = { 0.0f };
    float NormalXYColor[2] = { 0.0f, 1.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    pd3dDevice->ClearRenderTargetView( g_pSceneTextureRTV, ClearColor );

    if( g_b10_1 )
    {
        pd3dDevice->ClearRenderTargetView( g_pNormalsZTextureRTV, NormalZColor );
        pd3dDevice->ClearRenderTargetView( g_pNormalsXYTextureRTV, NormalXYColor );
    }
    else
    {
        pd3dDevice->ClearRenderTargetView( g_pNormalsTextureRTV, NormalColor );
    }
        

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the depth stencil
    ID3D10DepthStencilView* pDSV = g_MagnifyTool.GetDepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0x00 );
     
    // MRT setup
    ID3D10RenderTargetView* pRTVs[4];

    if( g_b10_1 )
    {
        pRTVs[0] = g_pSceneTextureRTV;
        pRTVs[1] = g_pNormalsZTextureRTV;
        pRTVs[2] = g_pNormalsXYTextureRTV;
        pRTVs[3] = NULL;

        // bind to our scene texture and the main depth buffer 
        pd3dDevice->OMSetRenderTargets( 3, (ID3D10RenderTargetView*const*)&pRTVs, pDSV );
    }
    else
    {
        pRTVs[0] = g_pSceneTextureRTV;
        pRTVs[1] = g_pNormalsTextureRTV;
        pRTVs[2] = NULL;
        pRTVs[3] = NULL;

        // bind to our scene texture and the main depth buffer 
        pd3dDevice->OMSetRenderTargets( 2, (ID3D10RenderTargetView*const*)&pRTVs, pDSV );
    }
        

    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera[g_MeshType].GetWorldMatrix();
    mProj = *g_Camera[g_MeshType].GetProjMatrix();
    mView = *g_Camera[g_MeshType].GetViewMatrix();
    mWorldViewProjection = mWorld * mView * mProj;
    D3DXMatrixInverse( &mInvProj, NULL, &mProj );

    // Temporary light
    D3DXVECTOR3 LightDir( -0.21f, 0.6f, -0.37f );
    D3DXVec3TransformCoord( &LightDir, &LightDir, g_LightCamera.GetWorldMatrix() );

    // Update the effect's variables.  Instead of using strings, it would 
    // be more efficient to cache a handle to the parameter by calling 
    // ID3DXEffect::GetParameterByName
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProjection );
    g_pmInvProj->SetMatrix( ( float* )&mInvProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_pmView->SetMatrix( ( float* )&mView );
    g_pfTime->SetFloat( ( float )fTime );
    float fQ = g_Camera[g_MeshType].GetFarClip() / ( g_Camera[g_MeshType].GetFarClip() - g_Camera[g_MeshType].GetNearClip() );
    g_pfZFar->SetFloat( g_Camera[g_MeshType].GetFarClip() );
    g_pfZNear->SetFloat( g_Camera[g_MeshType].GetNearClip() );
    g_pfQ->SetFloat( fQ );
    g_pfQTimesZNear->SetFloat( fQ * g_Camera[g_MeshType].GetNearClip() );
    g_pLightDir->SetFloatVector( (float*)LightDir );
    g_pEyePt->SetFloatVector( (float*)g_Camera[g_MeshType].GetEyePt() );

    
    ///////////////////////////////////////////////////////////////////////////
    // Render scene
    
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Main Scene" );

    // Render the meshes
    switch( g_MeshType )
    {
        case MESH_TYPE_DESERT_TANK:
        default:
            pd3dDevice->IASetInputLayout( g_pHQVertexLayout );
            if( g_b10_1 )
            {
                g_TankMesh.Render( pd3dDevice, g_pRenderHQTexturedScene_10_1, g_pDiffuseTexVar, g_pNormalTexVar );
            }
            else
            {
                g_TankMesh.Render( pd3dDevice, g_pRenderHQTexturedScene_10_0, g_pDiffuseTexVar, g_pNormalTexVar );
            }
            break;
    }
            
    DXUT_EndPerfEvent();
        

    ///////////////////////////////////////////////////////////////////////////
    // HDAO render

    if( g_bHDAO )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HDAO" );

        // Render the HDAO buffer
        pd3dDevice->OMSetRenderTargets( 1, &g_pHDAOTextureRTV, NULL );
        g_pDepthTextureVar->SetResource( g_pDepthTextureSRV );
                
        if( g_b10_1 )
        {
            if( g_bUseNormals )
            {
                pTechnique = g_pRenderHDAO_Normals_10_1;
            }
            else
            {
                pTechnique = g_pRenderHDAO_10_1;
            }
            g_pNormalsZTextureVar->SetResource( g_pNormalsZTextureSRV );
            g_pNormalsXYTextureVar->SetResource( g_pNormalsXYTextureSRV );
        }
        else
        {
            if( g_bUseNormals )
            {
                pTechnique = g_pRenderHDAO_Normals_10_0;
            }
            else
            {
                pTechnique = g_pRenderHDAO_10_0;
            }
            g_pNormalsTextureVar->SetResource( g_pNormalsTextureSRV );
        }
                
        pTechnique->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pDepthTextureVar->SetResource( NULL );
        g_pNormalsTextureVar->SetResource( NULL );
        g_pNormalsZTextureVar->SetResource( NULL );
        g_pNormalsXYTextureVar->SetResource( NULL );
        pTechnique->GetPassByIndex( 0 )->Apply( 0 );
                
        // Combine the HDAO buffer with the scene buffer
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        g_pHDAOTextureVar->SetResource( g_pHDAOTextureSRV );
        g_pSceneTextureVar->SetResource( g_pSceneTextureSRV );
        g_pRenderCombined->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pHDAOTextureVar->SetResource( NULL );
        g_pSceneTextureVar->SetResource( NULL );
        g_pRenderCombined->GetPassByIndex( 0 )->Apply( 0 );
        
        DXUT_EndPerfEvent();
    }
    else
    {
        // Render the scene without HDAO
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        g_pSceneTextureVar->SetResource( g_pSceneTextureSRV );
        g_pRenderUnCombined->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        UINT Stride = sizeof( SpriteVertex );
        UINT Offset = 0;
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pSceneTextureVar->SetResource( NULL );
        g_pRenderUnCombined->GetPassByIndex( 0 )->Apply( 0 );
    }

    if( g_bViewHDAOBuffer )
    {
        // Render the HDAO buffer
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        g_pHDAOTextureVar->SetResource( g_pHDAOTextureSRV );
        g_pRenderHDAOBuffer->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pHDAOTextureVar->SetResource( NULL );
        g_pRenderHDAOBuffer->GetPassByIndex( 0 )->Apply( 0 );
    }
    else if( g_bViewCameraZ )
    {
        // Render Camera Z
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        g_pDepthTextureVar->SetResource( g_pDepthTextureSRV );
        g_pRenderCameraZ->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pDepthTextureVar->SetResource( NULL );
        g_pRenderCameraZ->GetPassByIndex( 0 )->Apply( 0 );
    }
    else if( g_bViewNormalsBuffer )
    {
        // Render the Normals
        if( g_b10_1 )
        {
            pTechnique = g_pRenderNormalsBuffer_10_1;
            g_pNormalsZTextureVar->SetResource( g_pNormalsZTextureSRV );
            g_pNormalsXYTextureVar->SetResource( g_pNormalsXYTextureSRV );
        }
        else
        {
            pTechnique = g_pRenderNormalsBuffer_10_0;
            g_pNormalsTextureVar->SetResource( g_pNormalsTextureSRV );
        }
                    
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        pTechnique->GetPassByIndex( 0 )->Apply( 0 );
        pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );
        pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVertexBuffer, &Stride, &Offset );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 6, 0 );
        g_pNormalsTextureVar->SetResource( NULL );
        g_pNormalsZTextureVar->SetResource( NULL );
        g_pNormalsXYTextureVar->SetResource( NULL );
        pTechnique->GetPassByIndex( 0 )->Apply( 0 );
    }
    
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Magnify Tool" );

    g_MagnifyTool.Render();

    DXUT_EndPerfEvent();
    
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    g_MagnifyTool.GetMagnifyUI()->OnRender( fElapsedTime );    
    
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_RELEASE( g_pHDAOTextureSRV );
    SAFE_RELEASE( g_pHDAOTextureRTV );
    SAFE_RELEASE( g_pSceneTextureSRV );
    SAFE_RELEASE( g_pSceneTextureRTV );
    SAFE_RELEASE( g_pNormalsTextureSRV );
    SAFE_RELEASE( g_pNormalsTextureRTV );
    SAFE_RELEASE( g_pNormalsZTextureSRV );
    SAFE_RELEASE( g_pNormalsZTextureRTV );
    SAFE_RELEASE( g_pNormalsXYTextureSRV );
    SAFE_RELEASE( g_pNormalsXYTextureRTV );
    SAFE_RELEASE( g_pDepthTextureSRV );

    g_MagnifyTool.OnReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
 
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pHQVertexLayout );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    g_TankMesh.Destroy();

    SAFE_RELEASE( g_pHDAOTexture );
    SAFE_RELEASE( g_pHDAOTextureSRV );
    SAFE_RELEASE( g_pHDAOTextureRTV );
    
    SAFE_RELEASE( g_pSceneTexture );
    SAFE_RELEASE( g_pSceneTextureSRV );
    SAFE_RELEASE( g_pSceneTextureRTV );

    SAFE_RELEASE( g_pNormalsTexture );
    SAFE_RELEASE( g_pNormalsTextureSRV );
    SAFE_RELEASE( g_pNormalsTextureRTV );

    SAFE_RELEASE( g_pNormalsZTexture );
    SAFE_RELEASE( g_pNormalsZTextureSRV );
    SAFE_RELEASE( g_pNormalsZTextureRTV );

    SAFE_RELEASE( g_pNormalsXYTexture );
    SAFE_RELEASE( g_pNormalsXYTextureSRV );
    SAFE_RELEASE( g_pNormalsXYTextureRTV );

    SAFE_RELEASE( g_pDepthTextureSRV );

    SAFE_RELEASE( g_pQuadVertexLayout );
    SAFE_RELEASE( g_pQuadVertexBuffer );

    g_MagnifyTool.OnDestroyDevice();
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

        // Switch off v-sync
        pDeviceSettings->d3d10.SyncInterval = 0;
    }

    // Force MSAA off
    pDeviceSettings->d3d10.sd.SampleDesc.Count = 1;

    // Force D32 depth buffer format
    pDeviceSettings->d3d10.AutoDepthStencilFormat = DXGI_FORMAT_D32_FLOAT; 

    // The magnify tool creates the depth buffer, so that it's bindable as a shader 
    // resource
    g_MagnifyTool.ModifyDeviceSettings( pDeviceSettings, pUserContext );

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera[g_MeshType].FrameMove( fElapsedTime );

    if( !g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetChecked() )
    {
        g_LightCamera.FrameMove( fElapsedTime );
    }
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
    {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    *pbNoFurtherProcessing = g_MagnifyTool.GetMagnifyUI()->MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }
    
    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera[g_MeshType].HandleMessages( hWnd, uMsg, wParam, lParam );

    if( !g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetChecked() )
    {
        g_LightCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
    }
    
    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( ( nChar == VK_SPACE ) && (!bKeyDown) )
    {
        g_bHDAO = !g_bHDAO;
        g_SampleUI.GetCheckBox( IDC_CHECKBOX_HDAO_ENABLE )->SetChecked( g_bHDAO );
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    int nSelectedIndex = 0;
    int nTemp = 0;
    WCHAR szTemp[256];

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

        case IDC_COMBOBOX_MESH:
            nSelectedIndex = ((CDXUTComboBox*)pControl)->GetSelectedIndex();
            g_MeshType = (MESH_TYPE)nSelectedIndex;

            swprintf_s( szTemp, L"Reject Radius : %.2f", g_HDAOParams[g_MeshType].fRejectRadius );
            g_SampleUI.GetStatic( IDC_STATIC_REJECT_RADIUS )->SetText( szTemp );
            g_pHDAORejectRadius->SetFloat( g_HDAOParams[g_MeshType].fRejectRadius );
            g_SampleUI.GetSlider( IDC_SLIDER_REJECT_RADIUS )->SetValue( int( ( g_HDAOParams[g_MeshType].fRejectRadius / g_HDAOParams[g_MeshType].fRejectRadiusMax ) * 100.0f ) );

            swprintf_s( szTemp, L"Accept Radius : %.5f", g_HDAOParams[g_MeshType].fAcceptRadius );
            g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_RADIUS )->SetText( szTemp );
            g_pHDAOAcceptRadius->SetFloat( g_HDAOParams[g_MeshType].fAcceptRadius );
            g_SampleUI.GetSlider( IDC_SLIDER_ACCEPT_RADIUS )->SetValue( int( ( g_HDAOParams[g_MeshType].fAcceptRadius / g_HDAOParams[g_MeshType].fAcceptRadiusMax ) * 100.0f ) );

            swprintf_s( szTemp, L"Intensity : %.2f", g_HDAOParams[g_MeshType].fIntensity );
            g_SampleUI.GetStatic( IDC_STATIC_INTENSITY )->SetText( szTemp );
            g_pHDAOIntensity->SetFloat( g_HDAOParams[g_MeshType].fIntensity );
            g_SampleUI.GetSlider( IDC_SLIDER_INTENSITY )->SetValue( int( ( g_HDAOParams[g_MeshType].fIntensity / g_HDAOParams[g_MeshType].fIntensityMax ) * 100.0f ) );

            swprintf_s( szTemp, L"Normal Scale : %.5f", g_HDAOParams[g_MeshType].fNormalScale );
            g_SampleUI.GetStatic( IDC_STATIC_NORMAL_SCALE )->SetText( szTemp );
            g_pHDAONormalScale->SetFloat( g_HDAOParams[g_MeshType].fNormalScale );
            g_SampleUI.GetSlider( IDC_SLIDER_NORMAL_SCALE )->SetValue( int( ( g_HDAOParams[g_MeshType].fNormalScale / g_HDAOParams[g_MeshType].fNormalScaleMax ) * 100.0f ) );

            swprintf_s( szTemp, L"Accept Angle : %.2f", g_HDAOParams[g_MeshType].fAcceptAngle );
            g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_ANGLE )->SetText( szTemp );
            g_pHDAOAcceptAngle->SetFloat( g_HDAOParams[g_MeshType].fAcceptAngle );
            g_SampleUI.GetSlider( IDC_SLIDER_ACCEPT_ANGLE )->SetValue( int( ( g_HDAOParams[g_MeshType].fAcceptAngle / g_HDAOParams[g_MeshType].fAcceptAngleMax ) * 100.0f ) );
            break;

        case IDC_CHECKBOX_10_1_ENABLE:
            g_b10_1 = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;

        case IDC_CHECKBOX_HDAO_ENABLE:
            g_bHDAO = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;

        case IDC_SLIDER_REJECT_RADIUS:
            nTemp = ((CDXUTSlider*)pControl)->GetValue();
            g_HDAOParams[g_MeshType].fRejectRadius = ( nTemp / 100.0f );
            g_HDAOParams[g_MeshType].fRejectRadius *= g_HDAOParams[g_MeshType].fRejectRadiusMax; 
            swprintf_s( szTemp, L"Reject Radius : %.2f", g_HDAOParams[g_MeshType].fRejectRadius );
            g_SampleUI.GetStatic( IDC_STATIC_REJECT_RADIUS )->SetText( szTemp );
            g_pHDAORejectRadius->SetFloat( g_HDAOParams[g_MeshType].fRejectRadius );
            break;

        case IDC_SLIDER_ACCEPT_RADIUS:
            nTemp = ((CDXUTSlider*)pControl)->GetValue();
            g_HDAOParams[g_MeshType].fAcceptRadius = ( nTemp / 100.0f );
            g_HDAOParams[g_MeshType].fAcceptRadius *= g_HDAOParams[g_MeshType].fAcceptRadiusMax;
            swprintf_s( szTemp, L"Accept Radius : %.5f", g_HDAOParams[g_MeshType].fAcceptRadius );
            g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_RADIUS )->SetText( szTemp );
            g_pHDAOAcceptRadius->SetFloat( g_HDAOParams[g_MeshType].fAcceptRadius );
            break;

        case IDC_SLIDER_INTENSITY:
            nTemp = ((CDXUTSlider*)pControl)->GetValue();
            g_HDAOParams[g_MeshType].fIntensity = ( nTemp / 100.0f );
            g_HDAOParams[g_MeshType].fIntensity *= g_HDAOParams[g_MeshType].fIntensityMax;
            swprintf_s( szTemp, L"Intensity : %.2f", g_HDAOParams[g_MeshType].fIntensity );
            g_SampleUI.GetStatic( IDC_STATIC_INTENSITY )->SetText( szTemp );
            g_pHDAOIntensity->SetFloat( g_HDAOParams[g_MeshType].fIntensity );
            break;

        case IDC_SLIDER_NORMAL_SCALE:
            nTemp = ((CDXUTSlider*)pControl)->GetValue();
            g_HDAOParams[g_MeshType].fNormalScale = ( nTemp / 100.0f );
            g_HDAOParams[g_MeshType].fNormalScale *= g_HDAOParams[g_MeshType].fNormalScaleMax;
            swprintf_s( szTemp, L"Normal Scale : %.5f", g_HDAOParams[g_MeshType].fNormalScale );
            g_SampleUI.GetStatic( IDC_STATIC_NORMAL_SCALE )->SetText( szTemp );
            g_pHDAONormalScale->SetFloat( g_HDAOParams[g_MeshType].fNormalScale );
            break;

        case IDC_SLIDER_ACCEPT_ANGLE:
            nTemp = ((CDXUTSlider*)pControl)->GetValue();
            g_HDAOParams[g_MeshType].fAcceptAngle = ( nTemp / 100.0f );
            g_HDAOParams[g_MeshType].fAcceptAngle *= g_HDAOParams[g_MeshType].fAcceptAngleMax;
            swprintf_s( szTemp, L"Accept Angle : %.2f", g_HDAOParams[g_MeshType].fAcceptAngle );
            g_SampleUI.GetStatic( IDC_STATIC_ACCEPT_ANGLE )->SetText( szTemp );
            g_pHDAOAcceptAngle->SetFloat( g_HDAOParams[g_MeshType].fAcceptAngle );
            break;

        case IDC_CHECKBOX_VIEW_HDAO_BUFFER:
            g_bViewHDAOBuffer = ((CDXUTCheckBox*)pControl)->GetChecked();
            g_bViewNormalsBuffer = ( g_bViewHDAOBuffer ) ? ( false ) : ( g_bViewNormalsBuffer );
            g_bViewCameraZ = ( g_bViewHDAOBuffer ) ? ( false ) : ( g_bViewCameraZ );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_NORMALS_BUFFER )->SetChecked( g_bViewNormalsBuffer );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_CAMERA_Z )->SetChecked( g_bViewCameraZ );
            break;

        case IDC_CHECKBOX_VIEW_NORMALS_BUFFER:
            g_bViewNormalsBuffer = ((CDXUTCheckBox*)pControl)->GetChecked();
            g_bViewHDAOBuffer = ( g_bViewNormalsBuffer ) ? ( false ) : ( g_bViewHDAOBuffer );
            g_bViewCameraZ = ( g_bViewNormalsBuffer ) ? ( false ) : ( g_bViewCameraZ );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_HDAO_BUFFER )->SetChecked( g_bViewHDAOBuffer );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_CAMERA_Z )->SetChecked( g_bViewCameraZ );

            break;

        case IDC_CHECKBOX_VIEW_CAMERA_Z:
            g_bViewCameraZ = ((CDXUTCheckBox*)pControl)->GetChecked();
            g_bViewHDAOBuffer = ( g_bViewCameraZ ) ? ( false ) : ( g_bViewHDAOBuffer );
            g_bViewNormalsBuffer = ( g_bViewCameraZ ) ? ( false ) : ( g_bViewNormalsBuffer );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_HDAO_BUFFER )->SetChecked( g_bViewHDAOBuffer );
            g_SampleUI.GetCheckBox( IDC_CHECKBOX_VIEW_NORMALS_BUFFER )->SetChecked( g_bViewNormalsBuffer );
            break;

        case IDC_CHECKBOX_USE_NORMALS:
            g_bUseNormals = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
    }

    // Call the MagnifyTool gui event handler
    g_MagnifyTool.OnGUIEvent( nEvent, nControlID, pControl, pUserContext );
}


//--------------------------------------------------------------------------------------
// Utility function for surface creation
//--------------------------------------------------------------------------------------
HRESULT CreateSurface( ID3D10Texture2D** ppTexture, ID3D10ShaderResourceView** ppTextureSRV,
                       ID3D10RenderTargetView** ppTextureRTV, DXGI_FORMAT Format, unsigned int uWidth,
                       unsigned int uHeight )
{
    HRESULT hr;
    D3D10_TEXTURE2D_DESC Desc;
    D3D10_SHADER_RESOURCE_VIEW_DESC SRDesc;
    D3D10_RENDER_TARGET_VIEW_DESC RTDesc;

    SAFE_RELEASE( *ppTexture );
    SAFE_RELEASE( *ppTextureSRV );
    SAFE_RELEASE( *ppTextureRTV );

    ZeroMemory( &Desc, sizeof( Desc ) );
    Desc.Width = uWidth;
    Desc.Height = uHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = Format;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D10_USAGE_DEFAULT;
    Desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

    hr = DXUTGetD3D10Device()->CreateTexture2D( &Desc, NULL, ppTexture );
    assert( D3D_OK == hr );

    SRDesc.Format = Format;
    SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRDesc.Texture2D.MostDetailedMip = 0;
    SRDesc.Texture2D.MipLevels = 1;
    hr = DXUTGetD3D10Device()->CreateShaderResourceView( *ppTexture, &SRDesc, ppTextureSRV );
    assert( D3D_OK == hr );

    RTDesc.Format = Format;
    RTDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTDesc.Texture2D.MipSlice = 0;
    hr = DXUTGetD3D10Device()->CreateRenderTargetView( *ppTexture, &RTDesc, ppTextureRTV );
    assert( D3D_OK == hr );

    return hr;
}

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------