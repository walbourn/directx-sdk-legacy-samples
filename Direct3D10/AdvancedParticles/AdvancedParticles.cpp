//--------------------------------------------------------------------------------------
// File: ParticlesGS.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"
#include "Commdlg.h"

#define MAX_BONE_MATRICES 255

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
bool                                g_bFirst = true;

ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pParticleEffect10 = NULL;
ID3D10Effect*                       g_pMeshEffect10 = NULL;
ID3D10Effect*                       g_pVolEffect10 = NULL;
ID3D10Effect*                       g_pPaintEffect10 = NULL;
ID3D10InputLayout*                  g_pParticleVertexLayout = NULL;
ID3D10InputLayout*                  g_pMeshVertexLayout = NULL;
ID3D10InputLayout*                  g_pQuadVertexLayout = NULL;
ID3D10InputLayout*                  g_pAnimVertexLayout = NULL;
ID3D10InputLayout*                  g_pPaintAnimVertexLayout = NULL;

struct QUAD_VERTEX
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 Tex;
};
ID3D10Buffer*                       g_pQuadVB = NULL;

// particle necessities
D3DXVECTOR4                         g_vGravity( 0,-0.5f,0,0 );
D3DXVECTOR3                         g_vParticleColor( 1,0,0 );
float                               g_fParticleOpacity = 1.0f;
ID3D10Buffer*                       g_pParticleStart = NULL;
ID3D10Buffer*                       g_pParticleStreamTo = NULL;
ID3D10Buffer*                       g_pParticleDrawFrom = NULL;
ID3D10ShaderResourceView*           g_pParticleStreamToRV = NULL;
ID3D10ShaderResourceView*           g_pParticleDrawFromRV = NULL;
ID3D10ShaderResourceView*           g_pParticleTexRV = NULL;
ID3D10Texture1D*                    g_pRandomTexture = NULL;
ID3D10ShaderResourceView*           g_pRandomTexRV = NULL;

// render to volume necessities
D3DXVECTOR3                         g_vEmitterPos( 0,1.5f,0 );
UINT                                g_VolumeWidth = 32;
UINT                                g_VolumeHeight = 32;
UINT                                g_VolumeDepth = 32;
float                               g_VolumeSize = 2.0f;
float                               g_fEmitterSize = 0.3f;
ID3D10Texture3D*                    g_pVolumeTexture = NULL;
ID3D10ShaderResourceView*           g_pVolumeTexRV = NULL;
ID3D10RenderTargetView**            g_pVolumeTexRTVs = NULL;
ID3D10RenderTargetView*             g_pVolumeTexTotalRTV = NULL;
ID3D10Texture3D*                    g_pVelocityTexture = NULL;
ID3D10ShaderResourceView*           g_pVelocityTexRV = NULL;
ID3D10RenderTargetView**            g_pVelocityTexRTVs = NULL;
ID3D10RenderTargetView*             g_pVelocityTexTotalRTV = NULL;

// paint necessities
int                                 g_iControlMesh = 1;
bool                                g_bPaint = false;
bool                                g_bRandomizeColor = false;
UINT                                g_PositionWidth = 256;
UINT                                g_PositionHeight = 256;
UINT                                g_iParticleStart = 0;
UINT                                g_iParticleStep = 0;
float                               g_fParticlePaintRadSq = 0.0005f;

struct MESH
{
    CDXUTSDKMesh Mesh;
    ID3D10Texture2D* pMeshPositionTexture;
    ID3D10ShaderResourceView* pMeshPositionRV;
    ID3D10RenderTargetView* pMeshPositionRTV;

    UINT MeshTextureWidth;
    UINT MeshTextureHeight;
    ID3D10Texture2D* pMeshTexture;
    ID3D10ShaderResourceView* pMeshRV;
    ID3D10RenderTargetView* pMeshRTV;

    D3DXMATRIX mWorld;
    D3DXMATRIX mWorldPrev;

    D3DXVECTOR3 vPosition;
    D3DXVECTOR3 vRotation;
};

#define NUM_MESHES 2
MESH g_WorldMesh[NUM_MESHES];

// particle fx
ID3D10EffectTechnique*              g_pRenderParticles_Particle;
ID3D10EffectTechnique*              g_pAdvanceParticles_Particle;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj_Particle;
ID3D10EffectMatrixVariable*         g_pmInvView_Particle;
ID3D10EffectScalarVariable*         g_pfGlobalTime_Particle;
ID3D10EffectScalarVariable*         g_pfElapsedTime_Particle;
ID3D10EffectScalarVariable*         g_pfVolumeSize_Particle;
ID3D10EffectScalarVariable*         g_pfEmitterSize_Particle;
ID3D10EffectVectorVariable*         g_pvVolumeOffsets_Particle;
ID3D10EffectVectorVariable*         g_pvFrameGravity_Particle;
ID3D10EffectVectorVariable*         g_pvParticleColor_Particle;
ID3D10EffectVectorVariable*         g_pvEmitterPos_Particle;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse_Particle;
ID3D10EffectShaderResourceVariable* g_ptxRandom_Particle;
ID3D10EffectShaderResourceVariable* g_ptxVolume_Particle;
ID3D10EffectShaderResourceVariable* g_ptxVelocity_Particle;

// mesh fx
ID3D10EffectTechnique*              g_pRenderScene_Mesh;
ID3D10EffectTechnique*              g_pRenderAnimScene_Mesh;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj_Mesh;
ID3D10EffectMatrixVariable*         g_pmViewProj_Mesh;
ID3D10EffectMatrixVariable*         g_pmWorld_Mesh;
ID3D10EffectMatrixVariable*         g_pmBoneWorld_Mesh;
ID3D10EffectMatrixVariable*         g_pmBonePrev_Mesh;
ID3D10EffectVectorVariable*         g_pvLightDir_Mesh;
ID3D10EffectVectorVariable*         g_pvEyePt_Mesh;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse_Mesh;
ID3D10EffectShaderResourceVariable* g_ptxNormal_Mesh;
ID3D10EffectShaderResourceVariable* g_ptxSpecular_Mesh;
ID3D10EffectShaderResourceVariable* g_ptxPaint_Mesh;

// render to volume fx
ID3D10EffectTechnique*              g_pRenderScene_Vol;
ID3D10EffectTechnique*              g_pRenderAnimScene_Vol;
ID3D10EffectTechnique*              g_pRenderAnimVelocity_Vol;
ID3D10EffectTechnique*              g_pRenderVelocity_Vol;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj_Vol;
ID3D10EffectMatrixVariable*         g_pmViewProj_Vol;
ID3D10EffectMatrixVariable*         g_pmWorld_Vol;
ID3D10EffectMatrixVariable*         g_pmWorldPrev_Vol;
ID3D10EffectMatrixVariable*         g_pmBoneWorld_Vol;
ID3D10EffectMatrixVariable*         g_pmBonePrev_Vol;
ID3D10EffectScalarVariable*         g_pfElapsedTime_Vol;
ID3D10EffectScalarVariable*         g_pfPlaneStart_Vol;
ID3D10EffectScalarVariable*         g_pfPlaneStep_Vol;
ID3D10EffectVectorVariable*         g_pvFarClipPlane_Vol;
ID3D10EffectVectorVariable*         g_pvNearClipPlane_Vol;

