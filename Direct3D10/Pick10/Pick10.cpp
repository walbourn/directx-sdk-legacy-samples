//--------------------------------------------------------------------------------------
// File: Pick10.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Vertex format
//--------------------------------------------------------------------------------------
struct D3DVERTEX
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
    FLOAT tu, tv;

    static const DWORD FVF;
};
const DWORD                 D3DVERTEX::FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;



struct INTERSECTION
{
    DWORD dwFace;                 // mesh face that was intersected
    FLOAT fBary1, fBary2;         // barycentric coords of intersection
    FLOAT fDist;                  // distance from ray origin to intersection
    FLOAT tu, tv;                 // texture coords of intersection
};

// For simplicity's sake, we limit the number of simultaneously intersected 
// triangles to 16
#define MAX_INTERSECTIONS 16
#define CAMERA_DISTANCE 3.5f


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DX10Font*                g_pFont = NULL;         // Font for drawing text
ID3DX10Sprite*              g_pTextSprite = NULL;   // Sprite for batching draw text calls
CDXUTSDKMesh                g_Mesh;
ID3DX10Mesh*                g_pD3DXMesh;

ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10Effect*                       g_pEffect = NULL;
ID3D10EffectTechnique*              g_pTechRenderScene = NULL;
ID3D10EffectTechnique*              g_pTechRenderSceneWireframe = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectVectorVariable*         g_pLightDir = NULL;
ID3D10EffectVectorVariable*         g_pLightDiffuse = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectVectorVariable*         g_pMaterialDiffuseColor = NULL;
ID3D10EffectVectorVariable*         g_pMaterialAmbientColor = NULL;
ID3D10EffectScalarVariable*         g_pnNumLights = NULL;

CModelViewerCamera          g_Camera;               // A model viewing camera
UINT                        g_nNumIntersections;   // Number of faces intersected
INTERSECTION g_IntersectionArray[MAX_INTERSECTIONS]; // Intersection info
ID3D10Buffer*               g_pVB;                  // VB for picked triangles
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
bool                        g_bUseD3DXIntersect = true;      // Whether to use D3DXIntersect
bool                        g_bAllHits = true;      // Whether to just get the first "hit" or all "hits"
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_USED3DX             4
#define IDC_ALLHITS             5
#define IDC_TOGGLEWARP          6


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnReleasingSwapChain( void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
void RenderText();
HRESULT Pick();
bool IntersectTriangle( const D3DXVECTOR3& orig, const D3DXVECTOR3& dir,
                        D3DXVECTOR3& v0, D3DXVECTOR3& v1, D3DXVECTOR3& v2,
                        FLOAT* t, FLOAT* u, FLOAT* v );


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

    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.

    DXUTSetCallbackD3D10DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnResizedSwapChain );
    DXUTSetCallbackD3D10FrameRender( OnFrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnDestroyDevice );

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
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys
    DXUTCreateWindow( L"Pick10" );
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
    g_HUD.AddCheckBox( IDC_USED3DX, L"Use D3DXIntersect", 35, iY += 24, 125, 22, g_bUseD3DXIntersect, VK_F4 );
    g_HUD.AddCheckBox( IDC_ALLHITS, L"Show All Hits", 35, iY += 24, 125, 22, g_bAllHits, VK_F5 );

    g_SampleUI.SetCallback( OnGUIEvent );
}


