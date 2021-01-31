//--------------------------------------------------------------------------------------
// File: MotionBlur10.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#define MAX_TIME_STEPS 3    // This must stay in sync with MAX_TIME_STEPS in the FX file
#define MID_TIME_STEP 1
#define MAX_BONE_MATRICES 100  // This must stay in sync with MAX_BONE_MATRICES in the FX file

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect = NULL;

//MSAA rendertarget
UINT                                g_MSAASampleCount = 16;
UINT                                g_BackBufferWidth;
UINT                                g_BackBufferHeight;
ID3D10Texture2D*                    g_pRenderTarget = NULL;
ID3D10RenderTargetView*             g_pRTRV = NULL;
ID3D10Texture2D*                    g_pDSTarget = NULL;
ID3D10DepthStencilView*             g_pDSRV = NULL;

ID3D10InputLayout*                  g_pStaticVertexLayout;
ID3D10InputLayout*                  g_pSkinnedVertexLayout;

CDXUTSDKMesh*                       g_pLinkedMeshes = NULL;
CDXUTSDKMesh                        g_SceneMesh;
CDXUTSDKMesh                        g_FanMesh;
CDXUTSDKMesh                        g_AnimMesh;
D3DXMATRIX                          g_mFanWorld;
float                               g_fFanSpeed = 2 * D3DX_PI;
float                               g_fWarriorFade = 0.5f;
float                               g_fFanFade = 3.0f;
bool                                g_bUseMotionBlur = true;
bool                                g_bRenderOgre = true;

// Effect Variables
ID3D10EffectMatrixVariable*         g_pmWorldViewProj;
ID3D10EffectMatrixVariable*         g_pmViewProj;
ID3D10EffectMatrixVariable*         g_pmWorldView;
ID3D10EffectMatrixVariable*         g_pmBlurViewProj;
ID3D10EffectMatrixVariable*         g_pmBlurWorld;
ID3D10EffectMatrixVariable*         g_pmBoneWorld;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse;
ID3D10EffectScalarVariable*         g_pfFrameTime;
ID3D10EffectScalarVariable*         g_piNumSteps;
ID3D10EffectScalarVariable*         g_pfFadeDist;

// Effect Techniques
ID3D10EffectTechnique*              g_pRenderScene;
ID3D10EffectTechnique*              g_pRenderSkinnedScene;
ID3D10EffectTechnique*              g_pRenderMotionBlur;
ID3D10EffectTechnique*              g_pRenderSkinnedMotionBlur;

struct MESHLINK
{
    WCHAR szMeshName[MAX_PATH];
    UINT iBone;
};

MESHLINK g_MeshLinkages[] =
{
    { L"MotionBlur\\Hammer.sdkmesh", 13 },
    { L"MotionBlur\\LeftForearm.sdkmesh", 54 },
    { L"MotionBlur\\RightForearm.sdkmesh", 66 },
    { L"MotionBlur\\RightShoulder.sdkmesh", 72 },
    { L"MotionBlur\\LeftShoulder.sdkmesh", 72 },
    { L"MotionBlur\\BackPlate.sdkmesh", 72 },
    { L"MotionBlur\\Helmet.sdkmesh", 51 },
    { L"MotionBlur\\Eyes.sdkmesh", 51 },
    { L"MotionBlur\\Belt.sdkmesh", 63 },
    { L"MotionBlur\\LeftThigh.sdkmesh", 58 },
    { L"MotionBlur\\RightThigh.sdkmesh", 70 },
    { L"MotionBlur\\LeftShin.sdkmesh", 56 },
    { L"MotionBlur\\RightShin.sdkmesh", 68 },
};
UINT                                g_NumLinkedMeshes = sizeof( g_MeshLinkages ) / sizeof( MESHLINK );

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC             -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SAMPLE_COUNT        5
#define IDC_TOGGLE_BLUR         6
#define IDC_RENDER_OGRE	        7
#define IDC_TOGGLEWARP          8

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

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
HRESULT CreateRenderTarget( ID3D10Device* pd3dDevice, UINT uiWidth, UINT uiHeight, UINT uiSampleCount,
                            UINT uiSampleQuality );


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
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10SwapChainResized );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10SwapChainReleasing );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"MotionBlur10" );
    DXUTCreateDevice( true, 640, 480 );
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

    CDXUTCheckBox* pCheckBox = NULL;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_BLUR, L"Use Motion Blur", 0, 105, 140, 24, true, 0, false, &pCheckBox );
    g_SampleUI.AddCheckBox( IDC_RENDER_OGRE, L"Render Ogre", 0, 129, 140, 24, true, 0, false, &pCheckBox );

    g_SampleUI.AddStatic( IDC_STATIC, L"MSAA (S)amples", 0, 180, 105, 25 );

    CDXUTComboBox* pComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_SAMPLE_COUNT, 0, 205, 140, 24, 'S', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 30 );
}



