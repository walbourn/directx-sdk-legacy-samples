//--------------------------------------------------------------------------------------
// File: ProceduralMaterials.cpp
//
// This sample shows several examples of how to use procedural constructs to generate
// materials.  In addition, it demonstrates how to evaluate normals for combinations of
// materials that may have no analytical derivatives.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include <commdlg.h>
#include "WaitDlg.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                // A model viewing camera
CDXUTDirectionWidget                g_LightControl;
CD3DSettingsDlg                     g_D3DSettingsDlg;        // Device settings dialog
CDXUTDialog                         g_HUD;                   // manages the 3D   
CDXUTDialog                         g_SampleUI;              // dialog for sample specific controls
float                               g_fLightScale = 1.0f;
float                               g_fRTWidth = 0;           // Track rendertarget size for post effects
float                               g_fRTHeight = 0;
CDXUTTextHelper*                    g_pTxtHelper = NULL;      // Text rendering helper

// Direct3D10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTSDKMesh                        g_Mesh10;
ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10InputLayout*                  g_pQuadLayout = NULL;

ID3D10Texture2D*                    g_pPermTexture = NULL;          // random texture for noise lookups
ID3D10ShaderResourceView*           g_pPermTextureSRV = NULL;		
ID3D10Texture1D*                    g_pRandomVecTexture = NULL;     // random vector texture for voronoi noise
ID3D10ShaderResourceView*           g_pRandomVecTextureSRV = NULL;

ID3D10Texture2D*                    g_pScreenPositionTexture = NULL;// Full-screen position texture for normal generation
ID3D10RenderTargetView*             g_pScreenPositionRTV = NULL;
ID3D10ShaderResourceView*           g_pScreenPositionSRV = NULL;
ID3D10Texture2D*                    g_pScreenNormalTexture = NULL;	// Full-screen normal texture for deferred normals
ID3D10RenderTargetView*             g_pScreenNormalRTV = NULL;
ID3D10ShaderResourceView*           g_pScreenNormalSRV = NULL;

ID3D10Buffer*                       g_pQuadVB = NULL;               // VB for the full-screen quad

// Effect related resources
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxPositionDepthTexture = NULL;
ID3D10EffectShaderResourceVariable* g_ptxNormalDepthTexture = NULL;
ID3D10EffectShaderResourceVariable* g_ptxRandomByte = NULL;
ID3D10EffectShaderResourceVariable* g_ptxRandVector = NULL;

ID3D10EffectVectorVariable*         g_pvLightDir = NULL;
ID3D10EffectVectorVariable*         g_pvLightDiffuse = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectVectorVariable*         g_pvEyePt = NULL;
ID3D10EffectVectorVariable*         g_pvRenderTargetSize = NULL;
ID3D10EffectScalarVariable*         g_pfSliderVal = NULL;
ID3D10EffectScalarVariable*         g_pbUseDDXDDY = NULL;

ID3D10EffectTechnique*              g_pCurrentTech = NULL;
ID3D10EffectTechnique*              g_pCreateNormals = NULL;

// Sliders for the UI
#define NUM_SLIDERS 4
float g_fMaxSliderVal[NUM_SLIDERS] = {1,10,10,4};
float g_fSliderVal[NUM_SLIDERS] = {0.5f,6.0f,1.0f,1.0f};
bool                                g_bUseDDXDDY = FALSE;
WCHAR g_szCurrentTech[MAX_PATH] = {0};

