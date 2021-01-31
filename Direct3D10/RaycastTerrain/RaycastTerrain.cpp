//--------------------------------------------------------------------------------------
// File: DeferredParticles.cpp
//
// This sample shows a simple example of the Microsoft Direct3D's High-Level 
// Shader Language (HLSL) using the Effect interface. 
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
#include "Terrain.h"

#define MAX_LIGHTS 1
//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDirectionWidget g_LightControl[MAX_LIGHTS];
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D   
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
D3DXMATRIXA16                       g_mCenterMesh;
float                               g_fLightScale;
int                                 g_nNumActiveLights;
int                                 g_nActiveLight;
bool                                g_bShowHelp = false;    // If true, it renders the UI control text
bool                                g_bRenderShadows = true;
bool                                g_bRenderDetail = false;
float                               g_fShadowBias = 0.003f;
float                               g_fDetailAmount = 0.01f;
float                               g_fDetailRepeat = 35.0f;
float                               g_fDetailDistance = 1.0f;
float                               g_fHeightRatio;
CTerrain                            g_Terrain;
bool                                g_bRenderGeometry = false;
bool                                g_bWireframe = false;

float                               g_fMapHeight = 200.0f;
float                               g_fMapWidth = 2000.0f;
int                                 g_ShadowMapSize = 1024;
D3DXVECTOR3*                        g_pvBoxPositions = NULL;
int                                 g_NumBoxes = 20;

// Direct3D10 resources
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;

ID3D10InputLayout*                  g_pBoxLayout = NULL;
ID3D10InputLayout*                  g_pMeshLayout = NULL;
ID3D10Buffer*                       g_pScreenQuadVB = NULL;
CDXUTSDKMesh                        g_BoxMesh;

ID3D10Effect*                       g_pEffect10 = NULL;

// Raycast terrain techniques
ID3D10EffectTechnique*              g_pRenderTerrain_Inside = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Outside = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Ortho = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Inside_Detail = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Outside_Detail = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Ortho_Detail = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Inside_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Outside_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Ortho_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Inside_Detail_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Outside_Detail_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Ortho_Detail_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderTerrain_Wire = NULL;

// Geometry terrain techniques
ID3D10EffectTechnique*              g_pRenderGeometryTerrain = NULL;
ID3D10EffectTechnique*              g_pRenderGeometryTerrain_Shadow = NULL;
ID3D10EffectTechnique*              g_pRenderGeometryTerrain_Wire = NULL;
ID3D10EffectTechnique*              g_pRenderMesh = NULL;
ID3D10EffectTechnique*              g_pRenderMesh_Shadow = NULL;

ID3D10ShaderResourceView*           g_pDetailHeightTexture = NULL;
ID3D10ShaderResourceView*           g_pDetailColorTexture[4] =
{
    NULL, NULL, NULL, NULL
};
ID3D10ShaderResourceView*           g_pDetailGrad_RedGreen = NULL;
ID3D10ShaderResourceView*           g_pDetailGrad_BlueAlpha = NULL;

// Shadows
ID3D10Texture2D*                    g_pShadowDepthTexture = NULL;
ID3D10DepthStencilView*             g_pShadowDSV = NULL;
ID3D10ShaderResourceView*           g_pShadowSRV = NULL;

ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDetailDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDetailGrad_RedGreen = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDetailGrad_BlueAlpha = NULL;
ID3D10EffectShaderResourceVariable* g_ptxHeight = NULL;
ID3D10EffectShaderResourceVariable* g_ptxMask = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDetailHeight = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDepthMap = NULL;

ID3D10EffectVectorVariable*         g_pLightDir = NULL;
ID3D10EffectVectorVariable*         g_pLightDirTex = NULL;
ID3D10EffectVectorVariable*         g_pLightDiffuse = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldToTerrain = NULL;
ID3D10EffectMatrixVariable*         g_pmTexToViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmLightViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmTexToLightViewProj = NULL;
ID3D10EffectVectorVariable*         g_pvTextureEyePt = NULL;

ID3D10EffectScalarVariable*         g_pInvMapSize = NULL;
ID3D10EffectScalarVariable*         g_pMapSize = NULL;

ID3D10EffectScalarVariable*         g_pInvDetailMapSize = NULL;
ID3D10EffectScalarVariable*         g_pDetailMapSize = NULL;
ID3D10EffectScalarVariable*         g_pDetailRepeat = NULL;
ID3D10EffectScalarVariable*         g_pInvDetailRepeat = NULL;
ID3D10EffectScalarVariable*         g_pDetailHeight = NULL;
ID3D10EffectScalarVariable*         g_pShadowBias = NULL;
ID3D10EffectScalarVariable*         g_pDetailDistanceSq = NULL;
ID3D10EffectScalarVariable*         g_pHeightRatio = NULL;

// We can load multiple tiles, but for now, just show one
struct RAYCAST_TERRAIN_TILE
{
    WCHAR   m_szTerrainTextureDDS[MAX_PATH];
    WCHAR   m_szTerrainTextureBMP[MAX_PATH];
    WCHAR   m_szTerrainMask[MAX_PATH];
    ID3D10ShaderResourceView* m_pTerrainHeightTexture;
    ID3D10ShaderResourceView* m_pTerrainMask;
    D3DXVECTOR3 m_vOffset;
    D3DXMATRIX m_mTerrainMatrix;
    D3DXMATRIX m_mInvTerrainMatrix;
};

RAYCAST_TERRAIN_TILE g_TerrainTiles[] =
{
    { 
	  L"RaycastTerrain\\MSH1024.dds", 
	  L"RaycastTerrain\\MSH1024.bmp", 
	  L"RaycastTerrain\\MSH1024_Mask.dds",		
	  NULL, NULL, D3DXVECTOR3( -0.5f, 0, -0.5f ) 
	},
};
int                                 g_NumTerrainTiles = 1;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_NUM_LIGHTS          6
#define IDC_NUM_LIGHTS_STATIC   7
#define IDC_ACTIVE_LIGHT        8
#define IDC_LIGHT_SCALE         9
#define IDC_LIGHT_SCALE_STATIC  10
#define IDC_TOGGLEWARP          11

