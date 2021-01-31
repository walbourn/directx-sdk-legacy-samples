//--------------------------------------------------------------------------------------
// File: MeshFromOBJ10.cpp
//
// This sample shows how an ID3DXMesh object can be created from mesh data stored in an 
// .obj file. It's convenient to use .x files when working with ID3DXMesh objects since 
// D3DX can create and fill an ID3DXMesh object directly from an .x file; however, it’s 
// also easy to initialize an ID3DXMesh object with data gathered from any file format 
// or memory resource.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#pragma warning(disable: 4995)
#include "meshloader10.h"
#pragma warning(default: 4995)
#include "SDKmisc.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;

CDXUTTextHelper*                    g_pTxtHelper = NULL;

ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10EffectTechnique*              g_pTechnique = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuseVariable = NULL;

ID3D10EffectVectorVariable*         g_pAmbient = NULL;
ID3D10EffectVectorVariable*         g_pDiffuse = NULL;
ID3D10EffectVectorVariable*         g_pSpecular = NULL;
ID3D10EffectScalarVariable*         g_pOpacity = NULL;
ID3D10EffectScalarVariable*         g_pSpecularPower = NULL;
ID3D10EffectVectorVariable*         g_pLightColor = NULL;
ID3D10EffectVectorVariable*         g_pLightPosition = NULL;
ID3D10EffectVectorVariable*         g_pCameraPosition = NULL;
ID3D10EffectScalarVariable*         g_pTime = NULL;
ID3D10EffectMatrixVariable*         g_pWorld = NULL;
ID3D10EffectMatrixVariable*         g_pWorldViewProjection = NULL;

bool                        g_bShowHelp = true;      // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                   // dialog for standard controls
CDXUTDialog                 g_SampleUI;              // dialog for sample specific controls

CMeshLoader10                       g_MeshLoader;            // Loads a mesh from an .obj file
CModelViewerCamera                  g_Camera;                // A model viewing camera

WCHAR g_strFileSaveMessage[MAX_PATH] = {0}; // Text indicating file write success/failure

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC              -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_SUBSET              5
#define IDC_TOGGLEWARP          6

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

void InitApp();
void RenderText();
void RenderSubset( UINT iSubset );

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

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    // Set DXUT callbacks
    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"MeshFromOBJ10" );
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
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent );

    // Title font for comboboxes
    g_SampleUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    CDXUTElement* pElement = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    g_SampleUI.AddStatic( IDC_STATIC, L"(S)ubset", 20, 0, 105, 25 );
    g_SampleUI.AddComboBox( IDC_SUBSET, 20, 25, 140, 24, 'S' );
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
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );


    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"MeshFromOBJ10.fx" ) );
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

    // Obtain the technique
    g_pTechnique = g_pEffect10->GetTechniqueByName( "NoSpecular" );
    g_ptxDiffuseVariable = g_pEffect10->GetVariableByName( "g_MeshTexture" )->AsShaderResource();    
    
    g_pAmbient = g_pEffect10->GetVariableByName( "g_vMaterialAmbient" )->AsVector();
    g_pDiffuse = g_pEffect10->GetVariableByName( "g_vMaterialDiffuse" )->AsVector();
    g_pSpecular = g_pEffect10->GetVariableByName( "g_vMaterialSpecular" )->AsVector();
    g_pOpacity = g_pEffect10->GetVariableByName( "g_fMaterialAlpha" )->AsScalar();
    g_pSpecularPower = g_pEffect10->GetVariableByName( "g_nMaterialShininess" )->AsScalar();
    g_pLightColor = g_pEffect10->GetVariableByName( "g_vLightColor" )->AsVector();
    g_pLightPosition = g_pEffect10->GetVariableByName( "g_vLightPosition" )->AsVector();
    g_pCameraPosition = g_pEffect10->GetVariableByName( "g_vCameraPosition" )->AsVector();
    g_pTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();


    // Define the input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    } ;
    UINT numElements = sizeof( layout ) / sizeof( layout[0] );

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    g_pTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Load the mesh
    V_RETURN( g_MeshLoader.Create( pd3dDevice, L"media\\cup.obj" ) );

    // Add the identified subsets to the UI
    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_SUBSET );
    pComboBox->RemoveAllItems();
    pComboBox->AddItem( L"All", ( void* )( INT_PTR )-1 );

    for ( UINT subset = 0; subset < g_MeshLoader.GetNumSubsets(); ++subset )
    {
        Material* pMaterial = g_MeshLoader.GetSubsetMaterial( subset );
        pComboBox->AddItem( pMaterial->strName, ( void* )( INT_PTR )subset );
    }    

    // Store the correct technique for each material
    for ( UINT i = 0; i < g_MeshLoader.GetNumMaterials(); ++i )
    {
        Material* pMaterial = g_MeshLoader.GetMaterial( i );

        const char* strTechnique = "";

        if( pMaterial->pTextureRV10 && pMaterial->bSpecular )
            strTechnique = "TexturedSpecular";
        else if( pMaterial->pTextureRV10 && !pMaterial->bSpecular )
            strTechnique = "TexturedNoSpecular";
        else if( !pMaterial->pTextureRV10 && pMaterial->bSpecular )
            strTechnique = "Specular";
        else if( !pMaterial->pTextureRV10 && !pMaterial->bSpecular )
            strTechnique = "NoSpecular";

        pMaterial->pTechnique = g_pEffect10->GetTechniqueByName( strTechnique );
    }

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 2.0f, 1.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

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
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBufferSurfaceDesc ) );
    
    // Setup the camera's projection parameters
    float fAspectRatio = pBufferSurfaceDesc->Width / ( FLOAT )pBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBufferSurfaceDesc->Width, pBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBufferSurfaceDesc->Width - 170, pBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }
    
    //
    // Clear the back buffer
    //
    float ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f } ; // red, green, blue, alpha
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    //
    // Clear the depth stencil
    //
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    HRESULT hr;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;

    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera.GetWorldMatrix();
    mView = *g_Camera.GetViewMatrix();
    mProj = *g_Camera.GetProjMatrix();

    mWorldViewProjection = mWorld * mView * mProj;

    // Update the effect's variables. 
    V( g_pWorldViewProjection->SetMatrix( (float*)&mWorldViewProjection ) );
    V( g_pWorld->SetMatrix( (float*)&mWorld ) );
    V( g_pTime->SetFloat( (float)fTime ) );
    V( g_pCameraPosition->SetFloatVector( (float*)g_Camera.GetEyePt() ) );   

    //
    // Set the Vertex Layout
    //
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    UINT iCurSubset = ( UINT )( INT_PTR )g_SampleUI.GetComboBox( IDC_SUBSET )->GetSelectedData();

    //
    // Render the mesh
    //
    if ( iCurSubset == -1 )
    {
        for ( UINT iSubset = 0; iSubset < g_MeshLoader.GetNumSubsets(); ++iSubset )
        {
            RenderSubset( iSubset );
        }
    } else
    {
        RenderSubset( iCurSubset );
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );    
    DXUT_EndPerfEvent();
}

