//--------------------------------------------------------------------------------------
// File: Instancing.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "DXUTRes.h"
#include "resource.h"


//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------

struct QUAD_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR2 tex;
};

struct QUAD_MESH
{
    ID3D10Buffer* pVB;
    ID3D10Buffer* pIB;
    DWORD dwNumVerts;
    DWORD dwNumIndices;
    UINT uStride;
    ID3D10Texture2D** ppTexture;
    ID3D10ShaderResourceView** ppSRV;
};

#pragma pack(push, 1)
struct INSTANCEDATA
{
    D3DXMATRIX mMatWorld;
    float fOcc;
};
#pragma pack(pop)


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

// Textures for various texture arrays
LPCTSTR g_szLeafTextureNames[] =
{
    L"trees\\leaf_v3_green_tex.dds",
    L"trees\\leaf_v3_olive_tex.dds",
    L"trees\\leaf_v3_dark_tex.dds",
};

LPCTSTR g_szTreeLeafInstanceNames[] =
{
    L"data\\leaves5.mtx",
};

LPCTSTR g_szGrassTextureNames[] =
{
    L"IslandScene\\grass_v1_basic_tex.dds",
    L"IslandScene\\grass_v2_light_tex.dds",
    L"IslandScene\\grass_v3_dark_tex.dds",
    L"IslandScene\\dry_flowers_v1_tex.dds",
    L"IslandScene\\grass_guide_v3_tex.dds",
};

// List of tree matrices for instancing.  Trees and islands will be placed randomly throughout the scene.
#define MAX_TREE_INSTANCES 50
D3DXMATRIX g_treeInstanceMatrices[MAX_TREE_INSTANCES] =
{
    //center the very first tree
    D3DXMATRIX( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 ),
};

// Direct3D 10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect = NULL;
ID3D10RasterizerState*              g_pRasterState = NULL;

ID3D10InputLayout*                  g_pInstVertexLayout = NULL;
ID3D10InputLayout*                  g_pSkyVertexLayout = NULL;
ID3D10InputLayout*                  g_pLeafVertexLayout = NULL;

CDXUTSDKMesh                        g_MeshSkybox;
CDXUTSDKMesh                        g_MeshIsland;
CDXUTSDKMesh                        g_MeshIslandTop;
CDXUTSDKMesh                        g_MeshTree;
ID3D10Buffer*                       g_pLeafInstanceData = NULL;
DWORD                               g_dwNumLeaves;
QUAD_MESH                           g_MeshLeaf;

ID3D10Texture2D*                    g_pGrassTexture;
ID3D10ShaderResourceView*           g_pGrassTexRV;

ID3D10Texture1D*                    g_pRandomTexture = NULL;
ID3D10ShaderResourceView*           g_pRandomTexRV = NULL;
ID3D10Texture2D*                    g_pLeafTexture = NULL;
ID3D10ShaderResourceView*           g_pLeafTexRV = NULL;
ID3D10Texture2D*                    g_pRenderTarget = NULL;
ID3D10RenderTargetView*             g_pRTRV = NULL;
ID3D10Texture2D*                    g_pDSTarget = NULL;
ID3D10DepthStencilView*             g_pDSRV = NULL;
ID3D10Buffer*                       g_pTreeInstanceData = NULL;
D3DXMATRIX*                         g_pTreeInstanceList = NULL;
int                                 g_iNumTreeInstances;
int                                 g_iNumTreesToDraw = 1;//MAX_TREE_INSTANCES;
int                                 g_iGrassCoverage = 1;
float                               g_fGrassMessiness = 30.0f;
UINT                                g_MSAASampleCount = 16;
UINT                                g_BackBufferWidth;
UINT                                g_BackBufferHeight;
bool                                g_bAnimateCamera;   // Whether the camera movement is on

// Effect handles
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldView = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectShaderResourceVariable* g_pRandomTex = NULL;
ID3D10EffectShaderResourceVariable* g_pTextureArray = NULL;
ID3D10EffectMatrixVariable*         g_pmTreeMatrices = NULL;
ID3D10EffectScalarVariable*         g_piNumTrees = NULL;
ID3D10EffectScalarVariable*         g_pGrassWidth = NULL;
ID3D10EffectScalarVariable*         g_pGrassHeight = NULL;
ID3D10EffectScalarVariable*         g_piGrassCoverage = NULL;
ID3D10EffectScalarVariable*         g_pGrassMessiness = NULL;