#define IDC_RENDER_SHADOWS			20
#define IDC_RENDER_DETAIL			21
#define IDC_SHADOW_BIAS_STATIC		22
#define IDC_SHADOW_BIAS				23
#define IDC_DETAIL_AMOUNT_STATIC	24
#define IDC_DETAIL_AMOUNT			25
#define IDC_DETAIL_REPEAT_STATIC	26
#define IDC_DETAIL_REPEAT			27
#define IDC_DETAIL_DISTANCE_STATIC	28
#define IDC_DETAIL_DISTANCE			29
#define IDC_RENDER_GEOMETRY			50
#define IDC_RENDER_WIREFRAME		51

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
HRESULT PreprocessTerrain( ID3D10Device* pd3dDevice, WCHAR* strHeightMap, WCHAR* strMapOut );


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
    DXUTCreateWindow( L"RaycastTerrain" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    for( int i = 0; i < MAX_LIGHTS; i++ )
    {
        D3DXVECTOR3 vDir( -1, 1, 0 );
        D3DXVec3Normalize( &vDir, &vDir );
        g_LightControl[i].SetLightDirection( vDir );
    }

    g_nActiveLight = 0;
    g_nNumActiveLights = 1;
    g_fLightScale = 1.0f;

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
    swprintf_s( sz, 100, L"# Lights: %d", g_nNumActiveLights );
    g_SampleUI.AddStatic( IDC_NUM_LIGHTS_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_NUM_LIGHTS, 50, iY += 24, 100, 22, 1, MAX_LIGHTS, g_nNumActiveLights );

    iY += 24;
    swprintf_s( sz, 100, L"Light scale: %0.2f", g_fLightScale );
    g_SampleUI.AddStatic( IDC_LIGHT_SCALE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LIGHT_SCALE, 50, iY += 24, 100, 22, 0, 20, ( int )( g_fLightScale * 10.0f ) );

    g_SampleUI.AddButton( IDC_ACTIVE_LIGHT, L"Change active light (K)", 35, iY += 24, 125, 22, 'K' );

    g_SampleUI.AddCheckBox( IDC_RENDER_SHADOWS, L"Render Shadows", 35, iY += 24, 125, 22, g_bRenderShadows );
    g_SampleUI.AddCheckBox( IDC_RENDER_DETAIL, L"Render Detail", 35, iY += 24, 125, 22, g_bRenderDetail );
    swprintf_s( sz, 100, L"Shadow Bias: %0.4f", g_fShadowBias );
    g_SampleUI.AddStatic( IDC_SHADOW_BIAS_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_SHADOW_BIAS, 50, iY += 24, 100, 22, 0, 500, ( int )( g_fShadowBias * 10000.0f ) );

    swprintf_s( sz, 100, L"Detail Amount: %0.4f", g_fDetailAmount );
    g_SampleUI.AddStatic( IDC_DETAIL_AMOUNT_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_DETAIL_AMOUNT, 50, iY += 24, 100, 22, 0, 100, ( int )( g_fDetailAmount * 1000.0f ) );

    swprintf_s( sz, 100, L"Detail Repeat: %0.2f", g_fDetailRepeat );
    g_SampleUI.AddStatic( IDC_DETAIL_REPEAT_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_DETAIL_REPEAT, 50, iY += 24, 100, 22, 0, 128, ( int )( g_fDetailRepeat ) );

    swprintf_s( sz, 100, L"Detail Distance: %0.2f", g_fDetailDistance );
    g_SampleUI.AddStatic( IDC_DETAIL_DISTANCE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_DETAIL_DISTANCE, 50, iY += 24, 100, 22, 0, 1000, ( int )( g_fDetailDistance * 100.0f ) );

    g_SampleUI.AddCheckBox( IDC_RENDER_GEOMETRY, L"Render Geometry", 35, iY += 24, 125, 22, g_bRenderGeometry );
    g_SampleUI.AddCheckBox( IDC_RENDER_WIREFRAME, L"Render Wireframe", 35, iY += 24, 125, 22, g_bWireframe );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 100.0f, -500.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{

    // Disable vsync
    pDeviceSettings->d3d10.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

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
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
            DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        g_pTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 15 * 6 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls:" );
        g_pTxtHelper->DrawFormattedTextLine( L"Time: %f", DXUTGetTime() );
        DXUTGetGlobalTimer()->LimitThreadAffinityToCurrentProc();
    }

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

    g_LightControl[g_nActiveLight].HandleMessages( hWnd, uMsg, wParam, lParam );

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
        case IDC_ACTIVE_LIGHT:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                g_nActiveLight++;
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_NUM_LIGHTS:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                WCHAR sz[100];
                swprintf_s( sz, 100, L"# Lights: %d", g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->GetValue() );
                g_SampleUI.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetText( sz );

                g_nNumActiveLights = g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->GetValue();
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_LIGHT_SCALE:
            g_fLightScale = ( float )( g_SampleUI.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.10f );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Light scale: %0.2f", g_fLightScale );
            g_SampleUI.GetStatic( IDC_LIGHT_SCALE_STATIC )->SetText( sz );
            break;

        case IDC_RENDER_SHADOWS:
            g_bRenderShadows = !g_bRenderShadows;
            break;
        case IDC_RENDER_DETAIL:
            g_bRenderDetail = !g_bRenderDetail;
            break;
        case IDC_SHADOW_BIAS:
        {
            g_fShadowBias = ( float )( g_SampleUI.GetSlider( IDC_SHADOW_BIAS )->GetValue() * 0.0001f );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Shadow Bias: %0.4f", g_fShadowBias );
            g_SampleUI.GetStatic( IDC_SHADOW_BIAS_STATIC )->SetText( sz );

            g_pShadowBias->SetFloat( g_fShadowBias );
        }
            break;

        case IDC_DETAIL_AMOUNT:
        {
            g_fDetailAmount = ( float )( g_SampleUI.GetSlider( IDC_DETAIL_AMOUNT )->GetValue() / 1000.0f );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Detail Amount: %0.4f", g_fDetailAmount );
            g_SampleUI.GetStatic( IDC_DETAIL_AMOUNT_STATIC )->SetText( sz );

            g_pDetailHeight->SetFloat( g_fDetailAmount );
        }
            break;

        case IDC_DETAIL_REPEAT:
        {
            g_fDetailRepeat = ( float )( g_SampleUI.GetSlider( IDC_DETAIL_REPEAT )->GetValue() );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Detail Repeat: %0.2f", g_fDetailRepeat );
            g_SampleUI.GetStatic( IDC_DETAIL_REPEAT_STATIC )->SetText( sz );

            g_pDetailRepeat->SetFloat( g_fDetailRepeat );
            g_pInvDetailRepeat->SetFloat( 1.0f / g_fDetailRepeat );
        }
            break;
        case IDC_DETAIL_DISTANCE:
        {
            g_fDetailDistance = ( float )( g_SampleUI.GetSlider( IDC_DETAIL_DISTANCE )->GetValue() / 100.0f );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Detail Distance: %0.2f", g_fDetailDistance );
            g_SampleUI.GetStatic( IDC_DETAIL_DISTANCE_STATIC )->SetText( sz );

            g_pDetailDistanceSq->SetFloat( g_fDetailDistance * g_fDetailDistance );
        }
            break;
        case IDC_RENDER_GEOMETRY:
            g_bRenderGeometry = !g_bRenderGeometry;
            break;
        case IDC_RENDER_WIREFRAME:
            g_bWireframe = !g_bWireframe;
            break;
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

    g_SampleUI.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetVisible( false );
    g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->SetVisible( false );
    g_SampleUI.GetButton( IDC_ACTIVE_LIGHT )->SetVisible( false );

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    //dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // raycast technique objects
    g_pRenderTerrain_Inside = g_pEffect10->GetTechniqueByName( "RenderTerrain_Inside" );
    g_pRenderTerrain_Outside = g_pEffect10->GetTechniqueByName( "RenderTerrain_Outside" );
    g_pRenderTerrain_Ortho = g_pEffect10->GetTechniqueByName( "RenderTerrain_Ortho" );
    g_pRenderTerrain_Inside_Detail = g_pEffect10->GetTechniqueByName( "RenderTerrain_Inside_Detail" );
    g_pRenderTerrain_Outside_Detail = g_pEffect10->GetTechniqueByName( "RenderTerrain_Outside_Detail" );
    g_pRenderTerrain_Ortho_Detail = g_pEffect10->GetTechniqueByName( "RenderTerrain_Ortho_Detail" );
    g_pRenderTerrain_Inside_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Inside_Shadow" );
    g_pRenderTerrain_Outside_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Outside_Shadow" );
    g_pRenderTerrain_Ortho_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Ortho_Shadow" );
    g_pRenderTerrain_Inside_Detail_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Inside_Detail_Shadow" );
    g_pRenderTerrain_Outside_Detail_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Outside_Detail_Shadow" );
    g_pRenderTerrain_Ortho_Detail_Shadow = g_pEffect10->GetTechniqueByName( "RenderTerrain_Ortho_Detail_Shadow" );
    g_pRenderTerrain_Wire = g_pEffect10->GetTechniqueByName( "RenderTerrain_Wire" );

    // geometry technique objects
    g_pRenderGeometryTerrain = g_pEffect10->GetTechniqueByName( "RenderGeometryTerrain" );
    g_pRenderGeometryTerrain_Shadow = g_pEffect10->GetTechniqueByName( "RenderGeometryTerrain_Shadow" );
    g_pRenderGeometryTerrain_Wire = g_pEffect10->GetTechniqueByName( "RenderGeometryTerrain_Wire" );
    g_pRenderMesh = g_pEffect10->GetTechniqueByName( "RenderMesh" );
    g_pRenderMesh_Shadow = g_pEffect10->GetTechniqueByName( "RenderMesh_Shadow" );

    // texture variables
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxDetailDiffuse = g_pEffect10->GetVariableByName( "g_txDetailDiffuse" )->AsShaderResource();
    g_ptxDetailGrad_RedGreen = g_pEffect10->GetVariableByName( "g_txDetailGrad_RedGreen" )->AsShaderResource();
    g_ptxDetailGrad_BlueAlpha = g_pEffect10->GetVariableByName( "g_txDetailGrad_BlueAlpha" )->AsShaderResource();
    g_ptxHeight = g_pEffect10->GetVariableByName( "g_txHeight" )->AsShaderResource();
    g_ptxMask = g_pEffect10->GetVariableByName( "g_txMask" )->AsShaderResource();
    g_ptxDetailHeight = g_pEffect10->GetVariableByName( "g_txDetailHeight" )->AsShaderResource();
    g_ptxDepthMap = g_pEffect10->GetVariableByName( "g_txDepthMap" )->AsShaderResource();

    // constant variables
    g_pLightDir = g_pEffect10->GetVariableByName( "g_LightDir" )->AsVector();
    g_pLightDirTex = g_pEffect10->GetVariableByName( "g_LightDirTex" )->AsVector();
    g_pLightDiffuse = g_pEffect10->GetVariableByName( "g_LightDiffuse" )->AsVector();
    g_pmWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmWorldToTerrain = g_pEffect10->GetVariableByName( "g_mWorldToTerrain" )->AsMatrix();
    g_pmTexToViewProj = g_pEffect10->GetVariableByName( "g_mTexToViewProj" )->AsMatrix();
    g_pmTexToLightViewProj = g_pEffect10->GetVariableByName( "g_mTexToLightViewProj" )->AsMatrix();
    g_pmLightViewProj = g_pEffect10->GetVariableByName( "g_mLightViewProj" )->AsMatrix();
    g_pvTextureEyePt = g_pEffect10->GetVariableByName( "g_vTextureEyePt" )->AsVector();
    g_pInvMapSize = g_pEffect10->GetVariableByName( "g_InvMapSize" )->AsScalar();
    g_pMapSize = g_pEffect10->GetVariableByName( "g_MapSize" )->AsScalar();
    g_pInvDetailMapSize = g_pEffect10->GetVariableByName( "g_InvDetailMapSize" )->AsScalar();
    g_pDetailMapSize = g_pEffect10->GetVariableByName( "g_DetailMapSize" )->AsScalar();
    g_pDetailRepeat = g_pEffect10->GetVariableByName( "g_DetailRepeat" )->AsScalar();
    g_pInvDetailRepeat = g_pEffect10->GetVariableByName( "g_InvDetailRepeat" )->AsScalar();
    g_pDetailHeight = g_pEffect10->GetVariableByName( "g_DetailHeight" )->AsScalar();
    g_pShadowBias = g_pEffect10->GetVariableByName( "g_ShadowBias" )->AsScalar();
    g_pDetailDistanceSq = g_pEffect10->GetVariableByName( "g_DetailDistanceSq" )->AsScalar();
    g_pHeightRatio = g_pEffect10->GetVariableByName( "g_HeightRatio" )->AsScalar();

    D3D10_PASS_DESC PassDesc;
    const D3D10_INPUT_ELEMENT_DESC boxlayout[] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( g_pRenderTerrain_Inside->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( boxlayout, ARRAYSIZE( boxlayout ), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pBoxLayout ) );

    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,   D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, 24,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( g_pRenderMesh->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, ARRAYSIZE( meshlayout ), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshLayout ) );

    // Create the box
    D3D10_BUFFER_DESC BDesc;
    BDesc.ByteWidth = 6 * 6 * sizeof( D3DXVECTOR3 );
    BDesc.Usage = D3D10_USAGE_IMMUTABLE;
    BDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BDesc.CPUAccessFlags = 0;
    BDesc.MiscFlags = 0;

    float fFloor = -0.01f;
    D3DXVECTOR3 vVertices[6*6];
    // Bottom
    vVertices[0] = D3DXVECTOR3( 0, fFloor, 0 );
    vVertices[1] = D3DXVECTOR3( 0, fFloor, 1 );
    vVertices[2] = D3DXVECTOR3( 1, fFloor, 0 );
    vVertices[3] = D3DXVECTOR3( 1, fFloor, 0 );
    vVertices[4] = D3DXVECTOR3( 0, fFloor, 1 );
    vVertices[5] = D3DXVECTOR3( 1, fFloor, 1 );

    // Left
    vVertices[6] = D3DXVECTOR3( 0, fFloor, 0 );
    vVertices[7] = D3DXVECTOR3( 0, 1, 0 );
    vVertices[8] = D3DXVECTOR3( 0, fFloor, 1 );
    vVertices[9] = D3DXVECTOR3( 0, fFloor, 1 );
    vVertices[10] = D3DXVECTOR3( 0, 1, 0 );
    vVertices[11] = D3DXVECTOR3( 0, 1, 1 );

    // Right
    vVertices[12] = D3DXVECTOR3( 1, fFloor, 1 );
    vVertices[13] = D3DXVECTOR3( 1, 1, 1 );
    vVertices[14] = D3DXVECTOR3( 1, fFloor, 0 );
    vVertices[15] = D3DXVECTOR3( 1, fFloor, 0 );
    vVertices[16] = D3DXVECTOR3( 1, 1, 1 );
    vVertices[17] = D3DXVECTOR3( 1, 1, 0 );

    // Back
    vVertices[18] = D3DXVECTOR3( 0, fFloor, 1 );
    vVertices[19] = D3DXVECTOR3( 0, 1, 1 );
    vVertices[20] = D3DXVECTOR3( 1, fFloor, 1 );
    vVertices[21] = D3DXVECTOR3( 1, fFloor, 1 );
    vVertices[22] = D3DXVECTOR3( 0, 1, 1 );
    vVertices[23] = D3DXVECTOR3( 1, 1, 1 );

    // Front
    vVertices[24] = D3DXVECTOR3( 1, fFloor, 0 );
    vVertices[25] = D3DXVECTOR3( 1, 1, 0 );
    vVertices[26] = D3DXVECTOR3( 0, fFloor, 0 );
    vVertices[27] = D3DXVECTOR3( 0, fFloor, 0 );
    vVertices[28] = D3DXVECTOR3( 1, 1, 0 );
    vVertices[29] = D3DXVECTOR3( 0, 1, 0 );

    // Top
    vVertices[30] = D3DXVECTOR3( 0, 1, 1 );
    vVertices[31] = D3DXVECTOR3( 0, 1, 0 );
    vVertices[32] = D3DXVECTOR3( 1, 1, 1 );
    vVertices[33] = D3DXVECTOR3( 1, 1, 1 );
    vVertices[34] = D3DXVECTOR3( 0, 1, 0 );
    vVertices[35] = D3DXVECTOR3( 1, 1, 0 );

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &BDesc, &InitData, &g_pScreenQuadVB ) );

    // Setup shader variables based on terrain sizes
    g_fMapHeight = 200.0f;
    g_fMapWidth = 2000.0f;
    g_pMapSize->SetFloat( 1024.0f );
    g_pInvMapSize->SetFloat( 1.0f / 1024.0f );
    g_pDetailMapSize->SetFloat( 256.0f );
    g_pInvDetailMapSize->SetFloat( 1.0f / 256.0f );
    g_pDetailRepeat->SetFloat( g_fDetailRepeat );
    g_pInvDetailRepeat->SetFloat( 1.0f / g_fDetailRepeat );
    g_pDetailHeight->SetFloat( g_fDetailAmount );
    g_pShadowBias->SetFloat( g_fShadowBias );
    g_pDetailDistanceSq->SetFloat( g_fDetailDistance * g_fDetailDistance );
    g_fHeightRatio = g_fMapHeight / g_fMapWidth;
    g_pHeightRatio->SetFloat( g_fHeightRatio );

    // Load the detail texture
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMapHeight.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailHeightTexture, NULL );

    // Load the detail diffuse textures
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMap_Red.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailColorTexture[0], NULL );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMap_Green.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailColorTexture[1], NULL );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMap_Blue.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailColorTexture[2], NULL );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMap_Alpha.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailColorTexture[3], NULL );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMapGrad_RedGreen.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailGrad_RedGreen, NULL );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\DetailMapGrad_BlueAlpha.dds" ) );
    D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDetailGrad_BlueAlpha, NULL );

    g_NumTerrainTiles = ARRAYSIZE( g_TerrainTiles );
    for( int i = 0; i < g_NumTerrainTiles; i++ )
    {
        // Create a terrain from a bitmap
        hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_TerrainTiles[i].m_szTerrainTextureDDS );
        if( FAILED( hr ) )
        {
            WCHAR bmp[MAX_PATH];
            V_RETURN( DXUTFindDXSDKMediaFileCch( bmp, MAX_PATH, g_TerrainTiles[i].m_szTerrainTextureBMP ) );
            V_RETURN( PreprocessTerrain( pd3dDevice, bmp, g_TerrainTiles[i].m_szTerrainTextureDDS ) );
        }

        // Load the height and ddx,ddy texture
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_TerrainTiles[i].m_szTerrainTextureDDS ) );
        D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL,
                                                NULL, &g_TerrainTiles[i].m_pTerrainHeightTexture, NULL );

        // Load the mask
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_TerrainTiles[i].m_szTerrainMask ) );
        D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_TerrainTiles[i].m_pTerrainMask, NULL );

        // Setup our terrain matrix
        D3DXMATRIX mTranslation;
        D3DXMATRIX mScaling;
        D3DXMatrixTranslation( &mTranslation, g_TerrainTiles[i].m_vOffset.x, g_TerrainTiles[i].m_vOffset.y,
                               g_TerrainTiles[i].m_vOffset.z );

        D3DXMatrixScaling( &mScaling, g_fMapWidth, g_fMapHeight, g_fMapWidth );
        g_TerrainTiles[i].m_mTerrainMatrix = mTranslation * mScaling;
        D3DXMatrixInverse( &g_TerrainTiles[i].m_mInvTerrainMatrix, NULL, &g_TerrainTiles[i].m_mTerrainMatrix );
    }

    g_BoxMesh.Create( pd3dDevice, L"misc\\bobble.sdkmesh" );

    // Load the mesh version of the terrain
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RaycastTerrain\\MSH1024.bmp" ) );
    V_RETURN( g_Terrain.LoadTerrain( str, 1, 1200, g_fMapWidth, g_fMapHeight, 10, 1.0f, 2.0f ) );

    // Load terrain device objects
    V_RETURN( g_Terrain.OnCreateDevice( pd3dDevice ) );

    // Create positions for boxes
    g_pvBoxPositions = new D3DXVECTOR3[ g_NumBoxes ];
    for( int i = 0; i < g_NumBoxes; i++ )
    {
        g_pvBoxPositions[i].x = RPercent() * g_fMapWidth * 0.4f;
        g_pvBoxPositions[i].z = RPercent() * g_fMapWidth * 0.4f;
        g_pvBoxPositions[i].y = g_Terrain.GetHeightOnMap( &g_pvBoxPositions[i] ) - 5;
    }

    // Create shadow textures
    D3D10_TEXTURE2D_DESC ShadowDesc;
    ShadowDesc.Width = g_ShadowMapSize;
    ShadowDesc.Height = g_ShadowMapSize;
    ShadowDesc.MipLevels = 1;
    ShadowDesc.ArraySize = 1;
    ShadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    ShadowDesc.SampleDesc.Count = 1;
    ShadowDesc.SampleDesc.Quality = 0;
    ShadowDesc.Usage = D3D10_USAGE_DEFAULT;
    ShadowDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;
    ShadowDesc.CPUAccessFlags = 0;
    ShadowDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &ShadowDesc, NULL, &g_pShadowDepthTexture ) );

    D3D10_DEPTH_STENCIL_VIEW_DESC DSVDesc;
    DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DSVDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    DSVDesc.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pShadowDepthTexture, &DSVDesc, &g_pShadowDSV ) );

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = ShadowDesc.MipLevels;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pShadowDepthTexture, &SRVDesc, &g_pShadowSRV ) );	

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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 10.0f, 30000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 600 );
    g_SampleUI.SetSize( 170, 600 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
ID3D10EffectTechnique* GetInsideTechnique( bool bShadowDepthPass )
{
    if( bShadowDepthPass )
    {
        // If we're in the shadow pass, assume that the light is outside the terrain and use an orthographic raycast
        return g_pRenderTerrain_Ortho;
    }
    else
    {
        if( g_bRenderDetail )
        {
            if( g_bRenderShadows )
                return g_pRenderTerrain_Inside_Detail_Shadow;
            else
                return g_pRenderTerrain_Inside_Detail;
        }
        else
        {
            if( g_bRenderShadows )
                return g_pRenderTerrain_Inside_Shadow;
            else
                return g_pRenderTerrain_Inside;
        }
    }
}

//--------------------------------------------------------------------------------------
ID3D10EffectTechnique* GetOutsideTechnique( bool bShadowDepthPass )
{
    if( bShadowDepthPass )
    {
        // If we're in the shadow pass, assume that the light is outside the terrain and use an orthographic raycast
        return g_pRenderTerrain_Ortho;
    }
    else
    {
        if( g_bRenderDetail )
        {
            if( g_bRenderShadows )
                return g_pRenderTerrain_Outside_Detail_Shadow;
            else
                return g_pRenderTerrain_Outside_Detail;
        }
        else
        {
            if( g_bRenderShadows )
                return g_pRenderTerrain_Outside_Shadow;
            else
                return g_pRenderTerrain_Outside;
        }
    }
}

//--------------------------------------------------------------------------------------
// Renders the terrain using geometry
//--------------------------------------------------------------------------------------
void RenderTerrain( ID3D10Device* pd3dDevice, D3DXMATRIX* pmViewProj, ID3D10EffectTechnique* pTech )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );	

    pd3dDevice->IASetInputLayout( g_pMeshLayout );

    g_pmWorldViewProjection->SetMatrix( ( float* )pmViewProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );

    pd3dDevice->IASetIndexBuffer( g_Terrain.GetTerrainIB10(), DXGI_FORMAT_R32_UINT, 0 );

    if( g_bWireframe )
        pTech = g_pRenderGeometryTerrain_Wire;

    D3D10_TECHNIQUE_DESC techDesc;
    pTech->GetDesc( &techDesc );

    UINT NumTiles = g_Terrain.GetNumTiles();

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT i = 0; i < NumTiles; i++ )
        {
            TERRAIN_TILE* pTile = g_Terrain.GetTile( i );

            pTech->GetPassByIndex( p )->Apply( 0 );

            g_Terrain.RenderTile( pTile );
        }
    }
}