//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
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

    // Disable MSAA settings from the settings dialog
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled( false );
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled( false );
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL )->SetEnabled(
        false );
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL )->SetEnabled(
        false );

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
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

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_TOGGLE_BLUR:
            g_bUseMotionBlur = !g_bUseMotionBlur;
            break;
        case IDC_RENDER_OGRE:
            g_bRenderOgre = !g_bRenderOgre;
            break;
        case IDC_SAMPLE_COUNT:
        {
            CDXUTComboBox* pComboBox = ( CDXUTComboBox* )pControl;

            g_MSAASampleCount = ( UINT )PtrToInt( pComboBox->GetSelectedData() );

            HRESULT hr = S_OK;
            ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
            if( pd3dDevice )
                V( CreateRenderTarget( pd3dDevice, g_BackBufferWidth, g_BackBufferHeight, g_MSAASampleCount, 0 ) );
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

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"MotionBlur10.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &g_pEffect, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_pRenderSkinnedScene = g_pEffect->GetTechniqueByName( "RenderSkinnedScene" );
    g_pRenderMotionBlur = g_pEffect->GetTechniqueByName( "RenderMotionBlur" );
    g_pRenderSkinnedMotionBlur = g_pEffect->GetTechniqueByName( "RenderSkinnedMotionBlur" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmViewProj = g_pEffect->GetVariableByName( "g_mViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pmBlurViewProj = g_pEffect->GetVariableByName( "g_mBlurViewProj" )->AsMatrix();
    g_pmBlurWorld = g_pEffect->GetVariableByName( "g_mBlurWorld" )->AsMatrix();
    g_pmBoneWorld = g_pEffect->GetVariableByName( "g_mBoneWorld" )->AsMatrix();
    g_ptxDiffuse = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pfFrameTime = g_pEffect->GetVariableByName( "g_fFrameTime" )->AsScalar();
    g_piNumSteps = g_pEffect->GetVariableByName( "g_iNumSteps" )->AsScalar();
    g_pfFadeDist = g_pEffect->GetVariableByName( "g_fFadeDist" )->AsScalar();

    // Define our vertex data layout
    const D3D10_INPUT_ELEMENT_DESC staticlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pRenderScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( staticlayout, 4, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pStaticVertexLayout ) );

    //Create the scene meshes
    g_SceneMesh.Create( pd3dDevice, L"motionblur\\WindMillStage.sdkmesh" );
    g_FanMesh.Create( pd3dDevice, L"motionblur\\Fan.sdkmesh" );
    D3DXMatrixTranslation( &g_mFanWorld, 0.0f, 3.62f, 2.012f );

    //Create the Linked Meshes
    g_pLinkedMeshes = new CDXUTSDKMesh[ g_NumLinkedMeshes ];
    if( !g_pLinkedMeshes )
        return E_OUTOFMEMORY;
    for( UINT iMesh = 0; iMesh < g_NumLinkedMeshes; iMesh++ )
    {
        g_pLinkedMeshes[iMesh].Create( pd3dDevice, g_MeshLinkages[iMesh].szMeshName );
    }

    const D3D10_INPUT_ELEMENT_DESC skinnedlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_pRenderSkinnedScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( skinnedlayout, 6, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSkinnedVertexLayout ) );
    g_AnimMesh.Create( pd3dDevice, L"motionblur\\Warrior.sdkmesh" );
    g_AnimMesh.LoadAnimation( L"motionblur\\Warrior.sdkmesh_anim" );

    //camera
    D3DXVECTOR3 vEye( 2.0f, 1.3f, -4.0f );
    D3DXVECTOR3 vAt( 0.0f, 1.0f, -1.11f );
    g_Camera.SetViewParams( &vEye, &vAt );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Update the MSAA sample count combo box for this format
//--------------------------------------------------------------------------------------
void UpdateMSAASampleCounts( ID3D10Device* pd3dDevice, DXGI_FORMAT fmt )
{
    CDXUTComboBox* pComboBox = NULL;
    bool bResetSampleCount = false;
    UINT iHighestSampleCount = 0;

    pComboBox = g_SampleUI.GetComboBox( IDC_SAMPLE_COUNT );
    if( !pComboBox )
        return;

    pComboBox->RemoveAllItems();

    WCHAR val[10];
    for( UINT i = 1; i <= D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT; i++ )
    {
        UINT Quality;
        if( SUCCEEDED( pd3dDevice->CheckMultisampleQualityLevels( fmt, i, &Quality ) ) &&
            Quality > 0 )
        {
            swprintf_s( val, 10, L"%d", i );
            pComboBox->AddItem( val, IntToPtr( i ) );
            iHighestSampleCount = i;
        }
        else if( g_MSAASampleCount == i )
        {
            bResetSampleCount = true;
        }
    }

    if( bResetSampleCount )
        g_MSAASampleCount = iHighestSampleCount;

    pComboBox->SetSelectedByData( IntToPtr( g_MSAASampleCount ) );

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
    g_Camera.SetProjParams( 60.0f * ( D3DX_PI / 180.0f ), fAspectRatio, 0.1f, 100.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    // Update the sample count
    UpdateMSAASampleCounts( pd3dDevice, pBackBufferSurfaceDesc->Format );

    // Create a render target
    g_BackBufferWidth = pBackBufferSurfaceDesc->Width;
    g_BackBufferHeight = pBackBufferSurfaceDesc->Height;
    V_RETURN( CreateRenderTarget( pd3dDevice, g_BackBufferWidth, g_BackBufferHeight, g_MSAASampleCount, 0 ) );

    return hr;
}

//--------------------------------------------------------------------------------------
void RenderSceneMesh( ID3D10Device* pd3dDevice, CDXUTSDKMesh* pMesh, bool bMotionBlur )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );
    for( int i = 0; i < MAX_TIME_STEPS; i++ )
        g_pmBlurWorld->SetMatrixArray( ( float* )mWorld, i, 1 );

    g_pfFadeDist->SetFloat( g_fWarriorFade );
    pd3dDevice->IASetInputLayout( g_pStaticVertexLayout );
    if( !bMotionBlur )
        pMesh->Render( pd3dDevice, g_pRenderScene, g_ptxDiffuse );
    else
        pMesh->Render( pd3dDevice, g_pRenderMotionBlur, g_ptxDiffuse );
}

