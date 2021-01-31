//--------------------------------------------------------------------------------------
// File: HDRFormats10.cpp
//
// Basic starting point for new Direct3D samples
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
#include "skybox.h"

#define NUM_TONEMAP_TEXTURES  5       // Number of stages in the 3x3 down-scaling 
// of average luminance textures
#define NUM_BLOOM_TEXTURES    2
#define RGB16_MAX             100
#define RGB32_MAX             10000


enum ENCODING_MODE
{
    FP16,
    FP32,
    RGB16,
    RGBE8,
    RGB9E5,
    RG11B10,
    NUM_ENCODING_MODES
};
ENCODING_MODE g_EncodingModes[] =
{
    FP16, FP32, RGB16, RGBE8, RGB9E5, RG11B10
};
WCHAR*                      g_szEncodingModes[] =
{
    L"FP16", L"FP32", L"RGB16", L"RGBE8", L"RGB9E5", L"RG11B10"
};
DXGI_FORMAT g_EncodingFormats[] =
{
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_R11G11B10_FLOAT
};

enum RENDER_MODE
{
    DECODED,
    RGB_ENCODED,
    ALPHA_ENCODED,
    NUM_RENDER_MODES
};

enum NORMAL_MODE
{
    NORM_RGB8,
    NORM_FP32,
    NORM_FP16,
    NORM_FPRG11B10,
    NORM_FPR32G32,
    NORM_FPR16G16,
    NORM_R8G8U,
    NORM_BC1U,
    NORM_BC5S,
    NORM_BC5U,
    NUM_NORMAL_MODES
};
NORMAL_MODE g_NormalModes[] =
{
    NORM_RGB8,
    NORM_FP32,
    NORM_FP16,
    NORM_FPRG11B10,
    NORM_FPR32G32,
    NORM_FPR16G16,
    NORM_R8G8U,
    NORM_BC1U,
    NORM_BC5S,
    NORM_BC5U
};
DXGI_FORMAT g_NormalFormats[] =
{
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_BC5_UNORM
};
WCHAR*                      g_szNormalModes[] =
{
    L"RGB8",
    L"FP32",
    L"FP16",
    L"FPRG11B10",
    L"FPR32G32",
    L"FPR16G16",
    L"R8G8U",
    L"BC1U",
    L"BC5S",
    L"BC5U"
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // A model viewing camera
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
bool                        g_bShowText = true;
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
bool                        g_bBloom;             // Bloom effect on/off
ENCODING_MODE               g_eEncodingMode;
NORMAL_MODE                 g_eNormalMode;
RENDER_MODE                 g_eRenderMode;
CSkybox g_aSkybox[NUM_ENCODING_MODES];
bool                        g_bSupportsR16F = false;
bool                        g_bSupportsR32F = false;
bool                        g_bSupportsD16 = false;
bool                        g_bSupportsD32 = false;
bool                        g_bSupportsD24X8 = false;
bool                        g_bUseMultiSample = false; // True when using multisampling on a supported back buffer
double g_aPowsOfTwo[257]; // Lookup table for log calculations

extern IDirect3DDevice9*    g_pd3dDevice9;

struct TECH_HANDLES9
{
    D3DXHANDLE Scene;
    D3DXHANDLE DownScale2x2_Lum;
    D3DXHANDLE DownScale3x3;
    D3DXHANDLE DownScale3x3_BrightPass;
    D3DXHANDLE FinalPass;
};

extern TECH_HANDLES9 g_aTechHandles9[NUM_ENCODING_MODES];
extern TECH_HANDLES9*       g_pCurTechnique9;

struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;

    static const DWORD FVF;
};

const DWORD                 SCREEN_VERTEX::FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

namespace D3D10
{
struct TECH_HANDLES10
{
    ID3D10EffectTechnique* Scene;
    ID3D10EffectTechnique* SceneTwoComp;
    ID3D10EffectTechnique* DownScale2x2_Lum;
    ID3D10EffectTechnique* DownScale3x3;
    ID3D10EffectTechnique* DownScale3x3_BrightPass;
    ID3D10EffectTechnique* FinalPass;
};

ID3D10Device* g_pd3dDevice10 = NULL;
ID3D10Buffer* g_pScreenQuadVB = NULL;
ID3D10InputLayout* g_pSceneLayout = NULL;
ID3D10InputLayout* g_pQuadLayout = NULL;
ID3D10DepthStencilView* g_pDSV = NULL;
ID3D10Texture2D* g_pDepthStencil = NULL;
ID3DX10Font* g_pFont10 = NULL;       // Font for drawing text
ID3DX10Sprite* g_pSprite10 = NULL;     // Sprite for batching draw text calls
ID3D10Effect* g_pEffect10 = NULL;     // D3DX effect interface
ID3D10Texture2D* g_pMSRT10 = NULL;       // Multi-Sample float render target
ID3D10RenderTargetView* g_pMSRTV10 = NULL;
ID3D10Texture2D* g_pMSDS10 = NULL;       // Depth Stencil surface for the float RT
ID3D10DepthStencilView* g_pMSDSV10 = NULL;
ID3D10Texture2D* g_pTexRender10 = NULL;         // Render target texture
ID3D10ShaderResourceView* g_pTexRenderRV10 = NULL;
ID3D10RenderTargetView* g_pTexRenderRTV10 = NULL;
ID3D10Texture2D* g_pTexBrightPass10 = NULL;     // Bright pass filter
ID3D10ShaderResourceView* g_pTexBrightPassRV10 = NULL;
ID3D10RenderTargetView* g_pTexBrightPassRTV10 = NULL;
CDXUTSDKMesh g_Mesh10;
ID3D10Texture2D* g_apTexToneMap10[NUM_TONEMAP_TEXTURES]; // Tone mapping calculation textures
ID3D10ShaderResourceView* g_apTexToneMapRV10[NUM_TONEMAP_TEXTURES];
ID3D10RenderTargetView* g_apTexToneMapRTV10[NUM_TONEMAP_TEXTURES];
ID3D10Texture2D* g_apTexBloom10[NUM_BLOOM_TEXTURES];     // Blooming effect intermediate texture
ID3D10ShaderResourceView* g_apTexBloomRV10[NUM_BLOOM_TEXTURES];
ID3D10RenderTargetView* g_apTexBloomRTV10[NUM_BLOOM_TEXTURES];
TECH_HANDLES10 g_aTechHandles10[NUM_ENCODING_MODES];
TECH_HANDLES10* g_pCurTechnique10;
ID3D10Texture2D* g_aTexNormal[NUM_NORMAL_MODES] = {0};
ID3D10ShaderResourceView* g_aTexNormalRV[NUM_NORMAL_MODES] = {0};
ID3D10ShaderResourceView* g_pCurrentTexNormalRV = NULL;
UINT g_MaxMultiSampleType10 = 16;  // Non-Zero when g_bUseMultiSample is true
UINT g_dwMultiSampleQuality10 = 0; // Used when we have multisampling on a backbuffer

ID3D10EffectMatrixVariable* g_pmWorld;
ID3D10EffectMatrixVariable* g_pmWorldViewProj;
ID3D10EffectVectorVariable* g_pvEyePt;
ID3D10EffectShaderResourceVariable* g_ptCube;
ID3D10EffectShaderResourceVariable* g_ptNormal;
ID3D10EffectShaderResourceVariable* g_pS0;
ID3D10EffectShaderResourceVariable* g_pS1;
ID3D10EffectShaderResourceVariable* g_pS2;
ID3D10EffectVectorVariable* g_pavSampleOffsets;
ID3D10EffectVectorVariable* g_pavSampleWeights;

ID3D10EffectTechnique* g_pFinalPassEncoded_RGB;
ID3D10EffectTechnique* g_pFinalPassEncoded_A;
ID3D10EffectTechnique* g_pBloom;

ID3D10EffectTechnique* g_pDrawTexture;


}
;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC             -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_ENCODING_MODE       5
#define IDC_NORMAL_MODE         6
#define IDC_RENDER_MODE         7
#define IDC_BLOOM               8
#define IDC_TOGGLEWARP          9


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
//d3d9 callbacks
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