//--------------------------------------------------------------------------------------
// Raycasts the terrain
//--------------------------------------------------------------------------------------
void RaycastTerrain( ID3D10Device* pd3dDevice, D3DXMATRIX* pmViewProj, D3DXMATRIX* pmLightViewProj,
                     D3DXVECTOR3* pvLightDir,
                     D3DXVECTOR3* pvEye, bool bShadowDepthPass )
{
    HRESULT hr = S_OK;

    g_ptxDetailDiffuse->SetResourceArray( g_pDetailColorTexture, 0, 4 );
    g_ptxDetailGrad_RedGreen->SetResource( g_pDetailGrad_RedGreen );
    g_ptxDetailGrad_BlueAlpha->SetResource( g_pDetailGrad_BlueAlpha );
    g_ptxDetailHeight->SetResource( g_pDetailHeightTexture );

    //IA setup
    pd3dDevice->IASetInputLayout( g_pBoxLayout );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D10Buffer* pVB[1];
    pVB[0] = g_pScreenQuadVB;
    Strides[0] = sizeof( D3DXVECTOR3 );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( NULL, DXGI_FORMAT_R16_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    for( int i = 0; i < g_NumTerrainTiles; i++ )
    {
        // Get the projection & view matrix from the camera class
        D3DXMATRIX mWorld = g_TerrainTiles[i].m_mTerrainMatrix;
        D3DXMATRIX mWorldViewProjection = mWorld * *pmViewProj;

        // Move the light into terrain texture space
        D3DXVECTOR4 vLightDir4;
        D3DXVec3TransformNormal( ( D3DXVECTOR3* )&vLightDir4, pvLightDir, &g_TerrainTiles[i].m_mInvTerrainMatrix );
        D3DXVec3Normalize( ( D3DXVECTOR3* )&vLightDir4, ( D3DXVECTOR3* )&vLightDir4 );
        D3DXVECTOR4 vSwizzle( vLightDir4.x, vLightDir4.z, vLightDir4.y, 1 );
        g_pLightDirTex->SetFloatVector( ( float* )&vSwizzle );

        D3DXMATRIX mTexToViewProj;
        mTexToViewProj = g_TerrainTiles[i].m_mTerrainMatrix * ( *pmViewProj );

        D3DXMATRIX mTexToLightViewProj;
        mTexToLightViewProj = g_TerrainTiles[i].m_mTerrainMatrix * ( *pmLightViewProj );

        // Set projection matrices
        V( g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection ) );
        V( g_pmWorld->SetMatrix( ( float* )&mWorld ) );
        V( g_pmTexToViewProj->SetMatrix( ( float* )&mTexToViewProj ) );
        V( g_pmTexToLightViewProj->SetMatrix( ( float* )&mTexToLightViewProj ) );

        // Move the eye point into texture space
        D3DXVECTOR4 vEye4;
        D3DXVec3Transform( &vEye4, pvEye, &g_TerrainTiles[i].m_mInvTerrainMatrix );
        g_pvTextureEyePt->SetFloatVector( ( float* )&vEye4 );

        // Set the terrain texture
        g_ptxHeight->SetResource( g_TerrainTiles[i].m_pTerrainHeightTexture );
        g_ptxMask->SetResource( g_TerrainTiles[i].m_pTerrainMask );

        ID3D10EffectTechnique* pTechnique = NULL;
        if( g_bWireframe )
        {
            pTechnique = g_pRenderTerrain_Wire;
        }
        else
        {
            // Determine whether we're inside or outside the box
            if( vEye4.x < 0 || vEye4.y < 0 || vEye4.z < 0 ||
                vEye4.x > 1 || vEye4.y > 1 || vEye4.z > 1 )
            {
                pTechnique = GetOutsideTechnique( bShadowDepthPass );
            }
            else
            {
                pTechnique = GetInsideTechnique( bShadowDepthPass );
            }
        }

        //Render
        D3D10_TECHNIQUE_DESC techDesc;
        pTechnique->GetDesc( &techDesc );
        for( UINT p = 0; p < techDesc.Passes; ++p )
        {
            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->Draw( 36, 0 );
        }
    }
}