//--------------------------------------------------------------------------------------
void RenderFanMesh( ID3D10Device* pd3dDevice, CDXUTSDKMesh* pMesh, double fTime, bool bMotionBlur )
{
    D3DXMATRIX mWorld;
    D3DXMATRIX mRot;

    float fRotStep = ( 1.0f / 30.0f ) / MAX_TIME_STEPS;
    float fRot = ( ( float )fTime - fRotStep * 2.0f );

    for( int i = 0; i < MAX_TIME_STEPS; i++ )
    {
        D3DXMatrixRotationZ( &mRot, -g_fFanSpeed * fRot );
        mWorld = mRot * g_mFanWorld;
        g_pmBlurWorld->SetMatrixArray( ( float* )mWorld, i, 1 );

        fRot += fRotStep;
    }

    g_pfFadeDist->SetFloat( g_fFanFade );
    pd3dDevice->IASetInputLayout( g_pStaticVertexLayout );
    if( !bMotionBlur )
        pMesh->Render( pd3dDevice, g_pRenderScene, g_ptxDiffuse );
    else
        pMesh->Render( pd3dDevice, g_pRenderMotionBlur, g_ptxDiffuse );
}

//--------------------------------------------------------------------------------------
void RenderLinkedMesh( ID3D10Device* pd3dDevice, CDXUTSDKMesh* pLinkedMesh, CDXUTSDKMesh* pAnimMesh, UINT iInfluence,
                       double fTime, bool bMotionBlur )
{
    // Set the bone matrices
    double fStepSize = 1.0f / 60.0f;
    double fTimeShift = fTime - MAX_TIME_STEPS * fStepSize;
    for( int s = 0; s < MAX_TIME_STEPS; s++ )
    {
        fTimeShift += fStepSize;

        D3DXMATRIX mIdentity;
        D3DXMatrixIdentity( &mIdentity );
        pAnimMesh->TransformMesh( &mIdentity, fTimeShift );

        const D3DXMATRIX* pMat = pAnimMesh->GetMeshInfluenceMatrix( 0, iInfluence );
        g_pmBlurWorld->SetMatrixArray( ( float* )pMat, s, 1 );
    }

    g_pfFadeDist->SetFloat( g_fWarriorFade );
    pd3dDevice->IASetInputLayout( g_pStaticVertexLayout );
    if( !bMotionBlur )
        pLinkedMesh->Render( pd3dDevice, g_pRenderScene, g_ptxDiffuse );
    else
        pLinkedMesh->Render( pd3dDevice, g_pRenderMotionBlur, g_ptxDiffuse );
}