ID3D10EffectTechnique*              g_pRenderInstancedVertLighting = NULL;
ID3D10EffectTechnique*              g_pRenderSkybox = NULL;
ID3D10EffectTechnique*              g_pRenderQuad = NULL;
ID3D10EffectTechnique*              g_pRenderGrass = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC                 -1
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               2
#define IDC_CHANGEDEVICE            3
#define IDC_NUMTREES_STATIC         4
#define IDC_NUMTREES                5
#define IDC_GRASSCOVERAGE_STATIC    6
#define IDC_GRASSCOVERAGE           7
#define IDC_GRASSMESSINESS_STATIC   8
#define IDC_GRASSMESSINESS          9
#define IDC_SAMPLE_COUNT           10
#define IDC_TOGGLEWARP             11

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
void UpdateLeafVertexLayout( ID3D10Device* pd3dDevice );
HRESULT RenderSceneGeometry( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj );
HRESULT RenderInstancedQuads( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj );
HRESULT CreateQuadMesh( ID3D10Device* pd3dDevice, QUAD_MESH* pMesh, float fWidth, float fHeight );
HRESULT LoadInstanceData( ID3D10Device* pd3dDevice, ID3D10Buffer** ppInstanceData, DWORD* pdwNumLeaves,
                          LPCTSTR szFileName );