HRESULT GetSampleOffsets_Bloom_D3D10( DWORD dwD3DTexSize, float afTexCoordOffset[15], D3DXVECTOR4* avColorWeight,
                                      float fDeviation, float fMultiplier=1.0f );

//d3d10 callbacks
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

HRESULT RetrieveD3D10TechHandles();
HRESULT LoadD3D10CubeTexture( ID3D10Device* pd3dDevice,
                              WCHAR* strPath,
                              DXGI_FORMAT fmt,
                              ID3D10Texture2D** ppCubeTexture,
                              ID3D10ShaderResourceView** ppCubeRV );
HRESULT LoadD3D10NormalTexture( ID3D10Device* pd3dDevice,
                                WCHAR* strPath,
                                DXGI_FORMAT fmt,
                                ID3D10Texture2D** ppTexture,
                                ID3D10ShaderResourceView** ppRV );
HRESULT CreateEncodedTexture10( ID3D10Device* pd3dDevice,
                                WCHAR* strPath,
                                ENCODING_MODE eTarget,
                                ID3D10Texture2D** ppTexOut,
                                ID3D10ShaderResourceView** ppCubeRV );

HRESULT RenderBloom10();
HRESULT MeasureLuminance10();
HRESULT BrightPassFilter10();
void DrawFullScreenQuad10( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height );


LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

void InitApp();
void RenderD3D9Text( double fTime );
void RenderD3D10Text( double fTime );

float GaussianDistribution( float x, float y, float rho );
int log2_ceiling( float val );
VOID EncodeRGB9E5( D3DXFLOAT16* pSrc, BYTE** ppDest );
VOID EncodeRGBE8( D3DXFLOAT16* pSrc, BYTE** ppDest );
VOID EncodeRGB16( D3DXFLOAT16* pSrc, BYTE** ppDest );

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

    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.

    // Disable gamma correction in DXUT
    DXUTSetIsInGammaCorrectMode( false );

    // Direct3D9 callbacks
    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );

    // Direct3D10 callbacks
    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

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
    DXUTInit( true, true ); // Parse the command line, handle the default hotkeys, and show msgboxes
    DXUTCreateWindow( L"HDRFormats10" );
    DXUTCreateDevice( true, 640, 480 );

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

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    g_bShowHelp = false;
    g_bShowText = true;

    for( int i = 0; i <= 256; i++ )
    {
        g_aPowsOfTwo[i] = powf( 2.0f, ( float )( i - 128 ) );
    }

    g_HUD.SetFont( 0, L"Arial", 14, 400 );
    g_HUD.SetCallback( OnGUIEvent );

    iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );

    g_SampleUI.SetCallback( OnGUIEvent );

    // Title font for comboboxes
    g_SampleUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    CDXUTElement* pElement = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    CDXUTComboBox* pComboBox = NULL;


    const WCHAR* RENDER_MODE_NAMES[] =
    {
        L"Decoded scene",
        L"RGB channels",
        L"Alpha channel",
    };

    g_SampleUI.AddStatic( IDC_STATIC, L"(E)ncoding mode", 0, 0, 105, 25 );
    g_SampleUI.AddComboBox( IDC_ENCODING_MODE, 0, 25, 140, 24, 'E', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 50 );

    g_SampleUI.AddStatic( IDC_STATIC, L"(N)ormal mode", 0, 40, 105, 25 );
    g_SampleUI.AddComboBox( IDC_NORMAL_MODE, 0, 65, 140, 24, 'N', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 50 );

    g_SampleUI.AddStatic( IDC_STATIC, L"(R)ender mode", 0, 80, 105, 25 );
    g_SampleUI.AddComboBox( IDC_RENDER_MODE, 0, 105, 140, 24, 'R', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 30 );

    for( int i = 0; i < NUM_RENDER_MODES; i++ )
    {
        pComboBox->AddItem( RENDER_MODE_NAMES[i], IntToPtr( i ) );
    }

    g_SampleUI.AddCheckBox( IDC_BLOOM, L"Show (B)loom", 0, 200, 140, 18, g_bBloom, 'B' );
}


//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Bloom_D3D10( DWORD dwD3DTexSize,
                                      float afTexCoordOffset[15],
                                      D3DXVECTOR4* avColorWeight,
                                      float fDeviation, float fMultiplier )
{
    int i = 0;
    float tu = 1.0f / ( float )dwD3DTexSize;

    // Fill the center texel
    float weight = 1.0f * GaussianDistribution( 0, 0, fDeviation );
    avColorWeight[0] = D3DXVECTOR4( weight, weight, weight, 1.0f );

    afTexCoordOffset[0] = 0.0f;

    // Fill the right side
    for( i = 1; i < 8; i++ )
    {
        weight = fMultiplier * GaussianDistribution( ( float )i, 0, fDeviation );
        afTexCoordOffset[i] = i * tu;

        avColorWeight[i] = D3DXVECTOR4( weight, weight, weight, 1.0f );
    }

    // Copy to the left side
    for( i = 8; i < 15; i++ )
    {
        avColorWeight[i] = avColorWeight[i - 7];
        afTexCoordOffset[i] = -afTexCoordOffset[i - 7];
    }

    return S_OK;
}

using namespace D3D10;

bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D10DestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    g_pd3dDevice10 = pd3dDevice;

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDRFormats10.fx" ) );

    // If this fails, there should be debug output as to 
    // why the .fx file failed to compile
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

    ZeroMemory( g_apTexToneMap10, sizeof( g_apTexToneMap10 ) );
    ZeroMemory( g_apTexBloom10, sizeof( g_apTexBloom10 ) );
    ZeroMemory( g_aTechHandles10, sizeof( g_aTechHandles10 ) );

    //get effects techniques
    RetrieveD3D10TechHandles();

    //get the effect variables
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_ptCube = g_pEffect10->GetVariableByName( "g_tCube" )->AsShaderResource();
    g_ptNormal = g_pEffect10->GetVariableByName( "g_tNormal" )->AsShaderResource();
    g_pS0 = g_pEffect10->GetVariableByName( "s0" )->AsShaderResource();
    g_pS1 = g_pEffect10->GetVariableByName( "s1" )->AsShaderResource();
    g_pS2 = g_pEffect10->GetVariableByName( "s2" )->AsShaderResource();
    g_pavSampleOffsets = g_pEffect10->GetVariableByName( "g_avSampleOffsets" )->AsVector();
    g_pavSampleWeights = g_pEffect10->GetVariableByName( "g_avSampleWeights" )->AsVector();

    // Determine which encoding modes this device can support
    WCHAR strPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"Light Probes\\uffizi_cross.dds" ) );

    ID3D10Texture2D* pCubeTexture = NULL;
    ID3D10ShaderResourceView* pCubeRV = NULL;
    UINT SupportCaps = 0;

    g_eEncodingMode = ( ENCODING_MODE )-1;

    for( int i = 0; i < NUM_ENCODING_MODES; i++ )
    {
        if( DXGI_FORMAT_R9G9B9E5_SHAREDEXP == g_EncodingFormats[i] )
        {
            // Direct3D 10 doesn't support R9G9B9E5 render targets, so use FP16 render targets
            // when using R9G9B9E5 as a source
            pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R16G16B16A16_FLOAT, &SupportCaps );
        }
        else
        {
            pd3dDevice->CheckFormatSupport( g_EncodingFormats[i], &SupportCaps );
        }
        if( SupportCaps & D3D10_FORMAT_SUPPORT_TEXTURECUBE &&
            SupportCaps & D3D10_FORMAT_SUPPORT_RENDER_TARGET &&
            SupportCaps & D3D10_FORMAT_SUPPORT_TEXTURE2D )
        {
            if( g_EncodingModes[i] == RGB16 ||
                g_EncodingModes[i] == RGB9E5 ||
                g_EncodingModes[i] == RGBE8 )
            {
                hr = CreateEncodedTexture10( pd3dDevice,
                                             strPath,
                                             g_EncodingModes[i],
                                             &pCubeTexture,
                                             &pCubeRV );
            }
            else
            {
                D3DX10_IMAGE_LOAD_INFO LoadInfo;
                LoadInfo.Format = g_EncodingFormats[i];
                hr = D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, &LoadInfo, NULL, &pCubeRV, NULL );
                if( SUCCEEDED( hr ) )
                    pCubeRV->GetResource( ( ID3D10Resource** )&pCubeTexture );
            }

            if( SUCCEEDED( hr ) )
            {
                V_RETURN( g_aSkybox[g_EncodingModes[i]].OnD3D10CreateDevice( pd3dDevice, 50, pCubeTexture, pCubeRV,
                                                                             L"skybox10.fx" ) );
                g_SampleUI.GetComboBox( IDC_ENCODING_MODE )->AddItem( g_szEncodingModes[i],
                                                                      ( void* )g_EncodingModes[i] );
                g_eEncodingMode = g_EncodingModes[i];
            }
        }
    }

    if( g_eEncodingMode == -1 )
        return E_FAIL;

    // Determine which normal map formats this device can support
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"misc\\NormTest.dds" ) );

    g_eNormalMode = ( NORMAL_MODE )-1;

    for( int i = 0; i < NUM_NORMAL_MODES; i++ )
    {
        pd3dDevice->CheckFormatSupport( g_NormalFormats[i], &SupportCaps );
        if( SupportCaps & D3D10_FORMAT_SUPPORT_TEXTURE2D )
        {
            D3DX10_IMAGE_LOAD_INFO LoadInfo;
            LoadInfo.Format = g_NormalFormats[i];
            hr = D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, &LoadInfo, NULL, &g_aTexNormalRV[i],
                                                         NULL );
            if( SUCCEEDED( hr ) )
            {
                g_aTexNormalRV[i]->GetResource( ( ID3D10Resource** )&g_aTexNormal[i] );
                g_SampleUI.GetComboBox( IDC_NORMAL_MODE )->AddItem( g_szNormalModes[i], ( void* )g_NormalModes[i] );
                g_pCurrentTexNormalRV = g_aTexNormalRV[i];
                g_eNormalMode = g_NormalModes[i];
            }
        }
    }

    if( g_eNormalMode == -1 )
        return E_FAIL;

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC scenelayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_aTechHandles10[ FP32 ].Scene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( scenelayout, 4, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSceneLayout ) );

    // Create our quad input layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_aTechHandles10[ FP32 ].DownScale2x2_Lum->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, 2, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    // create our mesh
    V( g_Mesh10.Create( pd3dDevice, L"misc\\balltan.sdkmesh", true ) );

    // Create a screen quad for all render to texture operations
    SCREEN_VERTEX svQuad[4];
    svQuad[0].pos = D3DXVECTOR4( -1.0f, 1.0f, 0.5f, 1.0f );
    svQuad[0].tex = D3DXVECTOR2( 0.0f, 0.0f );
    svQuad[1].pos = D3DXVECTOR4( 1.0f, 1.0f, 0.5f, 1.0f );
    svQuad[1].tex = D3DXVECTOR2( 1.0f, 0.0f );
    svQuad[2].pos = D3DXVECTOR4( -1.0f, -1.0f, 0.5f, 1.0f );
    svQuad[2].tex = D3DXVECTOR2( 0.0f, 1.0f );
    svQuad[3].pos = D3DXVECTOR4( 1.0f, -1.0f, 0.5f, 1.0f );
    svQuad[3].tex = D3DXVECTOR2( 1.0f, 1.0f );

    D3D10_BUFFER_DESC vbdesc =
    {
        4 * sizeof( SCREEN_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = svQuad;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pScreenQuadVB ) );


    D3DXVECTOR3 vecEye( 0.0f, 0.5f, -3.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    g_bBloom = TRUE;
    g_eRenderMode = DECODED;
    g_pCurTechnique10 = &g_aTechHandles10[ g_eEncodingMode ];

    return S_OK;
}

HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    // SwapChain has changed and may have new attributes such as size. Application
    // should create resources dependent on SwapChain, such as 2D textures that
    // need to have the same size as the back buffer.
    //
    DXUTTRACE( L"SwapChain Resized\n" );

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    for( int i = 0; i < NUM_ENCODING_MODES; i++ )
    {
        g_aSkybox[i].OnD3D10ResizedSwapChain( pBackBufferSurfaceDesc );
    }

    // Create depth stencil texture.
    //
    D3D10_TEXTURE2D_DESC smtex;
    smtex.Width = pBackBufferSurfaceDesc->Width;
    smtex.Height = pBackBufferSurfaceDesc->Height;
    smtex.MipLevels = 1;
    smtex.ArraySize = 1;
    smtex.Format = DXGI_FORMAT_R32_TYPELESS;
    smtex.SampleDesc.Count = 1;
    smtex.SampleDesc.Quality = 0;
    smtex.Usage = D3D10_USAGE_DEFAULT;
    smtex.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    smtex.CPUAccessFlags = 0;
    smtex.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &smtex, NULL, &g_pDepthStencil ) );

    // Create the depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
    DescDS.Format = DXGI_FORMAT_D32_FLOAT;
    DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    DescDS.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &DescDS, &g_pDSV ) );

    // Set the render target and depth stencil
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->OMSetRenderTargets( 1, &pRTV, g_pDSV );

    DXGI_FORMAT fmt = g_EncodingFormats[ g_eEncodingMode ];
    if( DXGI_FORMAT_R9G9B9E5_SHAREDEXP == fmt )
    {
        // We can't create an R9G9B9E5 render target
        // so for R9G9B9E5 HDR formats, use 16bit render targets
        fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
    }

    // Create the render target texture
    D3D10_TEXTURE2D_DESC Desc;
    ZeroMemory( &Desc, sizeof( D3D10_TEXTURE2D_DESC ) );
    Desc.ArraySize = 1;
    Desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    Desc.Usage = D3D10_USAGE_DEFAULT;
    Desc.Format = fmt;
    Desc.Width = pBackBufferSurfaceDesc->Width;
    Desc.Height = pBackBufferSurfaceDesc->Height;
    Desc.MipLevels = 1;
    Desc.SampleDesc.Count = 1;
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pTexRender10 ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = Desc.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pTexRender10, &DescRT, &g_pTexRenderRTV10 ) );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC DescRV;
    DescRV.Format = Desc.Format;
    DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    DescRV.Texture2D.MipLevels = 1;
    DescRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pTexRender10, &DescRV, &g_pTexRenderRV10 ) );

    // Create the bright pass texture
    Desc.Width /= 8;
    Desc.Height /= 8;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pTexBrightPass10 ) );

    // Create the render target view
    DescRT.Format = Desc.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pTexBrightPass10, &DescRT, &g_pTexBrightPassRTV10 ) );

    // Create the resource view
    DescRV.Format = Desc.Format;
    DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    DescRV.Texture2D.MipLevels = 1;
    DescRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pTexBrightPass10, &DescRV, &g_pTexBrightPassRV10 ) );

    // Determine whether we can and should support a multisampling on the HDR render target
    g_bUseMultiSample = false;
    UINT SupportCaps = 0;
    g_bSupportsD16 = false;
    g_bSupportsD32 = false;
    g_bSupportsD24X8 = false;

    pd3dDevice->CheckFormatSupport( DXGI_FORMAT_D16_UNORM, &SupportCaps );
    if( SupportCaps & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL )
        g_bSupportsD16 = true;

    pd3dDevice->CheckFormatSupport( DXGI_FORMAT_D32_FLOAT, &SupportCaps );
    if( SupportCaps & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL )
        g_bSupportsD32 = true;

    pd3dDevice->CheckFormatSupport( DXGI_FORMAT_D32_FLOAT_S8X24_UINT, &SupportCaps );
    if( SupportCaps & D3D10_FORMAT_SUPPORT_DEPTH_STENCIL )
        g_bSupportsD24X8 = true;

    DXGI_FORMAT dfmt = DXGI_FORMAT_UNKNOWN;
    if( g_bSupportsD16 )
        dfmt = DXGI_FORMAT_D16_UNORM;
    else if( g_bSupportsD32 )
        dfmt = DXGI_FORMAT_D32_FLOAT;
    else if( g_bSupportsD24X8 )
        dfmt = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    if( dfmt != D3DFMT_UNKNOWN )
    {
        g_MaxMultiSampleType10 = 0;
        for( UINT i = 1; i <= D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT; i ++ )
        {
            UINT msQuality1 = 0;
            UINT msQuality2 = 0;
            if( SUCCEEDED( pd3dDevice->CheckMultisampleQualityLevels( fmt, i, &msQuality1 ) ) &&
                msQuality1 > 0 )
            {
                if( SUCCEEDED( pd3dDevice->CheckMultisampleQualityLevels( dfmt, i, &msQuality2 ) ) &&
                    msQuality2 > 0 )

                    g_bUseMultiSample = true;
                g_MaxMultiSampleType10 = i;
                g_dwMultiSampleQuality10 = min( msQuality1 - 1, msQuality2 - 1 );
            }
        }

        // Create the Multi-Sample floating point render target
        if( g_bUseMultiSample )
        {
            UINT uiWidth = pBackBufferSurfaceDesc->Width;
            UINT uiHeight = pBackBufferSurfaceDesc->Height;

            HRESULT hr = S_OK;
            D3D10_TEXTURE2D_DESC dstex;
            dstex.Width = uiWidth;
            dstex.Height = uiHeight;
            dstex.MipLevels = 1;
            dstex.Format = fmt;
            dstex.SampleDesc.Count = g_MaxMultiSampleType10;
            dstex.SampleDesc.Quality = g_dwMultiSampleQuality10;
            dstex.Usage = D3D10_USAGE_DEFAULT;
            dstex.BindFlags = D3D10_BIND_RENDER_TARGET;
            dstex.CPUAccessFlags = 0;
            dstex.MiscFlags = 0;
            dstex.ArraySize = 1;
            V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pMSRT10 ) );

            // Create the render target view
            D3D10_RENDER_TARGET_VIEW_DESC DescRT;
            DescRT.Format = dstex.Format;
            DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
            DescRT.Texture2D.MipSlice = 0;
            V_RETURN( pd3dDevice->CreateRenderTargetView( g_pMSRT10, &DescRT, &g_pMSRTV10 ) );

            //
            // Create depth stencil texture.
            //
            dstex.Width = uiWidth;
            dstex.Height = uiHeight;
            dstex.MipLevels = 1;
            dstex.Format = dfmt;
            dstex.SampleDesc.Count = g_MaxMultiSampleType10;
            dstex.SampleDesc.Quality = g_dwMultiSampleQuality10;
            dstex.Usage = D3D10_USAGE_DEFAULT;
            dstex.BindFlags = D3D10_BIND_DEPTH_STENCIL;
            dstex.CPUAccessFlags = 0;
            dstex.MiscFlags = 0;
            V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pMSDS10 ) );

            // Create the depth stencil view
            D3D10_DEPTH_STENCIL_VIEW_DESC DescDS;
            DescDS.Format = dfmt;
            DescDS.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
            DescDS.Texture2D.MipSlice = 0;
            V_RETURN( pd3dDevice->CreateDepthStencilView( g_pMSDS10, &DescDS, &g_pMSDSV10 ) );
        }
    }

    // For each scale stage, create a texture to hold the intermediate results
    // of the luminance calculation
    fmt = DXGI_FORMAT_UNKNOWN;
    switch( g_eEncodingMode )
    {
        case FP16:
            fmt = DXGI_FORMAT_R16_FLOAT; break;
        case FP32:
            fmt = DXGI_FORMAT_R32_FLOAT; break;
        case RGB16:
            fmt = DXGI_FORMAT_R16_UNORM; break;
        case RGBE8:
            fmt = DXGI_FORMAT_R8G8_UNORM; break;
        case RGB9E5:
            fmt = DXGI_FORMAT_R16_FLOAT; break;	// Again, we can't use R9G9B9E5 render targets, so use FP16
        case RG11B10:
            fmt = DXGI_FORMAT_R11G11B10_FLOAT; break;
    }

    int nSampleLen = 1;
    for( int i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        D3D10_TEXTURE2D_DESC tmdesc;
        ZeroMemory( &tmdesc, sizeof( D3D10_TEXTURE2D_DESC ) );
        tmdesc.ArraySize = 1;
        tmdesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
        tmdesc.Usage = D3D10_USAGE_DEFAULT;
        tmdesc.Format = fmt;
        tmdesc.Width = nSampleLen;
        tmdesc.Height = nSampleLen;
        tmdesc.MipLevels = 1;
        tmdesc.SampleDesc.Count = 1;

        V_RETURN( pd3dDevice->CreateTexture2D( &tmdesc, NULL, &g_apTexToneMap10[i] ) );

        // Create the render target view
        D3D10_RENDER_TARGET_VIEW_DESC DescRT;
        DescRT.Format = tmdesc.Format;
        DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
        DescRT.Texture2D.MipSlice = 0;
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_apTexToneMap10[i], &DescRT, &g_apTexToneMapRTV10[i] ) );

        // Create the shader resource view
        D3D10_SHADER_RESOURCE_VIEW_DESC DescRV;
        DescRV.Format = tmdesc.Format;
        DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        DescRV.Texture2D.MipLevels = 1;
        DescRV.Texture2D.MostDetailedMip = 0;
        V_RETURN( pd3dDevice->CreateShaderResourceView( g_apTexToneMap10[i], &DescRV, &g_apTexToneMapRV10[i] ) );

        nSampleLen *= 3;
    }

    // Create the temporary blooming effect textures
    for( int i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        D3D10_TEXTURE2D_DESC bmdesc;
        ZeroMemory( &bmdesc, sizeof( D3D10_TEXTURE2D_DESC ) );
        bmdesc.ArraySize = 1;
        bmdesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
        bmdesc.Usage = D3D10_USAGE_DEFAULT;
        bmdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bmdesc.Width = pBackBufferSurfaceDesc->Width / 8;
        bmdesc.Height = pBackBufferSurfaceDesc->Height / 8;
        bmdesc.MipLevels = 1;
        bmdesc.SampleDesc.Count = 1;

        V_RETURN( pd3dDevice->CreateTexture2D( &bmdesc, NULL, &g_apTexBloom10[i] ) );

        // Create the render target view
        D3D10_RENDER_TARGET_VIEW_DESC DescRT;
        DescRT.Format = bmdesc.Format;
        DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
        DescRT.Texture2D.MipSlice = 0;
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_apTexBloom10[i], &DescRT, &g_apTexBloomRTV10[i] ) );

        // Create the shader resource view
        D3D10_SHADER_RESOURCE_VIEW_DESC DescRV;
        DescRV.Format = bmdesc.Format;
        DescRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        DescRV.Texture2D.MipLevels = 1;
        DescRV.Texture2D.MostDetailedMip = 0;
        V_RETURN( pd3dDevice->CreateShaderResourceView( g_apTexBloom10[i], &DescRV, &g_apTexBloomRV10[i] ) );

    }

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );

    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 150, pBackBufferSurfaceDesc->Height - 250 );
    g_SampleUI.SetSize( 150, 110 );

    CDXUTCheckBox* pCheckBox = g_SampleUI.GetCheckBox( IDC_BLOOM );
    pCheckBox->SetChecked( g_bBloom );

    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_ENCODING_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eEncodingMode );

    pComboBox = g_SampleUI.GetComboBox( IDC_RENDER_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eRenderMode );

    pComboBox = g_SampleUI.GetComboBox( IDC_NORMAL_MODE );
    pComboBox->SetSelectedByData( ( void* )g_eNormalMode );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    return S_OK;
}