// Define a the vertex structure for a fullscreen quad
struct QUAD_VERTEX
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 UV;
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               2
#define IDC_CHANGEDEVICE            3
#define IDC_LOAD_MESH               4
#define IDC_TECHNIQUE               5
#define IDC_USEDDXDDY               6
#define IDC_RELOADEFFECT            7
#define IDC_SLIDER_STATIC           8
#define IDC_TOGGLEWARP              9
#define IDC_SLIDER                  10

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
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();
HRESULT RecreateEffect( ID3D10Device* pd3dDevice );
HRESULT CreateRandomPermTexture( ID3D10Device* pd3dDevice );
HRESULT CreateRandomVectorTexture( ID3D10Device* pd3dDevice );

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
    DXUTGetD3D10Enumeration( false, true );

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Procedural Materials" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Init the light
    g_LightControl.SetLightDirection( D3DXVECTOR3( 0.547f, -0.547f, 0.547f ) );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    WCHAR sz[100];

    iY += 24;
    g_SampleUI.AddButton( IDC_LOAD_MESH, L"Load Mesh", 35, iY += 24, 125, 22 );
    g_SampleUI.AddComboBox( IDC_TECHNIQUE, -50, iY += 24, 210, 22 );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_USEDDXDDY, L"Use DDX / DDY", 35, iY += 24, 125, 22, g_bUseDDXDDY );

    iY += 24;
    g_SampleUI.AddButton( IDC_RELOADEFFECT, L"Reload Effect", 35, iY += 24, 125, 22 );

    iY += 24;
    for( int i = 0; i < NUM_SLIDERS; i++ )
    {
        swprintf_s( sz, 100, L"Slider %d: %.2f", i, g_fSliderVal[i] );
        g_SampleUI.AddStatic( IDC_SLIDER_STATIC + i, sz, 35, iY += 24, 125, 22 );
        g_SampleUI.AddSlider( IDC_SLIDER + i, 50, iY += 24, 100, 22, 0, ( int )( g_fMaxSliderVal[i] * 1000.0f ),
                              ( int )( g_fSliderVal[i] * 1000.0f ) );
    }

    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -10.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( 3.0f, 0.1f, 20.0f );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Disable vsync since we want to see framerate differences between generating normals using ddx/ddy and using
    // a fullscreen pass
    pDeviceSettings->d3d10.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
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

    // The light control gets the next shot at handling messages
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

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
        case IDC_LOAD_MESH:
        {
            WCHAR szCurrentDir[MAX_PATH];
            GetCurrentDirectory( MAX_PATH, szCurrentDir );

            OPENFILENAME ofn;
            WCHAR szFile[MAX_PATH];
            ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
            ofn.lStructSize = sizeof( OPENFILENAME );
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = 0;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"SDKMesh file\0*.sdkmesh\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            BOOL bRet = GetOpenFileName( &ofn );
            SetCurrentDirectory( szCurrentDir );

            if( bRet )
            {
                g_Mesh10.Destroy();
                g_Mesh10.Create( DXUTGetD3D10Device(), szFile );
            }
        }
            break;

        case IDC_TECHNIQUE:
        {
            CDXUTComboBox* pComboBox = ( CDXUTComboBox* )pControl;
            g_pCurrentTech = ( ID3D10EffectTechnique* )pComboBox->GetSelectedData();

            wcscpy_s( g_szCurrentTech, MAX_PATH, pComboBox->GetSelectedItem()->strText );
        }
            break;

        case IDC_USEDDXDDY:
        {
            g_bUseDDXDDY = !g_bUseDDXDDY;

            // Setup the shader constant that turns DDXDDY on and off
            g_pbUseDDXDDY->SetBool( g_bUseDDXDDY );
        }
            break;

        case IDC_RELOADEFFECT:
        {
            RecreateEffect( DXUTGetD3D10Device() );
        }
            break;

        case IDC_SLIDER:
        case IDC_SLIDER+1:
        case IDC_SLIDER+2:
        case IDC_SLIDER+3:
            {
                int i = nControlID - IDC_SLIDER;
                g_fSliderVal[i] = ( float )( g_SampleUI.GetSlider( nControlID )->GetValue() ) / 1000.0f;

                WCHAR sz[100];
                swprintf_s( sz, 100, L"Slider %d: %.2f", i, g_fSliderVal[i] );
                g_SampleUI.GetStatic( IDC_SLIDER_STATIC + i )->SetText( sz );

                // Set the slider values in the Effect
                g_pfSliderVal->SetFloatArray( g_fSliderVal, 0, NUM_SLIDERS );
            }
            break;
    }

}

