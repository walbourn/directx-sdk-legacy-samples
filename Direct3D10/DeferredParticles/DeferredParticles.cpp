//--------------------------------------------------------------------------------------
// File: DeferredParticles.cpp
//
// This sample shows a method of rendering deferred particles systems.  This is a variation
// of the method of deferred particle shading shown at unity3d.com/blogs/nf/files/page0_blog_entry73_1.pdf
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
#include "ParticleSystem.h"
#include "BreakableWall.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDirectionWidget                g_LightControl;
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D   
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

// Direct3D10 resources
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTSDKMesh                        g_WallMesh;
CDXUTSDKMesh g_ChunkMesh[NUM_CHUNKS];

#define MAX_BUILDINGS 5
CBuilding g_Building[MAX_BUILDINGS];
CGrowableArray <D3DXMATRIX>         g_BaseMeshMatrices;
CGrowableArray <D3DXMATRIX> g_ChunkMeshMatrices[NUM_CHUNKS];

ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10InputLayout*                  g_pScreenQuadLayout = NULL;
ID3D10InputLayout*                  g_pMeshLayout = NULL;

UINT                                g_NumParticles = 200;
float                               g_fSpread = 4.0f;
float                               g_fStartSize = 0.0f;
float                               g_fEndSize = 10.0f;
float                               g_fSizeExponent = 128.0f;

float                               g_fMushroomCloudLifeSpan = 10.0f;
float                               g_fGroundBurstLifeSpan = 9.0f;
float                               g_fPopperLifeSpan = 9.0f;


float                               g_fMushroomStartSpeed = 20.0f;
float                               g_fStalkStartSpeed = 50.0f;
float                               g_fGroundBurstStartSpeed = 100.0f;
float                               g_fLandMineStartSpeed = 250.0f;

float                               g_fEndSpeed = 4.0f;
float                               g_fSpeedExponent = 32.0f;
float                               g_fFadeExponent = 4.0f;
float                               g_fRollAmount = 0.2f;
float                               g_fWindFalloff = 20.0f;
D3DXVECTOR3                         g_vPosMul( 1,1,1 );
D3DXVECTOR3                         g_vDirMul( 1,1,1 );
D3DXVECTOR3                         g_vWindVel( -2.0f,10.0f,0 );
D3DXVECTOR3                         g_vGravity( 0,-9.8f,0.0f );

float                               g_fGroundPlane = 0.5f;
float                               g_fLightRaise = 1.0f;

float                               g_fWorldBounds = 100.0f;

#define MAX_FLASH_COLORS 4
D3DXVECTOR4 g_vFlashColor[MAX_FLASH_COLORS] =
{
    D3DXVECTOR4( 1.0f, 0.5f, 0.00f, 0.9f ),
    D3DXVECTOR4( 1.0f, 0.3f, 0.05f, 0.9f ),
    D3DXVECTOR4( 1.0f, 0.4f, 0.00f, 0.9f ),
    D3DXVECTOR4( 0.8f, 0.3f, 0.05f, 0.9f )
};

D3DXVECTOR4                         g_vFlashAttenuation( 0,0.0f,3.0f,0 );
D3DXVECTOR4                         g_vMeshLightAttenuation( 0,0,1.5f,0 );
float                               g_fFlashLife = 0.50f;
float                               g_fFlashIntensity = 1000.0f;

UINT                                g_NumParticlesToDraw = 0;
/*#define MAX_MUSHROOM_CLOUDS 16
   #define MAX_GROUND_BURSTS 46
   #define MAX_PARTICLE_SYSTEMS 60
   #define MAX_FLASH_LIGHTS 8
   #define MAX_INSTANCES 200*/
#define MAX_MUSHROOM_CLOUDS 8
#define MAX_GROUND_BURSTS 23
#define MAX_PARTICLE_SYSTEMS 30
#define MAX_FLASH_LIGHTS 8
#define MAX_INSTANCES 200

CParticleSystem**                   g_ppParticleSystem = NULL;
ID3D10Buffer*                       g_pParticleBuffer = NULL;
ID3D10Buffer*                       g_pScreenQuadVB = NULL;

ID3D10Texture2D*                    g_pOffscreenParticleTex = NULL;
ID3D10ShaderResourceView*           g_pOffscreenParticleSRV = NULL;
ID3D10RenderTargetView*             g_pOffscreenParticleRTV = NULL;
ID3D10Texture2D*                    g_pOffscreenParticleColorTex = NULL;
ID3D10ShaderResourceView*           g_pOffscreenParticleColorSRV = NULL;
ID3D10RenderTargetView*             g_pOffscreenParticleColorRTV = NULL;

