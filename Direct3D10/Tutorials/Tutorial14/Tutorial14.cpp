//--------------------------------------------------------------------------------------
// File: Tutorial14.cpp
//
// Render State Management
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

#define DEG2RAD( a ) ( a * D3DX_PI / 180.f )

WCHAR*                              g_szQuadTechniques[] =
{
    L"RenderQuadSolid",
    L"RenderQuadSrcAlphaAdd",
    L"RenderQuadSrcAlphaSub",
    L"RenderQuadSrcColorAdd",
    L"RenderQuadSrcColorSub"
};
#define MAX_QUAD_TECHNIQUES 5

WCHAR*                              g_szDepthStencilModes[] =
{
    L"DepthOff/StencilOff",
    L"DepthLess/StencilOff",
    L"DepthGreater/StencilOff",

    L"DepthOff/StencilIncOnFail",
    L"DepthLess/StencilIncOnFail",
    L"DepthGreater/StencilIncOnFail",

    L"DepthOff/StencilIncOnPass",
    L"DepthLess/StencilIncOnPass",
    L"DepthGreater/StencilIncOnPass",
};
#define MAX_DEPTH_STENCIL_MODES 9

WCHAR*                              g_szRasterizerModes[] =
{
    L"CullOff/FillSolid",
    L"CullFront/FillSolid",
    L"CullBack/FillSolid",

    L"CullOff/FillWire",
    L"CullFront/FillWire",
    L"CullBack/FillWire",
};
#define MAX_RASTERIZER_MODES 6

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager;// manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

ID3DX10Font*                        g_pFont = NULL;         // Font for drawing text
ID3DX10Sprite*                      g_pSprite = NULL;       // Sprite for batching text drawing
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect = NULL;       // D3DX effect interface
ID3D10InputLayout*                  g_pSceneLayout = NULL; // Scene Vertex Layout
ID3D10InputLayout*                  g_pQuadLayout = NULL;   // Quad Vertex Layout
ID3D10Buffer*                       g_pScreenQuadVB = NULL; // Screen Quad
CDXUTSDKMesh                        g_Mesh;
ID3D10ShaderResourceView*           g_pScreenRV[2] = { NULL };

UINT                                g_eSceneDepthStencilMode = 0;
ID3D10DepthStencilState*            g_pDepthStencilStates[MAX_DEPTH_STENCIL_MODES]; // Depth Stencil states for non-FX 
// depth stencil state managment
UINT                                g_eSceneRasterizerMode = 0;
ID3D10RasterizerState*              g_pRasterStates[MAX_RASTERIZER_MODES];  // Rasterizer states for non-FX 
// rasterizer state management
UINT                                g_eQuadRenderMode = 0;
ID3D10EffectTechnique*              g_pTechniqueQuad[MAX_QUAD_TECHNIQUES]; // Quad Techniques from the FX file for 
// FX based alpha blend state management
ID3D10EffectTechnique*              g_pTechniqueScene = NULL;             // FX technique for rendering the scene
ID3D10EffectTechnique*              g_pTechniqueRenderWithStencil = NULL; // FX technique for rendering using FX based depth
// stencil state management

ID3D10EffectShaderResourceVariable* g_ptxDiffuseVariable = NULL;
ID3D10EffectMatrixVariable*         g_pWorldVariable = NULL;
ID3D10EffectMatrixVariable*         g_pViewVariable = NULL;
ID3D10EffectMatrixVariable*         g_pProjectionVariable = NULL;
ID3D10EffectScalarVariable*         g_pPuffiness = NULL;
D3DXMATRIX                          g_World;
float                               g_fModelPuffiness = 0.0f;
bool                                g_bSpinning = true;

//--------------------------------------------------------------------------------------
// Struct describing our screen-space vertex
//--------------------------------------------------------------------------------------
struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN            1
#define IDC_TOGGLEREF                   2
#define IDC_CHANGEDEVICE                3
#define IDC_TOGGLESPIN                  4
#define IDC_QUADRENDER_MODE             5
#define IDC_SCENEDEPTHSTENCIL_MODE      6
#define IDC_SCENERASTERIZER_MODE        7
#define IDC_TOGGLEWARP                  8

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