//--------------------------------------------------------------------------------------
// Recreate the effect.  This code generally lives in OnD3D10CreateDevice, but we've
// pulled it into into its own function to facilitate reloading the effect without 
// exiting the app.
//--------------------------------------------------------------------------------------
HRESULT RecreateEffect( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Release the old effect
    g_pCurrentTech = NULL;
    SAFE_RELEASE( g_pEffect10 );

    // Shader flags
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Make sure that NUM_SLIDERS is defined the same in both the effect file and the app
    char strNumSliders[MAX_PATH];
    sprintf_s( strNumSliders, MAX_PATH, "%d", NUM_SLIDERS );
    D3D10_SHADER_MACRO macros[2] =
    {
        { "NUM_SLIDERS", strNumSliders },
        { NULL, NULL }
    };

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ProceduralMaterials.fx" ) );

    CWaitDlg CompilingShadersDlg;
    CompilingShadersDlg.ShowDialog( L"Compiling Shaders: This could take several minutes" );

    hr = D3DX10CreateEffectFromFile(  str,
                                      macros,
                                      NULL,
                                      "fx_4_0",    // Note, there is no reason that this effect cannot be compiled up to fx_4_1 in the future
                                      dwShaderFlags,
                                      0, pd3dDevice,
                                      NULL,
                                      NULL,
                                      &g_pEffect10,
                                      NULL,
                                      NULL );

    CompilingShadersDlg.DestroyDialog();

    V_RETURN( hr );

    // Obtain fixed techniques
    g_pCreateNormals = g_pEffect10->GetTechniqueByName( "CreateNormals" );

    // Obtain variables
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_MeshTexture" )->AsShaderResource();
    g_ptxPositionDepthTexture = g_pEffect10->GetVariableByName( "g_PositionDepthTexture" )->AsShaderResource();
    g_ptxNormalDepthTexture = g_pEffect10->GetVariableByName( "g_NormalDepthTexture" )->AsShaderResource();
    g_ptxRandomByte = g_pEffect10->GetVariableByName( "g_txRandomByte" )->AsShaderResource();
    g_ptxRandVector = g_pEffect10->GetVariableByName( "g_txRandVector" )->AsShaderResource();
    g_pvLightDir = g_pEffect10->GetVariableByName( "g_vLightDir" )->AsVector();
    g_pvLightDiffuse = g_pEffect10->GetVariableByName( "g_vLightDiffuse" )->AsVector();
    g_pmWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmViewProjection = g_pEffect10->GetVariableByName( "g_mViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_pvRenderTargetSize = g_pEffect10->GetVariableByName( "g_vRenderTargetSize" )->AsVector();
    g_pfSliderVal = g_pEffect10->GetVariableByName( "g_fSliderVal" )->AsScalar();
    g_pbUseDDXDDY = g_pEffect10->GetVariableByName( "g_bUseDDXDDY" )->AsScalar();

    // Reset the combo box
    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_TECHNIQUE );
    pComboBox->RemoveAllItems();
    pComboBox->SetDropHeight( 200 );

    // Enumerate the remaining techniques and populate the combo box
    int iSelected = -1;
    D3D10_EFFECT_DESC EffectDesc;
    g_pEffect10->GetDesc( &EffectDesc );

    // Loop over the effect techniques starting at 1.  We start at 1 because technique 0 is reserved for generating the normal buffer
    // from the position buffer.
    for( UINT i = 1; i < EffectDesc.Techniques; i++ )
    {
        // Get the technique
        ID3D10EffectTechnique* pTech = g_pEffect10->GetTechniqueByIndex( i );

        // If we don't have a current technique, then this one is a good candidate
        if( !g_pCurrentTech )
            g_pCurrentTech = pTech;

        // Get the name and add it to the combo box
        D3D10_TECHNIQUE_DESC TechDesc;
        pTech->GetDesc( &TechDesc );
        WCHAR szWide[MAX_PATH];
        ZeroMemory( szWide, sizeof( WCHAR ) * MAX_PATH );
        MultiByteToWideChar( CP_ACP, 0, TechDesc.Name, ( int )strlen( TechDesc.Name ), szWide, MAX_PATH );
        pComboBox->AddItem( szWide, ( void* )pTech );

        // Was this the technique we were previously using before we reloaded the effect?
        if( wcscmp( g_szCurrentTech, szWide ) == 0 )
            iSelected = ( int )( i - 1 );
    }

    // Reset the previous technique if it was there the last time we loaded this effect
    if( iSelected != -1 )
    {
        pComboBox->SetSelectedByIndex( iSelected );
        g_pCurrentTech = ( ID3D10EffectTechnique* )pComboBox->GetSelectedData();
    }

    // Bind the 2D permutation texture that we lookup for Simplex noise
    g_ptxRandomByte->SetResource( g_pPermTextureSRV );
    // Bind the random vector texture that we use for Voronoi noise
    g_ptxRandVector->SetResource( g_pRandomVecTextureSRV );
    // Set the slider values in the effect
    g_pfSliderVal->SetFloatArray( g_fSliderVal, 0, NUM_SLIDERS );
    g_pbUseDDXDDY->SetBool( g_bUseDDXDDY );

    return hr;
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

    // DXUT init
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Light control widget
    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );
    g_LightControl.SetRadius( 1.0f );

    // Seed our random number generator before creating these random textures
    srand( timeGetTime() );

    // Create random textures
    V_RETURN( CreateRandomPermTexture( pd3dDevice ) );
    V_RETURN( CreateRandomVectorTexture( pd3dDevice ) );

    // Create the effect
    V_RETURN( RecreateEffect( pd3dDevice ) );

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Grab a pass description from the technique so that we have a shader signature to validate against when calling CreateInputLayout
    D3D10_PASS_DESC PassDesc;
    V_RETURN( g_pCurrentTech->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    if( PassDesc.IAInputSignatureSize == 0 )
    {
        // If the first pass is NULL, fallback to the next pass, since not all techniques implement the first pass.
        // In the effect file, the technique can either be a bump mapped technique or a completely diffuse technique.
        // The first pass generates normals (if g_bUseDDXDDY is false).  If a technique doesn't require normals, then there
        // will be no first pass.
        V_RETURN( g_pCurrentTech->GetPassByIndex( 1 )->GetDesc( &PassDesc ) );
    }
    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Create our quad input layout
    const D3D10_INPUT_ELEMENT_DESC layoutquad[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( g_pCreateNormals->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( layoutquad, ARRAYSIZE( layoutquad ), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    // Load the default mesh
    V_RETURN( g_Mesh10.Create( pd3dDevice, L"misc\\cube.sdkmesh", true ) );

    // Create the fullscreen quad we use for post effects
    QUAD_VERTEX verts[4] =
    {
        { D3DXVECTOR3( -1, -1, 0.5f ), D3DXVECTOR2( 0, 0 ) },
        { D3DXVECTOR3( -1, 1, 0.5f ), D3DXVECTOR2( 0, 1 ) },
        { D3DXVECTOR3( 1, -1, 0.5f ), D3DXVECTOR2( 1, 0 ) },
        { D3DXVECTOR3( 1, 1, 0.5f ), D3DXVECTOR2( 1, 1 ) },
    };
    D3D10_BUFFER_DESC Desc;
    Desc.ByteWidth = 4 * sizeof( QUAD_VERTEX );
    Desc.Usage = D3D10_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = verts;
    InitData.SysMemPitch = Desc.ByteWidth;
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, &InitData, &g_pQuadVB ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 100.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    // Hud setup relative to screen space coordinates
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 650 );
    g_SampleUI.SetSize( 170, 300 );

    // Track the backbuffer height/width for fullscreen effects
    g_fRTWidth = ( float )pBackBufferSurfaceDesc->Width;
    g_fRTHeight = ( float )pBackBufferSurfaceDesc->Height;

    // Change the fullscreen position and normal textures
    D3D10_TEXTURE2D_DESC Desc;
    Desc.Width = pBackBufferSurfaceDesc->Width;
    Desc.Height = pBackBufferSurfaceDesc->Height;
    Desc.MipLevels = 1;   // Only 1 mip level since this will always be rendered at the screen resolution
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // We want good precision, so we evaluate the positions as full 32bit floats
    Desc.SampleDesc = pBackBufferSurfaceDesc->SampleDesc;
    Desc.Usage = D3D10_USAGE_DEFAULT;
    Desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    // Create a render target view so we can render to the fullscreen position texture
    D3D10_RENDER_TARGET_VIEW_DESC DescRTV;
    DescRTV.Format = Desc.Format;
    DescRTV.Texture2D.MipSlice = 0;
    if( Desc.SampleDesc.Count > 1 )
        DescRTV.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
    else
        DescRTV.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;

    // Create a shader resource view so we can sample from the fullscreen position texture
    D3D10_SHADER_RESOURCE_VIEW_DESC DescSRV;
    DescSRV.Format = Desc.Format;
    DescSRV.Texture2D.MipLevels = 1;
    DescSRV.Texture2D.MostDetailedMip = 0;
    if( Desc.SampleDesc.Count > 1 )
        DescSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
    else
        DescSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;

    // First create position texture, rendertargetview, shaderresourceview
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pScreenPositionTexture ) );
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pScreenPositionTexture, &DescRTV, &g_pScreenPositionRTV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pScreenPositionTexture, &DescSRV, &g_pScreenPositionSRV ) );

    // Same for the normal texture
    Desc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;  // Use a signed texture format so that we don't have to bias and unbias the normals
    DescRTV.Format = Desc.Format;
    DescSRV.Format = Desc.Format;
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pScreenNormalTexture ) );
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pScreenNormalTexture, &DescRTV, &g_pScreenNormalRTV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pScreenNormalTexture, &DescSRV, &g_pScreenNormalSRV ) );
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Renders a subset of a mesh with the specified technique and using the specified pass
//--------------------------------------------------------------------------------------
void RenderMesh( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, UINT iSubset, UINT iPass )
{
    //InputAssembler setup
    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    UINT Strides[1];   // Why use arrays of [1] here?  IASetVertexBuffers expects array input.  We could just use define UINT Stride, and use that
    UINT Offsets[1];   // but it's good to get into the habit of binding arrays.  It also saves API calls when binding multiple resources.
    ID3D10Buffer* pVB[1];
    pVB[0] = g_Mesh10.GetVB10( 0, 0 );
    Strides[0] = ( UINT )g_Mesh10.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_Mesh10.GetIB10( 0 ), g_Mesh10.GetIBFormat10( 0 ), 0 );

    //Get the technique description
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    if( iPass >= techDesc.Passes )
        return;

    if( iSubset >= g_Mesh10.GetNumSubsets( 0 ) )
        return;

    // Get the subset
    pSubset = g_Mesh10.GetSubset( 0, iSubset );

    // Set topology
    PrimType = CDXUTSDKMesh::GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
    pd3dDevice->IASetPrimitiveTopology( PrimType );

    // Draw the pass specified by iPass
    ID3D10EffectPass* pPass = pTechnique->GetPassByIndex( iPass );
    D3D10_PASS_DESC PassDesc;
    pPass->GetDesc( &PassDesc );
    if( PassDesc.IAInputSignatureSize > 0 )
    {
        pTechnique->GetPassByIndex( iPass )->Apply( 0 );
        pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                 ( UINT )pSubset->VertexStart );
    }

}