//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D10
    //pDeviceSettings->d3d10.CreateFlags |= D3D10_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    WCHAR str[MAX_PATH];
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );

    // Initialize the font
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont ) );

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"scanner\\scannerarm.sdkmesh", true ) );

    // Create the vertex buffer
    D3D10_BUFFER_DESC bufferDesc;
    bufferDesc.ByteWidth = ( UINT )( 3* MAX_INTERSECTIONS*sizeof(D3DVERTEX) );
    bufferDesc.Usage = D3D10_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;

    if( FAILED( pd3dDevice->CreateBuffer( &bufferDesc, NULL, &g_pVB ) ))
    {
        return E_FAIL;
    }

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif


    // Read the D3DX effect file
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Pick10.fx" ) );
    D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect, NULL, NULL );

    // Obtain technique objects
    g_pTechRenderScene = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_pTechRenderSceneWireframe = g_pEffect->GetTechniqueByName( "RenderSceneWireframe" );

    g_ptxDiffuse = g_pEffect->GetVariableByName( "g_MeshTexture" )->AsShaderResource();
    g_pLightDir = g_pEffect->GetVariableByName( "g_LightDir" )->AsVector();
    g_pLightDiffuse = g_pEffect->GetVariableByName( "g_LightDiffuse" )->AsVector();
    g_pmWorldViewProjection = g_pEffect->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pfTime = g_pEffect->GetVariableByName( "g_fTime" )->AsScalar();
    g_pMaterialAmbientColor = g_pEffect->GetVariableByName( "g_MaterialAmbientColor" )->AsVector();
    g_pMaterialDiffuseColor = g_pEffect->GetVariableByName( "g_MaterialDiffuseColor" )->AsVector();

    // Set effect variables as needed
    D3DXCOLOR colorMtrl( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXVECTOR3 vLightDir( 0.1f, -1.0f, 0.1f );
    D3DXCOLOR vLightDiffuse( 1,1,1,1 );
    V_RETURN( g_pMaterialAmbientColor->SetFloatVector( ( float* )&colorMtrl ) );
    V_RETURN( g_pMaterialDiffuseColor->SetFloatVector( ( float* )&colorMtrl ) );
    V_RETURN( g_pLightDir->SetRawValue( (float*)&vLightDir, 0, sizeof( D3DXVECTOR3) ) );
    V_RETURN( g_pLightDiffuse->SetFloatVector( (float*)&vLightDiffuse ) );
    
     // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Create a D3DX10 mesh to demonstrate intersection using D3DX.  We still use the 
    // DXUTSDKMesh for rendering as it is better suited for that purpose, and we can optionally 
    // use the vertex information from the DXUTSDKMesh for intersecting manually.
    D3DX10CreateMesh( pd3dDevice, layout, 3, layout[0].SemanticName, (UINT)g_Mesh.GetNumVertices(0,0), (UINT)g_Mesh.GetNumIndices(0)/3, D3DX10_MESH_32_BIT, &g_pD3DXMesh);
    g_pD3DXMesh->SetVertexData(0, (D3DVERTEX*)g_Mesh.GetRawVerticesAt(0) );
    g_pD3DXMesh->SetIndexData( (DWORD*)g_Mesh.GetRawIndicesAt(0), (UINT)g_Mesh.GetNumIndices(0));

    D3D10_PASS_DESC PassDesc;
    V_RETURN( g_pTechRenderScene->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );

    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -CAMERA_DISTANCE, 0.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Setup the world matrix of the camera
    // Change this to see how picking works with a translated object
    D3DXMATRIX mWorld;
    D3DXMatrixTranslation( &mWorld, 0.0f, -1.7f, 0.0f );
    g_Camera.SetWorldMatrix( mWorld );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    
    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pTextSprite ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Rotate the camera about the y-axis
    D3DXVECTOR3 vFromPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    vFromPt.x = -cosf( ( float )fTime / 3.0f ) * CAMERA_DISTANCE;
    vFromPt.y = 1.0f;
    vFromPt.z = sinf( ( float )fTime / 3.0f ) * CAMERA_DISTANCE;

    // Update the camera's position based on time
    g_Camera.SetViewParams( &vFromPt, &vLookatPt );
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. 
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Check for picked triangles
    Pick();

    // Clear the render target and the zbuffer 
    float ClearColor[4] = { 0.0f, 0.25f, 0.25f, 0.55f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );


    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    mWorldViewProjection = mWorld * mView * mProj;

    V( g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection ) );
    V( g_pmWorld->SetMatrix( ( float* )&mWorld ) );
    V( g_pfTime->SetFloat( ( float )fTime ) );

    //IA setup
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Set render mode to lit, solid triangles
    g_pTechRenderScene->GetPassByIndex( 0 )->Apply( 0 );

    // If a triangle is picked, draw it
    if( g_nNumIntersections > 0 )
    {
        // Draw the picked triangle
        UINT Strides[1];
        UINT Offsets[1];
        Strides[0] = sizeof(D3DVERTEX);
        Offsets[0] = 0;
     
        pd3dDevice->IASetVertexBuffers(0, 1, &g_pVB, Strides, Offsets);
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dDevice->Draw( 3*g_nNumIntersections, 0 );

        // Set render mode to wireframe
        g_pTechRenderSceneWireframe->GetPassByIndex( 0 )->Apply( 0 );
    }
 
    UINT Strides[1];
    UINT Offsets[1];
    ID3D10Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB10( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_Mesh.GetIB10( 0 ), g_Mesh.GetIBFormat10( 0 ), 0 );

    // Render the mesh
    D3D10_TECHNIQUE_DESC techDesc;
    g_pTechRenderScene->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    ID3D10ShaderResourceView* pDiffuseRV = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
        {
            // Get the subset
            pSubset = g_Mesh.GetSubset( 0, subset );

            PrimType = CDXUTSDKMesh::GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV10;
            g_ptxDiffuse->SetResource( pDiffuseRV );

            pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
    }

    g_pTechRenderScene->GetPassByIndex( 0 )->Apply( 0 );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
    RenderText();
    V( g_HUD.OnRender( fElapsedTime ) );
    V( g_SampleUI.OnRender( fElapsedTime ) );
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );
     
    // Output statistics
    txtHelper.Begin();
     
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    
    if( g_nNumIntersections < 1 )
    {
        txtHelper.DrawTextLine( L"Use mouse to pick a polygon" );
    }
    else
    {
        WCHAR wstrHitStat[256];

        for( DWORD dwIndex = 0; dwIndex < g_nNumIntersections; dwIndex++ )
        {
            swprintf_s( wstrHitStat, 256,
                             L"Face=%d, tu=%3.02f, tv=%3.02f",
                             g_IntersectionArray[dwIndex].dwFace,
                             g_IntersectionArray[dwIndex].tu,
                             g_IntersectionArray[dwIndex].tv );

            txtHelper.DrawTextLine( wstrHitStat );
        }
    }

    txtHelper.End();
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

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    switch( uMsg )
    {
        case WM_LBUTTONDOWN:
        {
            // Capture the mouse, so if the mouse button is 
            // released outside the window, we'll get the WM_LBUTTONUP message
            DXUTGetGlobalTimer()->Stop();
            SetCapture( hWnd );
            return TRUE;
        }

        case WM_LBUTTONUP:
        {
            ReleaseCapture();
            DXUTGetGlobalTimer()->Start();
            break;
        }

        case WM_CAPTURECHANGED:
        {
            if( ( HWND )lParam != hWnd )
            {
                ReleaseCapture();
                DXUTGetGlobalTimer()->Start();
            }
            break;
        }
    }

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
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
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
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_USED3DX:
            g_bUseD3DXIntersect = !g_bUseD3DXIntersect; break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_ALLHITS:
            g_bAllHits = !g_bAllHits; break;
    }
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
    SAFE_RELEASE( g_pTextSprite );
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here. 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    g_Mesh.Destroy();
    SAFE_RELEASE( g_pD3DXMesh );
    SAFE_RELEASE( g_pVB );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pVertexLayout );
}