//--------------------------------------------------------------------------------------
// Renders boxes
//--------------------------------------------------------------------------------------
void RenderBoxes( ID3D10Device* pd3dDevice, D3DXMATRIX* pmViewProj, ID3D10EffectTechnique* pTechnique )
{
    HRESULT hr = S_OK;
    // Set the layout
    pd3dDevice->IASetInputLayout( g_pMeshLayout );
    D3DXMATRIX mTrans;
    D3DXMATRIX mScale;

    for( int i = 0; i < g_NumBoxes; i++ )
    {
        D3DXMatrixScaling( &mScale, 20, 20, 20 );
        D3DXMatrixTranslation( &mTrans, g_pvBoxPositions[i].x, g_pvBoxPositions[i].y, g_pvBoxPositions[i].z );
        D3DXMATRIX mWorld = mScale * mTrans;
        D3DXMATRIX mWorldViewProjection = mWorld * ( *pmViewProj );

        // Set projection matrices
        V( g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection ) );
        V( g_pmWorld->SetMatrix( ( float* )&mWorld ) );
        g_BoxMesh.Render( pd3dDevice, pTechnique, g_ptxDiffuse );
    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    float ClearColor[4] =
    {
        0.1f, 0.25f, 0.4f, 1.0f
    };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    D3DXMATRIX mWorldViewProjection;
    D3DXVECTOR4 vLightDir4;
    D3DXVECTOR4 vLightDiffuse;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXVECTOR4 vEye4;

    D3DXVECTOR3 vEye = *g_Camera.GetEyePt();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mViewProj = mView * mProj;

    // Get and set light direction
    D3DXVECTOR3 vLightDir = g_LightControl[0].GetLightDirection();
    vLightDiffuse = g_fLightScale * D3DXVECTOR4( 1, 1, 1, 1 );
    g_pLightDiffuse->SetFloatVector( ( float* )&vLightDiffuse );
    g_pLightDir->SetFloatVector( ( float* )&vLightDir );

    // Create light frustum
    float fMapWidthFudge = g_fMapWidth * 1.2f;
    D3DXMATRIX mLightView;
    D3DXVECTOR3 vLightPos = vLightDir * g_fMapWidth * 1.2f;
    D3DXVECTOR3 vLightDirNeg = -vLightDir;
    D3DXVECTOR3 vUp( 0,1,0 );
    if( D3DXVec3Dot( &vUp, &vLightDirNeg ) > 0.99f )
        vUp = D3DXVECTOR3( 0, 0, 1 );
    D3DXMatrixLookAtLH( &mLightView, &vLightPos, &vLightDirNeg, &vUp );
    D3DXMATRIX mLightProj;
    D3DXMatrixOrthoLH( &mLightProj, fMapWidthFudge, fMapWidthFudge, 0.5f, g_fMapWidth * 2.4f );
    D3DXMATRIX mLightViewProj = mLightView * mLightProj;
    g_pmLightViewProj->SetMatrix( ( float* )&mLightViewProj );

    // shadow depthmap pass
    if( g_bRenderShadows )
    {
        // Setup render targets
        ID3D10RenderTargetView* pOldRTV = NULL;
        ID3D10DepthStencilView* pOldDSV = NULL;
        pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

        ID3D10RenderTargetView* pNull = NULL;
        pd3dDevice->OMSetRenderTargets( 1, &pNull, g_pShadowDSV );

        pd3dDevice->ClearDepthStencilView( g_pShadowDSV, D3D10_CLEAR_DEPTH, 1.0f, 0 );

        D3D10_VIEWPORT OldViewport;
        UINT NumViewports = 1;
        pd3dDevice->RSGetViewports( &NumViewports, &OldViewport );

        D3D10_VIEWPORT NewViewport;
        NewViewport.Width = g_ShadowMapSize;
        NewViewport.Height = g_ShadowMapSize;
        NewViewport.TopLeftX = 0;
        NewViewport.TopLeftY = 0;
        NewViewport.MinDepth = 0.0f;
        NewViewport.MaxDepth = 1.0f;
        pd3dDevice->RSSetViewports( NumViewports, &NewViewport );

        if( g_bRenderGeometry )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Terrain Geometry Shadow Pass" );
            RenderTerrain( pd3dDevice, &mLightViewProj, g_pRenderGeometryTerrain );
            DXUT_EndPerfEvent();
        }
        else
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Terrain Raycast Shadow Pass" );
            RaycastTerrain( pd3dDevice, &mLightViewProj, &mLightViewProj, &vLightDir, &vLightPos, true );
            DXUT_EndPerfEvent();
        }

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Object Shadow Pass" );
        RenderBoxes( pd3dDevice, &mLightViewProj, g_pRenderMesh );
        DXUT_EndPerfEvent();

        pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
        SAFE_RELEASE( pOldRTV );
        SAFE_RELEASE( pOldDSV );

        pd3dDevice->RSSetViewports( NumViewports, &OldViewport );
    }

    // Foward pass
    g_ptxDepthMap->SetResource( g_pShadowSRV );

    if( g_bRenderGeometry )
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Terrain Geometry Forward Pass" );
        if( g_bRenderShadows )
            RenderTerrain( pd3dDevice, &mViewProj, g_pRenderGeometryTerrain_Shadow );
        else
            RenderTerrain( pd3dDevice, &mViewProj, g_pRenderGeometryTerrain );
        DXUT_EndPerfEvent();
    }
    else
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Terrain Raycast Forward Pass" );
        RaycastTerrain( pd3dDevice, &mViewProj, &mLightViewProj, &vLightDir, &vEye, false );
        DXUT_EndPerfEvent();
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Object Forward Pass" );
    if( g_bRenderShadows )
        RenderBoxes( pd3dDevice, &mViewProj, g_pRenderMesh_Shadow );
    else
        RenderBoxes( pd3dDevice, &mViewProj, g_pRenderMesh );
    DXUT_EndPerfEvent();

    // Cleanup
    ID3D10ShaderResourceView* pNULLViews[10] =
    {
        0,0,0,0,0,0,0,0,0,0
    };
    pd3dDevice->PSSetShaderResources( 0, 10, pNULLViews );

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
    SAFE_RELEASE( g_pBoxLayout );
    SAFE_RELEASE( g_pMeshLayout );
    SAFE_RELEASE( g_pScreenQuadVB );
    SAFE_RELEASE( g_pDetailHeightTexture );
    SAFE_RELEASE( g_pDetailGrad_RedGreen );
    SAFE_RELEASE( g_pDetailGrad_BlueAlpha );
    for( int i = 0; i < 4; i++ )
    {
        SAFE_RELEASE( g_pDetailColorTexture[i] );
    }

    for( int i = 0; i < g_NumTerrainTiles; i++ )
    {
        SAFE_RELEASE( g_TerrainTiles[i].m_pTerrainHeightTexture );
        SAFE_RELEASE( g_TerrainTiles[i].m_pTerrainMask );
    }

    SAFE_RELEASE( g_pShadowDepthTexture );
    SAFE_RELEASE( g_pShadowDSV );
    SAFE_RELEASE( g_pShadowSRV );

    g_Terrain.OnDestroyDevice();
    g_BoxMesh.Destroy();

    SAFE_DELETE_ARRAY( g_pvBoxPositions );
}