// paint fx
ID3D10EffectTechnique*              g_pRenderToUV_Paint;
ID3D10EffectTechnique*              g_pRenderAnimToUV_Paint;
ID3D10EffectTechnique*              g_pPaint_Paint;
ID3D10EffectMatrixVariable*         g_pmWorld_Paint;
ID3D10EffectMatrixVariable*         g_pmBoneWorld_Paint;
ID3D10EffectScalarVariable*         g_pNumParticles_Paint;
ID3D10EffectScalarVariable*         g_pParticleStart_Paint;
ID3D10EffectScalarVariable*         g_pParticleStep_Paint;
ID3D10EffectScalarVariable*         g_pfParticleRadiusSq_Paint;
ID3D10EffectVectorVariable*         g_pvParticleColor_Paint;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse_Paint;
ID3D10EffectShaderResourceVariable* g_pParticleBuffer_Paint;

// for the dynamic mesh

struct PARTICLE_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR4 lastpos;
    D3DXVECTOR4 color;
    D3DXVECTOR3 vel;
    UINT ID;
};
#define MAX_PARTICLES 5000
UINT                                g_OptimalParticlesPerShader = 200;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWARP          5

// sample UI
#define IDC_TOGGLEMESH					49
#define IDC_PAINTTOGGLE					50
#define IDC_PARTICLECOLOR				51
#define IDC_PARTICLEOPACITY_STATIC		52
#define IDC_PARTICLEOPACITY				53
#define IDC_RANDOMIZECOLOR				54

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

HRESULT CreateParticleBuffer( ID3D10Device* pd3dDevice );
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice );
HRESULT CreateVolumeTexture( ID3D10Device* pd3dDevice );
HRESULT CreatePositionTextures( ID3D10Device* pd3dDevice, MESH* pMesh );
HRESULT CreateMeshTexture( ID3D10Device* pd3dDevice, UINT width, UINT height, MESH* pMesh );
HRESULT CreateQuadVB( ID3D10Device* pd3dDevice );
void HandleKeys( float fElapsedTime );

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
    DXUTCreateWindow( L"AdvancedParticles" );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop
}

