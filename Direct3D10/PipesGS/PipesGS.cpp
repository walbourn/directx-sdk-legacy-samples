//--------------------------------------------------------------------------------------
// File: PipesGS.cpp
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

LPCTSTR g_szPipeTextures[] =
{
    L"Crypt\\pipetex.dds",
    L"Crypt\\leaf1.dds",
    L"Crypt\\leaf2.dds",
    L"Crypt\\leaf3.dds",
    L"Crypt\\leaf4.dds",
    L"Crypt\\leaf5.dds"
};

LPCTSTR g_szSkyTextures[] =
{
    L"NightBox\\sky_bot.dds",
    L"NightBox\\sky_top.dds",
    L"NightBox\\sky_side.dds",
    L"NightBox\\sky_side.dds",
    L"NightBox\\sky_side.dds",
    L"NightBox\\sky_side.dds"
};

struct PIPE_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR3 dir;
    D3DXVECTOR2 timerNType;
    D3DXVECTOR3 targetdir;
    UINT currentface;
    UINT leaves;
    float pipelife;
};

#define MAX_PIPE_VERTS 30000


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
UINT                                g_numSpawnPoints = 400; // number of spawn points for pipes
float                               g_fLifeSpan = 140.0f;   // how long our pipe sections live
float                               g_fLifeSpanVar = 40.0f;
float                               g_fRadiusMin = 0.01f;
float                               g_fRadiusMax = 0.07f;
float                               g_fGrowTime = 60.0f;
float                               g_fStepSize = 0.02f;
float                               g_fTurnRate = 0.5f;
float                               g_fLeafRate = 0.3f;
float                               g_fTurnSpeed = 0.05f;
float                               g_fShrinkTime = 60.0f;
bool                                g_bCrawMethod = false;
bool                                g_bFirst = true;

ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;

ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10InputLayout*                  g_pMeshVertexLayout = NULL;

// mesh to grow on
CDXUTSDKMesh                        g_MeshSkybox;
CDXUTSDKMesh                        g_MeshScene;

// Buffers for streaming Pipes
ID3D10Buffer*                       g_pStart = NULL;
ID3D10Buffer*                       g_pStreamTo = NULL;
ID3D10Buffer*                       g_pDrawFrom = NULL;
ID3D10Buffer*                       g_pAdjBuffer = NULL;
ID3D10ShaderResourceView*           g_pAdjBufferRV = NULL;
ID3D10Buffer*                       g_pTriCenterBuffer = NULL;
ID3D10ShaderResourceView*           g_pTriCenterBufferRV = NULL;

ID3D10Texture1D*                    g_pRandomTexture = NULL;
ID3D10ShaderResourceView*           g_pRandomTexRV = NULL;
ID3D10Texture2D*                    g_pPipeTexture = NULL;
ID3D10ShaderResourceView*           g_pPipeTexRV = NULL;
ID3D10Texture2D*                    g_pSkyTexture = NULL;
ID3D10ShaderResourceView*           g_pSkyTexRV = NULL;

// Technique handles
ID3D10EffectTechnique*              g_pRenderPipes = NULL;
ID3D10EffectTechnique*              g_pAdvancePipes = NULL;
ID3D10EffectTechnique*              g_pAdvancePipesCrawl = NULL;
ID3D10EffectTechnique*              g_pRenderMesh = NULL;
ID3D10EffectTechnique*              g_pRenderSkybox = NULL;