void RenderText();
void InitApp();
void LoadQuadTechniques();
void LoadDepthStencilStates( ID3D10Device* pd3dDevice );
void LoadRasterizerStates( ID3D10Device* pd3dDevice );

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

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
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

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen

    InitApp();

    DXUTCreateWindow( L"Tutorial14" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_fModelPuffiness = 0.0f;
    g_bSpinning = true;

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent );

    CDXUTComboBox* pComboBox = NULL;

    iY = 0;
    g_SampleUI.AddStatic( IDC_STATIC, L"(Q)uad Render Mode", 0, iY, 200, 25 );
    iY += 25;
    g_SampleUI.AddComboBox( IDC_QUADRENDER_MODE, 0, iY, 220, 24, 'Q', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 150 );

    iY += 40;
    g_SampleUI.AddStatic( IDC_STATIC, L"Scene (R)asterizer Mode", 0, iY, 200, 25 );
    iY += 25;
    g_SampleUI.AddComboBox( IDC_SCENERASTERIZER_MODE, 0, iY, 220, 24, 'R', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 150 );

    iY += 40;
    g_SampleUI.AddStatic( IDC_STATIC, L"Scene Depth/(S)tencil Mode", 0, iY, 200, 25 );
    iY += 25;
    g_SampleUI.AddComboBox( IDC_SCENEDEPTHSTENCIL_MODE, 0, iY, 220, 24, 'S', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 150 );


    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 35, iY += 24, 125, 22, g_bSpinning );
}