ID3D10ShaderResourceView*           g_pParticleTextureSRV = NULL;

ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10EffectTechnique*              g_pRenderParticlesToBuffer = NULL;
ID3D10EffectTechnique*              g_pRenderParticles = NULL;
ID3D10EffectTechnique*              g_pCompositeParticlesToScene = NULL;
ID3D10EffectTechnique*              g_pRenderMesh = NULL;
ID3D10EffectTechnique*              g_pRenderMeshInst = NULL;

ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxParticleColor = NULL;
ID3D10EffectVectorVariable*         g_pLightDir = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmInvViewProj = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectVectorVariable*         g_pvEyePt = NULL;
ID3D10EffectVectorVariable*         g_pvRight = NULL;
ID3D10EffectVectorVariable*         g_pvUp = NULL;
ID3D10EffectVectorVariable*         g_pvForward = NULL;

ID3D10EffectScalarVariable*         g_pNumGlowLights = NULL;
ID3D10EffectVectorVariable*         g_pvGlowLightPosIntensity = NULL;
ID3D10EffectVectorVariable*         g_pvGlowLightColor = NULL;
ID3D10EffectVectorVariable*         g_pvGlowLightAttenuation = NULL;
ID3D10EffectVectorVariable*         g_pvMeshLightAttenuation = NULL;

ID3D10EffectMatrixVariable*         g_pmViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldInst = NULL;

bool                                g_bRenderDeferred = true;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_RESET				50
#define IDC_DEFERRED			51
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
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

void ResetBuildings();


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
    DXUTCreateWindow( L"DeferredParticles" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    D3DXVECTOR3 vDir( 1,1,0 );
    D3DXVec3Normalize( &vDir, &vDir );
    g_LightControl.SetLightDirection( vDir );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    iY += 24;
    g_SampleUI.AddButton( IDC_RESET, L"Reset Buildings (R)", 35, iY += 24, 125, 22, 'R' );

    g_SampleUI.AddCheckBox( IDC_DEFERRED, L"Render Deferred (D)", 35, iY += 24, 125, 22, g_bRenderDeferred, 'D' );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 150.0f, 336.0f );
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
    // Disable multisampling (not implemented for this sample)
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled( false );
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled( false );

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