// Effect Param handles
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectShaderResourceVariable* g_pTextureArray = NULL;
ID3D10EffectShaderResourceVariable* g_pRandomTex = NULL;
ID3D10EffectShaderResourceVariable* g_pAdjBufferVar = NULL;
ID3D10EffectShaderResourceVariable* g_pTriCenterBufferVar = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectScalarVariable*         g_pfGlobalTime = NULL;
ID3D10EffectScalarVariable*         g_pLifeSpan = NULL;
ID3D10EffectScalarVariable*         g_pLifeSpanVar = NULL;
ID3D10EffectScalarVariable*         g_pRadiusMin = NULL;
ID3D10EffectScalarVariable*         g_pRadiusMax = NULL;
ID3D10EffectScalarVariable*         g_pGrowTime = NULL;
ID3D10EffectScalarVariable*         g_pStepSize = NULL;
ID3D10EffectScalarVariable*         g_pTurnRate = NULL;
ID3D10EffectScalarVariable*         g_pLeafRate = NULL;
ID3D10EffectScalarVariable*         g_pTurnSpeed = NULL;
ID3D10EffectScalarVariable*         g_pShrinkTime = NULL;
ID3D10EffectScalarVariable*         g_pMaxFaces = NULL;

float* g_pdwAdjF;
DWORD* g_pdwAdj;
D3DXVECTOR4* g_pVerts;
DWORD g_dwNumFaces; 

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWARP          5

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
void DeinitApp();
void RenderText();
HRESULT CreatePipeBuffer( ID3D10Device* pd3dDevice, UINT uiNumSpawnPoints, UINT uiMaxFaces );
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice );
HRESULT GenerateTriCenterBuffer( ID3D10Device* pd3dDevice );
HRESULT LoadTextureArray( ID3D10Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D10Texture2D** ppTex2D, ID3D10ShaderResourceView** ppSRV );


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
    DXUTCreateWindow( L"PipesGS" );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop
    DeinitApp();
    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Deinitialize the app 
//--------------------------------------------------------------------------------------


void DeinitApp () 
{
    SAFE_DELETE_ARRAY( g_pVerts );
    SAFE_DELETE_ARRAY( g_pdwAdj );
    SAFE_DELETE_ARRAY( g_pdwAdjF );
    
}



