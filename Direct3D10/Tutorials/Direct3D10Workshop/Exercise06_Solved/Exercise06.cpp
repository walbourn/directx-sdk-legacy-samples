//--------------------------------------------------------------------------------------
// Exercise06.cpp
// Direct3D 10 Shader Model 4.0 Workshop
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "resource.h"

#define MAX_SPRITES 200


//-----------------------------------------------------------------------------------------
// o/__   <-- Breakdancin' Bob will guide you through the exercise
// |  (\    
//-----------------------------------------------------------------------------------------

struct TREE_VERTEX
{
    D3DXVECTOR3 Position;
    D3DXVECTOR3 Normal;
    D3DXVECTOR2 Tex;
    float GrowAmt;	
};

//--------------------------------------------------------------------------------------
// DXUT Specific variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

//--------------------------------------------------------------------------------------
// Button and Text Rendering Variables
//--------------------------------------------------------------------------------------
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;

//-----------------------------------------------------------------------------------------
// o/__   <-- HINT:  Modify these parameters to change attributes of the tree
// |  (\    
//-----------------------------------------------------------------------------------------
UINT                                g_NumGrowthSpurts = 8;
float                               g_fGrowth = 1.6f;
float                               g_LengthModifier = 0.72f;
float                               g_SpreadModifier = 0.2f;
float                               g_ShrinkModifier = 0.5f;
float                               g_ExtinctionRate = 0.3f;

//--------------------------------------------------------------------------------------
// Mesh Specific Variables
//--------------------------------------------------------------------------------------
ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10Buffer*                       g_pVB = NULL;
ID3D10Buffer*                       g_pIDVB = NULL;
ID3D10Buffer*                       g_pIB = NULL;
ID3D10Buffer*                       g_pIBAdj = NULL;
UINT                                g_SizeVB = 0;
UINT                                g_NumIndices = 0;
UINT                                g_VertStride = 0;
ID3D10ShaderResourceView*           g_pMeshTexRV = NULL;

//-----------------------------------------------------------------------------------------
// o/__   <-- Buffer of "Random" float values to make our tree look more realistic
// |  (\    
//-----------------------------------------------------------------------------------------
ID3D10Buffer*                       g_pRandomBuffer;
ID3D10ShaderResourceView*           g_pRandomBufferRV;

//--------------------------------------------------------------------------------------
// Geometry Stream Out Buffers
//--------------------------------------------------------------------------------------
ID3D10Buffer*                       g_pStreamTo = NULL;
ID3D10Buffer*                       g_pDrawFrom = NULL;

//--------------------------------------------------------------------------------------
// Effect variables
//--------------------------------------------------------------------------------------
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10EffectTechnique*              g_pRenderTextured = NULL;
ID3D10EffectTechnique*              g_pGrowBranches = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldView = NULL;
ID3D10EffectVectorVariable*         g_pViewSpaceLightDir = NULL;
ID3D10EffectScalarVariable*         g_pLengthModifier = NULL;
ID3D10EffectScalarVariable*         g_pSpreadModifier = NULL;
ID3D10EffectScalarVariable*         g_pShrinkModifier = NULL;
ID3D10EffectScalarVariable*         g_pExtinctionRate = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;