void NewExplosion( D3DXVECTOR3 vCenter, float fSize )
{
    D3DXVECTOR3 vDirMul( 0.2f,1.0f,0.2f );
    float fMinPower = 5.0f;
    float fMaxPower = 30.0f;
    for( UINT i = 0; i < MAX_BUILDINGS; i++ )
    {
        g_Building[i].CreateExplosion( vCenter, vDirMul, fSize, fMinPower, fMaxPower );
    }
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
    if (fElapsedTime  > 0.1f ) fElapsedTime = 0.1f;

    D3DXVECTOR3 vEye;
    D3DXMATRIX mView;
    vEye = *g_Camera.GetEyePt();
    mView = *g_Camera.GetViewMatrix();
    D3DXVECTOR3 vRight( mView._11, mView._21, mView._31 );
    D3DXVECTOR3 vUp( mView._12, mView._22, mView._32 );
    D3DXVECTOR3 vFoward( mView._13, mView._23, mView._33 );

    D3DXVec3Normalize( &vRight, &vRight );
    D3DXVec3Normalize( &vUp, &vUp );
    D3DXVec3Normalize( &vFoward, &vFoward );

    g_pvRight->SetFloatVector( ( float* )&vRight );
    g_pvUp->SetFloatVector( ( float* )&vUp );
    g_pvForward->SetFloatVector( ( float* )&vFoward );

    UINT NumActiveSystems = 0;
    D3DXVECTOR4 vGlowLightPosIntensity[MAX_PARTICLE_SYSTEMS];
    D3DXVECTOR4 vGlowLightColor[MAX_PARTICLE_SYSTEMS];

    // Advanced building pieces
    for( UINT i = 0; i < MAX_BUILDINGS; i++ )
    {
        g_Building[i].AdvancePieces( fElapsedTime, g_vGravity );
    }

    // Advance the system
    for( UINT i = 0; i < MAX_PARTICLE_SYSTEMS; i++ )
    {
        g_ppParticleSystem[i]->AdvanceSystem( ( float )fTime, fElapsedTime, vRight, vUp, g_vWindVel, g_vGravity );
    }

    PARTICLE_VERTEX* pVerts = NULL;
    g_pParticleBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, ( void** )&pVerts );

    CopyParticlesToVertexBuffer( pVerts, vEye, vRight, vUp );

    g_pParticleBuffer->Unmap();

    for( UINT i = 0; i < MAX_MUSHROOM_CLOUDS; i += 2 )
    {
        float fCurrentTime = g_ppParticleSystem[i]->GetCurrentTime();
        float fLifeSpan = g_ppParticleSystem[i]->GetLifeSpan();
        if( fCurrentTime > fLifeSpan )
        {
            D3DXVECTOR3 vCenter;
            vCenter.x = RPercent() * g_fWorldBounds;
            vCenter.y = g_fGroundPlane;
            vCenter.z = RPercent() * g_fWorldBounds;
            float fStartTime = -fabs( RPercent() ) * 4.0f;
            D3DXVECTOR4 vFlashColor = g_vFlashColor[ rand() % MAX_FLASH_COLORS ];

            g_ppParticleSystem[i]->SetCenter( vCenter );
            g_ppParticleSystem[i]->SetStartTime( fStartTime );
            g_ppParticleSystem[i]->SetFlashColor( vFlashColor );
            g_ppParticleSystem[i]->Init();

            g_ppParticleSystem[i + 1]->SetCenter( vCenter );
            g_ppParticleSystem[i + 1]->SetStartTime( fStartTime );
            g_ppParticleSystem[i + 1]->SetFlashColor( vFlashColor );
            g_ppParticleSystem[i + 1]->Init();
        }
        else if( fCurrentTime > 0.0f && fCurrentTime < g_fFlashLife && NumActiveSystems < MAX_FLASH_LIGHTS )
        {
            D3DXVECTOR3 vCenter = g_ppParticleSystem[i]->GetCenter();
            D3DXVECTOR4 vFlashColor = g_ppParticleSystem[i]->GetFlashColor();

            float fIntensity = g_fFlashIntensity * ( ( g_fFlashLife - fCurrentTime ) / g_fFlashLife );
            vGlowLightPosIntensity[NumActiveSystems] = D3DXVECTOR4( vCenter.x, vCenter.y + g_fLightRaise, vCenter.z,
                                                                    fIntensity );
            vGlowLightColor[NumActiveSystems] = vFlashColor;
            NumActiveSystems ++;
        }
    }

    // Ground bursts
    for( UINT i = MAX_MUSHROOM_CLOUDS; i < MAX_GROUND_BURSTS; i++ )
    {
        float fCurrentTime = g_ppParticleSystem[i]->GetCurrentTime();
        float fLifeSpan = g_ppParticleSystem[i]->GetLifeSpan();
        if( fCurrentTime > fLifeSpan )
        {
            D3DXVECTOR3 vCenter;
            vCenter.x = RPercent() * g_fWorldBounds;
            vCenter.y = g_fGroundPlane;
            vCenter.z = RPercent() * g_fWorldBounds;
            float fStartTime = -fabs( RPercent() ) * 4.0f;
            D3DXVECTOR4 vFlashColor = g_vFlashColor[ rand() % MAX_FLASH_COLORS ];

            float fStartSpeed = g_fGroundBurstStartSpeed + RPercent() * 30.0f;
            g_ppParticleSystem[i]->SetCenter( vCenter );
            g_ppParticleSystem[i]->SetStartTime( fStartTime );
            g_ppParticleSystem[i]->SetStartSpeed( fStartSpeed );
            g_ppParticleSystem[i]->SetFlashColor( vFlashColor );
            g_ppParticleSystem[i]->Init();
        }
        else if( fCurrentTime > 0.0f && fCurrentTime < g_fFlashLife && NumActiveSystems < MAX_FLASH_LIGHTS )
        {
            D3DXVECTOR3 vCenter = g_ppParticleSystem[i]->GetCenter();
            D3DXVECTOR4 vFlashColor = g_ppParticleSystem[i]->GetFlashColor();

            float fIntensity = g_fFlashIntensity * ( ( g_fFlashLife - fCurrentTime ) / g_fFlashLife );
            vGlowLightPosIntensity[NumActiveSystems] = D3DXVECTOR4( vCenter.x, vCenter.y + g_fLightRaise, vCenter.z,
                                                                    fIntensity );
            vGlowLightColor[NumActiveSystems] = vFlashColor;
            NumActiveSystems ++;
        }
    }

    // Land mines
    for( UINT i = MAX_GROUND_BURSTS; i < MAX_PARTICLE_SYSTEMS; i++ )
    {
        float fCurrentTime = g_ppParticleSystem[i]->GetCurrentTime();
        float fLifeSpan = g_ppParticleSystem[i]->GetLifeSpan();
        if( fCurrentTime > fLifeSpan )
        {
            D3DXVECTOR3 vCenter;
            vCenter.x = RPercent() * g_fWorldBounds;
            vCenter.y = g_fGroundPlane;
            vCenter.z = RPercent() * g_fWorldBounds;
            float fStartTime = -fabs( RPercent() ) * 4.0f;
            D3DXVECTOR4 vFlashColor = g_vFlashColor[ rand() % MAX_FLASH_COLORS ];

            float fStartSpeed = g_fLandMineStartSpeed + RPercent() * 100.0f;
            g_ppParticleSystem[i]->SetCenter( vCenter );
            g_ppParticleSystem[i]->SetStartTime( fStartTime );
            g_ppParticleSystem[i]->SetStartSpeed( fStartSpeed );
            g_ppParticleSystem[i]->SetFlashColor( vFlashColor );
            g_ppParticleSystem[i]->Init();
        }
        else if( fCurrentTime > 0.0f && fCurrentTime < g_fFlashLife && NumActiveSystems < MAX_FLASH_LIGHTS )
        {
            D3DXVECTOR3 vCenter = g_ppParticleSystem[i]->GetCenter();
            D3DXVECTOR4 vFlashColor = g_ppParticleSystem[i]->GetFlashColor();

            float fIntensity = g_fFlashIntensity * ( ( g_fFlashLife - fCurrentTime ) / g_fFlashLife );
            vGlowLightPosIntensity[NumActiveSystems] = D3DXVECTOR4( vCenter.x, vCenter.y + g_fLightRaise, vCenter.z,
                                                                    fIntensity );
            vGlowLightColor[NumActiveSystems] = vFlashColor;
            NumActiveSystems ++;
        }
    }

    // Setup light variables
    g_pNumGlowLights->SetInt( NumActiveSystems );
    g_pvGlowLightPosIntensity->SetFloatVectorArray( ( float* )&vGlowLightPosIntensity, 0, NumActiveSystems );
    g_pvGlowLightColor->SetFloatVectorArray( ( float* )&vGlowLightColor, 0, NumActiveSystems );
    g_pvGlowLightAttenuation->SetFloatVector( ( float* )&g_vFlashAttenuation );
    g_pvMeshLightAttenuation->SetFloatVector( ( float* )&g_vMeshLightAttenuation );
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
        case IDC_RESET:
            ResetBuildings();
            break;
        case IDC_DEFERRED:
            g_bRenderDeferred = !g_bRenderDeferred;
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