void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    UNREFERENCED_PARAMETER( fElapsedTime );

    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    ID3D10EffectShaderResourceVariable* pCube;
    ID3D10EffectShaderResourceVariable* pS0;
    ID3D10EffectShaderResourceVariable* pS1;
    pCube = g_ptCube;
    pS0 = g_pS0;
    pS1 = g_pS1;

    // Clear the depth stencil
    pd3dDevice->ClearDepthStencilView( g_pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // Get BackBuffer surface Desc
    const DXGI_SURFACE_DESC* pRTDesc = DXUTGetDXGIBackBufferSurfaceDesc();

    // Store off original render targets
    ID3D10RenderTargetView* pOrigRTV = NULL;
    ID3D10DepthStencilView* pOrigDSV = NULL;
    pd3dDevice->OMGetRenderTargets( 1, &pOrigRTV, &pOrigDSV );

    // Setup the HDR render target
    if( g_bUseMultiSample )
    {
        ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pMSRTV10 };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, g_pMSDSV10 );

        pd3dDevice->ClearRenderTargetView( g_pMSRTV10, ClearColor );
        pd3dDevice->ClearDepthStencilView( g_pMSDSV10, D3D10_CLEAR_DEPTH, 1.0, 0 );
    }
    else
    {
        ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pTexRenderRTV10 };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, pOrigDSV );

        pd3dDevice->ClearRenderTargetView( g_pTexRenderRTV10, ClearColor );
    }

    // Update matrices
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mWorldView;
    D3DXMATRIXA16 mWorldViewProj;
    D3DXMatrixIdentity( &mWorld );

    D3DXMatrixMultiply( &mWorldViewProj, g_Camera.GetViewMatrix(), g_Camera.GetProjMatrix() );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pvEyePt->SetRawValue( ( float* )g_Camera.GetEyePt(), 0, sizeof( D3DXVECTOR3 ) );

    // Draw the skybox
    g_aSkybox[g_eEncodingMode].D3D10Render( &mWorldViewProj );

    // Draw the mesh
    pd3dDevice->IASetInputLayout( g_pSceneLayout );
    pCube->SetResource( g_aSkybox[g_eEncodingMode].GetD3D10EnvironmentMapRV() );
    g_ptNormal->SetResource( g_pCurrentTexNormalRV );
    if( NORM_FPR32G32 == g_eNormalMode || NORM_FPR16G16 == g_eNormalMode || NORM_R8G8U == g_eNormalMode )
        g_Mesh10.Render( pd3dDevice, g_pCurTechnique10->SceneTwoComp );
    else
        g_Mesh10.Render( pd3dDevice, g_pCurTechnique10->Scene );

    // If using floating point multi sampling, copy over to the rendertarget
    if( g_bUseMultiSample )
    {
        D3D10_TEXTURE2D_DESC Desc;
        g_pTexRender10->GetDesc( &Desc );
        pd3dDevice->ResolveSubresource( g_pTexRender10, D3D10CalcSubresource( 0, 0, 1 ), g_pMSRT10,
                                        D3D10CalcSubresource( 0, 0, 1 ), Desc.Format );
        ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pTexRenderRTV10 };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, pOrigDSV );
    }

    MeasureLuminance10();
    BrightPassFilter10();

    if( g_bBloom )
        RenderBloom10();

    //---------------------------------------------------------------------
    // Final pass
    ID3D10RenderTargetView* aRTViews[ 1 ] = { pOrigRTV };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, pOrigDSV );

    pS0->SetResource( g_pTexRenderRV10 );
    pS1->SetResource( g_apTexToneMapRV10[0] );
    g_pS2->SetResource( g_bBloom ? g_apTexBloomRV10[0] : NULL );

    ID3D10EffectTechnique* pTech = NULL;
    switch( g_eRenderMode )
    {
        case DECODED:
            pTech = g_pCurTechnique10->FinalPass; break;
        case RGB_ENCODED:
            pTech = g_pFinalPassEncoded_RGB; break;
        case ALPHA_ENCODED:
            pTech = g_pFinalPassEncoded_A; break;
    }

    DrawFullScreenQuad10( pd3dDevice, pTech, pRTDesc->Width, pRTDesc->Height );
    SAFE_RELEASE( pOrigRTV );
    SAFE_RELEASE( pOrigDSV );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    if( g_bShowText )
        RenderD3D10Text( fTime );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}


void RenderD3D10Text( double fTime )
{

    UNREFERENCED_PARAMETER( fTime );

    const WCHAR* ENCODING_MODE_NAMES[] =
    {
        L"HDR Mode: 16-Bit floating-point (FP16)",
        L"HDR Mode: 32-Bit floating-point (FP32)",
        L"HDR Mode: 16-Bit integer (RGB16)",
        L"HDR Mode: 8-Bit integer w/ shared exponent (RGBE8)",
        L"HDR Mode: 9-Bit integer w/ shared exponent (RGB9E5)",
        L"HDR Mode: 11/10-Bit floating-point (RG11B10)",
    };

    const WCHAR* RENDER_MODE_NAMES[] =
    {
        L"Decoded scene",
        L"RGB channels of encoded textures",
        L"Alpha channel of encoded textures",
    };

    const WCHAR* NORMAL_MODE_NAMES[] =
    {
        L"Normal Format: 8-Bit integer (RGB8)",
        L"Normal Format: 32-Bit floating-point (FP32)",
        L"Normal Format: 16-Bit floating-point (FP16)",
        L"Normal Format: 11/10-Bit floating-point (RG11B10)",
        L"Normal Format: 32-Bit Two-Component (FPR32G32)",
        L"Normal Format: 16-Bit Two-Component (FPR16G16)",
        L"Normal Format: 8-Bit Two-Component (R8G8U)",
        L"Normal Format: BC1 Compressed UNorm (BC1U)",
        L"Normal Format: BC5 Compressed SNorm (BC5S)",
        L"Normal Format: BC5 Compressed UNorm (BC5U)",
    };

    // Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.90f, 0.90f, 1.0f, 1.0f ) );
    g_pTxtHelper->DrawFormattedTextLine( ENCODING_MODE_NAMES[ g_eEncodingMode ] );
    g_pTxtHelper->DrawFormattedTextLine( NORMAL_MODE_NAMES[ g_eNormalMode ] );
    g_pTxtHelper->DrawFormattedTextLine( RENDER_MODE_NAMES[ g_eRenderMode ] );

    if( g_bUseMultiSample )
    {
        g_pTxtHelper->DrawTextLine( L"Using MultiSample Render Target" );
        g_pTxtHelper->DrawFormattedTextLine( L"Number of Samples: %d", ( int )g_MaxMultiSampleType10 );
        g_pTxtHelper->DrawFormattedTextLine( L"Quality: %d", g_dwMultiSampleQuality10 );
    }

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        g_pTxtHelper->SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate camera: Right mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n"
                                    L"Quit: ESC" );

        g_pTxtHelper->SetInsertionPos( 250, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Cycle encoding: E\n"
                                    L"Cycle render mode: R\n"
                                    L"Toggle bloom: B\n"
                                    L"Hide text: T\n" );

        g_pTxtHelper->SetInsertionPos( 410, pd3dsdBackBuffer->Height - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Change Device: F2\n"
                                    L"Toggle HAL/REF: F3\n"
                                    L"View readme: F9\n" );
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }
    g_pTxtHelper->End();
}