//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont, g_pSprite, 15 );

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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Tutorial14.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect, NULL, NULL ) );

    // Obtain the technique handles
    g_pTechniqueScene = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_pTechniqueRenderWithStencil = g_pEffect->GetTechniqueByName( "RenderWithStencil" );
    LoadQuadTechniques();
    LoadDepthStencilStates( pd3dDevice );
    LoadRasterizerStates( pd3dDevice );

    // Obtain the variables
    g_ptxDiffuseVariable = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pWorldVariable = g_pEffect->GetVariableByName( "World" )->AsMatrix();
    g_pViewVariable = g_pEffect->GetVariableByName( "View" )->AsMatrix();
    g_pProjectionVariable = g_pEffect->GetVariableByName( "Projection" )->AsMatrix();
    g_pPuffiness = g_pEffect->GetVariableByName( "Puffiness" )->AsScalar();

    // Set Puffiness
    g_pPuffiness->SetFloat( g_fModelPuffiness );

    // Define the input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof( layout ) / sizeof( layout[0] );

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    g_pTechniqueScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSceneLayout ) );

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh", true ) );

    // Initialize the world matrices
    D3DXMatrixIdentity( &g_World );

    // Create a screen quad
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_pTechniqueQuad[0]->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, 2, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    SCREEN_VERTEX svQuad[4];
    float fSize = 1.0f;
    svQuad[0].pos = D3DXVECTOR4( -fSize, fSize, 0.0f, 1.0f );
    svQuad[0].tex = D3DXVECTOR2( 0.0f, 0.0f );
    svQuad[1].pos = D3DXVECTOR4( fSize, fSize, 0.0f, 1.0f );
    svQuad[1].tex = D3DXVECTOR2( 1.0f, 0.0f );
    svQuad[2].pos = D3DXVECTOR4( -fSize, -fSize, 0.0f, 1.0f );
    svQuad[2].tex = D3DXVECTOR2( 0.0f, 1.0f );
    svQuad[3].pos = D3DXVECTOR4( fSize, -fSize, 0.0f, 1.0f );
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

    // Load the texture for the screen quad
    WCHAR* szScreenTextures[] =
    {
        L"misc\\MarbleClouds.dds",
        L"misc\\NormTest.dds"
    };

    for( int i = 0; i < 2; i++ )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szScreenTextures[i] ) );
        V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pScreenRV[i], NULL ) );
    }

    // Initialize the camera
    D3DXVECTOR3 Eye( 0.0f, 0.0f, -800.0f );
    D3DXVECTOR3 At( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &Eye, &At );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = static_cast<float>( pBufferSurfaceDesc->Width ) /
        static_cast<float>( pBufferSurfaceDesc->Height );
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBufferSurfaceDesc->Width, pBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBufferSurfaceDesc->Width - 230, pBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    //
    // Clear the back buffer
    //
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red, green, blue, alpha
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    //
    // Clear the depth stencil
    //
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0 );

    //
    // Update variables that change once per frame
    //
    g_pProjectionVariable->SetMatrix( ( float* )g_Camera.GetProjMatrix() );
    g_pViewVariable->SetMatrix( ( float* )g_Camera.GetViewMatrix() );
    g_pWorldVariable->SetMatrix( ( float* )&g_World );

    //
    // Update the Cull Mode (non-FX method)
    //
    pd3dDevice->RSSetState( g_pRasterStates[ g_eSceneRasterizerMode ] );

    //
    // Update the Depth Stencil States (non-FX method)
    //
    pd3dDevice->OMSetDepthStencilState( g_pDepthStencilStates[ g_eSceneDepthStencilMode ], 0 );

    //
    // Render the mesh
    //
    pd3dDevice->IASetInputLayout( g_pSceneLayout );

    UINT Strides[1];
    UINT Offsets[1];
    ID3D10Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB10( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_Mesh.GetIB10( 0 ), g_Mesh.GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    g_pTechniqueScene->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    ID3D10ShaderResourceView* pDiffuseRV = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_Mesh.GetSubset( 0, subset );

            PrimType = g_Mesh.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV10;
            g_ptxDiffuseVariable->SetResource( pDiffuseRV );

            g_pTechniqueScene->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
    }

    //the mesh class also had a render method that allows rendering the mesh with the most common options
    //g_Mesh.Render( pd3dDevice, g_pTechniqueScene, g_ptxDiffuseVariable );

    //
    // Reset the world transform
    //
    D3DXMATRIX mWorld;
    D3DXMatrixScaling( &mWorld, 150.0f, 150.0f, 1.0f );
    g_pWorldVariable->SetMatrix( ( float* )&mWorld );

    //
    // Render the screen space quad
    //
    ID3D10EffectTechnique* pTech = g_pTechniqueQuad[ g_eQuadRenderMode ];
    g_ptxDiffuseVariable->SetResource( g_pScreenRV[0] );
    UINT strides = sizeof( SCREEN_VERTEX );
    UINT offsets = 0;
    ID3D10Buffer* pBuffers[1] = { g_pScreenQuadVB };

    pd3dDevice->IASetInputLayout( g_pQuadLayout );
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pTech->GetDesc( &techDesc );

    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0 );

        pd3dDevice->Draw( 4, 0 );
    }

    //
    // Render the screen space quad again, but this time with a different texture
    //  and only render where the stencil buffer is != 0
    //  Look at the FX file for the state settings
    //
    pTech = g_pTechniqueRenderWithStencil;
    g_ptxDiffuseVariable->SetResource( g_pScreenRV[1] );
    pd3dDevice->IASetInputLayout( g_pQuadLayout );
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pTech->GetDesc( &techDesc );
    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }

    //
    // Reset our Cull Mode (non-FX method)
    //
    pd3dDevice->RSSetState( g_pRasterStates[ 0 ] );

    //
    // Reset the Depth Stencil State
    //
    pd3dDevice->OMSetDepthStencilState( g_pDepthStencilStates[ 1 ], 0 );

    //
    // Render the UI
    //
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );

    RenderText();
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
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
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
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pSprite );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pSceneLayout );
    SAFE_RELEASE( g_pQuadLayout );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pScreenQuadVB );

    for( int i = 0; i < 2; i++ )
    {
        SAFE_RELEASE( g_pScreenRV[i] );
    }

    g_Mesh.Destroy();

    for( UINT i = 0; i < MAX_DEPTH_STENCIL_MODES; i++ )
        SAFE_RELEASE( g_pDepthStencilStates[i] );

    for( UINT i = 0; i < MAX_RASTERIZER_MODES; i++ )
        SAFE_RELEASE( g_pRasterStates[i] );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{

    pDeviceSettings->d3d10.AutoDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
    g_Camera.FrameMove( fElapsedTime );

    if( g_bSpinning )
        D3DXMatrixRotationY( &g_World, 60.0f * DEG2RAD((float)fTime) );
    else
        D3DXMatrixRotationY( &g_World, DEG2RAD( 180.0f ) );

    D3DXMATRIX mRot;
    D3DXMatrixRotationX( &mRot, DEG2RAD( -90.0f ) );
    g_World = mRot * g_World;
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

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

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
                break;
        }
    }
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
        case IDC_TOGGLESPIN:
        {
            g_bSpinning = g_SampleUI.GetCheckBox( IDC_TOGGLESPIN )->GetChecked();
            break;
        }

        case IDC_QUADRENDER_MODE:
        {
            CDXUTComboBox* pComboBox = NULL;
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eQuadRenderMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }

        case IDC_SCENEDEPTHSTENCIL_MODE:
        {
            CDXUTComboBox* pComboBox = NULL;
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eSceneDepthStencilMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }

        case IDC_SCENERASTERIZER_MODE:
        {
            CDXUTComboBox* pComboBox = NULL;
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eSceneRasterizerMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// LoadQuadTechniques
// Load the techniques for rendering the quad from the FX file.  The techniques in the
// FX file contain the alpha blending state setup.
//--------------------------------------------------------------------------------------
void LoadQuadTechniques()
{
    for( UINT i = 0; i < MAX_QUAD_TECHNIQUES; i++ )
    {
        char mbstr[MAX_PATH];
        WideCharToMultiByte( CP_ACP, 0, g_szQuadTechniques[i], -1, mbstr, MAX_PATH, 0, 0 );
        g_pTechniqueQuad[i] = g_pEffect->GetTechniqueByName( mbstr );

        g_SampleUI.GetComboBox( IDC_QUADRENDER_MODE )->AddItem( g_szQuadTechniques[i], ( void* )( UINT64 )i );

    }
}

//--------------------------------------------------------------------------------------
// LoadDepthStencilStates
// Create a set of depth stencil states for non-FX state managment.  These states
// will later be set using OMSetDepthStencilState in OnD3D10FrameRender.
//--------------------------------------------------------------------------------------
void LoadDepthStencilStates( ID3D10Device* pd3dDevice )
{
    BOOL bDepthEnable[ MAX_DEPTH_STENCIL_MODES ] =
    {
        FALSE,
        TRUE,
        TRUE,
        FALSE,
        TRUE,
        TRUE,
        FALSE,
        TRUE,
        TRUE
    };

    BOOL bStencilEnable[ MAX_DEPTH_STENCIL_MODES ] =
    {
        FALSE,
        FALSE,
        FALSE,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        TRUE
    };

    D3D10_COMPARISON_FUNC compFunc[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_GREATER,
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_GREATER,
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_LESS,
        D3D10_COMPARISON_GREATER,
    };

    D3D10_STENCIL_OP FailOp[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,

        D3D10_STENCIL_OP_INCR,
        D3D10_STENCIL_OP_INCR,
        D3D10_STENCIL_OP_INCR,

        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,
    };

    D3D10_STENCIL_OP PassOp[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,

        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,
        D3D10_STENCIL_OP_KEEP,

        D3D10_STENCIL_OP_INCR,
        D3D10_STENCIL_OP_INCR,
        D3D10_STENCIL_OP_INCR,
    };

    for( UINT i = 0; i < MAX_DEPTH_STENCIL_MODES; i++ )
    {
        D3D10_DEPTH_STENCIL_DESC dsDesc;
        dsDesc.DepthEnable = bDepthEnable[i];
        dsDesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = compFunc[i];

        // Stencil test parameters
        dsDesc.StencilEnable = bStencilEnable[i];
        dsDesc.StencilReadMask = 0xFF;
        dsDesc.StencilWriteMask = 0xFF;

        // Stencil operations if pixel is front-facing
        dsDesc.FrontFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = FailOp[i];
        dsDesc.FrontFace.StencilPassOp = PassOp[i];
        dsDesc.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;

        // Stencil operations if pixel is back-facing
        dsDesc.BackFace.StencilFailOp = D3D10_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = FailOp[i];
        dsDesc.BackFace.StencilPassOp = PassOp[i];
        dsDesc.BackFace.StencilFunc = D3D10_COMPARISON_ALWAYS;

        // Create depth stencil state
        pd3dDevice->CreateDepthStencilState( &dsDesc, &g_pDepthStencilStates[i] );

        g_SampleUI.GetComboBox( IDC_SCENEDEPTHSTENCIL_MODE )->AddItem( g_szDepthStencilModes[i],
                                                                       ( void* )( UINT64 )i );
    }
}

//--------------------------------------------------------------------------------------
// LoadRasterizerStates
// Create a set of rasterizer states for non-FX state managment.  These states
// will later be set using RSSetState in OnD3D10FrameRender.
//--------------------------------------------------------------------------------------
void LoadRasterizerStates( ID3D10Device* pd3dDevice )
{
    D3D10_FILL_MODE fill[MAX_RASTERIZER_MODES] =
    {
        D3D10_FILL_SOLID,
        D3D10_FILL_SOLID,
        D3D10_FILL_SOLID,
        D3D10_FILL_WIREFRAME,
        D3D10_FILL_WIREFRAME,
        D3D10_FILL_WIREFRAME
    };
    D3D10_CULL_MODE cull[MAX_RASTERIZER_MODES] =
    {
        D3D10_CULL_NONE,
        D3D10_CULL_FRONT,
        D3D10_CULL_BACK,
        D3D10_CULL_NONE,
        D3D10_CULL_FRONT,
        D3D10_CULL_BACK
    };

    for( UINT i = 0; i < MAX_RASTERIZER_MODES; i++ )
    {
        D3D10_RASTERIZER_DESC rasterizerState;
        rasterizerState.FillMode = fill[i];
        rasterizerState.CullMode = cull[i];
        rasterizerState.FrontCounterClockwise = false;
        rasterizerState.DepthBias = false;
        rasterizerState.DepthBiasClamp = 0;
        rasterizerState.SlopeScaledDepthBias = 0;
        rasterizerState.DepthClipEnable = true;
        rasterizerState.ScissorEnable = false;
        rasterizerState.MultisampleEnable = false;
        rasterizerState.AntialiasedLineEnable = false;
        pd3dDevice->CreateRasterizerState( &rasterizerState, &g_pRasterStates[i] );

        g_SampleUI.GetComboBox( IDC_SCENERASTERIZER_MODE )->AddItem( g_szRasterizerModes[i], ( void* )( UINT64 )i );
    }
}