void ResetBuildings()
{
    float f2Third = 0.6666f;
    D3DXVECTOR3 vChunkOffsets[NUM_CHUNKS] =
    {
        D3DXVECTOR3( f2Third, -f2Third, 0.0f ),
        D3DXVECTOR3( -f2Third, f2Third, 0.0f ),
        D3DXVECTOR3( f2Third, f2Third, 0.0f ),
        D3DXVECTOR3( -f2Third, -f2Third, 0.0f ),
        D3DXVECTOR3( f2Third, 0, 0.0f ),
        D3DXVECTOR3( 0, f2Third, 0.0f ),
        D3DXVECTOR3( -f2Third, 0, 0.0f ),
        D3DXVECTOR3( 0, -f2Third, 0.0f ),
        D3DXVECTOR3( 0, 0, 0.0f )
    };

    for( UINT i = 0; i < MAX_BUILDINGS; i++ )
    {
        g_Building[i].SetBaseMesh( &g_WallMesh );
        for( UINT c = 0; c < NUM_CHUNKS; c++ )
        {
            g_Building[i].SetChunkMesh( c, &g_ChunkMesh[c], vChunkOffsets[c] );
        }
    }
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

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );

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
    char strMaxGlowLights[MAX_PATH];
    char strMaxInstances[MAX_PATH];
    sprintf_s( strMaxGlowLights, MAX_PATH, "%d", MAX_FLASH_LIGHTS );
    sprintf_s( strMaxInstances, MAX_PATH, "%d", MAX_INSTANCES );
    D3D10_SHADER_MACRO macros[3] =
    {
        { "MAX_GLOWLIGHTS", strMaxGlowLights },
        { "MAX_INSTANCES", strMaxInstances },
        { NULL, NULL }
    };

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DeferredParticles.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, macros, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain technique objects
    g_pRenderParticlesToBuffer = g_pEffect10->GetTechniqueByName( "RenderParticlesToBuffer" );
    g_pRenderParticles = g_pEffect10->GetTechniqueByName( "RenderParticles" );
    g_pCompositeParticlesToScene = g_pEffect10->GetTechniqueByName( "CompositeParticlesToScene" );
    g_pRenderMesh = g_pEffect10->GetTechniqueByName( "RenderMesh" );
    g_pRenderMeshInst = g_pEffect10->GetTechniqueByName( "RenderMeshInst" );

    // Obtain variables
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txMeshTexture" )->AsShaderResource();
    g_ptxParticleColor = g_pEffect10->GetVariableByName( "g_txParticleColor" )->AsShaderResource();
    g_pLightDir = g_pEffect10->GetVariableByName( "g_LightDir" )->AsVector();
    g_pmWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmInvViewProj = g_pEffect10->GetVariableByName( "g_mInvViewProj" )->AsMatrix();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_pvRight = g_pEffect10->GetVariableByName( "g_vRight" )->AsVector();
    g_pvUp = g_pEffect10->GetVariableByName( "g_vUp" )->AsVector();
    g_pvForward = g_pEffect10->GetVariableByName( "g_vForward" )->AsVector();

    g_pNumGlowLights = g_pEffect10->GetVariableByName( "g_NumGlowLights" )->AsScalar();
    g_pvGlowLightPosIntensity = g_pEffect10->GetVariableByName( "g_vGlowLightPosIntensity" )->AsVector();
    g_pvGlowLightColor = g_pEffect10->GetVariableByName( "g_vGlowLightColor" )->AsVector();
    g_pvGlowLightAttenuation = g_pEffect10->GetVariableByName( "g_vGlowLightAttenuation" )->AsVector();
    g_pvMeshLightAttenuation = g_pEffect10->GetVariableByName( "g_vMeshLightAttenuation" )->AsVector();

    g_pmWorldInst = g_pEffect10->GetVariableByName( "g_mWorldInst" )->AsMatrix();
    g_pmViewProj = g_pEffect10->GetVariableByName( "g_mViewProj" )->AsMatrix();

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "LIFE",      0, DXGI_FORMAT_R32_FLOAT,       0, 20, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "THETA",     0, DXGI_FORMAT_R32_FLOAT,       0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",     0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 28, D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };

    D3D10_PASS_DESC PassDesc;
    V_RETURN( g_pRenderParticlesToBuffer->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 5, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    const D3D10_INPUT_ELEMENT_DESC screenlayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( g_pCompositeParticlesToScene->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( screenlayout, 1, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pScreenQuadLayout ) );

    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32_FLOAT,       0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };
    V_RETURN( g_pRenderMesh->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshLayout ) );

    // Load the meshes
    V_RETURN( g_WallMesh.Create( pd3dDevice, L"DeferredParticles\\wallsegment.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[0].Create( pd3dDevice, L"DeferredParticles\\wallchunk0.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[1].Create( pd3dDevice, L"DeferredParticles\\wallchunk1.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[2].Create( pd3dDevice, L"DeferredParticles\\wallchunk2.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[3].Create( pd3dDevice, L"DeferredParticles\\wallchunk3.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[4].Create( pd3dDevice, L"DeferredParticles\\wallchunk4.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[5].Create( pd3dDevice, L"DeferredParticles\\wallchunk5.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[6].Create( pd3dDevice, L"DeferredParticles\\wallchunk6.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[7].Create( pd3dDevice, L"DeferredParticles\\wallchunk7.sdkmesh" ) );
    V_RETURN( g_ChunkMesh[8].Create( pd3dDevice, L"DeferredParticles\\wallchunk8.sdkmesh" ) );

    // Buildings
    g_Building[0].CreateBuilding( D3DXVECTOR3( 0, 0, 0 ), 2.0f, 50, 0, 50 );

    float fBuildingRange = g_fWorldBounds - 20.0f;

    for( UINT i = 1; i < MAX_BUILDINGS; i++ )
    {
        D3DXVECTOR3 vCenter;
        vCenter.x = RPercent() * fBuildingRange;
        vCenter.y = 0;
        vCenter.z = RPercent() * fBuildingRange;

        UINT x = ( rand() % 2 ) + 2;
        UINT y = ( rand() % 2 ) + 3;
        UINT z = ( rand() % 2 ) + 2;
        g_Building[i].CreateBuilding( vCenter, 2.0f, x * 2, y * 2, z * 2 );
    }

    ResetBuildings();

    // Particle system
    UINT NumStalkParticles = 500;
    UINT NumGroundExpParticles = 345;
    UINT NumLandMineParticles = 125;
    UINT MaxParticles = MAX_MUSHROOM_CLOUDS * ( g_NumParticles + NumStalkParticles ) +
        ( MAX_GROUND_BURSTS - MAX_MUSHROOM_CLOUDS ) * NumGroundExpParticles +
        ( MAX_PARTICLE_SYSTEMS - MAX_GROUND_BURSTS ) * NumLandMineParticles;
    V_RETURN( CreateParticleArray( MaxParticles ) );

    D3DXVECTOR4 vColor0( 1.0f,1.0f,1.0f,1 );
    D3DXVECTOR4 vColor1( 0.6f,0.6f,0.6f,1 );

    srand( timeGetTime() );
    g_ppParticleSystem = new CParticleSystem*[MAX_PARTICLE_SYSTEMS];
    g_NumParticlesToDraw = 0;
    for( UINT i = 0; i < MAX_MUSHROOM_CLOUDS; i += 2 )
    {
        D3DXVECTOR3 vLocation;
        vLocation.x = RPercent() * 50.0f;
        vLocation.y = g_fGroundPlane;
        vLocation.z = RPercent() * 50.0f;

        g_ppParticleSystem[i] = new CMushroomParticleSystem();
        g_ppParticleSystem[i]->CreateParticleSystem( g_NumParticles );
        g_ppParticleSystem[i]->SetSystemAttributes( vLocation,
                                                    g_fSpread, g_fMushroomCloudLifeSpan, g_fFadeExponent,
                                                    g_fStartSize, g_fEndSize, g_fSizeExponent,
                                                    g_fMushroomStartSpeed, g_fEndSpeed, g_fSpeedExponent,
                                                    g_fRollAmount, g_fWindFalloff,
                                                    1, 0, D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR3( 0, 0, 0 ),
                                                    vColor0, vColor1,
                                                    g_vPosMul, g_vDirMul );

        g_NumParticlesToDraw += g_NumParticles;

        g_ppParticleSystem[i + 1] = new CStalkParticleSystem();
        g_ppParticleSystem[i + 1]->CreateParticleSystem( NumStalkParticles );
        g_ppParticleSystem[i + 1]->SetSystemAttributes( vLocation,
                                                        15.0f, g_fMushroomCloudLifeSpan, g_fFadeExponent * 2.0f,
                                                        g_fStartSize * 0.5f, g_fEndSize * 0.5f, g_fSizeExponent,
                                                        g_fStalkStartSpeed, -1.0f, g_fSpeedExponent,
                                                        g_fRollAmount, g_fWindFalloff,
                                                        1, 0, D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR3( 0, 0, 0 ),
                                                        vColor0, vColor1,
                                                        D3DXVECTOR3( 1, 0.1f, 1 ), D3DXVECTOR3( 1, 0.1f, 1 ) );

        g_NumParticlesToDraw += NumStalkParticles;
    }

    for( UINT i = MAX_MUSHROOM_CLOUDS; i < MAX_GROUND_BURSTS; i++ )
    {
        D3DXVECTOR3 vLocation;
        vLocation.x = RPercent() * 50.0f;
        vLocation.y = g_fGroundPlane;
        vLocation.z = RPercent() * 50.0f;

        g_ppParticleSystem[i] = new CGroundBurstParticleSystem();
        g_ppParticleSystem[i]->CreateParticleSystem( NumGroundExpParticles );
        g_ppParticleSystem[i]->SetSystemAttributes( vLocation,
                                                    1.0f, g_fGroundBurstLifeSpan, g_fFadeExponent,
                                                    0.5f, 8.0f, 1.0f,
                                                    g_fGroundBurstStartSpeed, g_fEndSpeed, 4.0f,
                                                    g_fRollAmount, 1.0f,
                                                    30, 100.0f, D3DXVECTOR3( 0, 0.5f, 0 ), D3DXVECTOR3( 1.0f, 0.5f,
                                                                                                        1.0f ),
                                                    vColor0, vColor1,
                                                    g_vPosMul, g_vDirMul );

        g_NumParticlesToDraw += NumGroundExpParticles;
    }

    for( UINT i = MAX_GROUND_BURSTS; i < MAX_PARTICLE_SYSTEMS; i++ )
    {
        D3DXVECTOR3 vLocation;
        vLocation.x = RPercent() * 50.0f;
        vLocation.y = g_fGroundPlane;
        vLocation.z = RPercent() * 50.0f;

        g_ppParticleSystem[i] = new CLandMineParticleSystem();
        g_ppParticleSystem[i]->CreateParticleSystem( NumLandMineParticles );
        g_ppParticleSystem[i]->SetSystemAttributes( vLocation,
                                                    1.5f, g_fPopperLifeSpan, g_fFadeExponent,
                                                    1.0f, 6.0f, 1.0f,
                                                    g_fLandMineStartSpeed, g_fEndSpeed, 2.0f,
                                                    g_fRollAmount, 4.0f,
                                                    0, 70.0f, D3DXVECTOR3( 0, 0.8f, 0 ), D3DXVECTOR3( 0.3f, 0.2f,
                                                                                                      0.3f ),
                                                    vColor0, vColor1,
                                                    g_vPosMul, g_vDirMul );

        g_NumParticlesToDraw += NumGroundExpParticles;
    }

    D3D10_BUFFER_DESC BDesc;
    BDesc.ByteWidth = sizeof( PARTICLE_VERTEX ) * 6 * g_NumParticlesToDraw;
    BDesc.Usage = D3D10_USAGE_DYNAMIC;
    BDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    BDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &BDesc, NULL, &g_pParticleBuffer ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DeferredParticles\\DeferredParticle.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pParticleTextureSRV, NULL ) );
	
    // Create the screen quad
    BDesc.ByteWidth = 4 * sizeof( D3DXVECTOR3 );
    BDesc.Usage = D3D10_USAGE_IMMUTABLE;
    BDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BDesc.CPUAccessFlags = 0;
    BDesc.MiscFlags = 0;

    D3DXVECTOR3 verts[4] =
    {
        D3DXVECTOR3( -1, -1, 0.5f ),
        D3DXVECTOR3( -1, 1, 0.5f ),
        D3DXVECTOR3( 1, -1, 0.5f ),
        D3DXVECTOR3( 1, 1, 0.5f )
    };
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = verts;
    V_RETURN( pd3dDevice->CreateBuffer( &BDesc, &InitData, &g_pScreenQuadVB ) );

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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 2.0f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    // Create the offscreen particle buffer
    D3D10_TEXTURE2D_DESC Desc;
    Desc.Width = pBackBufferSurfaceDesc->Width;
    Desc.Height = pBackBufferSurfaceDesc->Height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Usage = D3D10_USAGE_DEFAULT;
    Desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pOffscreenParticleTex ) );
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &g_pOffscreenParticleColorTex ) );

    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    RTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;

    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pOffscreenParticleTex, &RTVDesc, &g_pOffscreenParticleRTV ) );
    RTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pOffscreenParticleColorTex, &RTVDesc,
                                                  &g_pOffscreenParticleColorRTV ) );

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = Desc.MipLevels;

    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pOffscreenParticleTex, &SRVDesc, &g_pOffscreenParticleSRV ) );
    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pOffscreenParticleColorTex, &SRVDesc,
                                                    &g_pOffscreenParticleColorSRV ) );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render particles