HRESULT LoadD3D9ResourcesBeforeDeviceWorkAround () 
{


    ID3DXBuffer* pMat = NULL;
    ID3DXMesh* pRawMesh = NULL;
    ID3DXMesh* pMesh = NULL;
    ID3DXBuffer* pAdj = NULL;
    DWORD cMat;

    HRESULT hr;
    WCHAR szMesh[260];
    V_RETURN( DXUTFindDXSDKMediaFileCch( szMesh, MAX_PATH, L"Crypt\\vinestart.x" ) );
    IDirect3DDevice9* pDev9 = NULL;


    // Create a d3d9 device
    LPDIRECT3D9 pD3D9 = Direct3DCreate9( D3D_SDK_VERSION );
    if( pD3D9 == NULL )
        return E_FAIL;

    D3DPRESENT_PARAMETERS pp;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetShellWindow();
    pp.Windowed = true;
    pp.Flags = 0;
    pp.FullScreen_RefreshRateInHz = 0;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    pp.EnableAutoDepthStencil = false;

    hr = pD3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                              D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9 );
    if( FAILED( hr ) )
    {
        hr = pD3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, NULL,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9 );
        if( FAILED( hr ) )
            return E_FAIL;
    }

    if( FAILED( D3DXLoadMeshFromX( szMesh, 0, pDev9, &pAdj, &pMat, NULL, &cMat, &pRawMesh ) ) )
        return E_FAIL;

    if( SUCCEEDED( pRawMesh->CloneMeshFVF( D3DXMESH_32BIT, D3DFVF_XYZ, pDev9, &pMesh ) ) )
    {
        SAFE_RELEASE( pRawMesh );

        g_pdwAdj = new DWORD[pMesh->GetNumFaces() * 3];
        if( !g_pdwAdj ) return E_OUTOFMEMORY;
        pMesh->GenerateAdjacency( 1e-5f, g_pdwAdj );
        g_pdwAdjF = new float[pMesh->GetNumFaces() * 3 ];
        if( !g_pdwAdjF ) return E_OUTOFMEMORY;

        for( UINT i = 0; i < pMesh->GetNumFaces(); i++ )
        {
            g_pdwAdjF[ i * 3 ] = ( float )g_pdwAdj[ i * 3 ];
            g_pdwAdjF[ i * 3 + 1 ] = ( float )g_pdwAdj[ i * 3 + 1 ];
            g_pdwAdjF[ i * 3 + 2 ] = ( float )g_pdwAdj[ i * 3 + 2 ];
        }

        // load vertices
        g_pVerts = new D3DXVECTOR4[ 2 * pMesh->GetNumFaces() ];
        if( !g_pVerts ) return E_OUTOFMEMORY;

        D3DXVECTOR3* pOrigVerts = NULL;
        DWORD* pdwOrigIndices = NULL;
        pMesh->LockVertexBuffer( 0, ( void** )&pOrigVerts );
        pMesh->LockIndexBuffer( 0, ( void** )&pdwOrigIndices );

        UINT index = 0;
        for( UINT i = 0; i < pMesh->GetNumFaces() * 3; i += 3 )
        {
            D3DXVECTOR3 AB = pOrigVerts[ pdwOrigIndices[i + 1] ] - pOrigVerts[ pdwOrigIndices[i] ];
            D3DXVECTOR3 AC = pOrigVerts[ pdwOrigIndices[i + 2] ] - pOrigVerts[ pdwOrigIndices[i] ];
            D3DXVECTOR3 N;
            D3DXVec3Cross( &N, &AB, &AC );
            D3DXVec3Normalize( &N, &N );

            D3DXVECTOR3 vCenter = ( pOrigVerts[ pdwOrigIndices[i] ] +
                                    pOrigVerts[ pdwOrigIndices[i + 1] ] +
                                    pOrigVerts[ pdwOrigIndices[i + 2] ] ) / 3.0f;
            g_pVerts[ index ].x = vCenter.x;
            g_pVerts[ index ].y = vCenter.y;
            g_pVerts[ index ].z = vCenter.z;
            g_pVerts[ index ].w = 0.0f;
            index ++;
            g_pVerts[ index ].x = N.x;
            g_pVerts[ index ].y = N.y;
            g_pVerts[ index ].z = N.z;
            g_pVerts[ index ].w = 0.0;
            index ++;
        }

        pMesh->UnlockVertexBuffer();
        pMesh->UnlockIndexBuffer();

    }
    g_dwNumFaces = pMesh->GetNumFaces();

    SAFE_RELEASE( pMat );
    SAFE_RELEASE( pRawMesh );
    SAFE_RELEASE( pMesh );
    SAFE_RELEASE( pAdj );

    SAFE_RELEASE( pDev9 );
    SAFE_RELEASE( pD3D9 );


    return S_OK;
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


    LoadD3D9ResourcesBeforeDeviceWorkAround ();
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PipesGS.fx" ) );
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
    g_pRenderPipes = g_pEffect10->GetTechniqueByName( "RenderPipes" );
    g_pAdvancePipes = g_pEffect10->GetTechniqueByName( "AdvancePipes" );
    g_pAdvancePipesCrawl = g_pEffect10->GetTechniqueByName( "AdvancePipesCrawl" );
    g_pRenderMesh = g_pEffect10->GetTechniqueByName( "RenderMesh" );
    g_pRenderSkybox = g_pEffect10->GetTechniqueByName( "RenderSkybox" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pfGlobalTime = g_pEffect10->GetVariableByName( "g_fGlobalTime" )->AsScalar();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pTextureArray = g_pEffect10->GetVariableByName( "g_tx2dArray" )->AsShaderResource();
    g_pRandomTex = g_pEffect10->GetVariableByName( "g_txRandom" )->AsShaderResource();
    g_pAdjBufferVar = g_pEffect10->GetVariableByName( "g_adjBuffer" )->AsShaderResource();
    g_pTriCenterBufferVar = g_pEffect10->GetVariableByName( "g_triCenterBuffer" )->AsShaderResource();
    g_pLifeSpan = g_pEffect10->GetVariableByName( "g_fLifeSpan" )->AsScalar();
    g_pLifeSpanVar = g_pEffect10->GetVariableByName( "g_fLifeSpanVar" )->AsScalar();
    g_pRadiusMin = g_pEffect10->GetVariableByName( "g_fRadiusMin" )->AsScalar();
    g_pRadiusMax = g_pEffect10->GetVariableByName( "g_fRadiusMax" )->AsScalar();
    g_pGrowTime = g_pEffect10->GetVariableByName( "g_fGrowTime" )->AsScalar();
    g_pStepSize = g_pEffect10->GetVariableByName( "g_fStepSize" )->AsScalar();
    g_pTurnRate = g_pEffect10->GetVariableByName( "g_fTurnRate" )->AsScalar();
    g_pLeafRate = g_pEffect10->GetVariableByName( "g_fLeafRate" )->AsScalar();
    g_pTurnSpeed = g_pEffect10->GetVariableByName( "g_fTurnSpeed" )->AsScalar();
    g_pShrinkTime = g_pEffect10->GetVariableByName( "g_fShrinkTime" )->AsScalar();
    g_pMaxFaces = g_pEffect10->GetVariableByName( "g_uMaxFaces" )->AsScalar();

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "DIRECTION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TIMERNTYPE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TARGETDIR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CURRENTFACE", 0, DXGI_FORMAT_R32_UINT, 0, 56, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "LEAVES", 0, DXGI_FORMAT_R32_UINT, 0, 60, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "PIPELIFE", 0, DXGI_FORMAT_R32_FLOAT, 0, 64, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pRenderPipes->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 8, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderMesh->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshVertexLayout ) );

    V_RETURN( g_MeshScene.Create( pd3dDevice, L"Crypt\\Crypt.sdkmesh", true ) );
    V_RETURN( g_MeshSkybox.Create( pd3dDevice, L"NightBox\\Cloud_skybox.sdkmesh", false ) );

    // Load the mesh we use to spawn vines
    V_RETURN( GenerateTriCenterBuffer( pd3dDevice ) );

    // Create the seeding pipe section
    V_RETURN( CreatePipeBuffer( pd3dDevice, g_numSpawnPoints, g_dwNumFaces ) );

    // Load the pipe Texture array
    V_RETURN( LoadTextureArray( pd3dDevice, g_szPipeTextures,
                                sizeof( g_szPipeTextures ) / sizeof( g_szPipeTextures[0] ),
                                &g_pPipeTexture, &g_pPipeTexRV ) );

    // Load the sky array texture
    V_RETURN( LoadTextureArray( pd3dDevice, g_szSkyTextures, 6,
                                &g_pSkyTexture, &g_pSkyTexRV ) );

    // Create the random texture that fuels our random vector generator in the effect
    V_RETURN( CreateRandomTexture( pd3dDevice ) );

    // Set Constant Effect Variables
    g_pLifeSpan->SetFloat( g_fLifeSpan );
    g_pLifeSpanVar->SetFloat( g_fLifeSpanVar );
    g_pRadiusMin->SetFloat( g_fRadiusMin );
    g_pRadiusMax->SetFloat( g_fRadiusMax );
    g_pGrowTime->SetFloat( g_fGrowTime );
    g_pStepSize->SetFloat( g_fStepSize );
    g_pTurnRate->SetFloat( g_fTurnRate );
    g_pLeafRate->SetFloat( g_fLeafRate );
    g_pTurnSpeed->SetFloat( g_fTurnSpeed );
    g_pShrinkTime->SetFloat( g_fShrinkTime );
    g_pMaxFaces->SetInt( g_dwNumFaces );
    g_pAdjBufferVar->SetResource( g_pAdjBufferRV );
    g_pTriCenterBufferVar->SetResource( g_pTriCenterBufferRV );
    g_pRandomTex->SetResource( g_pRandomTexRV );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -3.0f, 1.6f, -8.0f );
    D3DXVECTOR3 vecAt ( 1.0f, 1.2f, 0.0f );
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
    g_Camera.SetProjParams( D3DX_PI / 3, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    g_bFirst = true;

    return hr;
}