//--------------------------------------------------------------------------------------
// Post process that generates a fullscreen normal texture from an input texture
// containing only position data.
//--------------------------------------------------------------------------------------
void CalculateNormals( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique )
{
    // Setup for drawing a fullscreen quad
    pd3dDevice->IASetInputLayout( g_pQuadLayout );
    UINT stride = sizeof( QUAD_VERTEX );
    UINT offset = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVB, &stride, &offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // Bind the position texture
    g_ptxPositionDepthTexture->SetResource( g_pScreenPositionSRV );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    for( UINT iPass = 0; iPass < techDesc.Passes; iPass++ )
    {
        pTechnique->GetPassByIndex( iPass )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }
}

//--------------------------------------------------------------------------------------
// Set WorldViewProjection for the mesh
//--------------------------------------------------------------------------------------
void SetWVPForMesh( UINT i, D3DXMATRIX* pmView, D3DXMATRIX* pmProj )
{
    float fHeight = g_fSliderVal[3];
    D3DXMATRIX mWorld;
    if( i == 0 )
    {
        D3DXMatrixScaling( &mWorld, 1, fHeight, 1 );
    }
    else
    {
        D3DXMATRIX mScale;
        D3DXMATRIX mTrans;
        float fCapScale = 1.0f + g_fSliderVal[3] * 0.2f;
        D3DXMatrixScaling( &mScale, fCapScale, fCapScale, fCapScale );

        D3DXMatrixTranslation( &mTrans, 0, 0.75f * fHeight, 0 );
        mWorld = mScale * mTrans;
    }

    D3DXMATRIX mWorldViewProjection = mWorld * ( *pmView ) * ( *pmProj );
    D3DXMATRIX mViewProjection = ( *pmView ) * ( *pmProj );

    g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection );
    g_pmViewProjection->SetMatrix( ( float* )&mViewProjection );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Only render if the effect loaded properly
    if( g_pEffect10 )
    {
        D3DXMATRIX mWorldViewProjection;
        D3DXVECTOR4 vLightDir;
        D3DXVECTOR4 vLightDiffuse;
        D3DXMATRIX mWorld;
        D3DXMATRIX mView;
        D3DXMATRIX mProj;

        // Set the view specific data from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();
        D3DXVECTOR4 vEyePt = D3DXVECTOR4( *g_Camera.GetEyePt(), 1 );
        mWorldViewProjection = mWorld * mView * mProj;
        V( g_pvEyePt->SetFloatVector( ( float* )&vEyePt ) );

        // Set the light params
        vLightDir = D3DXVECTOR4( -g_LightControl.GetLightDirection(), 1 );
        vLightDiffuse = g_fLightScale * D3DXVECTOR4( 1, 1, 1, 1 );
        V( g_pvLightDir->SetFloatVector( ( float* )&vLightDir ) );
        V( g_pvLightDiffuse->SetFloatVector( ( float* )&vLightDiffuse ) );
        V( g_pfTime->SetFloat( ( float )fTime ) );

        // Set the rendertarget size
        D3DXVECTOR4 vRTSize;
        vRTSize.x = g_fRTWidth;
        vRTSize.y = g_fRTHeight;
        V( g_pvRenderTargetSize->SetFloatVector( ( float* )vRTSize ) );

        UINT NumSubsets = g_Mesh10.GetNumSubsets( 0 );
        if( !g_bUseDDXDDY )
        {
            // If we're not using DDX/DDY, we need to render the object and then do a fullscreen pass
            // to generate the normal buffer.  See the sample documentaiton for an the differences between
            // using DDX/DDY and doing a fullscreen pass to generate the normal buffer.

            // Store the old tender target and depth stencil texture
            ID3D10RenderTargetView* pOldRTV = NULL;
            ID3D10DepthStencilView* pOldDSV = NULL;
            pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

            // Set the position RT and DS
            float PositionClear[4] = { 0, 0, 0, 0 };
            pd3dDevice->ClearRenderTargetView( g_pScreenPositionRTV, PositionClear );
            pd3dDevice->OMSetRenderTargets( 1, &g_pScreenPositionRTV, pOldDSV );

            // For each mesh, render the position augmented by the shader into the rendertarget
            for( UINT i = 0; i < NumSubsets; i++ )
            {
                SetWVPForMesh( i, &mView, &mProj );
                // We select pass i*2 here because the even passes for a technique generate normals and the
                // and the odd passes do the diffuse part.
                RenderMesh( pd3dDevice, g_pCurrentTech, i, i * 2 );
            }

            // Set normal RT and DS
            pd3dDevice->OMSetRenderTargets( 1, &g_pScreenNormalRTV, pOldDSV );

            // Calculate the normals using a fullscreen pass
            CalculateNormals( pd3dDevice, g_pCreateNormals );

            // Reset the old RT
            pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
            SAFE_RELEASE( pOldRTV );
            SAFE_RELEASE( pOldDSV );
        }

        // Render the diffuse portion of the mesh
        g_ptxNormalDepthTexture->SetResource( g_pScreenNormalSRV );

        for( UINT i = 0; i < NumSubsets; i++ )
        {
            SetWVPForMesh( i, &mView, &mProj );
            // We select pass i*2 here because the even passes for a technique generate normals and the
            // and the odd passes do the diffuse part.
            RenderMesh( pd3dDevice, g_pCurrentTech, i, 1 + i * 2 );
        }
    }

    // Reset to NULL to avoid any hazard tracking that D3D might do if a buffer is bound to both input and output
    ID3D10ShaderResourceView* pViews[4] = {0,0,0,0};
    pd3dDevice->PSSetShaderResources( 0, 4, pViews );

    // Render the UI
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_RELEASE( g_pScreenPositionTexture );
    SAFE_RELEASE( g_pScreenPositionRTV );
    SAFE_RELEASE( g_pScreenPositionSRV );
    SAFE_RELEASE( g_pScreenNormalTexture );
    SAFE_RELEASE( g_pScreenNormalRTV );
    SAFE_RELEASE( g_pScreenNormalSRV );
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
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pQuadLayout );
    SAFE_RELEASE( g_pQuadVB );
    g_Mesh10.Destroy();

    SAFE_RELEASE( g_pPermTextureSRV );
    SAFE_RELEASE( g_pPermTexture );
    SAFE_RELEASE( g_pRandomVecTexture );
    SAFE_RELEASE( g_pRandomVecTextureSRV );
}