//--------------------------------------------------------------------------------------
void RenderParticles( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pRenderTechnique )
{
    //IA setup
    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D10Buffer* pVB[1];
    pVB[0] = g_pParticleBuffer;
    Strides[0] = sizeof( PARTICLE_VERTEX );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( NULL, DXGI_FORMAT_R16_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    g_ptxDiffuse->SetResource( g_pParticleTextureSRV );

    //Render
    D3D10_TECHNIQUE_DESC techDesc;
    pRenderTechnique->GetDesc( &techDesc );

    g_NumParticlesToDraw = GetNumActiveParticles();
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pRenderTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 6 * g_NumParticlesToDraw, 0 );
    }
}

//--------------------------------------------------------------------------------------
// Render particles into the offscreen buffer
//--------------------------------------------------------------------------------------
void RenderParticlesIntoBuffer( ID3D10Device* pd3dDevice )
{
    // Clear the new render target
    float color[4] =
    {
        0, 0, 0, 0
    };
    pd3dDevice->ClearRenderTargetView( g_pOffscreenParticleRTV, color );
    pd3dDevice->ClearRenderTargetView( g_pOffscreenParticleColorRTV, color );
	
    // get the old render targets
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // Set the new render targets
    ID3D10RenderTargetView* pViews[2];
    pViews[0] = g_pOffscreenParticleRTV;
    pViews[1] = g_pOffscreenParticleColorRTV;
    pd3dDevice->OMSetRenderTargets( 2, pViews, pOldDSV );

    // Render the particles
    RenderParticles( pd3dDevice, g_pRenderParticlesToBuffer );

    // restore the original render targets
    pViews[0] = pOldRTV;
    pViews[1] = NULL;
    pd3dDevice->OMSetRenderTargets( 2, pViews, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );
}