float RPercent()
{
    float ret = ( float )( ( rand() % 20000 ) - 10000 );
    return ret / 10000.0f;
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_iParticleStep = MAX_PARTICLES / g_OptimalParticlesPerShader;

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    int iX = 35;
    iY = 0;
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddButton( IDC_TOGGLEMESH, L"Toggle Active Mesh", iX, iY += 24, 125, 22 );
    iY += 30;
    g_SampleUI.AddCheckBox( IDC_PAINTTOGGLE, L"Toggle Painting", iX, iY, 125, 22 );
    g_SampleUI.AddButton( IDC_PARTICLECOLOR, L"Particle Color", iX, iY += 24, 125, 22 );
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Particle Opacity %.2f", g_fParticleOpacity );
    g_SampleUI.AddStatic( IDC_PARTICLEOPACITY_STATIC, str, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_PARTICLEOPACITY, iX - 20, iY += 24, 125, 22, 0,
                          100, ( int )( 100 * g_fParticleOpacity ) );
    g_SampleUI.AddCheckBox( IDC_RANDOMIZECOLOR, L"Randomize Color", iX, iY += 24, 125, 22 );

}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d10.SyncInterval = DXGI_SWAP_EFFECT_DISCARD;
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
static double                       fLastRandomize = 0.0;
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    if( g_bRandomizeColor )
    {
        if( fTime - fLastRandomize > 2.0 )
        {
            g_vParticleColor.x = fabs( RPercent() );
            g_vParticleColor.y = fabs( RPercent() );
            g_vParticleColor.z = fabs( RPercent() );

            fLastRandomize = fTime;
        }
    }

    HandleKeys( fElapsedTime );
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
static COLORREF g_customColors[16] = {0};
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
        case IDC_TOGGLEMESH:
        {
            g_iControlMesh ++;
            if( g_iControlMesh >= NUM_MESHES )
                g_iControlMesh = 0;
        }
            break;
        case IDC_PAINTTOGGLE:
            g_bPaint = !g_bPaint;
            break;
        case IDC_PARTICLECOLOR:
        {
            CHOOSECOLOR chcol;
            ZeroMemory( &chcol, sizeof( CHOOSECOLOR ) );
            chcol.lStructSize = sizeof( CHOOSECOLOR );
            chcol.rgbResult = RGB( ( BYTE )( g_vParticleColor.x * 255.0f ), ( BYTE )( g_vParticleColor.y * 255.0f ),
                                   ( BYTE )( g_vParticleColor.z * 255.0f ) );
            chcol.Flags = CC_RGBINIT | CC_FULLOPEN;
            chcol.lpCustColors = g_customColors;

            DXUTPause( true, true );
            if( ChooseColor( &chcol ) )
            {
                g_vParticleColor.x = GetRValue( chcol.rgbResult ) / 255.0f;
                g_vParticleColor.y = GetGValue( chcol.rgbResult ) / 255.0f;
                g_vParticleColor.z = GetBValue( chcol.rgbResult ) / 255.0f;
            }
            DXUTPause( false, false );
        }
            break;
        case IDC_PARTICLEOPACITY:
        {
            g_fParticleOpacity = g_SampleUI.GetSlider( IDC_PARTICLEOPACITY )->GetValue() / 100.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Particle Opacity %.2f", g_fParticleOpacity );
            g_SampleUI.GetStatic( IDC_PARTICLEOPACITY_STATIC )->SetText( str );
        }
            break;
        case IDC_RANDOMIZECOLOR:
            g_bRandomizeColor = !g_bRandomizeColor;
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

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    UINT uFlags = D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_DEBUG;
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"AdvancedParticles.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", uFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pParticleEffect10, NULL, NULL ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Meshes.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", uFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pMeshEffect10, NULL, NULL ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"RenderToVolume.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", uFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pVolEffect10, NULL, NULL ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Paint.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", uFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pPaintEffect10, NULL, NULL ) );


    // Obtain the technique handles for particle fx
    g_pRenderParticles_Particle = g_pParticleEffect10->GetTechniqueByName( "RenderParticles" );
    g_pAdvanceParticles_Particle = g_pParticleEffect10->GetTechniqueByName( "AdvanceParticles" );
    // Obtain the parameter handles for particle fx
    g_pmWorldViewProj_Particle = g_pParticleEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmInvView_Particle = g_pParticleEffect10->GetVariableByName( "g_mInvView" )->AsMatrix();
    g_pfGlobalTime_Particle = g_pParticleEffect10->GetVariableByName( "g_fGlobalTime" )->AsScalar();
    g_pfElapsedTime_Particle = g_pParticleEffect10->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_pfVolumeSize_Particle = g_pParticleEffect10->GetVariableByName( "g_fVolumeSize" )->AsScalar();
    g_pfEmitterSize_Particle = g_pParticleEffect10->GetVariableByName( "g_fEmitterSize" )->AsScalar();
    g_pvVolumeOffsets_Particle = g_pParticleEffect10->GetVariableByName( "g_vVolumeOffsets" )->AsVector();
    g_pvFrameGravity_Particle = g_pParticleEffect10->GetVariableByName( "g_vFrameGravity" )->AsVector();
    g_pvParticleColor_Particle = g_pParticleEffect10->GetVariableByName( "g_vParticleColor" )->AsVector();
    g_pvEmitterPos_Particle = g_pParticleEffect10->GetVariableByName( "g_vEmitterPos" )->AsVector();
    g_ptxDiffuse_Particle = g_pParticleEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxRandom_Particle = g_pParticleEffect10->GetVariableByName( "g_txRandom" )->AsShaderResource();
    g_ptxVolume_Particle = g_pParticleEffect10->GetVariableByName( "g_txVolume" )->AsShaderResource();
    g_ptxVelocity_Particle = g_pParticleEffect10->GetVariableByName( "g_txVelocity" )->AsShaderResource();

    // Obtain the technique handles for meshes fx
    g_pRenderScene_Mesh = g_pMeshEffect10->GetTechniqueByName( "RenderScene" );
    g_pRenderAnimScene_Mesh = g_pMeshEffect10->GetTechniqueByName( "RenderAnimScene" );
    // Obtain the parameter handles for meshes fx
    g_pmWorldViewProj_Mesh = g_pMeshEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmViewProj_Mesh = g_pMeshEffect10->GetVariableByName( "g_mViewProj" )->AsMatrix();
    g_pmWorld_Mesh = g_pMeshEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmBoneWorld_Mesh = g_pMeshEffect10->GetVariableByName( "g_mBoneWorld" )->AsMatrix();
    g_pmBonePrev_Mesh = g_pMeshEffect10->GetVariableByName( "g_mBonePrev" )->AsMatrix();
    g_pvLightDir_Mesh = g_pMeshEffect10->GetVariableByName( "g_vLightDir" )->AsVector();
    g_pvEyePt_Mesh = g_pMeshEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_ptxDiffuse_Mesh = g_pMeshEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxNormal_Mesh = g_pMeshEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();
    g_ptxSpecular_Mesh = g_pMeshEffect10->GetVariableByName( "g_txSpecular" )->AsShaderResource();
    g_ptxPaint_Mesh = g_pMeshEffect10->GetVariableByName( "g_txPaint" )->AsShaderResource();

    // Obtain the technique handles for volume fx
    g_pRenderScene_Vol = g_pVolEffect10->GetTechniqueByName( "RenderScene" );
    g_pRenderAnimScene_Vol = g_pVolEffect10->GetTechniqueByName( "RenderAnimScene" );
    g_pRenderVelocity_Vol = g_pVolEffect10->GetTechniqueByName( "RenderVelocity" );
    g_pRenderAnimVelocity_Vol = g_pVolEffect10->GetTechniqueByName( "RenderAnimVelocity" );
    // Obtain the parameter handles for volume fx
    g_pmWorldViewProj_Vol = g_pVolEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmViewProj_Vol = g_pVolEffect10->GetVariableByName( "g_mViewProj" )->AsMatrix();
    g_pmWorld_Vol = g_pVolEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmWorldPrev_Vol = g_pVolEffect10->GetVariableByName( "g_mWorldPrev" )->AsMatrix();
    g_pmBoneWorld_Vol = g_pVolEffect10->GetVariableByName( "g_mBoneWorld" )->AsMatrix();
    g_pmBonePrev_Vol = g_pVolEffect10->GetVariableByName( "g_mBonePrev" )->AsMatrix();
    g_pfElapsedTime_Vol = g_pVolEffect10->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_pfPlaneStart_Vol = g_pVolEffect10->GetVariableByName( "g_fPlaneStart" )->AsScalar();
    g_pfPlaneStep_Vol = g_pVolEffect10->GetVariableByName( "g_fPlaneStep" )->AsScalar();
    g_pvFarClipPlane_Vol = g_pVolEffect10->GetVariableByName( "g_vFarClipPlane" )->AsVector();
    g_pvNearClipPlane_Vol = g_pVolEffect10->GetVariableByName( "g_vNearClipPlane" )->AsVector();

    // Obtain the technique handles for paint fx
    g_pRenderToUV_Paint = g_pPaintEffect10->GetTechniqueByName( "RenderToUV" );
    g_pRenderAnimToUV_Paint = g_pPaintEffect10->GetTechniqueByName( "RenderAnimToUV" );
    g_pPaint_Paint = g_pPaintEffect10->GetTechniqueByName( "Paint" );
    // Obtain the parameter handles for paint fx
    g_pmWorld_Paint = g_pPaintEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmBoneWorld_Paint = g_pPaintEffect10->GetVariableByName( "g_mBoneWorld" )->AsMatrix();
    g_pNumParticles_Paint = g_pPaintEffect10->GetVariableByName( "g_NumParticles" )->AsScalar();
    g_pParticleStart_Paint = g_pPaintEffect10->GetVariableByName( "g_ParticleStart" )->AsScalar();
    g_pParticleStep_Paint = g_pPaintEffect10->GetVariableByName( "g_ParticleStep" )->AsScalar();
    g_pfParticleRadiusSq_Paint = g_pPaintEffect10->GetVariableByName( "g_fParticleRadiusSq" )->AsScalar();
    g_pvParticleColor_Paint = g_pPaintEffect10->GetVariableByName( "g_vParticleColor" )->AsVector();
    g_ptxDiffuse_Paint = g_pPaintEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pParticleBuffer_Paint = g_pPaintEffect10->GetVariableByName( "g_ParticleBuffer" )->AsShaderResource();

    // Create our vertex input layout for particles
    const D3D10_INPUT_ELEMENT_DESC particlelayout[] =
    {
        { "POSITION",		0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "LASTPOSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",			0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "VELOCITY",		0, DXGI_FORMAT_R32G32B32_FLOAT,	   0, 48, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "ID",				0, DXGI_FORMAT_R32_UINT,           0, 60, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pAdvanceParticles_Particle->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( particlelayout, sizeof( particlelayout ) / sizeof( particlelayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pParticleVertexLayout ) );

    // Create our vertex input layout for meshes
    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderScene_Mesh->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, sizeof( meshlayout ) / sizeof( meshlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshVertexLayout ) );

    const D3D10_INPUT_ELEMENT_DESC skinnedlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES",    0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderAnimScene_Mesh->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( skinnedlayout, sizeof( skinnedlayout ) / sizeof( skinnedlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pAnimVertexLayout ) );
    g_pRenderAnimToUV_Paint->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( skinnedlayout, sizeof( skinnedlayout ) / sizeof( skinnedlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pPaintAnimVertexLayout ) );

    // Create our vertex input layout for screen quads
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pPaint_Paint->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadVertexLayout ) );

    // Load meshes
    V_RETURN( g_WorldMesh[0].Mesh.Create( pd3dDevice, L"advancedparticles\\lizardSIG.sdkmesh" ) );
    V_RETURN( g_WorldMesh[0].Mesh.LoadAnimation( L"advancedparticles\\lizardSIG.sdkmesh_anim" ) );
    V_RETURN( CreateMeshTexture( pd3dDevice, 256, 256, &g_WorldMesh[0] ) );
    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity( &mIdentity );
    g_WorldMesh[0].Mesh.TransformBindPose( &mIdentity );

    V_RETURN( g_WorldMesh[1].Mesh.Create( pd3dDevice, L"advancedparticles\\blocktest.sdkmesh" ) );
    V_RETURN( CreateMeshTexture( pd3dDevice, 256, 256, &g_WorldMesh[1] ) );

    // Create the seeding particle
    V_RETURN( CreateParticleBuffer( pd3dDevice ) );

    // Load the Particle Texture
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\Particle.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pParticleTexRV, NULL ) );

    // Create the random texture that fuels our random vector generator in the effect
    V_RETURN( CreateRandomTexture( pd3dDevice ) );

    // Create the volume texture and render target views, etc
    V_RETURN( CreateVolumeTexture( pd3dDevice ) );

    // Create position texture for the mesh
    for( UINT i = 0; i < NUM_MESHES; i++ )
        V_RETURN( CreatePositionTextures( pd3dDevice, &g_WorldMesh[i] ) );

    // Let the app know that this if the first time drawing particles
    g_bFirst = true;

    V_RETURN( CreateQuadVB( pd3dDevice ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.6f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.6f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Move the block into position
    g_WorldMesh[1].vRotation.x = 0;
    g_WorldMesh[1].vRotation.y = 0;
    g_WorldMesh[1].vRotation.z = -30.0f * ( 180.0f / 3.14159f );
    g_WorldMesh[1].vPosition.x = 0.4f;
    g_WorldMesh[1].vPosition.y = 0.90f;

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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_MIDDLE_BUTTON | MOUSE_RIGHT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    BOOL bFullscreen = FALSE;
    pSwapChain->GetFullscreenState( &bFullscreen, NULL );
    g_SampleUI.GetButton( IDC_PARTICLECOLOR )->SetEnabled( !bFullscreen );

    return hr;
}


//--------------------------------------------------------------------------------------
bool AdvanceParticles( ID3D10Device* pd3dDevice, float fGlobalTime, float fElapsedTime )
{
    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1];
    if( g_bFirst )
        pBuffers[0] = g_pParticleStart;
    else
        pBuffers[0] = g_pParticleDrawFrom;
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Point to the correct output buffer
    pBuffers[0] = g_pParticleStreamTo;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    D3DXVECTOR4 vColor;
    vColor.x = g_vParticleColor.x;
    vColor.y = g_vParticleColor.y;
    vColor.z = g_vParticleColor.z;
    vColor.w = g_fParticleOpacity;

    // Set Effects Parameters
    g_pfGlobalTime_Particle->SetFloat( fGlobalTime );
    g_pfElapsedTime_Particle->SetFloat( fElapsedTime );
    g_pvFrameGravity_Particle->SetFloatVector( ( float* )&g_vGravity );
    g_pvParticleColor_Particle->SetFloatVector( ( float* )&vColor );
    D3DXVECTOR4 vEmitterPos( g_vEmitterPos, 1 );
    g_pvEmitterPos_Particle->SetFloatVector( ( float* )&vEmitterPos );
    g_ptxRandom_Particle->SetResource( g_pRandomTexRV );

    // set volume params
    g_ptxVolume_Particle->SetResource( g_pVolumeTexRV );
    g_ptxVelocity_Particle->SetResource( g_pVelocityTexRV );
    g_pfVolumeSize_Particle->SetFloat( g_VolumeSize );
    g_pfEmitterSize_Particle->SetFloat( g_fEmitterSize );
    D3DXVECTOR4 vOffsets( 0.5f / g_VolumeWidth, 0.5f / g_VolumeHeight, 0.5f / g_VolumeDepth, 0 );
    g_pvVolumeOffsets_Particle->SetFloatVector( ( float* )vOffsets );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pAdvanceParticles_Particle->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pAdvanceParticles_Particle->GetPassByIndex( p )->Apply( 0 );
        if( g_bFirst )
            pd3dDevice->Draw( MAX_PARTICLES, 0 );
        else
            pd3dDevice->DrawAuto();
    }

    // Get back to normal
    pBuffers[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pBuffers, offset );

    // Swap particle buffers
    ID3D10Buffer* pTemp = g_pParticleDrawFrom;
    g_pParticleDrawFrom = g_pParticleStreamTo;
    g_pParticleStreamTo = pTemp;

    ID3D10ShaderResourceView* pTempRV = g_pParticleDrawFromRV;
    g_pParticleDrawFromRV = g_pParticleStreamToRV;
    g_pParticleStreamToRV = pTempRV;

    g_bFirst = false;

    return true;
}