//--------------------------------------------------------------------------------------
bool AdvancePipes( ID3D10Device* pd3dDevice, float fGlobalTime, bool bCrawl )
{
    ID3D10EffectTechnique* pCurrentPipeTech = g_pAdvancePipes;
    if( bCrawl ) pCurrentPipeTech = g_pAdvancePipesCrawl;

    // Set IA parameters
    ID3D10Buffer* pBuffers[1];
    if( g_bFirst )
        pBuffers[0] = g_pStart;
    else
        pBuffers[0] = g_pDrawFrom;
    UINT stride[1] = { sizeof( PIPE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Point to the correct output buffer
    pBuffers[0] = g_pStreamTo;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    // Set Effects Parameters
    g_pfGlobalTime->SetFloat( fGlobalTime );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    pCurrentPipeTech->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pCurrentPipeTech->GetPassByIndex( p )->Apply( 0 );
        if( g_bFirst )
            pd3dDevice->Draw( g_numSpawnPoints, 0 );
        else
            pd3dDevice->DrawAuto();
    }

    // Get back to normal
    pBuffers[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    // Swap particle buffers
    ID3D10Buffer* pTemp = g_pDrawFrom;
    g_pDrawFrom = g_pStreamTo;
    g_pStreamTo = pTemp;

    g_bFirst = false;

    return true;
}


//--------------------------------------------------------------------------------------
bool RenderPipes( ID3D10Device* pd3dDevice )
{
    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pDrawFrom };
    UINT stride[1] = { sizeof( PIPE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP );

    // Set texture
    g_pDiffuseTex->SetResource( g_pPipeTexRV );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderPipes->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pRenderPipes->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawAuto();
    }

    return true;
}


//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // Clear the depth stencil
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
        pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Get the projection & view matrix from the camera class
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldViewProj;
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    // Set the Vertex Layout for meshes
    pd3dDevice->IASetInputLayout( g_pMeshVertexLayout );

    // Render the skybox
    D3DXMATRIX mViewSkybox = mView;
    mViewSkybox._41 = 0.0f;
    mViewSkybox._42 = 0.0f;
    mViewSkybox._43 = 0.0f;
    D3DXMatrixMultiply( &mWorldViewProj, &mViewSkybox, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pTextureArray->SetResource( g_pSkyTexRV );
    g_MeshSkybox.Render( pd3dDevice, g_pRenderSkybox, g_pDiffuseTex );

    // Render the Mesh
    g_pTextureArray->SetResource( g_pPipeTexRV );
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_MeshScene.Render( pd3dDevice, g_pRenderMesh, g_pDiffuseTex );

    // Set the vertex layout for pipes
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Advance the Pipes
    static float fGlobalTime = 0.0f;
    fElapsedTime = 1.0f;
    AdvancePipes( pd3dDevice, fGlobalTime, g_bCrawMethod );
    fGlobalTime += fElapsedTime;

    // Render the Pipes
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    RenderPipes( pd3dDevice );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
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
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pMeshVertexLayout );
    SAFE_RELEASE( g_pStart );
    SAFE_RELEASE( g_pStreamTo );
    SAFE_RELEASE( g_pDrawFrom );
    SAFE_RELEASE( g_pAdjBuffer );
    SAFE_RELEASE( g_pAdjBufferRV );
    SAFE_RELEASE( g_pTriCenterBuffer );
    SAFE_RELEASE( g_pTriCenterBufferRV );
    SAFE_RELEASE( g_pRandomTexture );
    SAFE_RELEASE( g_pRandomTexRV );
    SAFE_RELEASE( g_pPipeTexture );
    SAFE_RELEASE( g_pPipeTexRV );
    SAFE_RELEASE( g_pSkyTexture );
    SAFE_RELEASE( g_pSkyTexRV );

    g_MeshSkybox.Destroy();
    g_MeshScene.Destroy();
}


