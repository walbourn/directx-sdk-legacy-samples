//--------------------------------------------------------------------------------------
// File: CubeMapGS.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#include "resource.h"

#define ENVMAPSIZE 256
#define MIPLEVELS 9
#define DEG2RAD( a ) ( a * D3DX_PI / 180.f )

struct CubeMapVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR3 Normal;
    D3DXVECTOR2 Tex;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
bool                                g_bShowHelp = true;     // If true, it renders the UI control text
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
bool                                g_bUseRenderTargetArray = true;     // Whether render target array is used to render cube map in one pass
bool                                g_bRenderCar = false;                // Whether to render the car or the ball
bool                                g_bRenderWithInstancing = true;		// Whether ot render with instancing

ID3DX10Font*                        g_pFont10 = NULL;       // Font for drawing text
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;

ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10InputLayout*                  g_pVertexLayoutCM = NULL;
ID3D10InputLayout*                  g_pVertexLayoutCMInst = NULL;
ID3D10InputLayout*                  g_pVertexLayoutEnv = NULL;

CDXUTSDKMesh                        g_MeshCar;
CDXUTSDKMesh                        g_MeshCarInnards;
CDXUTSDKMesh                        g_MeshCarGlass;
CDXUTSDKMesh                        g_MeshRoom;
CDXUTSDKMesh                        g_MeshMonitors;
CDXUTSDKMesh                        g_MeshArm;
CDXUTSDKMesh                        g_MeshBall;

ID3D10Texture2D*                    g_pEnvMap;          // Environment map
ID3D10RenderTargetView*             g_pEnvMapRTV;       // Render target view for the alpha map
ID3D10RenderTargetView*             g_apEnvMapOneRTV[6];// 6 render target view, each view is used for 1 face of the env map
ID3D10ShaderResourceView*           g_pEnvMapSRV;       // Shader resource view for the cubic env map
ID3D10ShaderResourceView*           g_apEnvMapOneSRV[6];// Single-face shader resource view
ID3D10Texture2D*                    g_pEnvMapDepth;     // Depth stencil for the environment map
ID3D10DepthStencilView*             g_pEnvMapDSV;       // Depth stencil view for environment map for all 6 faces
ID3D10DepthStencilView*             g_pEnvMapOneDSV;    // Depth stencil view for environment map for all 1 face
ID3D10Buffer*                       g_pVBVisual;        // Vertex buffer for quad used for visualization
ID3D10ShaderResourceView*           g_pFalloffTexRV;    // Resource view for the falloff texture

D3DXMATRIX                          g_mWorldRoom;    // World matrix of the room
D3DXMATRIX                          g_mWorldArm;     // World matrix of the Arm
D3DXMATRIX                          g_mWorldCar;     // World matrix of the car
D3DXMATRIX g_amCubeMapViewAdjust[6]; // Adjustment for view matrices when rendering the cube map
D3DXMATRIX                          g_mProjCM;       // Projection matrix for cubic env map rendering

ID3D10EffectMatrixVariable*         g_pmWorldViewProj;
ID3D10EffectMatrixVariable*         g_pmWorldView;
ID3D10EffectMatrixVariable*         g_pmWorld;
ID3D10EffectMatrixVariable*         g_pmView;
ID3D10EffectMatrixVariable*         g_pmProj;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse;
ID3D10EffectShaderResourceVariable* g_ptxEnvMap;
ID3D10EffectShaderResourceVariable* g_ptxFalloffMap;
ID3D10EffectVectorVariable*         g_pvDiffuse;
ID3D10EffectVectorVariable*         g_pvSpecular;
ID3D10EffectVectorVariable*         g_pvEye;
ID3D10EffectMatrixVariable*         g_pmViewCM;

ID3D10EffectTechnique*              g_pRenderCubeMapTech;
ID3D10EffectTechnique*              g_pRenderCubeMapInstTech;
ID3D10EffectTechnique*              g_pRenderSceneTech;
ID3D10EffectTechnique*              g_pRenderEnvMappedSceneTech;
ID3D10EffectTechnique*              g_pRenderEnvMappedSceneNoTexTech;
ID3D10EffectTechnique*              g_pRenderEnvMappedGlassTech;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_RENDERCAR           5
#define IDC_RENDERINSTANCED		6
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
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();
HRESULT RenderScene( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj, bool bRenderCubeMap,
                     ID3D10EffectTechnique* pTechnique );