//--------------------------------------------------------------------------------------
void RenderSkinnedMesh( ID3D10Device* pd3dDevice, CDXUTSDKMesh* pAnimMesh, double fTime, bool bMotionBlur )
{
    ID3D10Buffer* pBuffers[1];
    UINT stride[1];
    UINT offset[1] = { 0 };

    g_pfFadeDist->SetFloat( g_fWarriorFade );
    // Set vertex Layout
    pd3dDevice->IASetInputLayout( g_pSkinnedVertexLayout );

    // Render the meshes
    for( UINT m = 0; m < pAnimMesh->GetNumMeshes(); m++ )
    {
        pBuffers[0] = pAnimMesh->GetVB10( m, 0 );
        stride[0] = ( UINT )pAnimMesh->GetVertexStride( m, 0 );
        offset[0] = 0;

        pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
        pd3dDevice->IASetIndexBuffer( pAnimMesh->GetIB10( m ), pAnimMesh->GetIBFormat10( m ), 0 );

        SDKMESH_SUBSET* pSubset = NULL;
        SDKMESH_MATERIAL* pMat = NULL;
        D3D10_PRIMITIVE_TOPOLOGY PrimType;

        // Set the bone matrices
        double fStepSize = 1.0f / 60.0f;
        double fTimeShift = fTime - MAX_TIME_STEPS * fStepSize;
        for( int s = 0; s < MAX_TIME_STEPS; s++ )
        {
            fTimeShift += fStepSize;

            D3DXMATRIX mIdentity;
            D3DXMatrixIdentity( &mIdentity );
            pAnimMesh->TransformMesh( &mIdentity, fTimeShift );

            for( UINT i = 0; i < pAnimMesh->GetNumInfluences( m ); i++ )
            {
                const D3DXMATRIX* pMat = pAnimMesh->GetMeshInfluenceMatrix( m, i );
                g_pmBoneWorld->SetMatrixArray( ( float* )pMat, MAX_BONE_MATRICES * s + i, 1 );
            }
        }

        D3D10_TECHNIQUE_DESC techDesc;
        ID3D10EffectTechnique* pTech;
        if( !bMotionBlur )
        {
            pTech = g_pRenderSkinnedScene;
            pTech->GetDesc( &techDesc );
            for( UINT p = 0; p < techDesc.Passes; ++p )
            {
                for( UINT subset = 0; subset < pAnimMesh->GetNumSubsets( m ); subset++ )
                {
                    pSubset = pAnimMesh->GetSubset( m, subset );

                    PrimType = pAnimMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                    pd3dDevice->IASetPrimitiveTopology( PrimType );

                    pMat = pAnimMesh->GetMaterial( pSubset->MaterialID );
                    if( pMat )
                    {
                        g_ptxDiffuse->SetResource( pMat->pDiffuseRV10 );
                    }

                    pTech->GetPassByIndex( p )->Apply( 0 );
                    pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                             ( UINT )pSubset->VertexStart );
                }
            }
        }
        else
        {
            pTech = g_pRenderSkinnedMotionBlur;
            pTech->GetDesc( &techDesc );
            for( UINT p = 0; p < techDesc.Passes; ++p )
            {
                for( UINT subset = 0; subset < pAnimMesh->GetNumSubsets( m ); subset++ )
                {
                    pSubset = pAnimMesh->GetSubset( m, subset );

                    PrimType = pAnimMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                    pd3dDevice->IASetPrimitiveTopology( PrimType );

                    pMat = pAnimMesh->GetMaterial( pSubset->MaterialID );
                    if( pMat )
                    {
                        g_ptxDiffuse->SetResource( pMat->pDiffuseRV10 );
                    }

                    pTech->GetPassByIndex( p )->Apply( 0 );
                    pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                             ( UINT )pSubset->VertexStart );
                }
            }
        }
    }//nummeshes
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 1.0f, 1.0f, 1.0f, 0.0f }; // R, G, B, A
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Set our render target since we can't present multisampled ref
    ID3D10RenderTargetView* pOrigRT;
    ID3D10DepthStencilView* pOrigDS;
    pd3dDevice->OMGetRenderTargets( 1, &pOrigRT, &pOrigDS );
    ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pRTRV };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, g_pDSRV );

    pd3dDevice->ClearRenderTargetView( g_pRTRV, ClearColor );
    pd3dDevice->ClearDepthStencilView( g_pDSRV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Set Matrices
    D3DXMATRIX mWorldViewProj;
    D3DXMATRIX mWorldView;
    D3DXMATRIX mViewProj;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    // Fix the camera motion for now
    D3DXMATRIX mBlurViewProj[MAX_TIME_STEPS];
    for( int i = 0; i < MAX_TIME_STEPS; i++ )
    {
        mView = *g_Camera.GetViewMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mBlurViewProj[i] = mView * mProj;
    }
    g_pmBlurViewProj->SetMatrixArray( ( float* )mBlurViewProj, 0, MAX_TIME_STEPS );

    D3DXMatrixIdentity( &mWorld );
    mView = *g_Camera.GetViewMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mViewProj = mView * mProj;
    mWorldViewProj = mWorld * mViewProj;
    mWorldView = mWorld * mView;

    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmViewProj->SetMatrix( ( float* )&mViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mWorldView );

    RenderSceneMesh( pd3dDevice, &g_SceneMesh, false );
    RenderFanMesh( pd3dDevice, &g_FanMesh, fTime, false );
    if( g_bRenderOgre )
        RenderSkinnedMesh( pd3dDevice, &g_AnimMesh, fTime, false );

    for( UINT iMesh = 0; iMesh < g_NumLinkedMeshes; iMesh++ )
        RenderLinkedMesh( pd3dDevice, &g_pLinkedMeshes[iMesh], &g_AnimMesh, g_MeshLinkages[iMesh].iBone, fTime,
                          false );

    if( g_bUseMotionBlur )
    {
        RenderFanMesh( pd3dDevice, &g_FanMesh, fTime, true );
        if( g_bRenderOgre )
            RenderSkinnedMesh( pd3dDevice, &g_AnimMesh, fTime, true );

        for( UINT iMesh = 0; iMesh < g_NumLinkedMeshes; iMesh++ )
            RenderLinkedMesh( pd3dDevice, &g_pLinkedMeshes[iMesh], &g_AnimMesh, g_MeshLinkages[iMesh].iBone, fTime,
                              true );
    }

    //MSAA resolve
    ID3D10Resource* pRT;
    pOrigRT->GetResource( &pRT );
    D3D10_RENDER_TARGET_VIEW_DESC rtDesc;
    pOrigRT->GetDesc( &rtDesc );
    pd3dDevice->ResolveSubresource( pRT, D3D10CalcSubresource( 0, 0, 1 ), g_pRenderTarget, D3D10CalcSubresource( 0, 0,
                                                                                                                 1 ),
                                    rtDesc.Format );
    SAFE_RELEASE( pRT );

    // Use our Old RT again
    aRTViews[0] = pOrigRT;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, pOrigDS );
    SAFE_RELEASE( pOrigRT );
    SAFE_RELEASE( pOrigDS );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 0.6f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_RELEASE( g_pRenderTarget );
    SAFE_RELEASE( g_pRTRV );
    SAFE_RELEASE( g_pDSTarget );
    SAFE_RELEASE( g_pDSRV );
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pStaticVertexLayout );
    SAFE_RELEASE( g_pSkinnedVertexLayout );

    g_SceneMesh.Destroy();
    g_FanMesh.Destroy();
    for( UINT iMesh = 0; iMesh < g_NumLinkedMeshes; iMesh++ )
        g_pLinkedMeshes[iMesh].Destroy();
    SAFE_DELETE_ARRAY( g_pLinkedMeshes );

    g_AnimMesh.Destroy();
}