void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    DXUTTRACE( L"SwapChain Lost called\n" );

    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    // Release any back buffer resolution dependent objects
    SAFE_RELEASE( g_pDSV );
    SAFE_RELEASE( g_pDepthStencil );

    SAFE_RELEASE( g_pTexRender10 );
    SAFE_RELEASE( g_pTexRenderRV10 );
    SAFE_RELEASE( g_pTexRenderRTV10 );
    SAFE_RELEASE( g_pTexBrightPass10 );
    SAFE_RELEASE( g_pTexBrightPassRV10 );
    SAFE_RELEASE( g_pTexBrightPassRTV10 );

    SAFE_RELEASE( g_pMSRT10 );       // Multi-Sample float render target
    SAFE_RELEASE( g_pMSRTV10 );
    SAFE_RELEASE( g_pMSDS10 );       // Depth Stencil surface for the float RT
    SAFE_RELEASE( g_pMSDSV10 );

    for( int i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexToneMap10[i] ); // Tone mapping calculation textures
        SAFE_RELEASE( g_apTexToneMapRV10[i] );
        SAFE_RELEASE( g_apTexToneMapRTV10[i] );
    }
    for( int i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexBloom10[i] );     // Blooming effect intermediate texture
        SAFE_RELEASE( g_apTexBloomRV10[i] );
        SAFE_RELEASE( g_apTexBloomRTV10[i] );
    }

    for( int i = 0; i < NUM_ENCODING_MODES; i++ )
        g_aSkybox[i].OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D10CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pSceneLayout );
    SAFE_RELEASE( g_pQuadLayout );
    SAFE_RELEASE( g_pScreenQuadVB );
    g_Mesh10.Destroy();

    for( int i = 0; i < NUM_ENCODING_MODES; i++ )
        g_aSkybox[i].OnD3D10DestroyDevice();

    for( int i = 0; i < NUM_NORMAL_MODES; i++ )
    {
        SAFE_RELEASE( g_aTexNormal[i] );
        SAFE_RELEASE( g_aTexNormalRV[i] );
    }
}

//-----------------------------------------------------------------------------
// Name: RetrieveD3D10TechHandles()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT RetrieveD3D10TechHandles()
{
    CHAR* modes[] = { "FP", "FP", "RGB16", "RGBE8", "FP", "FP" };
    CHAR* techniques[] = { "Scene", "SceneTwoComp", "DownScale2x2_Lum", "DownScale3x3", "DownScale3x3_BrightPass",
            "FinalPass" };
    DWORD dwNumTechniques = sizeof( TECH_HANDLES10 ) / sizeof( ID3D10EffectTechnique* );

    CHAR strBuffer[MAX_PATH] = {0};

    ID3D10EffectTechnique** ppHandle = ( ID3D10EffectTechnique** )g_aTechHandles10;

    for( UINT m = 0; m < ( UINT )NUM_ENCODING_MODES; m++ )
    {
        for( UINT t = 0; t < dwNumTechniques; t++ )
        {
            sprintf_s( strBuffer, MAX_PATH - 1, "%s_%s", techniques[t], modes[m] );

            ( *ppHandle ) = g_pEffect10->GetTechniqueByName( strBuffer );
            ppHandle++;
        }
    }

    g_pFinalPassEncoded_RGB = g_pEffect10->GetTechniqueByName( "FinalPassEncoded_RGB" );
    g_pFinalPassEncoded_A = g_pEffect10->GetTechniqueByName( "FinalPassEncoded_A" );
    g_pBloom = g_pEffect10->GetTechniqueByName( "BloomTech" );

    g_pDrawTexture = g_pEffect10->GetTechniqueByName( "DrawTexture" );

    return S_OK;
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
    }

    if( pDeviceSettings->ver == DXUT_D3D10_DEVICE )
    {
        // Disable MSAA
        pDeviceSettings->d3d10.sd.SampleDesc.Count = 1;
        pDeviceSettings->d3d10.sd.SampleDesc.Quality = 0;
        g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled( false );
        g_SettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL )->SetEnabled(
            false );
        g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled(
            false );
        g_SettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL )->SetEnabled(
            false );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void    CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    UNREFERENCED_PARAMETER( fTime );

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
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

    UNREFERENCED_PARAMETER( pbNoFurtherProcessing );

    // Give on screen UI a chance to handle this message
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
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
    UNREFERENCED_PARAMETER( bAltDown );

    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
            case 'T':
                g_bShowText = !g_bShowText; break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    CDXUTComboBox* pComboBox = NULL;
    CDXUTCheckBox* pCheckBox = NULL;


    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_BLOOM:
            g_bBloom = !g_bBloom; break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_RENDER_MODE:
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eRenderMode = ( RENDER_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );
            break;

        case IDC_ENCODING_MODE:
        {
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eEncodingMode = ( ENCODING_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );

            DXUTDeviceSettings DeviceSettings = DXUTGetDeviceSettings();
            switch( DeviceSettings.ver )
            {
                case DXUT_D3D9_DEVICE:
                {
                    g_pCurTechnique9 = &g_aTechHandles9[ g_eEncodingMode ];

                    // Refresh resources
                    const D3DSURFACE_DESC* pBackBufDesc = DXUTGetD3D9BackBufferSurfaceDesc();
                    OnD3D9LostDevice( NULL );
                    OnD3D9ResetDevice( g_pd3dDevice9, pBackBufDesc, NULL );
                    break;
                }
                case DXUT_D3D10_DEVICE:
                {
                    D3D10::g_pCurTechnique10 = &D3D10::g_aTechHandles10[ g_eEncodingMode ];

                    // Refresh resources
                    const DXGI_SURFACE_DESC* pBackBufDesc = DXUTGetDXGIBackBufferSurfaceDesc();
                    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
                    OnD3D10ReleasingSwapChain( NULL );
                    OnD3D10ResizedSwapChain( g_pd3dDevice10, pSwapChain, pBackBufDesc, NULL );
                    break;
                }
            }

            break;
        }

        case IDC_NORMAL_MODE:
        {
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eNormalMode = ( NORMAL_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );

            DXUTDeviceSettings DeviceSettings = DXUTGetDeviceSettings();
            switch( DeviceSettings.ver )
            {
                case DXUT_D3D9_DEVICE:
                {
                    //no normal mode for 9
                    break;
                }
                case DXUT_D3D10_DEVICE:
                {
                    D3D10::g_pCurrentTexNormalRV = D3D10::g_aTexNormalRV[ g_eNormalMode ];
                    break;
                }
            }
            break;
        }

    }

    // Update the bloom checkbox based on new state
    pCheckBox = g_SampleUI.GetCheckBox( IDC_BLOOM );
    pCheckBox->SetEnabled( g_eRenderMode == DECODED );
    pCheckBox->SetChecked( g_eRenderMode == DECODED && g_bBloom );
}