//--------------------------------------------------------------------------------------
bool RenderParticles( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pParticleDrawFrom };
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Set Effects Parameters
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj_Particle->SetMatrix( ( float* )&mWorldViewProj );
    g_ptxDiffuse_Particle->SetResource( g_pParticleTexRV );
    D3DXMATRIX mInvView;
    D3DXMatrixInverse( &mInvView, NULL, &mView );
    g_pmInvView_Particle->SetMatrix( ( float* )&mInvView );

    // set volume params
    g_ptxVolume_Particle->SetResource( g_pVolumeTexRV );
    g_ptxVelocity_Particle->SetResource( g_pVelocityTexRV );
    g_pfVolumeSize_Particle->SetFloat( g_VolumeSize );
    D3DXVECTOR4 vOffsets( 0.5f / g_VolumeWidth, 0.5f / g_VolumeHeight, 0.5f / g_VolumeDepth, 0 );
    g_pvVolumeOffsets_Particle->SetFloatVector( ( float* )vOffsets );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderParticles_Particle->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pRenderParticles_Particle->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawAuto();
    }

    return true;
}


//--------------------------------------------------------------------------------------
bool RenderAnimatedMeshes( ID3D10Device* pd3dDevice,
                           D3DXMATRIX& mView,
                           D3DXMATRIX& mProj,
                           ID3D10EffectTechnique* pTechnique,
                           double fTime,
                           float fElapsedTime,
                           bool bVolume )
{
    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pAnimVertexLayout );

    for( UINT i = 0; i < 1; i++ )
    {
        D3DXMATRIX mWorldViewProj;
        D3DXMATRIX mViewProj;
        mWorldViewProj = g_WorldMesh[i].mWorld * mView * mProj;
        mViewProj = mView * mProj;

        CDXUTSDKMesh* pMesh = &g_WorldMesh[i].Mesh;

        // Set Effects Parameters
        if( bVolume )
        {
            g_pmWorldViewProj_Vol->SetMatrix( ( float* )&mWorldViewProj );
            g_pmViewProj_Vol->SetMatrix( ( float* )&mViewProj );
            g_pmWorld_Vol->SetMatrix( ( float* )&g_WorldMesh[i].mWorld );
            g_pmWorldPrev_Vol->SetMatrix( ( float* )&g_WorldMesh[i].mWorldPrev );

            UINT NumInfluences = pMesh->GetNumInfluences( 0 );
            if( NumInfluences > 0 )
            {
                // Transform current position
                pMesh->TransformMesh( &g_WorldMesh[i].mWorld, fTime );
                for( UINT g = 0; g < NumInfluences; g++ )
                {
                    const D3DXMATRIX* pMat = pMesh->GetMeshInfluenceMatrix( 0, g );
                    g_pmBoneWorld_Vol->SetMatrixArray( ( float* )pMat, g, 1 );
                }

                // Transform previous position
                pMesh->TransformMesh( &g_WorldMesh[i].mWorldPrev, fTime - fElapsedTime );
                for( UINT g = 0; g < NumInfluences; g++ )
                {
                    const D3DXMATRIX* pMat = pMesh->GetMeshInfluenceMatrix( 0, g );
                    g_pmBonePrev_Vol->SetMatrixArray( ( float* )pMat, g, 1 );
                }
            }

            ID3D10Buffer* pBuffer = pMesh->GetVB10( 0, 0 );
            UINT iStride = pMesh->GetVertexStride( 0, 0 );
            UINT iOffset = 0;
            pd3dDevice->IASetVertexBuffers( 0, 1, &pBuffer, &iStride, &iOffset );
            pd3dDevice->IASetIndexBuffer( pMesh->GetIB10( 0 ), pMesh->GetIBFormat10( 0 ), iOffset );

            D3D10_TECHNIQUE_DESC techDesc;
            pTechnique->GetDesc( &techDesc );
            SDKMESH_SUBSET* pSubset = NULL;
            D3D10_PRIMITIVE_TOPOLOGY PrimType;

            for( UINT p = 0; p < techDesc.Passes; ++p )
            {
                for( UINT subset = 0; subset < pMesh->GetNumSubsets( 0 ); subset++ )
                {
                    pSubset = pMesh->GetSubset( 0, subset );

                    PrimType = pMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                    pd3dDevice->IASetPrimitiveTopology( PrimType );

                    pTechnique->GetPassByIndex( p )->Apply( 0 );

                    UINT IndexCount = ( UINT )pSubset->IndexCount;
                    UINT IndexStart = ( UINT )pSubset->IndexStart;
                    UINT VertexStart = ( UINT )pSubset->VertexStart;
                    pd3dDevice->DrawIndexedInstanced( IndexCount, g_VolumeDepth, IndexStart, VertexStart, 0 );
                }
            }
        }
        else
        {
            g_pmWorldViewProj_Mesh->SetMatrix( ( float* )&mWorldViewProj );
            g_pmViewProj_Mesh->SetMatrix( ( float* )&mViewProj );
            g_pmWorld_Mesh->SetMatrix( ( float* )&g_WorldMesh[i].mWorld );
            D3DXVECTOR4 vLightDir( 0,1,0,0 );
            g_pvLightDir_Mesh->SetFloatVector( ( float* )&vLightDir );
            D3DXVECTOR4 vEye = D3DXVECTOR4( *g_Camera.GetEyePt(), 1 );
            g_pvEyePt_Mesh->SetFloatVector( ( float* )&vEye );

            UINT NumInfluences = pMesh->GetNumInfluences( 0 );
            if( NumInfluences > 0 )
            {
                // Transform current position
                pMesh->TransformMesh( &g_WorldMesh[i].mWorld, fTime );
                for( UINT g = 0; g < NumInfluences; g++ )
                {
                    const D3DXMATRIX* pMat = pMesh->GetMeshInfluenceMatrix( 0, g );
                    g_pmBoneWorld_Mesh->SetMatrixArray( ( float* )pMat, g, 1 );
                }

                // Transform previous position
                pMesh->TransformMesh( &g_WorldMesh[i].mWorldPrev, fTime - fElapsedTime );
                for( UINT g = 0; g < NumInfluences; g++ )
                {
                    const D3DXMATRIX* pMat = pMesh->GetMeshInfluenceMatrix( 0, g );
                    g_pmBonePrev_Mesh->SetMatrixArray( ( float* )pMat, g, 1 );
                }
            }

            g_ptxPaint_Mesh->SetResource( g_WorldMesh[i].pMeshRV );

            pMesh->Render( pd3dDevice, pTechnique, g_ptxDiffuse_Mesh, g_ptxNormal_Mesh, g_ptxSpecular_Mesh );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
bool RenderMeshes( ID3D10Device* pd3dDevice,
                   D3DXMATRIX& mView,
                   D3DXMATRIX& mProj,
                   ID3D10EffectTechnique* pTechnique,
                   bool bVolume )
{
    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pMeshVertexLayout );

    for( UINT i = 1; i < NUM_MESHES; i++ )
    {
        D3DXMATRIX mWorldViewProj;
        D3DXMATRIX mViewProj;
        mWorldViewProj = g_WorldMesh[i].mWorld * mView * mProj;
        mViewProj = mView * mProj;

        CDXUTSDKMesh* pMesh = &g_WorldMesh[i].Mesh;

        // Set Effects Parameters
        if( bVolume )
        {
            g_pmWorldViewProj_Vol->SetMatrix( ( float* )&mWorldViewProj );
            g_pmViewProj_Vol->SetMatrix( ( float* )&mViewProj );
            g_pmWorld_Vol->SetMatrix( ( float* )&g_WorldMesh[i].mWorld );
            g_pmWorldPrev_Vol->SetMatrix( ( float* )&g_WorldMesh[i].mWorldPrev );

            ID3D10Buffer* pBuffer = pMesh->GetVB10( 0, 0 );
            UINT iStride = pMesh->GetVertexStride( 0, 0 );
            UINT iOffset = 0;
            pd3dDevice->IASetVertexBuffers( 0, 1, &pBuffer, &iStride, &iOffset );
            pd3dDevice->IASetIndexBuffer( pMesh->GetIB10( 0 ), pMesh->GetIBFormat10( 0 ), iOffset );

            D3D10_TECHNIQUE_DESC techDesc;
            pTechnique->GetDesc( &techDesc );
            SDKMESH_SUBSET* pSubset = NULL;
            D3D10_PRIMITIVE_TOPOLOGY PrimType;

            for( UINT p = 0; p < techDesc.Passes; ++p )
            {
                for( UINT subset = 0; subset < pMesh->GetNumSubsets( 0 ); subset++ )
                {
                    pSubset = pMesh->GetSubset( 0, subset );

                    PrimType = pMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                    pd3dDevice->IASetPrimitiveTopology( PrimType );

                    pTechnique->GetPassByIndex( p )->Apply( 0 );

                    UINT IndexCount = ( UINT )pSubset->IndexCount;
                    UINT IndexStart = ( UINT )pSubset->IndexStart;
                    UINT VertexStart = ( UINT )pSubset->VertexStart;
                    pd3dDevice->DrawIndexedInstanced( IndexCount, g_VolumeDepth, IndexStart, VertexStart, 0 );
                }
            }
            //pMesh->Render( pd3dDevice, pTechnique );
        }
        else
        {
            g_pmWorldViewProj_Mesh->SetMatrix( ( float* )&mWorldViewProj );
            g_pmViewProj_Mesh->SetMatrix( ( float* )&mViewProj );
            g_pmWorld_Mesh->SetMatrix( ( float* )&g_WorldMesh[i].mWorld );
            D3DXVECTOR4 vLightDir( 0,1,0,0 );
            g_pvLightDir_Mesh->SetFloatVector( ( float* )&vLightDir );

            g_ptxDiffuse_Mesh->SetResource( g_WorldMesh[i].pMeshRV );
            pMesh->Render( pd3dDevice, pTechnique, NULL, g_ptxNormal_Mesh, g_ptxSpecular_Mesh );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
void ClearVolume( ID3D10Device* pd3dDevice )
{
    // clear the render target
    float ClearColor[4] = { 0.0f,0.0f,0.0f,0.0f };

    pd3dDevice->ClearRenderTargetView( g_pVolumeTexTotalRTV, ClearColor );
    pd3dDevice->ClearRenderTargetView( g_pVelocityTexTotalRTV, ClearColor );
}

//--------------------------------------------------------------------------------------
bool RenderMeshesIntoVolume( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, double fTime,
                             float fElapsedTime, bool bAnimated )
{
    // store the old render target and DS
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // store the old viewport
    D3D10_VIEWPORT oldViewport;
    UINT NumViewports = 1;
    pd3dDevice->RSGetViewports( &NumViewports, &oldViewport );

    // set the new viewport
    D3D10_VIEWPORT newViewport;
    newViewport.TopLeftX = 0;
    newViewport.TopLeftY = 0;
    newViewport.Width = g_VolumeWidth;
    newViewport.Height = g_VolumeHeight;
    newViewport.MinDepth = 0.0f;
    newViewport.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &newViewport );

    // setup and orthogonal view from the top
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXVECTOR3 vEye( 0,100,0 );
    D3DXVECTOR3 vAt( 0,0,0 );
    D3DXVECTOR3 vUp( 0,0,1 );
    D3DXMatrixLookAtLH( &mView, &vEye, &vAt, &vUp );
    D3DXMatrixOrthoLH( &mProj, g_VolumeSize, g_VolumeSize, 0.1f, 1000.0f );

    float fSliceSize = g_VolumeSize / ( float )( g_VolumeDepth + 1 );
    float fSliceStart = 0.0f;

    // Set the render target
    pd3dDevice->OMSetRenderTargets( 1, &g_pVolumeTexTotalRTV, NULL );

    // Set effect variables
    g_pfPlaneStart_Vol->SetFloat( fSliceStart );
    g_pfPlaneStep_Vol->SetFloat( fSliceSize );
    g_pfElapsedTime_Vol->SetFloat( fElapsedTime );

    // render the scene
    if( bAnimated )
        RenderAnimatedMeshes( pd3dDevice, mView, mProj, pTechnique, fTime, fElapsedTime, true );
    else
        RenderMeshes( pd3dDevice, mView, mProj, pTechnique, true );

    // restore the old viewport
    pd3dDevice->RSSetViewports( 1, &oldViewport );

    // restore old render target and DS
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    return true;
}

//--------------------------------------------------------------------------------------
bool RenderVelocitiesIntoVolume( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, double fTime,
                                 float fElapsedTime, bool bAnimated )
{
    // store the old render target and DS
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // store the old viewport
    D3D10_VIEWPORT oldViewport;
    UINT NumViewports = 1;
    pd3dDevice->RSGetViewports( &NumViewports, &oldViewport );

    // set the new viewport
    D3D10_VIEWPORT newViewport;
    newViewport.TopLeftX = 0;
    newViewport.TopLeftY = 0;
    newViewport.Width = g_VolumeWidth;
    newViewport.Height = g_VolumeHeight;
    newViewport.MinDepth = 0.0f;
    newViewport.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &newViewport );

    // setup and orthogonal view from the top
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXVECTOR3 vEye( 0,100,0 );
    D3DXVECTOR3 vAt( 0,0,0 );
    D3DXVECTOR3 vUp( 0,0,1 );
    D3DXMatrixLookAtLH( &mView, &vEye, &vAt, &vUp );
    D3DXMatrixOrthoLH( &mProj, g_VolumeSize, g_VolumeSize, 0.1f, 1000.0f );

    float fSliceSize = g_VolumeSize / ( float )( g_VolumeDepth + 1 );
    float fSliceStart = 0.0f;

    // Set the render target
    pd3dDevice->OMSetRenderTargets( 1, &g_pVelocityTexTotalRTV, NULL );

    // Calc clip planes
    D3DXVECTOR4 vFarClip( 0,1,0,0 );
    D3DXVECTOR4 vNearClip( 0,-1,0,0 );
    vFarClip.w = -fSliceStart;
    vNearClip.w = ( fSliceStart + fSliceSize );

    // Set effect variables
    g_pfPlaneStart_Vol->SetFloat( 0.0f );
    g_pfPlaneStep_Vol->SetFloat( fSliceSize );
    g_pfElapsedTime_Vol->SetFloat( fElapsedTime );

    // render the scene
    if( bAnimated )
        RenderAnimatedMeshes( pd3dDevice, mView, mProj, pTechnique, fTime, fElapsedTime, true );
    else
        RenderMeshes( pd3dDevice, mView, mProj, pTechnique, true );

    // restore the old viewport
    pd3dDevice->RSSetViewports( 1, &oldViewport );

    // restore old render target and DS
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    return true;
}

//--------------------------------------------------------------------------------------
bool RenderPositionIntoTexture( ID3D10Device* pd3dDevice, MESH* pMesh, double fTime )
{
    // store the old render target and DS
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // store the old viewport
    D3D10_VIEWPORT oldViewport;
    UINT NumViewports = 1;
    pd3dDevice->RSGetViewports( &NumViewports, &oldViewport );

    // set the new viewport
    D3D10_VIEWPORT newViewport;
    newViewport.TopLeftX = 0;
    newViewport.TopLeftY = 0;
    newViewport.Width = g_PositionWidth;
    newViewport.Height = g_PositionHeight;
    newViewport.MinDepth = 0.0f;
    newViewport.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &newViewport );

    float ClearColor[4] = { 0,0,0,0 };
    pd3dDevice->ClearRenderTargetView( pMesh->pMeshPositionRTV, ClearColor );

    // Set the render target
    pd3dDevice->OMSetRenderTargets( 1, &pMesh->pMeshPositionRTV, NULL );

    // Set effect variables
    g_pmWorld_Paint->SetMatrix( ( float* )&pMesh->mWorld );

    ID3D10EffectTechnique* pTech = NULL;

    UINT NumInfluences = pMesh->Mesh.GetNumInfluences( 0 );
    if( NumInfluences > 0 )
    {
        pd3dDevice->IASetInputLayout( g_pPaintAnimVertexLayout );
        pTech = g_pRenderAnimToUV_Paint;

        // Transform current position
        pMesh->Mesh.TransformMesh( &pMesh->mWorld, fTime );
        for( UINT g = 0; g < NumInfluences; g++ )
        {
            const D3DXMATRIX* pMat = pMesh->Mesh.GetMeshInfluenceMatrix( 0, g );
            g_pmBoneWorld_Paint->SetMatrixArray( ( float* )pMat, g, 1 );
        }
    }
    else
    {
        pd3dDevice->IASetInputLayout( g_pMeshVertexLayout );
        pTech = g_pRenderToUV_Paint;
    }

    // render the mesh
    pMesh->Mesh.Render( pd3dDevice, pTech );

    // restore the old viewport
    pd3dDevice->RSSetViewports( 1, &oldViewport );

    // restore old render target and DS
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    return true;
}

bool PaintWithParticles( ID3D10Device* pd3dDevice, MESH* pMesh )
{
    // store the old render target and DS
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // store the old viewport
    D3D10_VIEWPORT oldViewport;
    UINT NumViewports = 1;
    pd3dDevice->RSGetViewports( &NumViewports, &oldViewport );

    // set the new viewport
    D3D10_VIEWPORT newViewport;
    newViewport.TopLeftX = 0;
    newViewport.TopLeftY = 0;
    newViewport.Width = pMesh->MeshTextureWidth;
    newViewport.Height = pMesh->MeshTextureHeight;
    newViewport.MinDepth = 0.0f;
    newViewport.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &newViewport );

    // Set the render target
    pd3dDevice->OMSetRenderTargets( 1, &pMesh->pMeshRTV, NULL );

    // Set effect variables
    g_pNumParticles_Paint->SetInt( MAX_PARTICLES );
    g_pParticleStart_Paint->SetInt( g_iParticleStart );
    g_pParticleStep_Paint->SetInt( g_iParticleStep );
    g_pfParticleRadiusSq_Paint->SetFloat( g_fParticlePaintRadSq );
    D3DXVECTOR4 vParticleColor( 0,1,0,1 );
    g_pvParticleColor_Paint->SetFloatVector( ( float* )&vParticleColor );
    g_ptxDiffuse_Paint->SetResource( pMesh->pMeshPositionRV );
    g_pParticleBuffer_Paint->SetResource( g_pParticleDrawFromRV );

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pQuadVertexLayout );

    // Set IA params
    UINT offsets = 0;
    UINT strides = sizeof( QUAD_VERTEX );
    pd3dDevice->IASetVertexBuffers( 0, 1, &g_pQuadVB, &strides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // set the technique
    D3D10_TECHNIQUE_DESC techDesc;
    g_pPaint_Paint->GetDesc( &techDesc );

    // render the quad
    for( UINT p = 0; p < techDesc.Passes; p++ )
    {
        g_pPaint_Paint->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }

    // restore the old viewport
    pd3dDevice->RSSetViewports( 1, &oldViewport );

    // restore old render target and DS
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    return true;
}