HRESULT LoadTreeInstanceData( ID3D10Device* pd3dDevice, ID3D10Buffer** ppInstanceData, DWORD dwNumTrees );
HRESULT LoadTextureArray( ID3D10Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D10Texture2D** ppTex2D, ID3D10ShaderResourceView** ppSRV );
HRESULT CreateRenderTarget( ID3D10Device* pd3dDevice, UINT uiWidth, UINT uiHeight, UINT uiSampleCount,
                            UINT uiSampleQuality );
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice );
HRESULT CreateRandomTreeMatrices();


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
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10SwapChainReleasing );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Instancing" );
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
    g_SampleUI.SetCallback( OnGUIEvent );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    iY += 50;
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Trees: %d", g_iNumTreesToDraw );
    g_HUD.AddStatic( IDC_NUMTREES_STATIC, str, 25, iY += 24, 135, 22 );
    g_HUD.AddSlider( IDC_NUMTREES, 35, iY += 24, 135, 22, 0, 20, g_iNumTreesToDraw );

    swprintf_s( str, MAX_PATH, L"Grass Coverage: %d", g_iGrassCoverage );
    g_HUD.AddStatic( IDC_GRASSCOVERAGE_STATIC, str, 25, iY += 24, 135, 22 );
    g_HUD.AddSlider( IDC_GRASSCOVERAGE, 35, iY += 24, 135, 22, 0, 50, g_iGrassCoverage );

    swprintf_s( str, MAX_PATH, L"Grass Messiness: %f", g_fGrassMessiness );
    g_HUD.AddStatic( IDC_GRASSMESSINESS_STATIC, str, 20, iY += 24, 140, 22 );
    g_HUD.AddSlider( IDC_GRASSMESSINESS, 35, iY += 24, 135, 22, 0, 2000, ( int )( g_fGrassMessiness * 25 ) );

    g_HUD.AddStatic( IDC_STATIC, L"MSAA (S)amples", 20, iY += 24, 105, 25 );
    CDXUTComboBox* pComboBox = NULL;
    g_HUD.AddComboBox( IDC_SAMPLE_COUNT, 20, iY += 24, 140, 24, 'S', false, &pComboBox );
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
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR str[MAX_PATH] = {0};

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
        case IDC_NUMTREES:
        {
            g_iNumTreesToDraw = g_HUD.GetSlider( IDC_NUMTREES )->GetValue();

            swprintf_s( str, MAX_PATH, L"Trees: %d", g_iNumTreesToDraw );
            g_HUD.GetStatic( IDC_NUMTREES_STATIC )->SetText( str );

            UpdateLeafVertexLayout( DXUTGetD3D10Device() );
            break;
        }
        case IDC_GRASSCOVERAGE:
        {
            g_iGrassCoverage = g_HUD.GetSlider( IDC_GRASSCOVERAGE )->GetValue();

            swprintf_s( str, MAX_PATH, L"Grass Coverage: %d", g_iGrassCoverage );
            g_HUD.GetStatic( IDC_GRASSCOVERAGE_STATIC )->SetText( str );
            break;
        }
        case IDC_GRASSMESSINESS:
        {
            g_fGrassMessiness = g_HUD.GetSlider( IDC_GRASSMESSINESS )->GetValue() / ( float )25.0;
            g_pGrassMessiness->SetFloat( g_fGrassMessiness );

            swprintf_s( str, MAX_PATH, L"Grass Messiness: %f", g_fGrassMessiness );
            g_HUD.GetStatic( IDC_GRASSMESSINESS_STATIC )->SetText( str );
            break;
        }

        case IDC_SAMPLE_COUNT:
        {
            CDXUTComboBox* pComboBox = ( CDXUTComboBox* )pControl;

            g_MSAASampleCount = ( UINT )PtrToInt( pComboBox->GetSelectedData() );

            HRESULT hr = S_OK;
            ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
            if( pd3dDevice )
                V( CreateRenderTarget( pd3dDevice, g_BackBufferWidth, g_BackBufferHeight, g_MSAASampleCount, 0 ) );

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

    // Set us up for multisampling
    D3D10_RASTERIZER_DESC CurrentRasterizerState;
    CurrentRasterizerState.FillMode = D3D10_FILL_SOLID;
    CurrentRasterizerState.CullMode = D3D10_CULL_FRONT;
    CurrentRasterizerState.FrontCounterClockwise = true;
    CurrentRasterizerState.DepthBias = false;
    CurrentRasterizerState.DepthBiasClamp = 0;
    CurrentRasterizerState.SlopeScaledDepthBias = 0;
    CurrentRasterizerState.DepthClipEnable = true;
    CurrentRasterizerState.ScissorEnable = false;
    CurrentRasterizerState.MultisampleEnable = true;
    CurrentRasterizerState.AntialiasedLineEnable = false;
    V_RETURN( pd3dDevice->CreateRasterizerState( &CurrentRasterizerState, &g_pRasterState ) );
    pd3dDevice->RSSetState( g_pRasterState );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Instancing.fx" ) );
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
    g_pRenderInstancedVertLighting = g_pEffect->GetTechniqueByName( "RenderInstancedVertLighting" );
    g_pRenderSkybox = g_pEffect->GetTechniqueByName( "RenderSkybox" );
    g_pRenderQuad = g_pEffect->GetTechniqueByName( "RenderQuad" );
    g_pRenderGrass = g_pEffect->GetTechniqueByName( "RenderGrass" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pDiffuseTex = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pTextureArray = g_pEffect->GetVariableByName( "g_tx2dArray" )->AsShaderResource();
    g_pRandomTex = g_pEffect->GetVariableByName( "g_txRandom" )->AsShaderResource();
    g_pmTreeMatrices = g_pEffect->GetVariableByName( "g_mTreeMatrices" )->AsMatrix();
    g_piNumTrees = g_pEffect->GetVariableByName( "g_iNumTrees" )->AsScalar();
    g_pGrassWidth = g_pEffect->GetVariableByName( "g_GrassWidth" )->AsScalar();
    g_pGrassHeight = g_pEffect->GetVariableByName( "g_GrassHeight" )->AsScalar();
    g_piGrassCoverage = g_pEffect->GetVariableByName( "g_iGrassCoverage" )->AsScalar();
    g_pGrassMessiness = g_pEffect->GetVariableByName( "g_GrassMessiness" )->AsScalar();

    g_pGrassMessiness->SetFloat( g_fGrassMessiness );

    // Define our instanced vertex data layout
    const D3D10_INPUT_ELEMENT_DESC instlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "mTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
    };
    int iNumElements = sizeof( instlayout ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    D3D10_PASS_DESC PassDesc;
    ID3D10EffectPass* pPass = g_pRenderInstancedVertLighting->GetPassByIndex( 0 );
    pPass->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( instlayout, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pInstVertexLayout ) );

    g_iNumTreeInstances = MAX_TREE_INSTANCES;
    g_pTreeInstanceList = g_treeInstanceMatrices;

    // We use a special function to update our leaf layout to make sure that we can modify it to match the number of trees we're drawing
    UpdateLeafVertexLayout( pd3dDevice );

    // Define our scene vertex layout
    const D3D10_INPUT_ELEMENT_DESC scenelayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    iNumElements = sizeof( scenelayout ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    pPass = g_pRenderSkybox->GetPassByIndex( 0 );
    pPass->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( scenelayout, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSkyVertexLayout ) );

    // Load meshes
    g_MeshSkybox.Create( pd3dDevice, L"CloudBox\\skysphere.sdkmesh", true );
    g_MeshIsland.Create( pd3dDevice, L"IslandScene\\island.sdkmesh", true );
    g_MeshIslandTop.Create( pd3dDevice, L"IslandScene\\islandtop_opt.sdkmesh", true );
    g_MeshTree.Create( pd3dDevice, L"trees\\tree.sdkmesh", true );

    // Create some random tree matrices
    CreateRandomTreeMatrices();

    // Load our leaf instance data
    V_RETURN( LoadInstanceData( pd3dDevice, &g_pLeafInstanceData, &g_dwNumLeaves, g_szTreeLeafInstanceNames[0] ) );

    // Get the list of tree instances
    V_RETURN( LoadTreeInstanceData( pd3dDevice, &g_pTreeInstanceData, g_iNumTreeInstances ) );

    // Create a leaf mesh consisting of 4 verts to be instanced all over the trees
    V_RETURN( CreateQuadMesh( pd3dDevice, &g_MeshLeaf, 80.0f, 80.0f ) );

    // Load the leaf array textures we'll be using
    V_RETURN( LoadTextureArray( pd3dDevice, g_szLeafTextureNames, sizeof( g_szLeafTextureNames ) / sizeof
                                ( g_szLeafTextureNames[0] ),
                                &g_pLeafTexture, &g_pLeafTexRV ) );

    // Load the grass texture arrays
    V_RETURN( LoadTextureArray( pd3dDevice, g_szGrassTextureNames, sizeof( g_szGrassTextureNames ) / sizeof
                                ( g_szGrassTextureNames[0] ),
                                &g_pGrassTexture, &g_pGrassTexRV ) );

    // Create the random texture that fuels our random vector generator in the effect
    V_RETURN( CreateRandomTexture( pd3dDevice ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vEyeStart( 100, 400, 2000 );
    D3DXVECTOR3 vAtStart( 0, 0, -2000 );
    g_Camera.SetViewParams( &vEyeStart, &vAtStart );

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

    pComboBox = g_HUD.GetComboBox( IDC_SAMPLE_COUNT );
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
    g_Camera.SetProjParams( 53.4f * ( 3.14159f / 180.0f ), fAspectRatio, 20.0f, 30000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    // Update the sample count
    UpdateMSAASampleCounts( pd3dDevice, pBackBufferSurfaceDesc->Format );

    // Create a multi-sample render target
    g_BackBufferWidth = pBackBufferSurfaceDesc->Width;
    g_BackBufferHeight = pBackBufferSurfaceDesc->Height;
    V_RETURN( CreateRenderTarget( pd3dDevice, g_BackBufferWidth, g_BackBufferHeight, g_MSAASampleCount, 0 ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT RenderSceneGeometry( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Set IA parameters
    pd3dDevice->IASetInputLayout( g_pSkyVertexLayout );

    // Render the skybox
    D3DXMATRIX mViewSkybox = mView;
    mViewSkybox._41 = 0.0f;
    mViewSkybox._42 = 0.0f;
    mViewSkybox._43 = 0.0f;
    D3DXMatrixMultiply( &mWorldViewProj, &mViewSkybox, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mViewSkybox );

    g_MeshSkybox.Render( pd3dDevice, g_pRenderSkybox, g_pDiffuseTex );

    // set us up for instanced rendering
    pd3dDevice->IASetInputLayout( g_pInstVertexLayout );
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3D10EffectTechnique* pTechnique = g_pRenderInstancedVertLighting;
    ID3D10Buffer* pVB[2];
    UINT Strides[2];
    UINT Offsets[2] = {0,0};

    // Render the island instanced
    pVB[0] = g_MeshIsland.GetVB10( 0, 0 );
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshIsland.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( D3DXMATRIX );

    pd3dDevice->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_MeshIsland.GetIB10( 0 ), g_MeshIsland.GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshIsland.GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_MeshIsland.GetSubset( 0, subset );

            PrimType = g_MeshIsland.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pMat = g_MeshIsland.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV10 );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

    // Render the tree instanced
    pVB[0] = g_MeshTree.GetVB10( 0, 0 );
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshTree.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( D3DXMATRIX );

    pd3dDevice->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_MeshTree.GetIB10( 0 ), g_MeshTree.GetIBFormat10( 0 ), 0 );

    pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshTree.GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_MeshTree.GetSubset( 0, subset );

            PrimType = g_MeshTree.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pMat = g_MeshTree.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV10 );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT RenderInstancedQuads( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Init Input Assembler states
    UINT strides[2] = { g_MeshLeaf.uStride, sizeof( INSTANCEDATA ) };
    UINT offsets[2] = {0, 0};

    // Draw leaves for all trees
    pd3dDevice->IASetInputLayout( g_pLeafVertexLayout );

    g_pmTreeMatrices->SetMatrixArray( ( float* )g_pTreeInstanceList, 0, g_iNumTreesToDraw );
    g_piNumTrees->SetInt( g_iNumTreesToDraw );
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3D10Buffer* pBuffers[2] =
    {
        g_MeshLeaf.pVB, g_pLeafInstanceData
    };
    pd3dDevice->IASetVertexBuffers( 0, 2, pBuffers, strides, offsets );
    pd3dDevice->IASetIndexBuffer( g_MeshLeaf.pIB, DXGI_FORMAT_R16_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderQuad->GetDesc( &techDesc );
    g_pTextureArray->SetResource( g_pLeafTexRV );
    for( UINT iPass = 0; iPass < techDesc.Passes; iPass++ )
    {
        g_pRenderQuad->GetPassByIndex( iPass )->Apply( 0 );
        pd3dDevice->DrawIndexedInstanced( g_MeshLeaf.dwNumIndices, g_dwNumLeaves * g_iNumTreesToDraw, 0, 0, 0 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
void RenderGrass( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    // Draw Grass
    D3DXMATRIX mIdentity;
    D3DXMATRIX mWorldViewProj;
    D3DXMatrixIdentity( &mIdentity );
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );
    g_pRandomTex->SetResource( g_pRandomTexRV );

    int iGrassCoverage = g_iGrassCoverage;

    g_pGrassWidth->SetFloat( 50.0f );
    g_pGrassHeight->SetFloat( 50.0f );
    g_pTextureArray->SetResource( g_pGrassTexRV );
    g_piGrassCoverage->SetInt( iGrassCoverage );

    // set us up for instanced rendering
    pd3dDevice->IASetInputLayout( g_pInstVertexLayout );
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3D10EffectTechnique* pTechnique = g_pRenderGrass;
    ID3D10Buffer* pVB[2];
    UINT Strides[2];
    UINT Offsets[2] = {0,0};

    // Render the island tops instanced
    pVB[0] = g_MeshIslandTop.GetVB10( 0, 0 );
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshIslandTop.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( D3DXMATRIX );

    pd3dDevice->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_MeshIslandTop.GetIB10( 0 ), g_MeshIslandTop.GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshIslandTop.GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_MeshIslandTop.GetSubset( 0, subset );

            PrimType = g_MeshIslandTop.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pMat = g_MeshIslandTop.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV10 );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }
}


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
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Set our render target since we can't present multisampled ref
    ID3D10RenderTargetView* pOrigRT;
    ID3D10DepthStencilView* pOrigDS;
    pd3dDevice->OMGetRenderTargets( 1, &pOrigRT, &pOrigDS );

    ID3D10RenderTargetView* aRTViews[ 1 ] = { g_pRTRV };
    pd3dDevice->OMSetRenderTargets( 1, aRTViews, g_pDSRV );

    // Clear the render target and DSV
    pd3dDevice->ClearRenderTargetView( g_pRTRV, ClearColor );
    pd3dDevice->ClearDepthStencilView( g_pDSRV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    RenderSceneGeometry( pd3dDevice, mView, mProj ); // Render the scene with texture    
    RenderInstancedQuads( pd3dDevice, mView, mProj ); // Render the instanced leaves for each tree
    RenderGrass( pd3dDevice, mView, mProj ); // Render grass

    // Copy it over because we can't resolve on present at the moment
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
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Update the vertex layout for the number of leaves based upon the number
// of trees we're drawing.  This is to help us efficiently render leaves using
// instancing across the tree mesh and the leaf points on the trees.
//--------------------------------------------------------------------------------------
void UpdateLeafVertexLayout( ID3D10Device* pd3dDevice )
{
    SAFE_RELEASE( g_pLeafVertexLayout );

    // Define our leaf vertex data layout
    const D3D10_INPUT_ELEMENT_DESC leaflayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "mTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D10_INPUT_PER_INSTANCE_DATA,
            g_iNumTreesToDraw },
        { "mTransform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D10_INPUT_PER_INSTANCE_DATA,
            g_iNumTreesToDraw },
        { "mTransform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D10_INPUT_PER_INSTANCE_DATA,
            g_iNumTreesToDraw },
        { "mTransform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D10_INPUT_PER_INSTANCE_DATA,
            g_iNumTreesToDraw },
        { "fOcc",       0, DXGI_FORMAT_R32_FLOAT,          1, 64, D3D10_INPUT_PER_INSTANCE_DATA,
            g_iNumTreesToDraw },
    };
    int iNumElements = sizeof( leaflayout ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    D3D10_PASS_DESC PassDesc;
    g_pRenderQuad->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    pd3dDevice->CreateInputLayout( leaflayout, iNumElements, PassDesc.pIAInputSignature,
                                   PassDesc.IAInputSignatureSize, &g_pLeafVertexLayout );
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_RELEASE( g_pRTRV );
    SAFE_RELEASE( g_pDSRV );
    SAFE_RELEASE( g_pDSTarget );
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
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pInstVertexLayout );
    SAFE_RELEASE( g_pSkyVertexLayout );
    SAFE_RELEASE( g_pLeafVertexLayout );

    SAFE_RELEASE( g_pLeafInstanceData );
    SAFE_RELEASE( g_pLeafTexture );
    SAFE_RELEASE( g_pLeafTexRV );
    SAFE_RELEASE( g_pTreeInstanceData );

    SAFE_RELEASE( g_pRenderTarget );
    SAFE_RELEASE( g_pRTRV );
    SAFE_RELEASE( g_pDSTarget );
    SAFE_RELEASE( g_pDSRV );

    SAFE_RELEASE( g_pGrassTexture );
    SAFE_RELEASE( g_pGrassTexRV );
    SAFE_RELEASE( g_pRandomTexture );
    SAFE_RELEASE( g_pRandomTexRV );

    SAFE_RELEASE( g_MeshLeaf.pVB );
    SAFE_RELEASE( g_MeshLeaf.pIB );

    g_MeshSkybox.Destroy();
    g_MeshIsland.Destroy();
    g_MeshIslandTop.Destroy();
    g_MeshTree.Destroy();

    SAFE_RELEASE( g_pRasterState );
}


//--------------------------------------------------------------------------------------
HRESULT CreateQuadMesh( ID3D10Device* pd3dDevice, QUAD_MESH* pMesh, float fWidth, float fHeight )
{
    HRESULT hr = S_OK;

    pMesh->dwNumVerts = 4;
    pMesh->dwNumIndices = 12;
    pMesh->uStride = sizeof( QUAD_VERTEX );

    /********************************
      leaves are quads anchored on the branch
      
      |---------|
      |D       C|
      |         |
      |         |
      |A       B|
      |---------|
      O<-----branch
      
     ********************************/
    fWidth /= 2.0f;
    QUAD_VERTEX quadVertices[] =
    {
        { D3DXVECTOR3( -fWidth, 0.0f, 0.0f ), D3DXVECTOR2( 0, 1 ) },
        { D3DXVECTOR3( fWidth, 0.0f, 0.0f ), D3DXVECTOR2( 1, 1 ) },
        { D3DXVECTOR3( fWidth, fHeight, 0.0f ), D3DXVECTOR2( 1, 0 ) },
        { D3DXVECTOR3( -fWidth, fHeight, 0.0f ), D3DXVECTOR2( 0, 0 ) },
    };

    // Create a resource with the input vertices
    // Create it with usage default, because it will never change
    D3D10_BUFFER_DESC bufferDesc =
    {
        pMesh->dwNumVerts * sizeof( QUAD_VERTEX ),
        D3D10_USAGE_DEFAULT, D3D10_BIND_VERTEX_BUFFER, 0, 0
    };
    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );
    vbInitData.pSysMem = quadVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &pMesh->pVB ) );

    // Create the index buffer
    bufferDesc.ByteWidth = pMesh->dwNumIndices * sizeof( WORD );
    bufferDesc.Usage = D3D10_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    WORD wIndices[] =
    {
        0,1,2,
        0,2,3,
        0,2,1,
        0,3,2,
    };
    D3D10_SUBRESOURCE_DATA ibInitData;
    ZeroMemory( &ibInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );
    ibInitData.pSysMem = wIndices;

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &ibInitData, &pMesh->pIB ) );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT LoadInstanceData( ID3D10Device* pd3dDevice, ID3D10Buffer** ppInstanceData, DWORD* pdwNumLeaves,
                          LPCTSTR szFileName )
{
    HRESULT hr = S_OK;

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    *pdwNumLeaves = 0;
    HANDLE hFile = CreateFile( str, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pdwNumLeaves, sizeof( DWORD ), &dwBytesRead, NULL ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Make sure we have some leaves
    if( ( *pdwNumLeaves ) == 0 )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Create a resource with the input matrices
    // We're creating this buffer as dynamic because in a game, the instance data could be dynamic... aka
    // we could have moving leaves.
    D3D10_BUFFER_DESC bufferDesc =
    {
        ( *pdwNumLeaves ) * sizeof( INSTANCEDATA ),
        D3D10_USAGE_DYNAMIC,
        D3D10_BIND_VERTEX_BUFFER,
        D3D10_CPU_ACCESS_WRITE,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, NULL, ppInstanceData ) );

    INSTANCEDATA* pInstances = NULL;
    if( FAILED( ( *ppInstanceData )->Map( D3D10_MAP_WRITE_DISCARD, NULL, ( void** )&pInstances ) ) || !pInstances )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    if( !ReadFile( hFile, pInstances, sizeof( INSTANCEDATA ) * ( *pdwNumLeaves ), &dwBytesRead, NULL ) )
    {
        CloseHandle( hFile );
        ( *ppInstanceData )->Unmap();
        return E_FAIL;
    }

    ( *ppInstanceData )->Unmap();

    CloseHandle( hFile );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT LoadTreeInstanceData( ID3D10Device* pd3dDevice, ID3D10Buffer** ppInstanceData, DWORD dwNumTrees )
{
    HRESULT hr = S_OK;

    // Create a resource with the input matrices
    // We're creating this buffer as dynamic because in a game, the instance data could be dynamic... aka
    // we could have moving trees.
    D3D10_BUFFER_DESC bufferDesc =
    {
        dwNumTrees * sizeof( D3DXMATRIX ),
        D3D10_USAGE_DYNAMIC,
        D3D10_BIND_VERTEX_BUFFER,
        D3D10_CPU_ACCESS_WRITE,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, NULL, ppInstanceData ) );

    D3DXMATRIX* pMatrices = NULL;
    ( *ppInstanceData )->Map( D3D10_MAP_WRITE_DISCARD, NULL, ( void** )&pMatrices );

    memcpy( pMatrices, g_pTreeInstanceList, dwNumTrees * sizeof( D3DXMATRIX ) );

    ( *ppInstanceData )->Unmap();

    return hr;
}