//-----------------------------------------------------------------------------------------
// o/__   <-- We need a shader variable to get our random texture to the effect
// |  (\    
//-----------------------------------------------------------------------------------------
ID3D10EffectShaderResourceVariable* g_pRandomBuf = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4


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
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void InitApp();
HRESULT LoadMesh( ID3D10Device* pd3dDevice );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
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
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"D3D10 Shader Model 4.0 Workshop: Exercise06" );
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
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
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
    V_RETURN( D3DX10CreateSprite( pd3dDevice, MAX_SPRITES, &g_pSprite10 ) );

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Exercise06.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderTextured = g_pEffect10->GetTechniqueByName( "RenderTextured" );
    g_pGrowBranches = g_pEffect10->GetTechniqueByName( "GrowBranches" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmWorldView = g_pEffect10->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pViewSpaceLightDir = g_pEffect10->GetVariableByName( "g_ViewSpaceLightDir" )->AsVector();
    g_pLengthModifier = g_pEffect10->GetVariableByName( "LengthModifier" )->AsScalar();
    g_pSpreadModifier = g_pEffect10->GetVariableByName( "SpreadModifier" )->AsScalar();
    g_pShrinkModifier = g_pEffect10->GetVariableByName( "ShrinkModifier" )->AsScalar();
    g_pExtinctionRate = g_pEffect10->GetVariableByName( "ExtinctionRate" )->AsScalar();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pRandomBuf = g_pEffect10->GetVariableByName( "g_RandomBuffer" )->AsShaderResource();

    // Define our vertex data layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "GROWAMT", 0, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    UINT numElements = sizeof( layout ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    g_pRenderTextured->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Load our IB and VB with mesh data
    V_RETURN( LoadMesh( pd3dDevice ) );

    // Create our StreamTo and DrawFrom buffers
    const UINT MaxVertices = 300000;
    D3D10_BUFFER_DESC bd;
    bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = MaxVertices * g_VertStride;
    bd.BindFlags = D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &g_pStreamTo ) );
    V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &g_pDrawFrom ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -15.0f );
    D3DXVECTOR3 vecAt( 0.0f,0.0f,0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    g_pLengthModifier->SetFloat( g_LengthModifier );
    g_pSpreadModifier->SetFloat( g_SpreadModifier );
    g_pShrinkModifier->SetFloat( g_ShrinkModifier );
    g_pExtinctionRate->SetFloat( g_ExtinctionRate );


    //-----------------------------------------------------------------------------------------
    // o/__   <-- Create a buffer full of random vectors.  These will be indexed by our
    // |  (\      tree generation algorithm to give the tree a more natural look.
    //-----------------------------------------------------------------------------------------
    int iNumRandValues = 1024;
    srand( 200 );
    //create the data
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new float[iNumRandValues * 4];
    if( !InitData.pSysMem )
        return E_OUTOFMEMORY;
    for( int i = 0; i < iNumRandValues * 3; i++ )
    {
        ( ( float* )InitData.pSysMem )[i] = float( ( rand() % 10000 ) - 5000 ) / 5000.0f;
    }

    D3D10_BUFFER_DESC bufferDesc;
    bufferDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    bufferDesc.ByteWidth = 1024 * 4 * sizeof( float );
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.Usage = D3D10_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &InitData, &g_pRandomBuffer ) );
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = 1024;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomBuffer, &SRVDesc, &g_pRandomBufferRV ) );

    SAFE_DELETE_ARRAY( InitData.pSysMem );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.05f, 500.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return hr;
}

void GrowBranches( ID3D10Device* pd3dDevice )
{
    // Set IA parameters
    ID3D10Buffer* pBuffers[2];
    pBuffers[1] = g_pIDVB;

    //-----------------------------------------------------------------------------------------
    // o/__   <-- BreakdancinBob NOTE:  If this is the first time through, we should draw from
    // |  (\			 the original vertex buffer and stream to the g_pStreamTo buffer.  From
    //					 that point, we can keep drawing from the g_pStreamFrom buffer to the
    //					 g_pStreamTo buffer and keep swapping the buffers after every operation.
    //
    //					 It's like ping-ponging with textures.
    //-----------------------------------------------------------------------------------------	
    static bool bDrawFromOriginalBuffer = true;
    if( bDrawFromOriginalBuffer )
    {
        pBuffers[0] = g_pVB;
        pd3dDevice->IASetIndexBuffer( g_pIB, DXGI_FORMAT_R32_UINT, 0 );
    }
    else
    {
        pBuffers[0] = g_pDrawFrom;
    }

    UINT stride[2] = { g_VertStride, sizeof( FLOAT ) };
    UINT offset[2] = { 0, 0 };
    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    pd3dDevice->IASetVertexBuffers( 0, 2, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Point to the correct output buffer
    pBuffers[0] = g_pStreamTo;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    // Set Effects Parameters

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pGrowBranches->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pGrowBranches->GetPassByIndex( p )->Apply( 0 );
        if( bDrawFromOriginalBuffer )
        {
            //If we're drawing from the original buffer (first time), draw the number of indices
            //that our mesh contains.
            pd3dDevice->DrawIndexed( g_NumIndices, 0, 0 );
            bDrawFromOriginalBuffer = false;
        }
        else
        {
            //If we're drawing from a buffer we've previously streamed to, let the hardware
            //determine how many triangles to draw.
            pd3dDevice->DrawAuto();
        }
    }

    // Get back to normal
    pBuffers[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    // Swap stream out buffers
    ID3D10Buffer* pTemp = g_pDrawFrom;
    g_pDrawFrom = g_pStreamTo;
    g_pStreamTo = pTemp;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // Clear the render target
    float ClearColor[4] = { 0.9569f, 0.9569f, 1.0f, 0.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Make sure we only grow the tree the first time we go to render.
    // If we recreated the same tree every frame, we would definitely waste time.
    static bool bFirst = true;
    if( bFirst )
    {
        //-----------------------------------------------------------------------------------------
        // o/__   <-- BreakdancinBob NOTE: Give the grow algorithm access to the random buffer
        // |  (\    
        //-----------------------------------------------------------------------------------------
        g_pRandomBuf->SetResource( g_pRandomBufferRV );

        // Grow the branches one step at a time
        for( UINT i = 0; i < g_NumGrowthSpurts; i++ )
        {
            GrowBranches( pd3dDevice );
        }
        bFirst = false;
    }

    // Render the result
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldViewProj;
    D3DXMATRIX mWorldView;
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldView = mWorld * mView;
    mWorldViewProj = mWorldView * mProj;

    // Set variables
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_pmWorldView->SetMatrix( ( float* )&mWorldView );
    g_pDiffuseTex->SetResource( g_pMeshTexRV );
    D3DXVECTOR3 lightDir( -1,1,-1 );
    D3DXVECTOR3 viewLightDir;
    D3DXVec3TransformNormal( &viewLightDir, &lightDir, &mView );
    D3DXVec3Normalize( &viewLightDir, &viewLightDir );
    g_pViewSpaceLightDir->SetFloatVector( ( float* )&viewLightDir );

    // Set Input Assembler params
    ID3D10Buffer* pBuffers[1];
    pBuffers[0] = g_pDrawFrom;
    UINT stride = g_VertStride;
    UINT offset = 0;
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &stride, &offset );

    // Render using the technique g_pRenderTextured
    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderTextured->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; p++ )
    {
        g_pRenderTextured->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawAuto();
    }
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D10DestroyDevice();

    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVB );
    SAFE_RELEASE( g_pIDVB );
    SAFE_RELEASE( g_pIB );
    SAFE_RELEASE( g_pIBAdj );
    SAFE_RELEASE( g_pMeshTexRV );

    SAFE_RELEASE( g_pStreamTo );
    SAFE_RELEASE( g_pDrawFrom );

    SAFE_RELEASE( g_pRandomBuffer );
    SAFE_RELEASE( g_pRandomBufferRV );
}