//--------------------------------------------------------------------------------------
void ClearShaderResources( ID3D10Device* pd3dDevice )
{
    // unbind resources
    ID3D10ShaderResourceView* pNULLViews[8] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
    pd3dDevice->VSSetShaderResources( 0, 8, pNULLViews );
    pd3dDevice->GSSetShaderResources( 0, 8, pNULLViews );
    pd3dDevice->PSSetShaderResources( 0, 8, pNULLViews );

}


//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // calc world matrix for dynamic object
    D3DXMATRIX mScale;
    D3DXMATRIX mRotX, mRotY, mRotZ;
    D3DXMATRIX mTrans;

    for( UINT i = 0; i < NUM_MESHES; i++ )
    {
        D3DXMatrixScaling( &mScale, 0.2f, 0.2f, 0.2f );
        D3DXMatrixRotationX( &mRotX, g_WorldMesh[i].vRotation.x );
        D3DXMatrixRotationY( &mRotY, g_WorldMesh[i].vRotation.y );
        D3DXMatrixRotationZ( &mRotZ, g_WorldMesh[i].vRotation.z );
        D3DXMatrixTranslation( &mTrans, g_WorldMesh[i].vPosition.x, g_WorldMesh[i].vPosition.y,
                               g_WorldMesh[i].vPosition.z );
        g_WorldMesh[i].mWorld = mScale * mRotX * mRotY * mRotZ * mTrans;
    }

    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // Rendet the mesh position into a texture
    if( g_bPaint )
    {
        for( UINT i = 0; i < NUM_MESHES; i++ )
        {
            RenderPositionIntoTexture( pd3dDevice, &g_WorldMesh[i], fTime );
        }
    }

    // Clear the volume
    ClearVolume( pd3dDevice );

    // Render meshes into the volume
    RenderMeshesIntoVolume( pd3dDevice, g_pRenderScene_Vol, fTime, fElapsedTime, false );
    RenderVelocitiesIntoVolume( pd3dDevice, g_pRenderVelocity_Vol, fTime, fElapsedTime, false );

    // Render animated meshes into the volume
    RenderMeshesIntoVolume( pd3dDevice, g_pRenderAnimScene_Vol, fTime, fElapsedTime, true );
    RenderVelocitiesIntoVolume( pd3dDevice, g_pRenderAnimVelocity_Vol, fTime, fElapsedTime, true );

    // Advance the Particles
    AdvanceParticles( pd3dDevice, ( float )fTime, fElapsedTime );

    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    // Get the projection & view matrix from the camera class
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    // Render scene meshes
    RenderMeshes( pd3dDevice, mView, mProj, g_pRenderScene_Mesh, false );
    RenderAnimatedMeshes( pd3dDevice, mView, mProj, g_pRenderAnimScene_Mesh, fTime, fElapsedTime, false );

    ClearShaderResources( pd3dDevice );

    // Render the particles
    RenderParticles( pd3dDevice, mView, mProj );

    if( g_bPaint )
    {
        // Paint the scene meshes with the particles
        for( UINT i = 0; i < NUM_MESHES; i++ )
            PaintWithParticles( pd3dDevice, &g_WorldMesh[i] );

        // step up
        g_iParticleStart ++;
        if( g_iParticleStart >= g_iParticleStep )
            g_iParticleStart = 0;
    }

    ClearShaderResources( pd3dDevice );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();

    // store the previous world
    for( UINT i = 0; i < NUM_MESHES; i++ )
        g_WorldMesh[i].mWorldPrev = g_WorldMesh[i].mWorld;
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( true ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( L"Use A,D,W,S,Q,E keys to move the active mesh" );
    g_pTxtHelper->DrawTextLine( L"Use SHIFT + A,D,W,S,Q,E keys to rotate the active mesh" );
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
    SAFE_RELEASE( g_pParticleEffect10 );
    SAFE_RELEASE( g_pMeshEffect10 );
    SAFE_RELEASE( g_pVolEffect10 );
    SAFE_RELEASE( g_pPaintEffect10 );

    SAFE_RELEASE( g_pQuadVB );

    SAFE_RELEASE( g_pParticleVertexLayout );
    SAFE_RELEASE( g_pMeshVertexLayout );
    SAFE_RELEASE( g_pQuadVertexLayout );
    SAFE_RELEASE( g_pAnimVertexLayout );
    SAFE_RELEASE( g_pPaintAnimVertexLayout );

    for( UINT i = 0; i < NUM_MESHES; i++ )
    {
        SAFE_RELEASE( g_WorldMesh[i].pMeshPositionTexture );
        SAFE_RELEASE( g_WorldMesh[i].pMeshPositionRV );
        SAFE_RELEASE( g_WorldMesh[i].pMeshPositionRTV );

        SAFE_RELEASE( g_WorldMesh[i].pMeshTexture );
        SAFE_RELEASE( g_WorldMesh[i].pMeshRV );
        SAFE_RELEASE( g_WorldMesh[i].pMeshRTV );

        g_WorldMesh[i].Mesh.Destroy();
    }

    // particles
    SAFE_RELEASE( g_pParticleStart );
    SAFE_RELEASE( g_pParticleStreamTo );
    SAFE_RELEASE( g_pParticleDrawFrom );
    SAFE_RELEASE( g_pParticleStreamToRV );
    SAFE_RELEASE( g_pParticleDrawFromRV );
    SAFE_RELEASE( g_pParticleTexRV );
    SAFE_RELEASE( g_pRandomTexture );
    SAFE_RELEASE( g_pRandomTexRV );

    // volume
    if( g_pVolumeTexRTVs )
    {
        for( UINT i = 0; i < g_VolumeDepth; i++ )
        {
            SAFE_RELEASE( g_pVolumeTexRTVs[i] );
        }
        SAFE_DELETE_ARRAY( g_pVolumeTexRTVs );
    }
    SAFE_RELEASE( g_pVolumeTexRV );
    SAFE_RELEASE( g_pVolumeTexture );

    SAFE_RELEASE( g_pVolumeTexTotalRTV );

    if( g_pVelocityTexRTVs )
    {
        for( UINT i = 0; i < g_VolumeDepth; i++ )
        {
            SAFE_RELEASE( g_pVelocityTexRTVs[i] );
        }
        SAFE_DELETE_ARRAY( g_pVelocityTexRTVs );
    }
    SAFE_RELEASE( g_pVelocityTexRV );
    SAFE_RELEASE( g_pVelocityTexture );

    SAFE_RELEASE( g_pVelocityTexTotalRTV );
}