HRESULT RenderSceneIntoCubeMap( ID3D10Device* pd3dDevice );


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
    DXUTCreateWindow( L"CubeMapGS" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    iY += 50;

    g_HUD.AddCheckBox( IDC_RENDERCAR, L"Render Car", 35, iY += 24, 125, 22, g_bRenderCar );
    g_HUD.AddCheckBox( IDC_RENDERINSTANCED, L"Use Instancing", 35, iY += 24, 125, 22, g_bRenderWithInstancing );
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
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
    g_Camera.FrameMove( fElapsedTime );

    float fRotSpeed = 60.0f;

    // Rotate the arm
    D3DXMatrixRotationY( &g_mWorldArm, ( float )( fTime * DEG2RAD(fRotSpeed) ) );
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
        case IDC_RENDERCAR:
            g_bRenderCar = !g_bRenderCar; break;
        case IDC_RENDERINSTANCED:
            g_bRenderWithInstancing = !g_bRenderWithInstancing; break;
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

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"CubeMapGS.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect, NULL, NULL ) );

    D3DXMatrixScaling( &g_mWorldRoom, 3.7f, 3.7f, 3.7f );
    D3DXMATRIX m;
    D3DXMatrixTranslation( &m, 0.0f, 0.0f, 0.0f );
    D3DXMatrixMultiply( &g_mWorldRoom, &g_mWorldRoom, &m );

    D3DXMatrixIdentity( &g_mWorldArm );

    D3DXMatrixScaling( &m, 1.0f, 1.0f, 1.0f );
    D3DXMatrixRotationY( &g_mWorldCar, D3DX_PI * 0.5f );
    D3DXMatrixMultiply( &g_mWorldCar, &g_mWorldCar, &m );
    D3DXMatrixTranslation( &m, 0.0f, 3.2f, 0.0f );
    D3DXMatrixMultiply( &g_mWorldCar, &g_mWorldCar, &m );

    // Generate cube map view matrices
    float fHeight = 1.5f;
    D3DXVECTOR3 vEyePt = D3DXVECTOR3( 0.0f, fHeight, 0.0f );
    D3DXVECTOR3 vLookDir;
    D3DXVECTOR3 vUpDir;

    vLookDir = D3DXVECTOR3( 1.0f, fHeight, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[0], &vEyePt, &vLookDir, &vUpDir );
    vLookDir = D3DXVECTOR3( -1.0f, fHeight, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[1], &vEyePt, &vLookDir, &vUpDir );
    vLookDir = D3DXVECTOR3( 0.0f, fHeight + 1.0f, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[2], &vEyePt, &vLookDir, &vUpDir );
    vLookDir = D3DXVECTOR3( 0.0f, fHeight - 1.0f, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[3], &vEyePt, &vLookDir, &vUpDir );
    vLookDir = D3DXVECTOR3( 0.0f, fHeight, 1.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[4], &vEyePt, &vLookDir, &vUpDir );
    vLookDir = D3DXVECTOR3( 0.0f, fHeight, -1.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &g_amCubeMapViewAdjust[5], &vEyePt, &vLookDir, &vUpDir );

    // Obtain the technique handles
    g_pRenderCubeMapTech = g_pEffect->GetTechniqueByName( "RenderCubeMap" );
    g_pRenderCubeMapInstTech = g_pEffect->GetTechniqueByName( "RenderCubeMap_Inst" );
    g_pRenderSceneTech = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_pRenderEnvMappedSceneTech = g_pEffect->GetTechniqueByName( "RenderEnvMappedScene" );
    g_pRenderEnvMappedSceneNoTexTech = g_pEffect->GetTechniqueByName( "RenderEnvMappedScene_NoTexture" );
    g_pRenderEnvMappedGlassTech = g_pEffect->GetTechniqueByName( "RenderEnvMappedGlass" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect->GetVariableByName( "mWorldViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect->GetVariableByName( "mWorldView" )->AsMatrix();
    g_pmWorld = g_pEffect->GetVariableByName( "mWorld" )->AsMatrix();
    g_pmView = g_pEffect->GetVariableByName( "mView" )->AsMatrix();
    g_pmProj = g_pEffect->GetVariableByName( "mProj" )->AsMatrix();
    g_ptxDiffuse = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxEnvMap = g_pEffect->GetVariableByName( "g_txEnvMap" )->AsShaderResource();
    g_ptxFalloffMap = g_pEffect->GetVariableByName( "g_txFalloff" )->AsShaderResource();
    g_pvDiffuse = g_pEffect->GetVariableByName( "vMaterialDiff" )->AsVector();
    g_pvSpecular = g_pEffect->GetVariableByName( "vMaterialSpec" )->AsVector();
    g_pvEye = g_pEffect->GetVariableByName( "vEye" )->AsVector();
    g_pmViewCM = g_pEffect->GetVariableByName( "g_mViewCM" )->AsMatrix();

    // Load a simple 1d falloff map for our car shader
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ExoticCar\\FalloffRamp.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pFalloffTexRV, NULL ) );

    // Create cubic depth stencil texture.
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = ENVMAPSIZE;
    dstex.Height = ENVMAPSIZE;
    dstex.MipLevels = 1;
    dstex.ArraySize = 6;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Format = DXGI_FORMAT_D32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pEnvMapDepth ) );

    // Create the depth stencil view for the entire cube
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
    DescDS.Format = DXGI_FORMAT_D32_FLOAT;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
    DescDS.Texture2DArray.FirstArraySlice = 0;
    DescDS.Texture2DArray.ArraySize = 6;
    DescDS.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pEnvMapDepth, &DescDS, &g_pEnvMapDSV ) );

    // Create the depth stencil view for single face rendering
    DescDS.Texture2DArray.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pEnvMapDepth, &DescDS, &g_pEnvMapOneDSV ) );

    // Create the cube map for env map render target
    dstex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS | D3D10_RESOURCE_MISC_TEXTURECUBE;
    dstex.MipLevels = MIPLEVELS;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pEnvMap ) );

    // Create the 6-face render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
    DescRT.Texture2DArray.FirstArraySlice = 0;
    DescRT.Texture2DArray.ArraySize = 6;
    DescRT.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pEnvMap, &DescRT, &g_pEnvMapRTV ) );

    // Create the one-face render target views
    DescRT.Texture2DArray.ArraySize = 1;
    for( int i = 0; i < 6; ++i )
    {
        DescRT.Texture2DArray.FirstArraySlice = i;
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_pEnvMap, &DescRT, &g_apEnvMapOneRTV[i] ) );
    }

    // Create the shader resource view for the cubic env map
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.TextureCube.MipLevels = MIPLEVELS;
    SRVDesc.TextureCube.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pEnvMap, &SRVDesc, &g_pEnvMapSRV ) );

    // Define our vertex data layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pRenderSceneTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    g_pRenderCubeMapTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayoutCM ) );

    g_pRenderCubeMapInstTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayoutCMInst ) );

    g_pRenderEnvMappedSceneTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayoutEnv ) );

    // Create mesh objects
    g_MeshCar.Create( pd3dDevice, L"exoticcar\\ExoticCar.sdkmesh", true );
    g_MeshCarInnards.Create( pd3dDevice, L"exoticcar\\CarInnards.sdkmesh", true );
    g_MeshCarGlass.Create( pd3dDevice, L"exoticcar\\CarGlass.sdkmesh", true );
    g_MeshRoom.Create( pd3dDevice, L"Scanner\\ScannerRoom.sdkmesh", true );
    g_MeshMonitors.Create( pd3dDevice, L"Scanner\\ScannerMonitors.sdkmesh", true );
    g_MeshArm.Create( pd3dDevice, L"Scanner\\ScannerArm.sdkmesh", true );
    g_MeshBall.Create( pd3dDevice, L"misc\\ReflectSphere.sdkmesh", true );

    // Create the projection matrices
    D3DXMatrixPerspectiveFovLH( &g_mProjCM, D3DX_PI * 0.5f, 1.0f, .5f, 1000.f );

    // Create a vertex buffer consisting of a quad for visualization
    {
        D3D10_BUFFER_DESC bufferdesc =
        {
            6 * sizeof( CubeMapVertex ),
            D3D10_USAGE_DEFAULT,
            D3D10_BIND_VERTEX_BUFFER,
            0,
            0
        };
        CubeMapVertex Quad[6] =
        {
            { D3DXVECTOR3( -1.0f, 1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 0.0f, 0.0f )
            },
            { D3DXVECTOR3( 1.0f, 1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 1.0f, 0.0f )
            },
            { D3DXVECTOR3( -1.0f, -1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 0.0f, 1.0f )
            },
            { D3DXVECTOR3( -1.0f, -1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 0.0f, 1.0f )
            },
            { D3DXVECTOR3( 1.0f, 1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 1.0f, 0.0f )
            },
            { D3DXVECTOR3( 1.0f, -1.0f, 0.5f ),
                D3DXVECTOR3( 0.0f, 0.0f, -1.0f ),
                D3DXVECTOR2( 1.0f, 1.0f )
            }
        };
        D3D10_SUBRESOURCE_DATA InitData =
        {
            Quad,
            sizeof( Quad ),
            sizeof( Quad )
        };
        V_RETURN( pd3dDevice->CreateBuffer( &bufferdesc, &InitData, &g_pVBVisual ) );
    }

    D3DXVECTOR3 vecEye( 0.0f, 5.7f, -6.5f );
    D3DXVECTOR3 vecAt ( 0.0f, 4.7f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

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
    g_Camera.SetProjParams( D3DX_PI / 3, fAspectRatio, 0.5f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return hr;
}

void RenderMeshInstanced( ID3D10Device* pd3dDevice,
                          CDXUTSDKMesh* pMesh,
                          ID3D10EffectTechnique* pTechnique,
                          ID3D10EffectShaderResourceVariable* ptxDiffuse )
{
    ID3D10Buffer* pVB[1];
    UINT Strides[1];
    UINT Offsets[1] = {0};

    // Render the mesh instanced
    pVB[0] = pMesh->GetVB10( 0, 0 );
    Strides[0] = ( UINT )pMesh->GetVertexStride( 0, 0 );

    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( pMesh->GetIB10( 0 ), pMesh->GetIBFormat10( 0 ), 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < pMesh->GetNumSubsets( 0 ); ++subset )
        {
            pSubset = pMesh->GetSubset( 0, subset );

            PrimType = pMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pMat = pMesh->GetMaterial( pSubset->MaterialID );
            if( pMat )
                ptxDiffuse->SetResource( pMat->pDiffuseRV10 );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, 6, 0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

}

//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------
HRESULT RenderScene( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj, bool bRenderCubeMap,
                     ID3D10EffectTechnique* pTechnique )
{
    HRESULT hr = S_OK;
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Clear the render target
    if( !bRenderCubeMap )
    {
        // Cleanup the Render Target View
        ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
        float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
        pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

        // Clear the depth stencil
        ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
        pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
    }

    g_pmView->SetMatrix( ( float* )&mView );
    g_pmProj->SetMatrix( ( float* )&mProj );

    //
    // Render room
    //

    // Calculate the matrix world*view*proj
    D3DXMatrixMultiply( &mWorldView, &g_mWorldRoom, &mView );
    D3DXMatrixMultiply( &mWorldViewProj, &mWorldView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorld->SetMatrix( ( float* )&g_mWorldRoom );

    if( g_bRenderWithInstancing )
    {
        RenderMeshInstanced( pd3dDevice, &g_MeshRoom, pTechnique, g_ptxDiffuse );
        RenderMeshInstanced( pd3dDevice, &g_MeshMonitors, pTechnique, g_ptxDiffuse );
    }
    else
    {
        g_MeshRoom.Render( pd3dDevice, pTechnique, g_ptxDiffuse );
        g_MeshMonitors.Render( pd3dDevice, pTechnique, g_ptxDiffuse );
    }

    D3DXMATRIX mArm;
    D3DXMatrixMultiply( &mArm, &g_mWorldArm, &g_mWorldRoom );
    D3DXMatrixMultiply( &mWorldView, &mArm, &mView );
    D3DXMatrixMultiply( &mWorldViewProj, &mWorldView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorld->SetMatrix( ( float* )&mArm );

    if( g_bRenderWithInstancing )
    {
        RenderMeshInstanced( pd3dDevice, &g_MeshArm, pTechnique, g_ptxDiffuse );
    }
    else
    {
        g_MeshArm.Render( pd3dDevice, pTechnique, g_ptxDiffuse );
    }

    //
    // Render environment-mapped objects
    //

    // Set IA parameters
    pd3dDevice->IASetInputLayout( g_pVertexLayoutEnv );

    if( !bRenderCubeMap )
    {
        // Calculate the matrix world*view*proj
        D3DXMatrixMultiply( &mWorldView, &g_mWorldCar, &mView );
        D3DXMatrixMultiply( &mWorldViewProj, &mWorldView, &mProj );
        g_pmWorldView->SetMatrix( ( float* )&mWorldView );
        g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
        g_pmWorld->SetMatrix( ( float* )&g_mWorldCar );
        g_ptxFalloffMap->SetResource( g_pFalloffTexRV );

        // Set cube texture parameter in the effect
        g_ptxEnvMap->SetResource( g_pEnvMapSRV );

        if( g_bRenderCar )
        {
            // Render the shiny car shell
            g_MeshCar.Render( pd3dDevice, g_pRenderEnvMappedSceneTech, NULL, NULL, NULL, g_pvDiffuse, g_pvSpecular );

            // Render the rest of the car that doesn't need to have a fresnel shader
            g_MeshCarInnards.Render( pd3dDevice, g_pRenderEnvMappedSceneNoTexTech, NULL, NULL, NULL, g_pvDiffuse,
                                     g_pvSpecular );

            // Finally, render the glass
            g_MeshCarGlass.Render( pd3dDevice, g_pRenderEnvMappedGlassTech, NULL, NULL, NULL, g_pvDiffuse,
                                   g_pvSpecular );
        }
        else
        {
            // Just render a shiny ball
            g_MeshBall.Render( pd3dDevice, g_pRenderEnvMappedSceneNoTexTech, NULL, NULL, NULL, g_pvDiffuse,
                               g_pvSpecular );
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Render the scene into a cube map
//--------------------------------------------------------------------------------------
HRESULT RenderSceneIntoCubeMap( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Save the old RT and DS buffer views
    ID3D10RenderTargetView* apOldRTVs[1] = { NULL };
    ID3D10DepthStencilView* pOldDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, apOldRTVs, &pOldDS );

    // Save the old viewport
    D3D10_VIEWPORT OldVP;
    UINT cRT = 1;
    pd3dDevice->RSGetViewports( &cRT, &OldVP );

    // Set a new viewport for rendering to cube map
    D3D10_VIEWPORT SMVP;
    SMVP.Height = ENVMAPSIZE;
    SMVP.Width = ENVMAPSIZE;
    SMVP.MinDepth = 0;
    SMVP.MaxDepth = 1;
    SMVP.TopLeftX = 0;
    SMVP.TopLeftY = 0;
    pd3dDevice->RSSetViewports( 1, &SMVP );

    // Here, compute the view matrices used for cube map rendering.
    // First, construct mViewAlignCM, a view matrix with the same
    // orientation as m_mView but with eye point at the car position.
    //
    D3DXMATRIX mViewAlignCM;
    D3DXMatrixIdentity( &mViewAlignCM );
    mViewAlignCM._41 = -g_mWorldCar._41;
    mViewAlignCM._42 = -g_mWorldCar._42;
    mViewAlignCM._43 = -g_mWorldCar._43;

    // Combine with the 6 different view directions to obtain the final
    // view matrices.
    //
    D3DXMATRIX amViewCM[6];
    for( int view = 0; view < 6; ++view )
        D3DXMatrixMultiply( &amViewCM[view], &mViewAlignCM, &g_amCubeMapViewAdjust[view] );

    if( g_bUseRenderTargetArray )
    {
        float ClearColor[4] = { 0.0, 1.0, 0.0, 0.0 };

        pd3dDevice->ClearRenderTargetView( g_pEnvMapRTV, ClearColor );
        pd3dDevice->ClearDepthStencilView( g_pEnvMapDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

        ID3D10InputLayout* pLayout = g_pVertexLayoutCM;
        ID3D10EffectTechnique* pTechnique = g_pRenderCubeMapTech;
        if( g_bRenderWithInstancing )
        {
            pLayout = g_pVertexLayoutCMInst;
            pTechnique = g_pRenderCubeMapInstTech;
        }

        ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pEnvMapRTV };
        pd3dDevice->OMSetRenderTargets( sizeof( aRTViews ) / sizeof( aRTViews[0] ), aRTViews, g_pEnvMapDSV );
        pd3dDevice->IASetInputLayout( pLayout );
        g_pmViewCM->SetMatrixArray( ( float* )amViewCM, 0, 6 );

        RenderScene( pd3dDevice, amViewCM[0], g_mProjCM, true, pTechnique );
    }
    else
    {
        //
        // Render one cube face at a time
        //

        pd3dDevice->IASetInputLayout( g_pVertexLayout );

        for( int view = 0; view < 6; ++view )
        {
            float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
            pd3dDevice->ClearRenderTargetView( g_apEnvMapOneRTV[view], ClearColor );
            pd3dDevice->ClearDepthStencilView( g_pEnvMapOneDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

            ID3D10RenderTargetView* aRTViews[ 1 ] = { g_apEnvMapOneRTV[view] };
            pd3dDevice->OMSetRenderTargets( sizeof( aRTViews ) / sizeof( aRTViews[0] ), aRTViews, g_pEnvMapOneDSV );

            RenderScene( pd3dDevice, amViewCM[view], g_mProjCM, true, g_pRenderSceneTech );
        }
    }

    // Restore old view port
    pd3dDevice->RSSetViewports( 1, &OldVP );

    // Restore old RT and DS buffer views
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

    // Generate Mip Maps
    pd3dDevice->GenerateMips( g_pEnvMapSRV );

    SAFE_RELEASE( apOldRTVs[0] );
    SAFE_RELEASE( pOldDS );

    return hr;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXVECTOR3 vCameraPos = *g_Camera.GetEyePt();
    g_pvEye->SetFloatVector( ( float* )&vCameraPos );

    // Construct the cube map
    if( !g_D3DSettingsDlg.IsActive() )
        RenderSceneIntoCubeMap( pd3dDevice );

    ID3D10ShaderResourceView*const pSRV[4] = { NULL, NULL, NULL, NULL };
    pd3dDevice->PSSetShaderResources( 0, 4, pSRV );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();

    float ClearColor[4] = { 0.0, 1.0, 1.0, 0.0 };
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();

    RenderScene( pd3dDevice, mView, mProj, false, g_pRenderSceneTech );

    pd3dDevice->PSSetShaderResources( 0, 4, pSRV );

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
    // Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
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
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper )
        SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVertexLayoutCM );
    SAFE_RELEASE( g_pVertexLayoutCMInst );
    SAFE_RELEASE( g_pVertexLayoutEnv );
    SAFE_RELEASE( g_pEnvMap );
    SAFE_RELEASE( g_pEnvMapRTV );
    for( int i = 0; i < 6; ++i )
    {
        SAFE_RELEASE( g_apEnvMapOneSRV[i] );
        SAFE_RELEASE( g_apEnvMapOneRTV[i] );
    }
    SAFE_RELEASE( g_pEnvMapSRV );
    SAFE_RELEASE( g_pEnvMapDepth );
    SAFE_RELEASE( g_pEnvMapDSV );
    SAFE_RELEASE( g_pEnvMapOneDSV );
    SAFE_RELEASE( g_pVBVisual );

    SAFE_RELEASE( g_pFalloffTexRV );

    g_MeshCar.Destroy();
    g_MeshCarInnards.Destroy();
    g_MeshCarGlass.Destroy();
    g_MeshRoom.Destroy();
    g_MeshMonitors.Destroy();
    g_MeshArm.Destroy();
    g_MeshBall.Destroy();
}