//--------------------------------------------------------------------------------------
// Checks if mouse point hits geometry the scene.
//--------------------------------------------------------------------------------------
HRESULT Pick()
{
    HRESULT hr;
    D3DXVECTOR3 vPickRayDir;
    D3DXVECTOR3 vPickRayOrig;
    const DXGI_SURFACE_DESC* pd3dsdBackBuffer = DXUTGetDXGIBackBufferSurfaceDesc();

    g_nNumIntersections = 0L;

    // Get the pick ray from the mouse position
    if( GetCapture() )
    {
        const D3DXMATRIX* pmatProj = g_Camera.GetProjMatrix();

        POINT ptCursor;
        GetCursorPos( &ptCursor );
        ScreenToClient( DXUTGetHWND(), &ptCursor );

        // Compute the vector of the pick ray in screen space
        D3DXVECTOR3 v;
        v.x = ( ( ( 2.0f * ptCursor.x ) / pd3dsdBackBuffer->Width ) - 1 ) / pmatProj->_11;
        v.y = -( ( ( 2.0f * ptCursor.y ) / pd3dsdBackBuffer->Height ) - 1 ) / pmatProj->_22;
        v.z = 1.0f;

        // Get the inverse view matrix
        const D3DXMATRIX matView = *g_Camera.GetViewMatrix();
        const D3DXMATRIX matWorld = *g_Camera.GetWorldMatrix();
        D3DXMATRIX mWorldView = matWorld * matView;
        D3DXMATRIX m;
        D3DXMatrixInverse( &m, NULL, &mWorldView );

        // Transform the screen space pick ray into 3D space
        vPickRayDir.x = v.x * m._11 + v.y * m._21 + v.z * m._31;
        vPickRayDir.y = v.x * m._12 + v.y * m._22 + v.z * m._32;
        vPickRayDir.z = v.x * m._13 + v.y * m._23 + v.z * m._33;
        vPickRayOrig.x = m._41;
        vPickRayOrig.y = m._42;
        vPickRayOrig.z = m._43;
    }

    // Get the picked triangle
    if( GetCapture() )
    {
        ID3DX10Mesh* pMesh;
        const D3D10_INPUT_ELEMENT_DESC* pDesc = NULL;
        UINT nDeclCount;
        g_pD3DXMesh->GetVertexDescription( &pDesc, &nDeclCount);
        g_pD3DXMesh->CloneMesh( D3DX10_MESH_32_BIT, pDesc[0].SemanticName, pDesc, nDeclCount, &pMesh);

        DWORD* pIndices;
        D3DVERTEX* pVertices;
        pVertices = (D3DVERTEX*)g_Mesh.GetRawVerticesAt(0);
        pIndices = (DWORD*)g_Mesh.GetRawIndicesAt(0);

        if( g_bUseD3DXIntersect )
        {
            // When calling D3DXIntersect, one can get just the closest intersection and not
            // need to work with a D3DXBUFFER.  Or, to get all intersections between the ray and 
            // the mesh, one can use a D3DXBUFFER to receive all intersections.  We show both
            // methods.
            if( !g_bAllHits )
            {
                // Collect only the closest intersection
                UINT nFace;
                FLOAT fBary1, fBary2, fDist;
                UINT nHitCount = 0;
                g_pD3DXMesh->Intersect( &vPickRayOrig, &vPickRayDir, &nHitCount, &nFace, &fBary1, &fBary2, &fDist, NULL);
                if( nHitCount > 0 )
                {
                    g_nNumIntersections = 1;
                    g_IntersectionArray[0].dwFace = nFace;
                    g_IntersectionArray[0].fBary1 = fBary1;
                    g_IntersectionArray[0].fBary2 = fBary2;
                    g_IntersectionArray[0].fDist = fDist;
                }
                else
                {
                    g_nNumIntersections = 0;
                }
            }
            else
            {
                // Collect all intersections
                ID3D10Blob* pBuffer = NULL;
                D3DXINTERSECTINFO* pIntersectInfoArray;
                if( FAILED( hr = g_pD3DXMesh->Intersect( &vPickRayOrig, &vPickRayDir, &g_nNumIntersections, NULL,
                                                         NULL, NULL, NULL, &pBuffer) ) )
                {
                    SAFE_RELEASE( pMesh );
                    return hr;
                }
                if( g_nNumIntersections > 0 )
                {
                    pIntersectInfoArray = ( D3DXINTERSECTINFO* )pBuffer->GetBufferPointer();
                    if( g_nNumIntersections > MAX_INTERSECTIONS )
                        g_nNumIntersections = MAX_INTERSECTIONS;
                    for( DWORD iIntersection = 0; iIntersection < g_nNumIntersections; iIntersection++ )
                    {
                        g_IntersectionArray[iIntersection].dwFace = pIntersectInfoArray[iIntersection].FaceIndex;
                        g_IntersectionArray[iIntersection].fBary1 = pIntersectInfoArray[iIntersection].U;
                        g_IntersectionArray[iIntersection].fBary2 = pIntersectInfoArray[iIntersection].V;
                        g_IntersectionArray[iIntersection].fDist = pIntersectInfoArray[iIntersection].Dist;
                    }
                }
                SAFE_RELEASE( pBuffer );
            }

        }
        else
        {
            // Not using D3DX
            DWORD dwNumFaces = (DWORD)g_Mesh.GetNumIndices(0)/3;
            FLOAT fBary1, fBary2;
            FLOAT fDist;
            for( DWORD i = 0; i < dwNumFaces; i++ )
            {
                D3DXVECTOR3 v0 = pVertices[pIndices[3 * i + 0]].p;
                D3DXVECTOR3 v1 = pVertices[pIndices[3 * i + 1]].p;
                D3DXVECTOR3 v2 = pVertices[pIndices[3 * i + 2]].p;

                // Check if the pick ray passes through this point
                if( IntersectTriangle( vPickRayOrig, vPickRayDir, v0, v1, v2,
                                       &fDist, &fBary1, &fBary2 ) )
                {
                    if( g_bAllHits || g_nNumIntersections == 0 || fDist < g_IntersectionArray[0].fDist )
                    {
                        if( !g_bAllHits )
                            g_nNumIntersections = 0;
                        g_IntersectionArray[g_nNumIntersections].dwFace = i;
                        g_IntersectionArray[g_nNumIntersections].fBary1 = fBary1;
                        g_IntersectionArray[g_nNumIntersections].fBary2 = fBary2;
                        g_IntersectionArray[g_nNumIntersections].fDist = fDist;
                        g_nNumIntersections++;
                        if( g_nNumIntersections == MAX_INTERSECTIONS )
                            break;
                    }
                }
            }
        }

        // Now, for each intersection, add a triangle to g_pVB and compute texture coordinates
        if( g_nNumIntersections > 0 )
        {
            D3DVERTEX* v;
            D3DVERTEX* vThisTri;
            DWORD* iThisTri;
            D3DVERTEX v1, v2, v3;
            INTERSECTION* pIntersection;

            g_pVB->Map( D3D10_MAP_WRITE_DISCARD, 0, ( void** )&v );

            for( DWORD iIntersection = 0; iIntersection < g_nNumIntersections; iIntersection++ )
            {
                pIntersection = &g_IntersectionArray[iIntersection];

                vThisTri = &v[iIntersection * 3];
                iThisTri = &pIndices[3 * pIntersection->dwFace];
                // get vertices hit
                vThisTri[0] = pVertices[iThisTri[0]];
                vThisTri[1] = pVertices[iThisTri[1]];
                vThisTri[2] = pVertices[iThisTri[2]];

                // If all you want is the vertices hit, then you are done.  In this sample, we
                // want to show how to infer texture coordinates as well, using the BaryCentric
                // coordinates supplied by D3DXIntersect
                FLOAT dtu1 = vThisTri[1].tu - vThisTri[0].tu;
                FLOAT dtu2 = vThisTri[2].tu - vThisTri[0].tu;
                FLOAT dtv1 = vThisTri[1].tv - vThisTri[0].tv;
                FLOAT dtv2 = vThisTri[2].tv - vThisTri[0].tv;
                pIntersection->tu = vThisTri[0].tu + pIntersection->fBary1 * dtu1 + pIntersection->fBary2 * dtu2;
                pIntersection->tv = vThisTri[0].tv + pIntersection->fBary1 * dtv1 + pIntersection->fBary2 * dtv2;
            }

            g_pVB->Unmap();
        }


        SAFE_RELEASE( pMesh );
        
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Given a ray origin (orig) and direction (dir), and three vertices of a triangle, this
// function returns TRUE and the interpolated texture coordinates if the ray intersects 
// the triangle
//--------------------------------------------------------------------------------------
bool IntersectTriangle( const D3DXVECTOR3& orig, const D3DXVECTOR3& dir,
                        D3DXVECTOR3& v0, D3DXVECTOR3& v1, D3DXVECTOR3& v2,
                        FLOAT* t, FLOAT* u, FLOAT* v )
{
    // Find vectors for two edges sharing vert0
    D3DXVECTOR3 edge1 = v1 - v0;
    D3DXVECTOR3 edge2 = v2 - v0;

    // Begin calculating determinant - also used to calculate U parameter
    D3DXVECTOR3 pvec;
    D3DXVec3Cross( &pvec, &dir, &edge2 );

    // If determinant is near zero, ray lies in plane of triangle
    FLOAT det = D3DXVec3Dot( &edge1, &pvec );

    D3DXVECTOR3 tvec;
    if( det > 0 )
    {
        tvec = orig - v0;
    }
    else
    {
        tvec = v0 - orig;
        det = -det;
    }

    if( det < 0.0001f )
        return FALSE;

    // Calculate U parameter and test bounds
    *u = D3DXVec3Dot( &tvec, &pvec );
    if( *u < 0.0f || *u > det )
        return FALSE;

    // Prepare to test V parameter
    D3DXVECTOR3 qvec;
    D3DXVec3Cross( &qvec, &tvec, &edge1 );

    // Calculate V parameter and test bounds
    *v = D3DXVec3Dot( &dir, &qvec );
    if( *v < 0.0f || *u + *v > det )
        return FALSE;

    // Calculate t, scale parameters, ray intersects triangle
    *t = D3DXVec3Dot( &edge2, &qvec );
    FLOAT fInvDet = 1.0f / det;
    *t *= fInvDet;
    *u *= fInvDet;
    *v *= fInvDet;

    return TRUE;
}