//--------------------------------------------------------------------------------------
// This helper function creates 3 vertex buffers.  The first is used to seed the
// particle system.  The second two are used as output and intput buffers alternatively
// for the GS particle system.  Since a buffer cannot be both an input to the GS and an
// output of the GS, we must ping-pong between the two.
//--------------------------------------------------------------------------------------
HRESULT CreateParticleBuffer( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    D3D10_BUFFER_DESC vbdesc =
    {
        MAX_PARTICLES * sizeof( PARTICLE_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );

    srand( 100 );
    PARTICLE_VERTEX* pVertices = new PARTICLE_VERTEX[ MAX_PARTICLES ];
    for( UINT i = 0; i < MAX_PARTICLES; i++ )
    {
        pVertices[i].pos.x = g_vEmitterPos.x + RPercent() * g_fEmitterSize;
        pVertices[i].pos.y = g_vEmitterPos.y + RPercent() * g_fEmitterSize;
        pVertices[i].pos.z = g_vEmitterPos.z + RPercent() * g_fEmitterSize;
        pVertices[i].pos.w = 1.0f;
        pVertices[i].lastpos = pVertices[i].pos;
        pVertices[i].color = D3DXVECTOR4( 1, 0, 0, 1 );
        pVertices[i].vel = D3DXVECTOR3( 0, 0, 0 );
        pVertices[i].ID = i;
    }
    vbInitData.pSysMem = pVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &vbInitData, &g_pParticleStart ) );
    SAFE_DELETE_ARRAY( vbInitData.pSysMem );

    vbdesc.ByteWidth = MAX_PARTICLES * sizeof( PARTICLE_VERTEX );
    vbdesc.BindFlags |= D3D10_BIND_STREAM_OUTPUT | D3D10_BIND_SHADER_RESOURCE;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleDrawFrom ) );
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleStreamTo ) );

    // create resource views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = MAX_PARTICLES * ( sizeof( PARTICLE_VERTEX ) / sizeof( D3DXVECTOR4 ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pParticleStreamTo, &SRVDesc, &g_pParticleDrawFromRV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pParticleDrawFrom, &SRVDesc, &g_pParticleStreamToRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors.  The shader uses