//--------------------------------------------------------------------------------------
// LoadTextureArray loads a texture array and associated view from a series
// of textures on disk.
//--------------------------------------------------------------------------------------
HRESULT LoadTextureArray( ID3D10Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D10Texture2D** ppTex2D, ID3D10ShaderResourceView** ppSRV )
{
    HRESULT hr = S_OK;
    D3D10_TEXTURE2D_DESC desc;
    ZeroMemory( &desc, sizeof( D3D10_TEXTURE2D_DESC ) );

    WCHAR str[MAX_PATH];
    for( int i = 0; i < iNumTextures; i++ )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szTextureNames[i] ) );

        ID3D10Resource* pRes = NULL;
        D3DX10_IMAGE_LOAD_INFO loadInfo;
        ZeroMemory( &loadInfo, sizeof( D3DX10_IMAGE_LOAD_INFO ) );
        loadInfo.Width = D3DX_FROM_FILE;
        loadInfo.Height = D3DX_FROM_FILE;
        loadInfo.Depth = D3DX_FROM_FILE;
        loadInfo.FirstMipLevel = 0;
        loadInfo.MipLevels = 1;
        loadInfo.Usage = D3D10_USAGE_STAGING;
        loadInfo.BindFlags = 0;
        loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;
        loadInfo.MiscFlags = 0;
        loadInfo.Format = MAKE_TYPELESS( DXGI_FORMAT_R8G8B8A8_UNORM );
        loadInfo.Filter = D3DX10_FILTER_NONE;
        loadInfo.MipFilter = D3DX10_FILTER_NONE;

        V_RETURN( D3DX10CreateTextureFromFile( pd3dDevice, str, &loadInfo, NULL, &pRes, NULL ) );
        if( pRes )
        {
            ID3D10Texture2D* pTemp;
            pRes->QueryInterface( __uuidof( ID3D10Texture2D ), ( LPVOID* )&pTemp );
            pTemp->GetDesc( &desc );

            D3D10_MAPPED_TEXTURE2D mappedTex2D;

            if( desc.MipLevels > 4 )
                desc.MipLevels -= 4;
            if( !( *ppTex2D ) )
            {
                desc.Usage = D3D10_USAGE_DEFAULT;
                desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
                desc.CPUAccessFlags = 0;
                desc.ArraySize = iNumTextures;
                V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, ppTex2D ) );
            }

            for( UINT iMip = 0; iMip < desc.MipLevels; iMip++ )
            {
                pTemp->Map( iMip, D3D10_MAP_READ, 0, &mappedTex2D );

                pd3dDevice->UpdateSubresource( ( *ppTex2D ),
                                               D3D10CalcSubresource( iMip, i, desc.MipLevels ),
                                               NULL,
                                               mappedTex2D.pData,
                                               mappedTex2D.RowPitch,
                                               0 );

                pTemp->Unmap( iMip );
            }

            SAFE_RELEASE( pRes );
            SAFE_RELEASE( pTemp );
        }
        else
        {
            return false;
        }
    }

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = MAKE_SRGB( DXGI_FORMAT_R8G8B8A8_UNORM );
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
    SRVDesc.Texture2DArray.ArraySize = iNumTextures;
    V_RETURN( pd3dDevice->CreateShaderResourceView( *ppTex2D, &SRVDesc, ppSRV ) );

    return hr;
}


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

    // Create depth stencil texture.
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


