//--------------------------------------------------------------------------------------
// File: SparseMorphTargets.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include "MorphTarget.h"
#include "LightProbe.h"

struct SCENE_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR2 tex;
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
float                               g_fLightScale = 1.30f;
float                               g_fOily = 0.05f;
UINT                                g_iMorphsToApply = 5;

// Direct3D 10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;

ID3D10InputLayout*                  g_pQuadLayout = NULL;
ID3D10InputLayout*                  g_pMeshLayout = NULL;
ID3D10InputLayout*                  g_pSceneLayout = NULL;

// Technique handles
ID3D10EffectTechnique*              g_pRenderReferencedObject;
ID3D10EffectTechnique*              g_pRender2DQuad;
ID3D10EffectTechnique*              g_pRender2DQuadNoAlpha;
ID3D10EffectTechnique*              g_pShow2DQuad;
ID3D10EffectTechnique*              g_pRenderScene;

// Effect Param handles
ID3D10EffectShaderResourceVariable* g_ptxDiffuse;
ID3D10EffectShaderResourceVariable* g_ptxNormal;
ID3D10EffectShaderResourceVariable* g_ptxEnvMap;
ID3D10EffectShaderResourceVariable* g_ptxVertData;
ID3D10EffectShaderResourceVariable* g_ptxVertDataOrig;

ID3D10EffectScalarVariable*         g_pfBlendAmt;
ID3D10EffectVectorVariable*         g_pvMaxDeltas;
ID3D10EffectScalarVariable*         g_pDataTexSize;
ID3D10EffectScalarVariable*         g_pRT;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj;
ID3D10EffectMatrixVariable*         g_pmWorld;
ID3D10EffectVectorVariable*         g_pvLightDir;
ID3D10EffectVectorVariable*         g_pvCameraPos;
ID3D10EffectScalarVariable*         g_pfOily;
ID3D10EffectVectorVariable*         g_pRLight;
ID3D10EffectVectorVariable*         g_pGLight;
ID3D10EffectVectorVariable*         g_pBLight;

// LDPRT basis textures
ID3D10EffectShaderResourceVariable* g_pCLinBF;
ID3D10EffectShaderResourceVariable* g_pQuadBF;
ID3D10EffectShaderResourceVariable* g_pCubeBFA;
ID3D10EffectShaderResourceVariable* g_pCubeBFB;
ID3D10EffectShaderResourceVariable* g_pQuarBFA;
ID3D10EffectShaderResourceVariable* g_pQuarBFB;
ID3D10EffectShaderResourceVariable* g_pQuarBFC;
ID3D10EffectShaderResourceVariable* g_pQuinBFA;
ID3D10EffectShaderResourceVariable* g_pQuinBFB;

// MorphTarget object
CMorphTargetManager                 g_MorphObject;

// Light probe
CLightProbe                         g_LightProbe;
ID3D10Texture2D*                    g_pSHBasisTextures[9];  // Store SH basis functions using textures
ID3D10ShaderResourceView*           g_pSHBasisTexRV[9];  // resource views for the above