//--------------------------------------------------------------------------------------
// This preprocessing code isn't used in the sample, but is provided for completeness.
// It shows one way to create the cone ratios for an input texture.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// For an explanation of the cone-step mapping technique see
// www.lonesock.net/files/ConeStepMapping.pdf
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
float GetMaxConeRatio( int StartX, int StartY, float* pBits, int Width, int Height )
{
    float fMinRatio = 27.0f; // Radius / DeltaHeight
    float fConeStartHeight = pBits[ StartY * Width + StartX ];
    float fStartX = StartX / ( float )Width;
    float fStartY = StartY / ( float )Height;
    float fY = 0;
    float fStepX = 1.0f / ( float )Width;
    float fStepY = 1.0f / ( float )Height;

    for( int y = 0; y < Height; y++ )
    {
        float fDeltaY = fStartY - fY;
        float fX = 0;
        for( int x = 0; x < Width; x++ )
        {
            if( StartX != x && StartY != y )
            {
                float fTestHeight = pBits[ y * Width + x ];
                float fHeightDeltaSq = fTestHeight - fConeStartHeight;

                if( fHeightDeltaSq > 0.00001f )
                {
                    float fDeltaX = fStartX - fX;
                    float fRadiusToTestSq = ( fDeltaX * fDeltaX + fDeltaY * fDeltaY );
                    fHeightDeltaSq *= fHeightDeltaSq;

                    if( fHeightDeltaSq * fMinRatio > fRadiusToTestSq )
                    {
                        fMinRatio = fRadiusToTestSq / fHeightDeltaSq;
                    }
                }
            }
            fX += fStepX;
        }
        fY += fStepY;
    }

    return min( 1, sqrtf( sqrtf( fMinRatio ) ) );
}