//--------------------------------------------------------------------------------------
// This helper function creates a 2D texture full of random bytes
//--------------------------------------------------------------------------------------
HRESULT CreateRandomPermTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Create our 256 x 256 random number texture up front
    BYTE data[256 * 256];
    for( int i = 0; i < 256 * 256; i++ )
        data[i] = ( BYTE )( rand() % 256 );

    // When we create the texture, pass in the data
    D3D10_TEXTURE2D_DESC desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_TYPELESS; // We can use typeless resource here as long as we make sure that give it an explicite type
    desc.SampleDesc.Count = 1;             // when we create a view on it.
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA dataDesc;
    dataDesc.pSysMem = data;
    dataDesc.SysMemPitch = 256;
    V_RETURN( pd3dDevice->CreateTexture2D( &desc, &dataDesc, &g_pPermTexture ) );

    // Create the shader resource view so we can bind the texture
    D3D10_SHADER_RESOURCE_VIEW_DESC descSRV;
    ZeroMemory( &descSRV, sizeof( descSRV ) );
    descSRV.Format = DXGI_FORMAT_R8_UNORM;
    descSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    descSRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pPermTexture, &descSRV, &g_pPermTextureSRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors
//--------------------------------------------------------------------------------------
HRESULT CreateRandomVectorTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Create the random data up front
    float data[4096];
    for( int i = 0; i < 4096; i++ )
        data[i] = float( rand() - ( RAND_MAX / 2 ) ) / ( float )( RAND_MAX / 2 );

    // Create the texture with the data pass in
    D3D10_TEXTURE1D_DESC dstex;
    dstex.Width = 1024;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = data;
    InitData.SysMemPitch = 1024 * sizeof( float );
    InitData.SysMemSlicePitch = 1024 * sizeof( float );
    V_RETURN( pd3dDevice->CreateTexture1D( &dstex, &InitData, &g_pRandomVecTexture ) );

    // Create the resource view so that we can bind this
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
    SRVDesc.Texture2D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomVecTexture, &SRVDesc, &g_pRandomVecTextureSRV ) );

    return hr;
}