//--------------------------------------------------------------------------------------
// Create an MSAA rendertarget and DS surface
//--------------------------------------------------------------------------------------
HRESULT CreateRenderTarget( ID3D10Device* pd3dDevice, UINT uiWidth, UINT uiHeight, UINT uiSampleCount,
                            UINT uiSampleQuality )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pRenderTarget );
    SAFE_RELEASE( g_pRTRV );
    SAFE_RELEASE( g_pDSTarget );
    SAFE_RELEASE( g_pDSRV );

    ID3D10RenderTargetView* pOrigRT = NULL;
    ID3D10DepthStencilView* pOrigDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, &pOrigRT, &pOrigDS );

    D3D10_RENDER_TARGET_VIEW_DESC DescRTV;
    pOrigRT->GetDesc( &DescRTV );
    SAFE_RELEASE( pOrigRT );
    SAFE_RELEASE( pOrigDS );

    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = uiWidth;
    dstex.Height = uiHeight;
    dstex.MipLevels = 1;
    dstex.Format = DescRTV.Format;
    dstex.SampleDesc.Count = uiSampleCount;
    dstex.SampleDesc.Quality = uiSampleQuality;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pRenderTarget ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pRenderTarget, &DescRT, &g_pRTRV ) );

    //
    // Create depth stencil texture.
    //
    dstex.Width = uiWidth;
    dstex.Height = uiHeight;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_D32_FLOAT;
    dstex.SampleDesc.Count = uiSampleCount;
    dstex.SampleDesc.Quality = uiSampleQuality;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pDSTarget ) );

    // Create the depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
    DescDS.Format = DXGI_FORMAT_D32_FLOAT;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pDSTarget, &DescDS, &g_pDSRV ) );

    return hr;
}