//--------------------------------------------------------------------------------------
// Helper to load a VB and IB from a mesh 
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( ID3D10Device* pd3dDevice )
{
    static float treeVertexData[] =
    {
        0.000000f, -1.834569f, 0.000000f, 0.000000f, -1.000000f, 0.000000f, 0.232397f, 0.999001f, 0.000000f,
        0.636337f, -1.834569f, 0.000000f, 0.803211f, -0.595694f, 0.000000f, -0.237213f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, 0.605192f, 0.248206f, -0.595694f, 0.763899f, 0.017676f, 0.999001f, 0.000000f,
        0.000000f, -1.834569f, 0.000000f, 0.000000f, -1.000000f, 0.000000f, 0.232397f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, 0.605192f, 0.248206f, -0.595694f, 0.763899f, 0.017676f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, 0.374029f, -0.649812f, -0.595694f, 0.472116f, 0.174170f, 0.999001f, 0.000000f,
        0.000000f, -1.834569f, 0.000000f, 0.000000f, -1.000000f, 0.000000f, 0.232397f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, 0.374029f, -0.649812f, -0.595694f, 0.472116f, 0.174170f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, -0.374029f, -0.649812f, -0.595694f, -0.472116f, 0.315994f, 0.999001f, 0.000000f,
        0.000000f, -1.834569f, 0.000000f, 0.000000f, -1.000000f, 0.000000f, 0.232397f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, -0.374029f, -0.649812f, -0.595694f, -0.472116f, 0.315994f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, -0.605192f, 0.248206f, -0.595694f, -0.763899f, 0.480517f, 0.999001f, 0.000000f,
        0.000000f, -1.834569f, 0.000000f, 0.000000f, -1.000000f, 0.000000f, 0.232397f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, -0.605192f, 0.248206f, -0.595694f, -0.763899f, 0.480517f, 0.999001f, 0.000000f,
        0.636337f, -1.834569f, 0.000000f, 0.803211f, -0.595694f, 0.000000f, -0.237213f, 0.999001f, 0.000000f,
        0.636337f, -1.834569f, 0.000000f, 0.803211f, -0.595694f, 0.000000f, -0.237213f, 0.999001f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, -0.237213f, -0.194237f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.636337f, -1.834569f, 0.000000f, 0.803211f, -0.595694f, 0.000000f, -0.237213f, 0.999001f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.196639f, -1.834569f, 0.605192f, 0.248206f, -0.595694f, 0.763899f, 0.017676f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, 0.605192f, 0.248206f, -0.595694f, 0.763899f, 0.017676f, 0.999001f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        0.196639f, -1.834569f, 0.605192f, 0.248206f, -0.595694f, 0.763899f, 0.017676f, 0.999001f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.514807f, -1.834569f, 0.374029f, -0.649812f, -0.595694f, 0.472116f, 0.174170f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, 0.374029f, -0.649812f, -0.595694f, 0.472116f, 0.174170f, 0.999001f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        -0.514807f, -1.834569f, 0.374029f, -0.649812f, -0.595694f, 0.472116f, 0.174170f, 0.999001f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        -0.514807f, -1.834569f, -0.374029f, -0.649812f, -0.595694f, -0.472116f, 0.315994f, 0.999001f, 0.000000f,
        -0.514807f, -1.834569f, -0.374029f, -0.649812f, -0.595694f, -0.472116f, 0.315994f, 0.999001f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        -0.514807f, -1.834569f, -0.374029f, -0.649812f, -0.595694f, -0.472116f, 0.315994f, 0.999001f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        0.196639f, -1.834569f, -0.605192f, 0.248206f, -0.595694f, -0.763899f, 0.480517f, 0.999001f, 0.000000f,
        0.196639f, -1.834569f, -0.605192f, 0.248206f, -0.595694f, -0.763899f, 0.480517f, 0.999001f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, 0.762787f, -0.194237f, 0.000000f,
        0.196639f, -1.834569f, -0.605192f, 0.248206f, -0.595694f, -0.763899f, 0.480517f, 0.999001f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, 0.762787f, -0.194237f, 0.000000f,
        0.636337f, -1.834569f, 0.000000f, 0.803211f, -0.595694f, 0.000000f, 0.762787f, 0.999001f, 0.000000f,
        0.606364f, 1.514307f, 0.440550f, -0.245871f, 0.952700f, -0.178636f, 0.900607f, -0.999001f, 1.600000f,
        0.803003f, 1.102575f, 1.045742f, 0.122381f, 0.171919f, 0.977480f, 0.926464f, -0.753354f, 1.600000f,
        1.242701f, 1.102575f, 0.440550f, 0.967456f, 0.171919f, -0.185666f, 0.821507f, -0.753354f, 1.600000f,
        -0.173708f, 1.280021f, 0.534618f, 0.093915f, 0.952699f, -0.289039f, 0.105600f, -0.859222f, 1.600000f,
        -0.688515f, 0.868289f, 0.908647f, -0.891821f, 0.171919f, 0.418449f, 0.126479f, -0.613575f, 1.600000f,
        0.022931f, 0.868289f, 1.139810f, 0.475540f, 0.171919f, 0.862732f, 0.032802f, -0.613575f, 1.600000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.193009f, 1.358116f, -0.594020f, 0.093915f, 0.952699f, 0.289039f, 0.391077f, -0.905815f, 1.600000f,
        0.003630f, 0.946384f, -1.199212f, 0.475540f, 0.171919f, -0.862732f, 0.464690f, -0.660168f, 1.600000f,
        -0.707816f, 0.946384f, -0.968049f, -0.891821f, 0.171919f, -0.418450f, 0.371931f, -0.660168f, 1.600000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, -0.237213f, -0.194237f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, -0.237213f, -0.194237f, 0.000000f,
        0.803003f, 1.102575f, 1.045742f, 0.122381f, 0.171919f, 0.977480f, -0.073536f, -0.753354f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.803003f, 1.102575f, 1.045742f, 0.122381f, 0.171919f, 0.977480f, 0.926464f, -0.753354f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, 0.762787f, -0.194237f, 0.000000f,
        1.242701f, 1.102575f, 0.440550f, 0.967456f, 0.171919f, -0.185666f, 0.821507f, -0.753354f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        1.242701f, 1.102575f, 0.440550f, 0.967456f, 0.171919f, -0.185666f, -0.178493f, -0.753354f, 0.000000f,
        0.636337f, 0.165431f, 0.000000f, 0.965927f, 0.072050f, -0.248584f, -0.237213f, -0.194237f, 0.000000f,
        1.242701f, 1.102575f, 0.440550f, 0.967456f, 0.171919f, -0.185666f, -0.178493f, -0.753354f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        0.606364f, 1.514307f, 0.440550f, -0.245871f, 0.952700f, -0.178636f, -0.099393f, -0.999001f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.606364f, 1.514307f, 0.440550f, -0.245871f, 0.952700f, -0.178636f, -0.099393f, -0.999001f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        0.606364f, 1.514307f, 0.440550f, -0.245871f, 0.952700f, -0.178636f, -0.099393f, -0.999001f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.803003f, 1.102575f, 1.045742f, 0.122381f, 0.171919f, 0.977480f, -0.073536f, -0.753354f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        0.003630f, 0.946384f, -1.199212f, 0.475540f, 0.171919f, -0.862732f, 0.464690f, -0.660168f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        0.003630f, 0.946384f, -1.199212f, 0.475540f, 0.171919f, -0.862732f, 0.464690f, -0.660168f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        -0.707816f, 0.946384f, -0.968049f, -0.891821f, 0.171919f, -0.418450f, 0.371931f, -0.660168f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.707816f, 0.946384f, -0.968049f, -0.891821f, 0.171919f, -0.418450f, 0.371931f, -0.660168f, 0.000000f,
        -0.514807f, 0.165431f, -0.374029f, -0.927565f, 0.072050f, -0.366649f, 0.315994f, -0.194237f, 0.000000f,
        -0.707816f, 0.946384f, -0.968049f, -0.891821f, 0.171919f, -0.418450f, 0.371931f, -0.660168f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.193009f, 1.358116f, -0.594020f, 0.093915f, 0.952699f, 0.289039f, 0.391077f, -0.905815f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        -0.193009f, 1.358116f, -0.594020f, 0.093915f, 0.952699f, 0.289039f, 0.391077f, -0.905815f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.193009f, 1.358116f, -0.594020f, 0.093915f, 0.952699f, 0.289039f, 0.391077f, -0.905815f, 0.000000f,
        0.196639f, 0.165431f, -0.605192f, 0.534905f, 0.072050f, -0.841834f, 0.480517f, -0.194237f, 0.000000f,
        0.003630f, 0.946384f, -1.199212f, 0.475540f, 0.171919f, -0.862732f, 0.464690f, -0.660168f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        -0.688515f, 0.868289f, 0.908647f, -0.891821f, 0.171919f, 0.418449f, 0.126479f, -0.613575f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.688515f, 0.868289f, 0.908647f, -0.891821f, 0.171919f, 0.418449f, 0.126479f, -0.613575f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.022931f, 0.868289f, 1.139810f, 0.475540f, 0.171919f, 0.862732f, 0.032802f, -0.613575f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        0.022931f, 0.868289f, 1.139810f, 0.475540f, 0.171919f, 0.862732f, 0.032802f, -0.613575f, 0.000000f,
        0.196639f, 0.165431f, 0.605192f, 0.303425f, -0.189382f, 0.933846f, 0.017676f, -0.194237f, 0.000000f,
        0.022931f, 0.868289f, 1.139810f, 0.475540f, 0.171919f, 0.862732f, 0.032802f, -0.613575f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.173708f, 1.280021f, 0.534618f, 0.093915f, 0.952699f, -0.289039f, 0.105600f, -0.859222f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.173708f, 1.280021f, 0.534618f, 0.093915f, 0.952699f, -0.289039f, 0.105600f, -0.859222f, 0.000000f,
        0.000000f, 0.577163f, 0.000000f, -0.076035f, 0.969256f, -0.234012f, 0.232397f, -0.439883f, 0.000000f,
        -0.173708f, 1.280021f, 0.534618f, 0.093915f, 0.952699f, -0.289039f, 0.105600f, -0.859222f, 0.000000f,
        -0.514807f, 0.165431f, 0.374029f, -0.927565f, 0.072050f, 0.366649f, 0.174170f, -0.194237f, 0.000000f,
        -0.688515f, 0.868289f, 0.908647f, -0.891821f, 0.171919f, 0.418449f, 0.126479f, -0.613575f, 0.000000f,
    };

    static DWORD treeIndexData[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
        77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102,
        103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113
    };

    HRESULT hr = S_OK;
    g_NumIndices = sizeof( treeIndexData ) / sizeof( DWORD );
    g_VertStride = sizeof( TREE_VERTEX );

    D3D10_BUFFER_DESC bd;
    bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( treeVertexData );
    bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = treeVertexData;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVB ) );

    bd.Usage = D3D10_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( treeIndexData );
    bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    InitData.pSysMem = treeIndexData;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData, &g_pIB ) );

    // Load the Texture
    WCHAR strPath[MAX_PATH] = {0};
    DXUTFindDXSDKMediaFileCch( strPath, sizeof( strPath ) / sizeof( WCHAR ), L"bark_diff.dds" );
    hr = D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, NULL, NULL, &g_pMeshTexRV, NULL );

    return hr;
}