// MorphTarget textures
ID3D10ShaderResourceView*           g_pDiffuseTexRV;
ID3D10ShaderResourceView*           g_pNormalTexRV;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_NUMMORPHS_STATIC	5
#define IDC_NUMMORPHS           6
#define IDC_TOGGLEWARP          7

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
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
void UpdateLightingEnvironment();
HRESULT CreateSHBasisTextures( ID3D10Device* pd3dDevice );
void BindLDPRTBasisTextures();


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

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10SwapChainResized );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10SwapChainReleasing );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SparseMorphTargets" );
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

    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Morphs to Apply: %d", g_iMorphsToApply );
    g_SampleUI.AddStatic( IDC_NUMMORPHS_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_NUMMORPHS, 35, iY += 24, 135, 22, 0, 22, g_iMorphsToApply );
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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_NUMMORPHS:
        {
            g_iMorphsToApply = g_SampleUI.GetSlider( IDC_NUMMORPHS )->GetValue();

            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Morphs to Apply: %d", g_iMorphsToApply );
            g_SampleUI.GetStatic( IDC_NUMMORPHS_STATIC )->SetText( str );

            break;
        }
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SparseMorphTargets.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

	// Check for D3D10 linear filtering on DXGI_FORMAT_R32G32B32A32.  If the card can't do it, use point filtering.  Pointer filter may cause
	// banding in the lighting.
	UINT FormatSupport = 0;
	V_RETURN( pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R32G32B32A32_FLOAT, &FormatSupport ) );

	char strUsePointCubeSampling[MAX_PATH];
    sprintf_s( strUsePointCubeSampling, ARRAYSIZE( strUsePointCubeSampling ), "%d", ( FormatSupport & D3D10_FORMAT_SUPPORT_SHADER_SAMPLE ) > 0 );

    D3D10_SHADER_MACRO macros[] =
    {
        { "USE_POINT_CUBE_SAMPLING", strUsePointCubeSampling },
        { NULL, NULL }
    };

    V_RETURN( D3DX10CreateEffectFromFile( str, macros, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderReferencedObject = g_pEffect10->GetTechniqueByName( "RenderReferencedObject" );
    g_pRender2DQuad = g_pEffect10->GetTechniqueByName( "Render2DQuad" );
    g_pRender2DQuadNoAlpha = g_pEffect10->GetTechniqueByName( "Render2DQuadNoAlpha" );
    g_pShow2DQuad = g_pEffect10->GetTechniqueByName( "Show2DQuad" );
    g_pRenderScene = g_pEffect10->GetTechniqueByName( "RenderScene" );

    // Obtain the parameter handles
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxNormal = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();
    g_ptxEnvMap = g_pEffect10->GetVariableByName( "g_txEnvMap" )->AsShaderResource();
    g_ptxVertData = g_pEffect10->GetVariableByName( "g_txVertData" )->AsShaderResource();
    g_ptxVertDataOrig = g_pEffect10->GetVariableByName( "g_txVertDataOrig" )->AsShaderResource();

    g_pfBlendAmt = g_pEffect10->GetVariableByName( "g_fBlendAmt" )->AsScalar();
    g_pvMaxDeltas = g_pEffect10->GetVariableByName( "g_vMaxDeltas" )->AsVector();
    g_pDataTexSize = g_pEffect10->GetVariableByName( "g_DataTexSize" )->AsScalar();
    g_pRT = g_pEffect10->GetVariableByName( "g_RT" )->AsScalar();
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pvLightDir = g_pEffect10->GetVariableByName( "g_vLightDir" )->AsVector();
    g_pvCameraPos = g_pEffect10->GetVariableByName( "g_vCameraPos" )->AsVector();
    g_pfOily = g_pEffect10->GetVariableByName( "g_fOily" )->AsScalar();
    g_pRLight = g_pEffect10->GetVariableByName( "RLight" )->AsVector();
    g_pGLight = g_pEffect10->GetVariableByName( "GLight" )->AsVector();
    g_pBLight = g_pEffect10->GetVariableByName( "BLight" )->AsVector();

    //ldprt basis textures
    g_pCLinBF = g_pEffect10->GetVariableByName( "CLinBF" )->AsShaderResource();
    g_pQuadBF = g_pEffect10->GetVariableByName( "QuadBF" )->AsShaderResource();
    g_pCubeBFA = g_pEffect10->GetVariableByName( "CubeBFA" )->AsShaderResource();
    g_pCubeBFB = g_pEffect10->GetVariableByName( "CubeBFB" )->AsShaderResource();
    g_pQuarBFA = g_pEffect10->GetVariableByName( "QuarBFA" )->AsShaderResource();
    g_pQuarBFB = g_pEffect10->GetVariableByName( "QuarBFB" )->AsShaderResource();
    g_pQuarBFC = g_pEffect10->GetVariableByName( "QuarBFC" )->AsShaderResource();
    g_pQuinBFA = g_pEffect10->GetVariableByName( "QuinBFA" )->AsShaderResource();
    g_pQuinBFB = g_pEffect10->GetVariableByName( "QuinBFB" )->AsShaderResource();

    // Create our quad vertex input layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pRender2DQuad->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    // Create our morph object vertex layout
    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "REFERENCE", 0, DXGI_FORMAT_R32_UINT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4, D3D10_INPUT_PER_VERTEX_DATA, 0 },

        { "COEFFSET",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COEFFSET", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COEFFSET", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COEFFSET", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COEFFSET", 4, DXGI_FORMAT_R32G32_FLOAT, 0, 76, D3D10_INPUT_PER_VERTEX_DATA, 0 },

    };
    g_pRenderReferencedObject->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, sizeof( meshlayout ) / sizeof( meshlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshLayout ) );

    // Create our scene vertex layout
    const D3D10_INPUT_ELEMENT_DESC scenelayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( scenelayout, sizeof( scenelayout ) / sizeof( scenelayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSceneLayout ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -20.0f, 5.0f, -40.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( 50.0f, 10.0f, 200.0f );

    // Load the morph object
    WCHAR str2[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SparseMorphTargets\\SoldierHead.mt" ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str2, MAX_PATH, L"SparseMorphTargets\\soldierhead_prtresults.ldprt" ) );
    V_RETURN( g_MorphObject.Create( pd3dDevice, str, str2 ) );
    g_MorphObject.SetVertexLayouts( g_pMeshLayout, g_pQuadLayout );

    // Load the textures for the morph object
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SparseMorphTargets\\soldier_diff.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDiffuseTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SparseMorphTargets\\soldier_norm.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pNormalTexRV, NULL ) );

    // Load the environment
    g_LightProbe.OnCreateDevice( pd3dDevice, L"BatHead\\rnl_cross_mip.dds", false );
    V_RETURN( CreateSHBasisTextures( pd3dDevice ) );

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

    g_LightProbe.OnSwapChainResized( pBackBufferSurfaceDesc );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return hr;
}