//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors.  The shader uses
// the current time value to index into this texture to get a random vector.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    int iNumRandValues = 1024;
    srand( 0 );
    //create the data
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new float[iNumRandValues * 4];
    if( !InitData.pSysMem )
        return E_OUTOFMEMORY;
    InitData.SysMemPitch = iNumRandValues * 4 * sizeof( float );
    InitData.SysMemSlicePitch = iNumRandValues * 4 * sizeof( float );
    for( int i = 0; i < iNumRandValues * 4; i++ )
    {
        ( ( float* )InitData.pSysMem )[i] = float( ( rand() % 10000 ) - 5000 );
    }

    // Create the texture
    D3D10_TEXTURE1D_DESC dstex;
    dstex.Width = iNumRandValues;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateTexture1D( &dstex, &InitData, &g_pRandomTexture ) );
    SAFE_DELETE_ARRAY( InitData.pSysMem );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
    SRVDesc.Texture1D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomTexture, &SRVDesc, &g_pRandomTexRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = ( float )( ( rand() % 20000 ) - 10000 );
    return ret / 10000.0f;
}

//--------------------------------------------------------------------------------------
// This helper function creates a bunch of random tree matrices to show off
// instancing the same tree and island multiple times.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTreeMatrices()
{
    srand( 100 );	//use the same random seed every time so we can verify output
    float fScale = 100.0f;

    for( int i = 1; i < MAX_TREE_INSTANCES; i++ )
    {
        //find a random position
        D3DXVECTOR3 pos;
        pos.x = RPercent() * 140.0f;
        pos.y = RPercent() * 20.0f - 10.0f;
        pos.z = 15.0f + fabs( RPercent() ) * 200.0f;

        pos.x *= -1;
        pos.z *= -1;

        pos *= fScale;

        float fRot = RPercent() * D3DX_PI;

        D3DXMATRIX mTrans;
        D3DXMATRIX mRot;
        D3DXMatrixRotationY( &mRot, fRot );
        D3DXMatrixTranslation( &mTrans, pos.x, pos.y, pos.z );

        g_treeInstanceMatrices[i] = mRot * mTrans;
    }
    return S_OK;
}