//--------------------------------------------------------------------------------------
float GaussianDistribution( float x, float y, float rho )
{
    float g = 1.0f / sqrtf( 2.0f * D3DX_PI * rho * rho );
    g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

    return g;
}


//--------------------------------------------------------------------------------------
int log2_ceiling( float val )
{
    int iMax = 256;
    int iMin = 0;

    while( iMax - iMin > 1 )
    {
        int iMiddle = ( iMax + iMin ) / 2;

        if( val > g_aPowsOfTwo[iMiddle] )
            iMin = iMiddle;
        else
            iMax = iMiddle;
    }

    return iMax - 128;
}


//--------------------------------------------------------------------------------------
VOID EncodeRGBE8( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Determine the largest color component
    float maxComponent = max( max( r, g ), b );

    // Round to the nearest integer exponent
    int nExp = log2_ceiling( maxComponent );

    // Divide the components by the shared exponent
    FLOAT fDivisor = ( FLOAT )g_aPowsOfTwo[ nExp + 128 ];

    r /= fDivisor;
    g /= fDivisor;
    b /= fDivisor;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store the shared exponent in the alpha channel
    UINT* pDestColor = ( UINT* )*ppDest;
    *pDestColor = ( ( nExp + 128 ) & 255 ) << 24;
    *pDestColor |= ( ( UINT )( b * 255 ) & 255 ) << 16;
    *pDestColor |= ( ( UINT )( g * 255 ) & 255 ) << 8;
    *pDestColor |= ( ( UINT )( r * 255 ) & 255 );

    *ppDest += sizeof( UINT );
}

//--------------------------------------------------------------------------------------
VOID EncodeRGB9E5( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Determine the largest color component
    float maxComponent = max( max( r, g ), b );

    // Round to the nearest integer exponent
    int nExp = log2_ceiling( maxComponent );
    nExp = max( nExp, -15 );	//keep us in the -15 to 16 range
    nExp = min( nExp, 16 );

    // Divide the components by the shared exponent
    FLOAT fDivisor = ( FLOAT )g_aPowsOfTwo[ nExp + 128 ];

    r /= fDivisor;
    g /= fDivisor;
    b /= fDivisor;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store the shared exponent in the alpha channel
    UINT32* pDestColor = ( UINT32* )*ppDest;
    *pDestColor = ( ( nExp + 15 ) & 31 ) << 27;
    *pDestColor |= ( ( UINT )( b * 511 ) & 511 ) << 18;
    *pDestColor |= ( ( UINT )( g * 511 ) & 511 ) << 9;
    *pDestColor |= ( ( UINT )( r * 511 ) & 511 );

    *ppDest += sizeof( UINT32 );  // Move forward 32 bits
}


//--------------------------------------------------------------------------------------
VOID EncodeRGB16( D3DXFLOAT16* pSrc, BYTE** ppDest )
{
    FLOAT r, g, b;

    r = ( FLOAT )*( pSrc + 0 );
    g = ( FLOAT )*( pSrc + 1 );
    b = ( FLOAT )*( pSrc + 2 );

    // Divide the components by the multiplier
    r /= RGB16_MAX;
    g /= RGB16_MAX;
    b /= RGB16_MAX;

    // Constrain the color components
    r = max( 0, min( 1, r ) );
    g = max( 0, min( 1, g ) );
    b = max( 0, min( 1, b ) );

    // Store
    USHORT* pDestColor = ( USHORT* )*ppDest;
    *pDestColor++ = ( USHORT )( r * 65536 );
    *pDestColor++ = ( USHORT )( g * 65536 );
    *pDestColor++ = ( USHORT )( b * 65536 );

    *ppDest += sizeof( UINT64 );
}