float GetMorphPoseTime( double fTime, double Start, double Stop )
{
    double delta = ( Stop - Start ) / 2.0;
    double Mid = ( Stop + Start ) / 2.0;
    double t = 0;
    if( fTime < Mid )
    {
        t = ( fTime - Start ) / delta;
        t = pow( t, 3.0 );
    }
    else
    {
        t = ( fTime - Mid ) / delta;
        t = 1.0 - pow( t, 3.0 );
    }

    return ( float )t;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0f, 0 );

    // Store the original rt and ds buffer views
    ID3D10RenderTargetView* apOldRTVs[1] = { NULL };
    ID3D10DepthStencilView* pOldDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, apOldRTVs, &pOldDS );

    // Reset the mesh to its base pose
    g_MorphObject.ResetToBase( pd3dDevice, g_pRender2DQuadNoAlpha, g_ptxVertData, g_pfBlendAmt, g_pvMaxDeltas );

    // Apply morph targets to distort this base mesh
    for( UINT i = 1; i < g_iMorphsToApply; i++ )
    {
        float fAmount = sinf( ( float )fTime * ( i * 0.25f ) );
        fAmount *= 0.5;
        fAmount += 0.5f;
        g_MorphObject.ApplyMorph( pd3dDevice, i, fAmount, g_pRender2DQuad, g_ptxVertData, g_pfBlendAmt,
                                  g_pvMaxDeltas );
    }

    // Restore the original RT and DS
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );
    SAFE_RELEASE( apOldRTVs[0] );
    SAFE_RELEASE( pOldDS );

    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldViewProj;

    // Get the projection & view matrix from the camera class
    D3DXMatrixIdentity( &mWorld );
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldViewProj = mWorld * mView * mProj;

    // Render the lightprobe
    g_LightProbe.Render( &mWorldViewProj, 1.0f, 1.0f, false );

    // Update the lighting environment
    UpdateLightingEnvironment();
    BindLDPRTBasisTextures();

    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    const D3DXVECTOR3* pEyePt = g_Camera.GetEyePt();
    g_pvCameraPos->SetFloatVector( ( float* )pEyePt );
    g_pfOily->SetFloat( g_fOily );

    // Render the Morphed Mesh
    g_ptxDiffuse->SetResource( g_pDiffuseTexRV );
    g_ptxNormal->SetResource( g_pNormalTexRV );
    g_ptxEnvMap->SetResource( g_LightProbe.GetEnvironmentMapRV() );
    g_MorphObject.Render( pd3dDevice, g_pRenderReferencedObject, g_ptxVertData, g_ptxVertDataOrig, g_pDataTexSize );

    // reset resources
    ID3D10ShaderResourceView* pNulls[4] = {0,0,0,0};
    pd3dDevice->PSSetShaderResources( 0, 4, pNulls );
    pd3dDevice->GSSetShaderResources( 0, 4, pNulls );
    pd3dDevice->VSSetShaderResources( 0, 4, pNulls );

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
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
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

    g_LightProbe.OnSwapChainReleasing();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    g_LightProbe.OnDestroyDevice();

    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pQuadLayout );
    SAFE_RELEASE( g_pMeshLayout );
    SAFE_RELEASE( g_pSceneLayout );

    SAFE_RELEASE( g_pDiffuseTexRV );
    SAFE_RELEASE( g_pNormalTexRV );

    for( int i = 0; i < 9; i++ )
    {
        SAFE_RELEASE( g_pSHBasisTextures[i] );
        SAFE_RELEASE( g_pSHBasisTexRV[i] );
    }

    g_MorphObject.Destroy();
}