//--------------------------------------------------------------------------------------
// This helper function creates 3 vertex buffers.  The first is used to seed the
// pipe system.  The second two are used as output and intput buffers alternatively
// for the GS pipe system.  Since a buffer cannot be both an input to the GS and an
// output of the GS, we must ping-pong between the two.
//--------------------------------------------------------------------------------------
HRESULT CreatePipeBuffer( ID3D10Device* pd3dDevice, UINT uiNumSpawnPoints, UINT uiMaxFaces )
{
    HRESULT hr = S_OK;

    if( uiNumSpawnPoints < 1 )
        return E_INVALIDARG;

    D3D10_BUFFER_DESC vbdesc =
    {
        g_numSpawnPoints * sizeof( PIPE_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );

    srand( timeGetTime() );
    PIPE_VERTEX* pVertStart = new PIPE_VERTEX[ uiNumSpawnPoints ];
    if( !pVertStart )
        return E_OUTOFMEMORY;
    ZeroMemory( pVertStart, uiNumSpawnPoints * sizeof( PIPE_VERTEX ) );
    for( UINT i = 0; i < uiNumSpawnPoints; i++ )
    {
        pVertStart[i].currentface = rand() % uiMaxFaces;
        int iCountDown = rand() % ( ( int )g_fLifeSpan );
        pVertStart[i].timerNType.x = ( float )iCountDown;
    }

    vbInitData.pSysMem = pVertStart;
    vbInitData.SysMemPitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &vbInitData, &g_pStart ) );
    delete[] pVertStart;

    vbdesc.ByteWidth = g_numSpawnPoints * ( ( UINT )g_fLifeSpan + 3 ) * sizeof( PIPE_VERTEX );
    vbdesc.BindFlags |= D3D10_BIND_STREAM_OUTPUT;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pDrawFrom ) );
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pStreamTo ) );

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
    srand( timeGetTime() );
    //create the data
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new float[iNumRandValues * 3];
    if( !InitData.pSysMem )
        return E_OUTOFMEMORY;
    InitData.SysMemPitch = iNumRandValues * 3 * sizeof( float );
    InitData.SysMemSlicePitch = iNumRandValues * 3 * sizeof( float );
    for( int i = 0; i < iNumRandValues * 3; i++ )
    {
        ( ( float* )InitData.pSysMem )[i] = float( ( rand() % 10000 ) - 5000 );
    }

    // Create the texture
    D3D10_TEXTURE1D_DESC dstex;
    dstex.Width = iNumRandValues;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32_FLOAT;
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
    SRVDesc.Texture2D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomTexture, &SRVDesc, &g_pRandomTexRV ) );

    return hr;
}


//--------------------------------------------------------------------------------------
// This function generates a buffer of face centers and a buffer of normals
//--------------------------------------------------------------------------------------
HRESULT GenerateTriCenterBuffer( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Make a clone with the desired vertex format (position).


    D3D10_BUFFER_DESC adesc =
    {
        g_dwNumFaces * 3 * sizeof( float ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_SHADER_RESOURCE,
        0,
        0
    };
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = g_pdwAdjF;
    InitData.SysMemPitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &adesc, &InitData, &g_pAdjBuffer ) );

    // load vertices


    D3D10_BUFFER_DESC bdesc =
    {
        2 * g_dwNumFaces * sizeof( D3DXVECTOR4 ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_SHADER_RESOURCE,
        0,
        0
    };
    InitData.pSysMem = g_pVerts;
    InitData.SysMemPitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, &InitData, &g_pTriCenterBuffer ) );

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = 2 * g_dwNumFaces;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pTriCenterBuffer, &SRVDesc, &g_pTriCenterBufferRV ) );

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