//-----------------------------------------------------------------------------
// Name: MeasureLuminance()
// Desc: Measure the average log luminance in the scene.
//-----------------------------------------------------------------------------
HRESULT MeasureLuminance10()
{
    ID3D10EffectShaderResourceVariable* pS0;
    ID3D10EffectShaderResourceVariable* pS1;
    pS0 = g_pS0;
    pS1 = g_pS1;

    ID3D10ShaderResourceView* pTexSrc = NULL;
    ID3D10ShaderResourceView* pTexDest = NULL;
    ID3D10RenderTargetView* pSurfDest = NULL;

    //-------------------------------------------------------------------------
    // Initial sampling pass to convert the image to the log of the grayscale
    //-------------------------------------------------------------------------
    pTexSrc = g_pTexRenderRV10;
    pTexDest = g_apTexToneMapRV10[NUM_TONEMAP_TEXTURES - 1];
    pSurfDest = g_apTexToneMapRTV10[NUM_TONEMAP_TEXTURES - 1];

    D3D10_TEXTURE2D_DESC descSrc;
    g_pTexRender10->GetDesc( &descSrc );
    D3D10_TEXTURE2D_DESC descDest;
    g_apTexToneMap10[NUM_TONEMAP_TEXTURES - 1]->GetDesc( &descDest );

    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    ID3D10RenderTargetView* aRTViews[ 1 ] = { pSurfDest };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    pS0->SetResource( pTexSrc );

    DrawFullScreenQuad10( pd3dDevice, g_pCurTechnique10->DownScale2x2_Lum, descDest.Width, descDest.Height );

    pS0->SetResource( NULL );

    //-------------------------------------------------------------------------
    // Iterate through the remaining tone map textures
    //------------------------------------------------------------------------- 
    for( int i = NUM_TONEMAP_TEXTURES - 1; i > 0; i-- )
    {
        // Cycle the textures
        pTexSrc = g_apTexToneMapRV10[i];
        pTexDest = g_apTexToneMapRV10[i - 1];
        pSurfDest = g_apTexToneMapRTV10[i - 1];

        D3D10_TEXTURE2D_DESC desc;
        g_apTexToneMap10[i]->GetDesc( &desc );

        ID3D10RenderTargetView* aRTViews[ 1 ] = { pSurfDest };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

        pS0->SetResource( pTexSrc );

        DrawFullScreenQuad10( pd3dDevice, g_pCurTechnique10->DownScale3x3, desc.Width / 3, desc.Height / 3 );

        pS0->SetResource( NULL );
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: BrightPassFilter
// Desc: Prepare for the bloom pass by removing dark information from the scene
//-----------------------------------------------------------------------------
HRESULT BrightPassFilter10()
{
    ID3D10EffectShaderResourceVariable* pS0;
    ID3D10EffectShaderResourceVariable* pS1;
    pS0 = g_pS0;
    pS1 = g_pS1;

    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();

    const DXGI_SURFACE_DESC* pBackBufDesc = DXUTGetDXGIBackBufferSurfaceDesc();

    ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pTexBrightPassRTV10 };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    pS0->SetResource( g_pTexRenderRV10 );
    pS1->SetResource( g_apTexToneMapRV10[0] );

    DrawFullScreenQuad10( pd3dDevice, g_pCurTechnique10->DownScale3x3_BrightPass, pBackBufDesc->Width / 8,
                          pBackBufDesc->Height / 8 );

    pS0->SetResource( NULL );
    pS1->SetResource( NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderBloom()
// Desc: Render the blooming effect
//-----------------------------------------------------------------------------
HRESULT RenderBloom10()
{
    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();

    HRESULT hr = S_OK;
    int i = 0;

    ID3D10EffectShaderResourceVariable* pS0;
    ID3D10EffectShaderResourceVariable* pS1;
    pS0 = g_pS0;
    pS1 = g_pS1;

    D3DXVECTOR4 avSampleOffsets[15];
    float afSampleOffsets[15];
    D3DXVECTOR4 avSampleWeights[15];

    // Vert Blur
    ID3D10RenderTargetView* pSurfDest = g_apTexBloomRTV10[1];
    D3D10_TEXTURE2D_DESC RTDesc;
    g_apTexBloom10[1]->GetDesc( &RTDesc );

    D3D10_TEXTURE2D_DESC desc;
    g_pTexBrightPass10->GetDesc( &desc );

    hr = GetSampleOffsets_Bloom_D3D10( ( DWORD )desc.Width, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 15; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR4( afSampleOffsets[i], 0.0f, 0.0f, 0.0f );
    }

    g_pavSampleOffsets->SetFloatVectorArray( ( float* )avSampleOffsets, 0, 15 );
    g_pavSampleWeights->SetFloatVectorArray( ( float* )avSampleWeights, 0, 15 );

    ID3D10RenderTargetView* aRTViews[ 1 ] = { pSurfDest };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    pS0->SetResource( g_pTexBrightPassRV10 );
    DrawFullScreenQuad10( pd3dDevice, g_pBloom, RTDesc.Width, RTDesc.Height );
    pS0->SetResource( NULL );
    ID3D10ShaderResourceView* pViews[4] = {0,0,0,0};
    pd3dDevice->PSSetShaderResources( 0, 4, pViews );

    // Horz Blur
    pSurfDest = g_apTexBloomRTV10[0];

    hr = GetSampleOffsets_Bloom_D3D10( ( DWORD )desc.Height, afSampleOffsets, avSampleWeights, 3.0f, 1.25f );
    for( i = 0; i < 15; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR4( 0.0f, afSampleOffsets[i], 0.0f, 0.0f );
    }

    g_pavSampleOffsets->SetRawValue( avSampleOffsets, 0, sizeof( avSampleOffsets ) );
    g_pavSampleWeights->SetRawValue( avSampleWeights, 0, sizeof( avSampleWeights ) );

    aRTViews[ 0 ] = pSurfDest;
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );

    pS0->SetResource( g_apTexBloomRV10[1] );
    DrawFullScreenQuad10( pd3dDevice, g_pBloom, RTDesc.Width, RTDesc.Height );
    pS0->SetResource( NULL );

    return hr;
}

void DrawFullScreenQuad10( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTech, UINT Width, UINT Height )
{
    // Save the Old viewport
    D3D10_VIEWPORT vpOld[D3D10_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT nViewPorts = 1;
    pd3dDevice->RSGetViewports( &nViewPorts, vpOld );

    // Setup the viewport to match the backbuffer
    D3D10_VIEWPORT vp;
    vp.Width = Width;
    vp.Height = Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pd3dDevice->RSSetViewports( 1, &vp );


    UINT strides = sizeof( SCREEN_VERTEX );
    UINT offsets = 0;
    ID3D10Buffer* pBuffers[1] = { g_pScreenQuadVB };

    pd3dDevice->IASetInputLayout( g_pQuadLayout );
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    D3D10_TECHNIQUE_DESC techDesc;
    pTech->GetDesc( &techDesc );

    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0 );

        pd3dDevice->Draw( 4, 0 );
    }

    // Restore the Old viewport
    pd3dDevice->RSSetViewports( nViewPorts, vpOld );
}

//-----------------------------------------------------------------------------
// Name: CreateEncodedTexture10
// Desc: Create a copy of the input floating-point texture with RGB9E5, RGB16,
//       or RGB32 encoding
//-----------------------------------------------------------------------------
HRESULT CreateEncodedTexture10( ID3D10Device* pd3dDevice, WCHAR* strPath, ENCODING_MODE eTarget,
                                ID3D10Texture2D** ppTexOut, ID3D10ShaderResourceView** ppCubeRV )
{
    HRESULT hr = S_OK;

    ID3D10Texture2D* pTexSrc = NULL;
    ID3D10Texture2D* pTexDest = NULL;
    ID3D10Resource* pRes = NULL;

    D3DX10_IMAGE_LOAD_INFO LoadInfo;
    ZeroMemory( &LoadInfo, sizeof( D3DX10_IMAGE_LOAD_INFO ) );
    LoadInfo.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    LoadInfo.Depth = D3DX_FROM_FILE;
    LoadInfo.BindFlags = 0;
    LoadInfo.MipLevels = 1;
    LoadInfo.Width = D3DX_FROM_FILE;
    LoadInfo.Height = D3DX_FROM_FILE;
    LoadInfo.FirstMipLevel = 0;
    LoadInfo.Filter = D3DX10_FILTER_POINT;
    LoadInfo.MipFilter = D3DX10_FILTER_NONE;
    LoadInfo.Usage = D3D10_USAGE_STAGING;
    LoadInfo.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
    LoadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;

    hr = D3DX10CreateTextureFromFile( pd3dDevice, strPath, &LoadInfo, NULL, &pRes, NULL );
    if( FAILED( hr ) || !pRes )
        return E_FAIL;

    D3D10_TEXTURE2D_DESC desc;
    ZeroMemory( &desc, sizeof( D3D10_TEXTURE2D_DESC ) );
    pRes->QueryInterface( __uuidof( ID3D10Texture2D ), ( LPVOID* )&pTexSrc );
    pTexSrc->GetDesc( &desc );
    SAFE_RELEASE( pRes );

    // Create a texture with equal dimensions to store the encoded texture
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    fmt = g_EncodingFormats[eTarget];

    // Create the destination staging resource
    desc.Format = fmt;
    V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, &pTexDest ) );

    // Create the permanent texture
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.Usage = D3D10_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, ppTexOut ) );

    for( UINT iFace = 0; iFace < 6; iFace++ )
    {
        UINT iSubResource = D3D10CalcSubresource( 0, iFace, desc.MipLevels );

        // Map the source texture for reading
        D3D10_MAPPED_TEXTURE2D MappedFaceSrc;
        V_RETURN( pTexSrc->Map( iSubResource, D3D10_MAP_READ_WRITE, 0, &MappedFaceSrc ) );

        // Map the destination texture for writing
        D3D10_MAPPED_TEXTURE2D MappedFaceDest;
        V_RETURN( pTexDest->Map( iSubResource, D3D10_MAP_READ_WRITE, 0, &MappedFaceDest ) );

        BYTE* pSrcBytes = ( BYTE* )MappedFaceSrc.pData;
        BYTE* pDestBytes = ( BYTE* )MappedFaceDest.pData;

        for( UINT y = 0; y < desc.Height; y++ )
        {
            D3DXFLOAT16* pSrc = ( D3DXFLOAT16* )pSrcBytes;
            BYTE* pDest = pDestBytes;

            for( UINT x = 0; x < desc.Width; x++ )
            {
                switch( eTarget )
                {
                    case RGB9E5:
                        EncodeRGB9E5( pSrc, &pDest ); break;
                    case RGBE8:
                        EncodeRGBE8( pSrc, &pDest ); break;
                    case RGB16:
                        EncodeRGB16( pSrc, &pDest ); break;
                    default:
                        return E_FAIL;
                }

                pSrc += 4;
            }

            pSrcBytes += MappedFaceSrc.RowPitch;
            pDestBytes += MappedFaceDest.RowPitch;
        }

        pd3dDevice->UpdateSubresource( ( *ppTexOut ), iSubResource, NULL, MappedFaceDest.pData,
                                       MappedFaceDest.RowPitch, 0 );

        // Release the maps
        pTexDest->Unmap( iFace );
        pTexSrc->Unmap( iFace );
    }

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
    SRVDesc.TextureCube.MipLevels = desc.MipLevels;
    SRVDesc.TextureCube.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( ( *ppTexOut ), &SRVDesc, ppCubeRV ) );

    SAFE_RELEASE( pTexDest );
    SAFE_RELEASE( pTexSrc );
    return S_OK;
}