//--------------------------------------------------------------------------------------
// Composite offscreen particle buffer back into the scene
//--------------------------------------------------------------------------------------
void CompositeParticlesIntoScene( ID3D10Device* pd3dDevice )
{
    // Render the particles
    ID3D10EffectTechnique* pRenderTechnique = g_pCompositeParticlesToScene;

    //IA setup
    pd3dDevice->IASetInputLayout( g_pScreenQuadLayout );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D10Buffer* pVB[1];
    pVB[0] = g_pScreenQuadVB;
    Strides[0] = sizeof( D3DXVECTOR3 );
    Offsets[0] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( NULL, DXGI_FORMAT_R16_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    g_ptxDiffuse->SetResource( g_pOffscreenParticleSRV );
    g_ptxParticleColor->SetResource( g_pOffscreenParticleColorSRV );

    //Render
    D3D10_TECHNIQUE_DESC techDesc;
    pRenderTechnique->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pRenderTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }

    // Un-set this resource, as it's associated with an OM output
    g_ptxParticleColor->SetResource( NULL );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pRenderTechnique->GetPassByIndex( p )->Apply( 0 );
    }
}

void RenderInstanced( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique, CDXUTSDKMesh* pMesh,
                      UINT NumInstances )
{
    ID3D10Buffer* pVB[1];
    UINT Strides[1];
    UINT Offsets[1] =
    {
        0
    };

    pVB[0] = pMesh->GetVB10( 0, 0 );
    Strides[0] = ( UINT )pMesh->GetVertexStride( 0, 0 );

    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( pMesh->GetIB10( 0 ), pMesh->GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < pMesh->GetNumSubsets( 0 ); ++subset )
        {
            pSubset = pMesh->GetSubset( 0, subset );

            PrimType = pMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, NumInstances,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }
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
    float ClearColor[4] =
    {
        0.0f, 0.0f, 0.0f, 1.0f
    };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    D3DXVECTOR3 vEyePt;
    D3DXMATRIX mWorldViewProjection;
    D3DXVECTOR4 vLightDir;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mViewProj;
    D3DXMATRIX mInvViewProj;

    // Get the projection & view matrix from the camera class
    D3DXMatrixIdentity( &mWorld );
    vEyePt = *g_Camera.GetEyePt();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    mWorldViewProjection = mView * mProj;
    mViewProj = mView * mProj;
    D3DXMatrixInverse( &mInvViewProj, NULL, &mViewProj );
    D3DXMATRIX mSceneWorld;
    D3DXMatrixScaling( &mSceneWorld, 20, 20, 20 );
    D3DXMATRIX mSceneWVP = mSceneWorld * mViewProj;
    vLightDir = D3DXVECTOR4( g_LightControl.GetLightDirection(), 1 );

    // Per frame variables
    V( g_pmWorldViewProjection->SetMatrix( ( float* )&mSceneWVP ) );
    V( g_pmWorld->SetMatrix( ( float* )&mSceneWorld ) );
    V( g_pLightDir->SetFloatVector( ( float* )vLightDir ) );
    V( g_pmInvViewProj->SetMatrix( ( float* )&mInvViewProj ) );
    V( g_pfTime->SetFloat( ( float )fTime ) );
    V( g_pvEyePt->SetFloatVector( ( float* )&vEyePt ) );
    V( g_pmViewProj->SetMatrix( ( float* )&mViewProj ) );

    // Gather up the instance matrices for the buildings and pieces
    g_BaseMeshMatrices.Reset();
    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        g_ChunkMeshMatrices[i].Reset();
    }

    // Get matrices
    for( UINT i = 0; i < MAX_BUILDINGS; i++ )
    {
        g_Building[i].CollectBaseMeshMatrices( &g_BaseMeshMatrices );
        for( UINT c = 0; c < NUM_CHUNKS; c++ )
        {
            g_Building[i].CollectChunkMeshMatrices( c, &g_ChunkMeshMatrices[c] );
        }
    }

    // Set our input layout
    pd3dDevice->IASetInputLayout( g_pMeshLayout );

    // Intact walls
    D3DXMATRIX* pmData = g_BaseMeshMatrices.GetData();
    UINT NumMeshes = g_BaseMeshMatrices.GetSize();
    UINT numrendered = 0;
    while( numrendered < NumMeshes )
    {
        UINT NumToRender = min( MAX_INSTANCES, NumMeshes - numrendered );

        g_pmWorldInst->SetMatrixArray( ( float* )&pmData[numrendered], 0, NumToRender );

        RenderInstanced( pd3dDevice, g_pRenderMeshInst, &g_WallMesh, NumToRender );

        numrendered += NumToRender;
    }

    // Chunks
    for( UINT c = 0; c < NUM_CHUNKS; c++ )
    {
        pmData = g_ChunkMeshMatrices[c].GetData();
        NumMeshes = g_ChunkMeshMatrices[c].GetSize();
        numrendered = 0;
        while( numrendered < NumMeshes )
        {
            UINT NumToRender = min( MAX_INSTANCES, NumMeshes - numrendered );

            g_pmWorldInst->SetMatrixArray( ( float* )&pmData[numrendered], 0, NumToRender );

            RenderInstanced( pd3dDevice, g_pRenderMeshInst, &g_ChunkMesh[c], NumToRender );

            numrendered += NumToRender;
        }
    }

    // Render particles
    V( g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection ) );
    V( g_pmWorld->SetMatrix( ( float* )&mWorld ) );

    if( g_bRenderDeferred )
    {
        RenderParticlesIntoBuffer( pd3dDevice );
        CompositeParticlesIntoScene( pd3dDevice );
    }
    else
    {
        RenderParticles( pd3dDevice, g_pRenderParticles );
    }

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

    SAFE_RELEASE( g_pOffscreenParticleTex );
    SAFE_RELEASE( g_pOffscreenParticleSRV );
    SAFE_RELEASE( g_pOffscreenParticleRTV );
    SAFE_RELEASE( g_pOffscreenParticleColorTex );
    SAFE_RELEASE( g_pOffscreenParticleColorSRV );
    SAFE_RELEASE( g_pOffscreenParticleColorRTV );
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
    SAFE_RELEASE( g_pScreenQuadLayout );
    SAFE_RELEASE( g_pMeshLayout );
    g_WallMesh.Destroy();
    for( UINT i = 0; i < NUM_CHUNKS; i++ )
    {
        g_ChunkMesh[i].Destroy();
    }

    SAFE_RELEASE( g_pScreenQuadVB );
    for( UINT i = 0; i < MAX_PARTICLE_SYSTEMS; i++ )
    {
        SAFE_DELETE( g_ppParticleSystem[i] );
    }
    SAFE_DELETE_ARRAY( g_ppParticleSystem );
    SAFE_RELEASE( g_pParticleBuffer );
    SAFE_RELEASE( g_pParticleTextureSRV );

    DestroyParticleArray();
}