// the current time value to index into this texture to get a random vector.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    int iNumRandValues = 4096;
    srand( timeGetTime() );
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
    SRVDesc.Texture2D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomTexture, &SRVDesc, &g_pRandomTexRV ) );

    return hr;
}

HRESULT CreateVolumeTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Create the texture
    D3D10_TEXTURE3D_DESC dstex;
    dstex.Width = g_VolumeWidth;
    dstex.Height = g_VolumeHeight;
    dstex.Depth = g_VolumeDepth;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture3D( &dstex, NULL, &g_pVolumeTexture ) );
    V_RETURN( pd3dDevice->CreateTexture3D( &dstex, NULL, &g_pVelocityTexture ) );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
    SRVDesc.Texture3D.MostDetailedMip = 0;
    SRVDesc.Texture3D.MipLevels = 1;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pVolumeTexture, &SRVDesc, &g_pVolumeTexRV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pVelocityTexture, &SRVDesc, &g_pVelocityTexRV ) );

    // Create the render target views
    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    RTVDesc.Format = dstex.Format;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
    RTVDesc.Texture3D.MipSlice = 0;
    RTVDesc.Texture3D.FirstWSlice = 0;
    RTVDesc.Texture3D.WSize = 1;

    g_pVolumeTexRTVs = new ID3D10RenderTargetView*[g_VolumeDepth];
    g_pVelocityTexRTVs = new ID3D10RenderTargetView*[g_VolumeDepth];
    for( UINT i = 0; i < g_VolumeDepth; i++ )
    {
        RTVDesc.Texture3D.FirstWSlice = i;
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_pVolumeTexture, &RTVDesc, &g_pVolumeTexRTVs[i] ) );
        V_RETURN( pd3dDevice->CreateRenderTargetView( g_pVelocityTexture, &RTVDesc, &g_pVelocityTexRTVs[i] ) );
    }

    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE3D;
    RTVDesc.Texture3D.MipSlice = 0;
    RTVDesc.Texture3D.FirstWSlice = 0;
    RTVDesc.Texture3D.WSize = g_VolumeDepth;

    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pVolumeTexture, &RTVDesc, &g_pVolumeTexTotalRTV ) );
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pVelocityTexture, &RTVDesc, &g_pVelocityTexTotalRTV ) );

    return hr;
}