//--------------------------------------------------------------------------------------
void UpdateLightingEnvironment()
{
    DWORD dwOrder = 4;  // Hard-code to 6 for LDPRT

    float fSum[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    ZeroMemory( fSum, 3 * D3DXSH_MAXORDER * D3DXSH_MAXORDER * sizeof( float ) );

    D3DXMATRIX mWorldInv;
    D3DXMatrixInverse( &mWorldInv, NULL, g_Camera.GetWorldMatrix() );

    float fLightProbe1[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    float fLightProbe1Rot[3][D3DXSH_MAXORDER*D3DXSH_MAXORDER];
    D3DXSHScale( fLightProbe1[0], dwOrder, g_LightProbe.GetSHData( 0 ), g_fLightScale );
    D3DXSHScale( fLightProbe1[1], dwOrder, g_LightProbe.GetSHData( 1 ), g_fLightScale );
    D3DXSHScale( fLightProbe1[2], dwOrder, g_LightProbe.GetSHData( 2 ), g_fLightScale );
    D3DXSHRotate( fLightProbe1Rot[0], dwOrder, &mWorldInv, fLightProbe1[0] );
    D3DXSHRotate( fLightProbe1Rot[1], dwOrder, &mWorldInv, fLightProbe1[1] );
    D3DXSHRotate( fLightProbe1Rot[2], dwOrder, &mWorldInv, fLightProbe1[2] );
    D3DXSHAdd( fSum[0], dwOrder, fSum[0], fLightProbe1Rot[0] );
    D3DXSHAdd( fSum[1], dwOrder, fSum[1], fLightProbe1Rot[1] );
    D3DXSHAdd( fSum[2], dwOrder, fSum[2], fLightProbe1Rot[2] );

    // Set shader coefficients 
    g_pRLight->SetFloatVectorArray( ( float* )fSum[0], 0, 9 );
    g_pGLight->SetFloatVectorArray( ( float* )fSum[1], 0, 9 );
    g_pBLight->SetFloatVectorArray( ( float* )fSum[2], 0, 9 );
}


//--------------------------------------------------------------------------------------
D3DXVECTOR4 ComputeCubeSH( D3DXVECTOR3* pTexCoord, UINT uStart )
{
    D3DXVECTOR3 nrm;
    D3DXVec3Normalize( &nrm, pTexCoord );
    float fBF[36];

    D3DXSHEvalDirection( fBF, 6, &nrm );

    D3DXVECTOR4 vUse;
    for( int i = 0; i < 4; i++ ) vUse[i] = fBF[uStart + i];

    return vUse;
}


//--------------------------------------------------------------------------------------
HRESULT FillCubeTextureWithSH( D3DXVECTOR4* pOut, UINT uStart )
{
    HRESULT hr = S_OK;

    float fStep = 2.0f / 32.0f;
    float fNegStart = -1.0f + fStep / 2.0f;
    float fPosStart = -fNegStart;
    D3DXVECTOR3 tex;

    UINT index = 0;

    // POSX
    for( float y = fPosStart; y > -1.0f; y -= fStep )
    {
        for( float z = fPosStart; z > -1.0f; z -= fStep )
        {
            tex = D3DXVECTOR3( 1.0f, y, z );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(1,0,0,0);
            index ++;
        }
    }

    // NEGX
    for( float y = fPosStart; y > -1.0f; y -= fStep )
    {
        for( float z = fNegStart; z < 1.0f; z += fStep )
        {
            tex = D3DXVECTOR3( -1.0f, y, z );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(0,1,0,0);
            index ++;
        }
    }

    // POSY
    for( float z = fNegStart; z < 1.0f; z += fStep )
    {
        for( float x = fNegStart; x < 1.0f; x += fStep )
        {
            tex = D3DXVECTOR3( x, 1.0f, z );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(0,0,1,0);
            index ++;
        }
    }

    // NEGY
    for( float z = fPosStart; z > -1.0f; z -= fStep )
    {
        for( float x = fNegStart; x < 1.0f; x += fStep )
        {
            tex = D3DXVECTOR3( x, -1.0f, z );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(1,1,0,0);
            index ++;
        }
    }

    //POSZ
    for( float y = fPosStart; y > -1.0f; y -= fStep )
    {
        for( float x = fNegStart; x < 1.0f; x += fStep )
        {
            tex = D3DXVECTOR3( x, y, 1.0f );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(0,1,1,0);
            index ++;
        }
    }

    // NEGZ
    for( float y = fPosStart; y > -1.0f; y -= fStep )
    {
        for( float x = fPosStart; x > -1.0f; x -= fStep )
        {
            tex = D3DXVECTOR3( x, y, -1.0f );
            pOut[index] = ComputeCubeSH( &tex, uStart );
            //pOut[index] = D3DXVECTOR4(1,0,1,0);
            index ++;
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CreateSHBasisTextures( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Fill the cube textures with SH basis functions
    for( UINT i = 0; i < 9; i++ )
    {
        // Create the data
        D3DXVECTOR4* pData = new D3DXVECTOR4[ 6 * 32 * 32 ];
        if( !pData )
            return E_OUTOFMEMORY;

        // Fill the data
        V_RETURN( FillCubeTextureWithSH( pData, i * 4 ) );

        // Create the texture
        D3D10_TEXTURE2D_DESC dstex;
        dstex.Width = 32;
        dstex.Height = 32;
        dstex.MipLevels = 1;
        dstex.ArraySize = 6;
        dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        dstex.Usage = D3D10_USAGE_DEFAULT;
        dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        dstex.CPUAccessFlags = 0;
        dstex.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
        dstex.SampleDesc.Count = 1;
        dstex.SampleDesc.Quality = 0;

        D3D10_SUBRESOURCE_DATA InitData[6];
        for( int s = 0; s < 6; s++ )
        {
            InitData[s].pSysMem = &pData[ 32 * 32 * s ];
            InitData[s].SysMemPitch = 32 * sizeof( D3DXVECTOR4 );
            InitData[s].SysMemSlicePitch = InitData[s].SysMemPitch * 32;
        }
        V_RETURN( pd3dDevice->CreateTexture2D( &dstex, InitData, &g_pSHBasisTextures[i] ) );
        SAFE_DELETE_ARRAY( pData );

        // Create Resource Views
        D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
        SRVDesc.TextureCube.MipLevels = 1;
        SRVDesc.TextureCube.MostDetailedMip = 0;
        V_RETURN( pd3dDevice->CreateShaderResourceView( g_pSHBasisTextures[i], &SRVDesc, &g_pSHBasisTexRV[i] ) );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
void BindLDPRTBasisTextures()
{
    g_pCLinBF->SetResource( g_pSHBasisTexRV[0] );
    g_pQuadBF->SetResource( g_pSHBasisTexRV[1] );

    g_pCubeBFA->SetResource( g_pSHBasisTexRV[2] );
    g_pCubeBFB->SetResource( g_pSHBasisTexRV[3] );

    g_pQuarBFA->SetResource( g_pSHBasisTexRV[4] );
    g_pQuarBFB->SetResource( g_pSHBasisTexRV[5] );
    g_pQuarBFC->SetResource( g_pSHBasisTexRV[6] );

    g_pQuinBFA->SetResource( g_pSHBasisTexRV[7] );
    g_pQuinBFB->SetResource( g_pSHBasisTexRV[8] );
}