//--------------------------------------------------------------------------------------
HRESULT PreprocessTerrain( ID3D10Device* pd3dDevice, WCHAR* strHeightMap, WCHAR* strMapOut )
{
    HRESULT hr = S_OK;
    HANDLE hFile = CreateFile( strHeightMap, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_INVALIDARG;

    DWORD dwBytesRead = 0;

    // read the bfh
    BITMAPFILEHEADER bfh;
    ReadFile( hFile, &bfh, sizeof( BITMAPFILEHEADER ), &dwBytesRead, NULL );
    unsigned long toBitmapData = bfh.bfOffBits;
    unsigned long bitmapSize = bfh.bfSize;

    // read the header
    BITMAPINFOHEADER bih;
    ReadFile( hFile, &bih, sizeof( BITMAPINFOHEADER ), &dwBytesRead, NULL );
    if( bih.biCompression != BI_RGB )
        goto Error;

    int HeightMapX;
    int HeightMapY;
    float* pHeightBits;

    // alloc memory
    int U = HeightMapX = bih.biWidth;
    int V = HeightMapY = abs( bih.biHeight );
    pHeightBits = new float[ U * V ];
    if( !pHeightBits )
        return E_OUTOFMEMORY;

    // find the step size
    int iStep = 4;
    if( 24 == bih.biBitCount )
        iStep = 3;

    // final check for size
    int UNew = ( ( U * iStep * 8 + 31 ) & ~31 ) / ( 8 * iStep );
    if( bitmapSize < UNew * V * iStep * sizeof( BYTE ) )
        goto Error;

    // seek
    LARGE_INTEGER liMove;
    liMove.QuadPart = toBitmapData;
    if( !SetFilePointerEx( hFile, liMove, NULL, FILE_BEGIN ) )
        goto Error;

    // read in the bits
    BYTE* pBits = new BYTE[ bitmapSize ];
    if( !pBits )
        return E_OUTOFMEMORY;
    ReadFile( hFile, pBits, bitmapSize * sizeof( BYTE ), &dwBytesRead, NULL );

    CloseHandle( hFile );

    // Load the Height Information
    int iHeight = 0;
    int iBitmap = 0;
    for( int y = 0; y < V; y++ )
    {
        iBitmap = y * UNew * iStep;
        for( int x = 0; x < U * iStep; x += iStep )
        {
            pHeightBits[iHeight] = pBits[ iBitmap ] / 255.0f;

            iHeight ++;
            iBitmap += iStep;
        }
    }

    SAFE_DELETE_ARRAY( pBits );

    // Now create the cone ratios
    float* pConeRatios = new float[ U * V ];
    if( !pConeRatios )
        return E_OUTOFMEMORY;

    WCHAR str[100];
    int index = 0;
    for( int y = 0; y < V; y++ )
    {
        for( int x = 0; x < U; x++ )
        {
            pConeRatios[index] = GetMaxConeRatio( x, y, pHeightBits, U, V );
            index ++;
        }
        swprintf_s( str, 100, L"Line: %d\n", y );
        OutputDebugString( str );
    }

    // Put it all into one texture
    D3D10_TEXTURE2D_DESC Desc;
    Desc.Width = U;
    Desc.Height = V;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Usage = D3D10_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    D3D10_SUBRESOURCE_DATA InitData;
    BYTE* pSysData = new BYTE[ U * V * 4 ];
    InitData.pSysMem = pSysData;
    InitData.SysMemPitch = V * 4;

    index = 0;
    for( int i = 0; i < U * V * 4; i += 4 )
    {
        pSysData[i    ] = ( BYTE )( pHeightBits[index] * 255.0f );

        // cone ratios
        pSysData[i + 1] = ( BYTE )( pConeRatios[index] * 255.0f );

        index ++;
    }

    ID3D10Texture2D* pTerrainTexture = NULL;
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, &InitData, &pTerrainTexture ) );
    SAFE_DELETE_ARRAY( InitData.pSysMem );

    // Save it out to a file
    D3DX10SaveTextureToFile( pTerrainTexture, D3DX10_IFF_DDS, strMapOut );

    SAFE_RELEASE( pTerrainTexture );
    SAFE_DELETE_ARRAY( pHeightBits );
    SAFE_DELETE_ARRAY( pConeRatios );
    return S_OK;

Error:
    CloseHandle( hFile );
    return E_FAIL;

}