//--------------------------------------------------------------------------------------
void RenderSubset( UINT iSubset )
{
    HRESULT hr;
    
    Material* pMaterial = g_MeshLoader.GetSubsetMaterial( iSubset );

    V( g_pAmbient->SetFloatVector( pMaterial->vAmbient ) );
    V( g_pDiffuse->SetFloatVector( pMaterial->vDiffuse ) );
    V( g_pSpecular->SetFloatVector( pMaterial->vSpecular ) );
    V( g_pOpacity->SetFloat( pMaterial->fAlpha ) );
    V( g_pSpecularPower->SetInt( pMaterial->nShininess ) );

    if ( !IsErrorResource( pMaterial->pTextureRV10 ) )
        g_ptxDiffuseVariable->SetResource( pMaterial->pTextureRV10 );

    D3D10_TECHNIQUE_DESC techDesc;
    pMaterial->pTechnique->GetDesc( &techDesc );

    for ( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pMaterial->pTechnique->GetPassByIndex(p)->Apply(0);
        g_MeshLoader.GetMesh()->DrawSubset(iSubset);
    }
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( g_strFileSaveMessage );

    // Draw help
    if( g_bShowHelp )
    {
        UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        g_pTxtHelper->SetInsertionPos( 10, nBackBufferHeight - 15 * 5 );
        g_pTxtHelper->SetForegroundColor( D3DCOLOR_ARGB( 200, 150, 150, 150 ) );
        g_pTxtHelper->DrawTextLine( L"Controls (F1 to hide):" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 15 * 4 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate camera: Right mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );

        g_pTxtHelper->SetInsertionPos( 250, nBackBufferHeight - 15 * 4 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n" );
        g_pTxtHelper->DrawTextLine( L"Quit: ESC\n" );
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    g_pTxtHelper->End();
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
    case IDC_TOGGLEWARP:
        DXUTToggleWARP(); break;
    case IDC_TOGGLEREF:
        DXUTToggleREF(); break;
    case IDC_CHANGEDEVICE:
        g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;    
    }
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
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_RELEASE( g_pEffect10 );
    g_MeshLoader.Destroy();
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D10_DEVICE );

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
    
    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
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