HRESULT CreatePositionTextures( ID3D10Device* pd3dDevice, MESH* pMesh )
{
    HRESULT hr = S_OK;

    // Create the texture
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = g_PositionWidth;
    dstex.Height = g_PositionHeight;
    dstex.MipLevels = 1;
    dstex.ArraySize = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &pMesh->pMeshPositionTexture ) );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;
    V_RETURN( pd3dDevice->CreateShaderResourceView( pMesh->pMeshPositionTexture, &SRVDesc, &pMesh->pMeshPositionRV ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    RTVDesc.Format = dstex.Format;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( pMesh->pMeshPositionTexture, &RTVDesc, &pMesh->pMeshPositionRTV ) );

    return hr;
}

HRESULT CreateMeshTexture( ID3D10Device* pd3dDevice, UINT width, UINT height, MESH* pMesh )
{
    HRESULT hr = S_OK;
    D3D10_TEXTURE2D_DESC dstex;

    pMesh->MeshTextureWidth = width;
    pMesh->MeshTextureHeight = height;

    // Create the texture
    dstex.Width = width;
    dstex.Height = height;
    dstex.ArraySize = 1;
    dstex.MipLevels = 1;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;

    dstex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, NULL, &pMesh->pMeshTexture ) );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;
    V_RETURN( pd3dDevice->CreateShaderResourceView( pMesh->pMeshTexture, &SRVDesc, &pMesh->pMeshRV ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    RTVDesc.Format = dstex.Format;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( pMesh->pMeshTexture, &RTVDesc, &pMesh->pMeshRTV ) );

    float ClearColor[4] = {1,1,1,0};
    pd3dDevice->ClearRenderTargetView( pMesh->pMeshRTV, ClearColor );

    return hr;
}

HRESULT CreateQuadVB( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    QUAD_VERTEX Verts[4];

    Verts[0].Pos = D3DXVECTOR3( -1, -1, 0 );
    Verts[0].Tex = D3DXVECTOR2( 0, 0 );
    Verts[1].Pos = D3DXVECTOR3( -1, 1, 0 );
    Verts[1].Tex = D3DXVECTOR2( 0, 1 );
    Verts[2].Pos = D3DXVECTOR3( 1, -1, 0 );
    Verts[2].Tex = D3DXVECTOR2( 1, 0 );
    Verts[3].Pos = D3DXVECTOR3( 1, 1, 0 );
    Verts[3].Tex = D3DXVECTOR2( 1, 1 );

    D3D10_BUFFER_DESC vbdesc =
    {
        4 * sizeof( QUAD_VERTEX ),
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = Verts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pQuadVB ) );

    return hr;
}

void HandleKeys( float fElapsedTime )
{	
    float fMoveAmt = 1.0f * fElapsedTime;
    float fRotateAmt = D3DX_PI * fElapsedTime;
    if( GetAsyncKeyState( VK_SHIFT ) & 0x8000 )
    {
        if( GetAsyncKeyState( 'W' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.x += fRotateAmt;
        }
        if( GetAsyncKeyState( 'S' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.x -= fRotateAmt;
        }

        if( GetAsyncKeyState( 'D' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.y += fRotateAmt;
        }
        if( GetAsyncKeyState( 'A' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.y -= fRotateAmt;
        }

        if( GetAsyncKeyState( 'E' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.z += fRotateAmt;
        }
        if( GetAsyncKeyState( 'Q' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vRotation.z -= fRotateAmt;
        }
    }
    else
    {
        if( GetAsyncKeyState( 'W' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.z += fMoveAmt;
        }
        if( GetAsyncKeyState( 'S' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.z -= fMoveAmt;
        }

        if( GetAsyncKeyState( 'D' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.x += fMoveAmt;
        }
        if( GetAsyncKeyState( 'A' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.x -= fMoveAmt;
        }

        if( GetAsyncKeyState( 'E' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.y += fMoveAmt;
        }
        if( GetAsyncKeyState( 'Q' ) & 0x8000 )
        {
            g_WorldMesh[g_iControlMesh].vPosition.y -= fMoveAmt;
        }
    }
}
