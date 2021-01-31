//--------------------------------------------------------------------------------------
// File: SoftParticles10.1.cpp
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

#define MAX_PARTICLES 500
struct PARTICLE_VERTEX
{
    D3DXVECTOR3	 Pos;
    D3DXVECTOR3  Vel;
    float		 Life;
    float		 Size;
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera      g_Camera;               // A model viewing camera
CDXUTDialogResourceManager g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg         g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog             g_HUD;                  // manages the 3D UI
CDXUTDialog             g_SampleUI;             // dialog for sample specific controls

PARTICLE_VERTEX*		g_pCPUParticles = NULL;
DWORD*					g_pCPUParticleIndices = NULL;
float*					g_pParticleDepthArray = NULL;
CDXUTSDKMesh			g_ObjectMesh;
CDXUTSDKMesh			g_SkyMesh;

ID3DX10Font*            g_pFont10 = NULL;       
ID3DX10Sprite*          g_pSprite10 = NULL;
CDXUTTextHelper*        g_pTxtHelper = NULL;
ID3D10Effect*           g_pEffect10 = NULL;
ID3D10InputLayout*      g_pSceneVertexLayout = NULL;
ID3D10InputLayout*      g_pParticleVertexLayout = NULL;

ID3D10Texture2D*		  g_pDepthStencilTexture = NULL;
ID3D10DepthStencilView*	  g_pDepthStencilDSV = NULL;
ID3D10ShaderResourceView* g_pDepthStencilSRV = NULL;

ID3D10Buffer*           g_pParticleVB = NULL;
ID3D10Buffer*           g_pParticleIB = NULL;
ID3D10ShaderResourceView* g_pParticleTexRV = NULL;
ID3D10Texture3D*        g_pNoiseVolume = NULL;
ID3D10ShaderResourceView* g_pNoiseVolumeRV = NULL;
ID3D10Texture2D*        g_pColorGradTexture = NULL;
ID3D10ShaderResourceView* g_pColorGradTexRV = NULL;

ID3D10EffectTechnique*  g_pRenderScene = NULL;
ID3D10EffectTechnique*  g_pRenderSky = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesHard = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesODepth = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesSoft = NULL;
ID3D10EffectTechnique*  g_pRenderVolumeParticlesHard = NULL;
ID3D10EffectTechnique*  g_pRenderVolumeParticlesSoft = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesODepthSoft = NULL;
ID3D10EffectTechnique*  g_pRenderVolumeParticlesSoftMSAA = NULL;
ID3D10EffectTechnique*  g_pRenderVolumeParticlesHardMSAA = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesSoftMSAA = NULL;
ID3D10EffectTechnique*  g_pRenderBillboardParticlesODepthSoftMSAA = NULL;
ID3D10EffectMatrixVariable* g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable* g_pmWorldView = NULL;
ID3D10EffectMatrixVariable* g_pmWorld = NULL;
ID3D10EffectMatrixVariable* g_pmInvView = NULL;
ID3D10EffectMatrixVariable* g_pmInvProj = NULL;
ID3D10EffectScalarVariable* g_pfFadeDistance = NULL;
ID3D10EffectScalarVariable* g_pfSizeZScale = NULL;
ID3D10EffectVectorVariable* g_pvViewLightDir1 = NULL;
ID3D10EffectVectorVariable* g_pvViewLightDir2 = NULL;
ID3D10EffectVectorVariable* g_pvWorldLightDir1 = NULL;
ID3D10EffectVectorVariable* g_pvWorldLightDir2 = NULL;
ID3D10EffectVectorVariable* g_pvEyePt = NULL;
ID3D10EffectVectorVariable* g_pvViewDir = NULL;
ID3D10EffectVectorVariable* g_pvOctaveOffsets = NULL;
ID3D10EffectVectorVariable* g_pvScreenSize = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectShaderResourceVariable* g_pNormalTex = NULL;
ID3D10EffectShaderResourceVariable* g_pColorGradient = NULL;
ID3D10EffectShaderResourceVariable* g_pVolumeDiffTex = NULL;
ID3D10EffectShaderResourceVariable* g_pVolumeNormTex = NULL;
ID3D10EffectShaderResourceVariable* g_pDepthTex = NULL;
ID3D10EffectShaderResourceVariable* g_pDepthMSAATex = NULL;

INT g_iWidth = 640;
INT g_iHeight = 480;
INT g_iSampleCount = 1;

enum PARTICLE_TECHNIQUE
{
    PT_VOLUME_SOFT = 0x0,
    PT_VOLUME_HARD,
    PT_BILLBOARD_ODEPTHSOFT,
    PT_BILLBOARD_ODEPTH,
    PT_BILLBOARD_SOFT,
    PT_BILLBOARD_HARD
};

float g_fFadeDistance = 1.0f;
float g_fParticleLifeSpan = 5.0f;
float g_fEmitRate = 0.015f;

float g_ParticleVel = 3.0f;
float g_fParticleMaxSize = 1.25f;
float g_fParticleMinSize = 1.0f;
bool  g_bAnimateParticles = true;


PARTICLE_TECHNIQUE g_ParticleTechnique = PT_VOLUME_SOFT;
D3DXVECTOR3 g_vLightDir1 = D3DXVECTOR3(1.705f,5.557f,-9.380f);
D3DXVECTOR3 g_vLightDir2 = D3DXVECTOR3(-5.947f,-5.342f,-5.733f);

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC             -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TECHNIQUE           5
#define IDC_TOGGLEWARP          6

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool    CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void    CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void    CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void    CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool    CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
void    CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void    CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

void    InitApp();
void    RenderText();

HRESULT CreateParticleBuffers(ID3D10Device* pd3dDevice);
HRESULT CreateNoiseVolume( ID3D10Device* pd3dDevice, UINT VolumeSize );
void SortParticleBuffer( D3DXVECTOR3 vEye, D3DXVECTOR3 vDir );
void AdvanceParticles( ID3D10Device* pd3dDevice, double fTime, float fTimeDelta );
void UpdateParticleBuffers( ID3D10Device* pd3dDevice );

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

	// Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

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
    DXUTCreateWindow( L"SoftParticles10.1" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT render loop
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

    CDXUTComboBox* pComboBox = NULL;

    g_SampleUI.AddStatic( IDC_STATIC, L"(T)echnique", 0, 0, 105, 25 );
    g_SampleUI.AddComboBox( IDC_TECHNIQUE, 0, 25, 140, 24, 'T', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 80 );

    pComboBox->AddItem( L"Volume Soft", (void*)PT_VOLUME_SOFT );
    pComboBox->AddItem( L"Volume Hard", (void*)PT_VOLUME_HARD );
    pComboBox->AddItem( L"Depth Sprites Soft", (void*)PT_BILLBOARD_ODEPTHSOFT );
    pComboBox->AddItem( L"Depth Sprites Hard", (void*)PT_BILLBOARD_ODEPTH );
    pComboBox->AddItem( L"Billboard Soft", (void*)PT_BILLBOARD_SOFT );
    pComboBox->AddItem( L"Billboard Hard", (void*)PT_BILLBOARD_HARD );
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
        if( (DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
            (DXUT_D3D10_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    pDeviceSettings->d3d10.SyncInterval = 0;

    return true;
}

static int iFrame = 0;
//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    D3DXVECTOR3 vEye = *g_Camera.GetEyePt();
    D3DXVECTOR3 vDir = *g_Camera.GetLookAtPt() - vEye;
    D3DXVec3Normalize( &vDir, &vDir );

    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    AdvanceParticles( pd3dDevice, fTime, fElapsedTime );
    SortParticleBuffer( vEye, vDir );
    UpdateParticleBuffers( pd3dDevice );

    // Update the movement of the noise octaves
    D3DXVECTOR4 OctaveOffsets[4];
    for( int i=0; i<4; i++ )
    {
        OctaveOffsets[i].x = -(float)(fTime*0.05);
        OctaveOffsets[i].y = 0;
        OctaveOffsets[i].z = 0;
        OctaveOffsets[i].w = 0;
    }
    g_pvOctaveOffsets->SetFloatVectorArray( (float*)OctaveOffsets, 0, 4 );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext )
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
        case IDC_TOGGLEFULLSCREEN: DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:        DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:     g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_TECHNIQUE:
        {
            CDXUTComboBox* pComboBox = (CDXUTComboBox*)pControl;
            g_ParticleTechnique = (PARTICLE_TECHNIQUE)(int)PtrToInt(pComboBox->GetSelectedData());
        }
        break;
    }    
}


//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    ID3D10Device1*  pd3dDevice1 = DXUTGetD3D10Device1();

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SoftParticles.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif

    if( NULL == pd3dDevice1 ) {
        V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL, NULL, &g_pEffect10, NULL, NULL ) );
    } else {
        V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_1", dwShaderFlags, 0, pd3dDevice, NULL, NULL, &g_pEffect10, NULL, NULL ) );
    }

    // Obtain the technique handles
    g_pRenderScene = g_pEffect10->GetTechniqueByName( "RenderScene" );
    g_pRenderSky = g_pEffect10->GetTechniqueByName( "RenderSky" );
    g_pRenderBillboardParticlesHard = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_Hard" );
    g_pRenderBillboardParticlesODepth = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_ODepth" );
    g_pRenderBillboardParticlesSoft = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_Soft" );
    g_pRenderBillboardParticlesODepthSoft = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_ODepthSoft" );
    g_pRenderVolumeParticlesHard = g_pEffect10->GetTechniqueByName( "RenderVolumeParticles_Hard" );
    g_pRenderVolumeParticlesSoft = g_pEffect10->GetTechniqueByName( "RenderVolumeParticles_Soft" );
    g_pRenderVolumeParticlesSoftMSAA = g_pEffect10->GetTechniqueByName( "RenderVolumeParticles_Soft_MSAA" );
    g_pRenderVolumeParticlesHardMSAA = g_pEffect10->GetTechniqueByName( "RenderVolumeParticles_Hard_MSAA" );
    g_pRenderBillboardParticlesSoftMSAA = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_Soft_MSAA" );
    g_pRenderBillboardParticlesODepthSoftMSAA = g_pEffect10->GetTechniqueByName( "RenderBillboardParticles_ODepthSoft_MSAA" );
  

        // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect10->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmInvView = g_pEffect10->GetVariableByName( "g_mInvView" )->AsMatrix();
    g_pmInvProj = g_pEffect10->GetVariableByName( "g_mInvProj" )->AsMatrix();
    g_pfFadeDistance = g_pEffect10->GetVariableByName( "g_fFadeDistance" )->AsScalar();
    g_pfSizeZScale = g_pEffect10->GetVariableByName( "g_fSizeZScale" )->AsScalar();
    g_pvViewLightDir1 = g_pEffect10->GetVariableByName( "g_vViewLightDir1" )->AsVector();
    g_pvViewLightDir2 = g_pEffect10->GetVariableByName( "g_vViewLightDir2" )->AsVector();
    g_pvWorldLightDir1 = g_pEffect10->GetVariableByName( "g_vWorldLightDir1" )->AsVector();
    g_pvWorldLightDir2 = g_pEffect10->GetVariableByName( "g_vWorldLightDir2" )->AsVector();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_pvViewDir = g_pEffect10->GetVariableByName( "g_vViewDir" )->AsVector();
    g_pvOctaveOffsets = g_pEffect10->GetVariableByName( "g_OctaveOffsets" )->AsVector();
    g_pvScreenSize = g_pEffect10->GetVariableByName( "g_vScreenSize" )->AsVector();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pNormalTex = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();
    g_pColorGradient = g_pEffect10->GetVariableByName( "g_txColorGradient" )->AsShaderResource();
    g_pVolumeDiffTex = g_pEffect10->GetVariableByName( "g_txVolumeDiff" )->AsShaderResource();
    g_pVolumeNormTex = g_pEffect10->GetVariableByName( "g_txVolumeNorm" )->AsShaderResource();
    g_pDepthTex = g_pEffect10->GetVariableByName( "g_txDepth" )->AsShaderResource();
    g_pDepthMSAATex = g_pEffect10->GetVariableByName( "g_txDepthMSAA" )->AsShaderResource();

    // Create our vertex input layouts
    const D3D10_INPUT_ELEMENT_DESC scenelayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pRenderScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( scenelayout, sizeof(scenelayout)/sizeof(scenelayout[0]), PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pSceneVertexLayout ) );

    const D3D10_INPUT_ELEMENT_DESC particlelayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "LIFE",     0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "SIZE",     0, DXGI_FORMAT_R32_FLOAT, 0, 28, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderBillboardParticlesHard->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( particlelayout, sizeof(particlelayout)/sizeof(scenelayout[0]), PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pParticleVertexLayout ) );

    // Load meshes
    V_RETURN( g_ObjectMesh.Create( pd3dDevice, L"SoftParticles\\TankScene.sdkmesh", true ) );
    V_RETURN( g_SkyMesh.Create( pd3dDevice, L"SoftParticles\\desertsky.sdkmesh", true ) );

    // Create the particles
    V_RETURN( CreateParticleBuffers( pd3dDevice ) );

    // Create the noise volume
    V_RETURN( CreateNoiseVolume( pd3dDevice, 32 ) );

    // Load the Particle Texture
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SoftParticles\\smokevol1.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pParticleTexRV, NULL ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SoftParticles\\colorgradient.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pColorGradTexRV, NULL ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye(2,1,-10);
    D3DXVECTOR3 vecAt (2,1,0);
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( 10.0f, 1.0f, 20.0f );

    g_pfFadeDistance->SetFloat( g_fFadeDistance );

    // Enable/Disable MSAA settings from the settings dialog based on whether we have dx10.1 
    // support or not.
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled( NULL != pd3dDevice1 );
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled( NULL != pd3dDevice1 );
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL )->SetEnabled( NULL != pd3dDevice1 );
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL )->SetEnabled( NULL != pd3dDevice1 );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    ID3D10Device1*  pd3dDevice1 = DXUTGetD3D10Device1();

    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fNearPlane = 0.1f;
    float fFarPlane = 150.0f;
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( 54.43f*(D3DX_PI/180.0f), fAspectRatio, fNearPlane, fFarPlane );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    // Set the effect variable
    g_pfSizeZScale->SetFloat( 1.0f/(fFarPlane - fNearPlane) );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width-170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width-170, pBackBufferSurfaceDesc->Height-200 );
    g_SampleUI.SetSize( 170, 200 );

    // Create a new Depth-Stencil texture to replace the DXUT created one
    D3D10_TEXTURE2D_DESC descDepth;
    descDepth.Width = pBackBufferSurfaceDesc->Width;
    descDepth.Height = pBackBufferSurfaceDesc->Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_R32_TYPELESS; // Use a typeless type here so that it can be both depth-stencil and shader resource.
                                                 // This will fail if we try a format like D32_FLOAT
    descDepth.SampleDesc = pBackBufferSurfaceDesc->SampleDesc;
    descDepth.Usage = D3D10_USAGE_DEFAULT;
    descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &descDepth, NULL, &g_pDepthStencilTexture ) );

    // Create the depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
    if( 1 == descDepth.SampleDesc.Count ) {
        descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    } else {
        descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    }

    descDSV.Format = DXGI_FORMAT_D32_FLOAT;	// Make the view see this as D32_FLOAT instead of typeless
    descDSV.Texture2D.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateDepthStencilView( g_pDepthStencilTexture, &descDSV, &g_pDepthStencilDSV ) );

    // Create the shader resource view
    if( 1 == descDepth.SampleDesc.Count ) {

        D3D10_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = DXGI_FORMAT_R32_FLOAT;	// Make the shaders see this as R32_FLOAT instead of typeless
        descSRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        descSRV.Texture2D.MipLevels = 1;
        descSRV.Texture2D.MostDetailedMip = 0;
        V_RETURN( pd3dDevice->CreateShaderResourceView( g_pDepthStencilTexture, &descSRV, &g_pDepthStencilSRV ) );

    } else
    if( NULL != pd3dDevice1 ) {

        D3D10_SHADER_RESOURCE_VIEW_DESC1    descSRV1;
        ID3D10ShaderResourceView1*          pSRView1 = NULL;              
            
        descSRV1.Format = DXGI_FORMAT_R32_FLOAT;
        descSRV1.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURE2DMS;

        V_RETURN( pd3dDevice1->CreateShaderResourceView1( g_pDepthStencilTexture, &descSRV1, &pSRView1 ) );

        // Get the original shader resource view interface from the dx10.1 interface
        V_RETURN( pSRView1->QueryInterface( __uuidof( ID3D10ShaderResourceView ), (LPVOID*)&g_pDepthStencilSRV ) );

        pSRView1->Release();

    } else {

        // Inconsistent state, we are trying to use MSAA with a device that does not support dx10.1
        V_RETURN( E_FAIL ); 
    }

    g_iWidth = pBackBufferSurfaceDesc->Width;
    g_iHeight = pBackBufferSurfaceDesc->Height;
    g_iSampleCount = pBackBufferSurfaceDesc->SampleDesc.Count;

    return hr;
}

//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };

    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor);
    pd3dDevice->ClearDepthStencilView( g_pDepthStencilDSV, D3D10_CLEAR_DEPTH, 1.0, 0);
    pd3dDevice->OMSetRenderTargets( 1, &pRTV, g_pDepthStencilDSV );

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Get the projection & view matrix from the camera class
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mInvView;
    D3DXMATRIX mInvProj;
    D3DXMatrixIdentity( &mWorld );
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mWorldViewProj = mWorld*mView*mProj;
    D3DXMATRIX mWorldView = mWorld*mView;
    D3DXMatrixInverse( &mInvView, NULL, &mView );
    D3DXMatrixInverse( &mInvProj, NULL, &mProj);
    D3DXVECTOR4 vViewLightDir1;
    D3DXVECTOR4 vWorldLightDir1;
    D3DXVECTOR4 vViewLightDir2;
    D3DXVECTOR4 vWorldLightDir2;
    D3DXVec3Normalize( (D3DXVECTOR3*)&vWorldLightDir1, &g_vLightDir1 );
    D3DXVec3TransformNormal( (D3DXVECTOR3*)&vViewLightDir1, &g_vLightDir1, &mView );
    D3DXVec3Normalize( (D3DXVECTOR3*)&vViewLightDir1, (D3DXVECTOR3*)&vViewLightDir1 );
    D3DXVec3Normalize( (D3DXVECTOR3*)&vWorldLightDir2, &g_vLightDir2 );
    D3DXVec3TransformNormal( (D3DXVECTOR3*)&vViewLightDir2, &g_vLightDir2, &mView );
    D3DXVec3Normalize( (D3DXVECTOR3*)&vViewLightDir2, (D3DXVECTOR3*)&vViewLightDir2 );
    D3DXVECTOR3 viewDir = *g_Camera.GetLookAtPt() - *g_Camera.GetEyePt();
    D3DXVec3Normalize( &viewDir, &viewDir );
    D3DXVECTOR3 vec3 = *g_Camera.GetEyePt();
    D3DXVECTOR4 vEyePt;
    vEyePt.x = vec3.x;
    vEyePt.y = vec3.y;
    vEyePt.z = vec3.z;
    FLOAT fScreenSize[ 2 ] = { (FLOAT)g_iWidth, (FLOAT)g_iHeight };

    g_pmWorldViewProj->SetMatrix( (float*)&mWorldViewProj );
    g_pmWorldView->SetMatrix( (float*)&mWorldView );
    g_pmWorld->SetMatrix( (float*)&mWorld );
    g_pmInvView->SetMatrix( (float*)&mInvView );
    g_pmInvProj->SetMatrix( (float*)&mInvProj );
    g_pvViewLightDir1->SetFloatVector( (float*)&vViewLightDir1 );
    g_pvWorldLightDir1->SetFloatVector( (float*)&vWorldLightDir1);
    g_pvViewLightDir2->SetFloatVector( (float*)&vViewLightDir2 );
    g_pvWorldLightDir2->SetFloatVector( (float*)&vWorldLightDir2);
    g_pvViewDir->SetFloatVector( (float*)&viewDir );
    g_pvEyePt->SetFloatVector( (float*)&vEyePt );
    g_pvScreenSize->SetFloatVector( fScreenSize );

    // Render the scene
    pd3dDevice->IASetInputLayout( g_pSceneVertexLayout );
    g_SkyMesh.Render( pd3dDevice, g_pRenderSky, g_pDiffuseTex );
    g_ObjectMesh.Render( pd3dDevice, g_pRenderScene, g_pDiffuseTex, g_pNormalTex );

    ID3D10EffectTechnique* pParticleTech = NULL;

    if( 1 == g_iSampleCount ) {
        switch( g_ParticleTechnique )
        {
        case PT_BILLBOARD_HARD:
            pParticleTech = g_pRenderBillboardParticlesHard;
            break;
        case PT_BILLBOARD_ODEPTH:
            pParticleTech = g_pRenderBillboardParticlesODepth;
            break;
        case PT_BILLBOARD_SOFT:
            pParticleTech = g_pRenderBillboardParticlesSoft;
            break;
        case PT_BILLBOARD_ODEPTHSOFT:
            pParticleTech = g_pRenderBillboardParticlesODepthSoft;
            break;
        case PT_VOLUME_HARD:
            pParticleTech = g_pRenderVolumeParticlesHard;
            break;
        case PT_VOLUME_SOFT:
            pParticleTech = g_pRenderVolumeParticlesSoft;
            break;
        };
    } else {
        switch( g_ParticleTechnique )
        {
        case PT_BILLBOARD_HARD:
            pParticleTech = g_pRenderBillboardParticlesHard;
            break;
        case PT_BILLBOARD_ODEPTH:
            pParticleTech = g_pRenderBillboardParticlesODepth;
            break;
        case PT_BILLBOARD_SOFT:
            pParticleTech = g_pRenderBillboardParticlesSoftMSAA;
            break;
        case PT_BILLBOARD_ODEPTHSOFT:
            pParticleTech = g_pRenderBillboardParticlesODepthSoftMSAA;
            break;
        case PT_VOLUME_HARD:
            pParticleTech = g_pRenderVolumeParticlesHardMSAA;
            break;
        case PT_VOLUME_SOFT:
            pParticleTech = g_pRenderVolumeParticlesSoftMSAA;
            break;
        };
    }

    if( PT_BILLBOARD_HARD != g_ParticleTechnique &&
        PT_BILLBOARD_ODEPTH != g_ParticleTechnique )
    {
        // Unbind the depth stencil texture from the device
        pd3dDevice->OMSetRenderTargets( 1, &pRTV, NULL );
        // Bind it instead as a shader resource view
        if( 1 == g_iSampleCount ) {
            g_pDepthTex->SetResource( g_pDepthStencilSRV );
        } else {
            g_pDepthMSAATex->SetResource( g_pDepthStencilSRV );
        }
    }

     // Render the particles
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );
    ID3D10Buffer *pBuffers[1] = { g_pParticleVB };
    UINT stride[1] = { sizeof(PARTICLE_VERTEX) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );
    pd3dDevice->IASetIndexBuffer( g_pParticleIB, DXGI_FORMAT_R32_UINT, 0 );
    
    if( PT_VOLUME_HARD == g_ParticleTechnique ||
        PT_VOLUME_SOFT == g_ParticleTechnique )
    {
        g_pVolumeDiffTex->SetResource( g_pNoiseVolumeRV );
        g_pVolumeNormTex->SetResource( NULL );
    }
    else 
    {
        g_pVolumeDiffTex->SetResource( g_pParticleTexRV );
    }
    g_pColorGradient->SetResource( g_pColorGradTexRV );

    D3D10_TECHNIQUE_DESC techDesc;
    pParticleTech->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pParticleTech->GetPassByIndex( p )->Apply(0);
        pd3dDevice->DrawIndexed( MAX_PARTICLES, 0, 0 );
    }
   
    // unbind the depth from the resource so we can set it as depth next time around
    ID3D10ShaderResourceView* Nulls[2] = {NULL,NULL};
    pd3dDevice->PSSetShaderResources( 0, 2, Nulls );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime ); 
    g_SampleUI.OnRender( fElapsedTime );  
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

    SAFE_RELEASE( g_pDepthStencilTexture );
    SAFE_RELEASE( g_pDepthStencilDSV );
    SAFE_RELEASE( g_pDepthStencilSRV );
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
    SAFE_RELEASE( g_pSceneVertexLayout );
    SAFE_RELEASE( g_pParticleVertexLayout );
    SAFE_RELEASE( g_pParticleVB );
    SAFE_RELEASE( g_pParticleIB );
    SAFE_RELEASE( g_pParticleTexRV );
    SAFE_RELEASE( g_pNoiseVolume );
    SAFE_RELEASE( g_pNoiseVolumeRV );
    SAFE_RELEASE( g_pColorGradTexRV );

    g_ObjectMesh.Destroy();
    g_SkyMesh.Destroy();
    SAFE_DELETE_ARRAY( g_pCPUParticles );
    SAFE_DELETE_ARRAY( g_pCPUParticleIndices );
    SAFE_DELETE_ARRAY( g_pParticleDepthArray );
}

//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = (float)((rand()%20000) - 10000 );
    return ret/10000.0f;
}

//--------------------------------------------------------------------------------------
void EmitParticle( PARTICLE_VERTEX* pParticle )
{
    pParticle->Pos.x = 0.0f;
    pParticle->Pos.y = 0.7f;
    pParticle->Pos.z = 3.0f;

    pParticle->Vel.x = 1.0f;
    pParticle->Vel.y = 0.3f*RPercent();
    pParticle->Vel.z = 0.3f*RPercent();

    D3DXVec3Normalize( &pParticle->Vel, &pParticle->Vel );
    pParticle->Vel *= g_ParticleVel;

    pParticle->Life = 0.0f;
    pParticle->Size = 0.0f;
}

//--------------------------------------------------------------------------------------
// Create a VB for particles
//--------------------------------------------------------------------------------------
HRESULT CreateParticleBuffers( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    D3D10_BUFFER_DESC vbdesc;
    vbdesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    vbdesc.ByteWidth = MAX_PARTICLES*sizeof(PARTICLE_VERTEX);
    vbdesc.CPUAccessFlags = 0;
    vbdesc.MiscFlags = 0;
    vbdesc.Usage = D3D10_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleVB ) );

    vbdesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    vbdesc.ByteWidth = MAX_PARTICLES*sizeof(DWORD);
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleIB ) );

    g_pCPUParticles = new PARTICLE_VERTEX[ MAX_PARTICLES ];
    if( !g_pCPUParticles )
        return E_OUTOFMEMORY;

    for( UINT i=0; i<MAX_PARTICLES; i++ )
    {
        g_pCPUParticles[i].Life = -1;	//kill all particles
    }

    g_pCPUParticleIndices = new DWORD[ MAX_PARTICLES ];
    if( !g_pCPUParticleIndices )
        return E_OUTOFMEMORY;
    g_pParticleDepthArray = new float[ MAX_PARTICLES ]; 
    if( !g_pParticleDepthArray )
        return E_OUTOFMEMORY;

    return hr;
}

struct CHAR4
{
    char x,y,z,w;
};

//--------------------------------------------------------------------------------------
float GetDensity( int x, int y, int z, CHAR4* pTexels, UINT VolumeSize )
{
    if( x < 0 )
        x += VolumeSize;
    if( y < 0 )
        y += VolumeSize;
    if( z < 0 )
        z += VolumeSize;

    x = x%VolumeSize;
    y = y%VolumeSize;
    z = z%VolumeSize;

    int index = x + y*VolumeSize + z*VolumeSize*VolumeSize;

    return (float)pTexels[index].w / 128.0f;
}

void SetNormal( D3DXVECTOR3 Normal, int x, int y, int z, CHAR4* pTexels, UINT VolumeSize )
{
    if( x < 0 )
        x += VolumeSize;
    if( y < 0 )
        y += VolumeSize;
    if( z < 0 )
        z += VolumeSize;

    x = x%VolumeSize;
    y = y%VolumeSize;
    z = z%VolumeSize;

    int index = x + y*VolumeSize + z*VolumeSize*VolumeSize;

    pTexels[index].x = (char)(Normal.x * 128.0f);
    pTexels[index].y = (char)(Normal.y * 128.0f);
    pTexels[index].z = (char)(Normal.z * 128.0f);
}

//--------------------------------------------------------------------------------------
// Create and blur a noise volume texture
//--------------------------------------------------------------------------------------
HRESULT CreateNoiseVolume( ID3D10Device* pd3dDevice, UINT VolumeSize )
{
    HRESULT hr = S_OK;

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new CHAR4[ VolumeSize*VolumeSize*VolumeSize ];
    InitData.SysMemPitch = VolumeSize*sizeof(CHAR4);
    InitData.SysMemSlicePitch = VolumeSize*VolumeSize*sizeof(CHAR4);

    // Gen a bunch of random values
    CHAR4* pData = (CHAR4*)InitData.pSysMem;
    for( UINT i=0; i<VolumeSize*VolumeSize*VolumeSize; i++ )
    {
        pData[i].w = (char)(RPercent() * 128.0f);
    }

    // Generate normals from the density gradient
    float heightAdjust = 0.5f;
    D3DXVECTOR3 Normal;
    D3DXVECTOR3 DensityGradient;
    for( UINT z=0; z<VolumeSize; z++ )
    {
        for( UINT y=0; y<VolumeSize; y++ )
        {
            for( UINT x=0; x<VolumeSize; x++ )
            {
                DensityGradient.x = GetDensity( x+1, y, z, pData, VolumeSize ) - GetDensity( x-1, y, z, pData, VolumeSize )/heightAdjust;
                DensityGradient.y = GetDensity( x, y+1, z, pData, VolumeSize ) - GetDensity( x, y-1, z, pData, VolumeSize )/heightAdjust;
                DensityGradient.z = GetDensity( x, y, z+1, pData, VolumeSize ) - GetDensity( x, y, z-1, pData, VolumeSize )/heightAdjust;

                D3DXVec3Normalize( &Normal, &DensityGradient );
                SetNormal( Normal, x,y,z, pData, VolumeSize );
            }
        }
    }

    D3D10_TEXTURE3D_DESC desc;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.Depth = desc.Height = desc.Width = VolumeSize;
    desc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
    desc.MipLevels = 1;
    desc.MiscFlags = 0;
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    V_RETURN( pd3dDevice->CreateTexture3D( &desc, &InitData, &g_pNoiseVolume ) );

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
    SRVDesc.Texture3D.MipLevels = desc.MipLevels;
    SRVDesc.Texture3D.MostDetailedMip = 0;
    V_RETURN(pd3dDevice->CreateShaderResourceView( g_pNoiseVolume, &SRVDesc, &g_pNoiseVolumeRV ));
    
    SAFE_DELETE_ARRAY( InitData.pSysMem );
    return hr;
}

//--------------------------------------------------------------------------------------
// Update the particle VB using UpdateSubresource
//--------------------------------------------------------------------------------------
void QuickDepthSort(DWORD* indices, float* depths, int lo, int hi)
{
//  lo is the lower index, hi is the upper index
//  of the region of array a that is to be sorted
    int i=lo, j=hi;
    float h;
    int index;
    float x=depths[(lo+hi)/2];

    //  partition
    do
    {    
        while (depths[i] > x) i++; 
        while (depths[j] < x) j--;
        if (i<=j)
        {
            h=depths[i]; depths[i]=depths[j]; depths[j]=h;
            index = indices[i]; indices[i] = indices[j]; indices[j] = index;
            i++; j--;
        }
    } while (i<=j);

    //  recursion
    if (lo<j) QuickDepthSort(indices, depths, lo, j);
    if (i<hi) QuickDepthSort(indices, depths, i, hi);
}

//--------------------------------------------------------------------------------------
// Sort the particle buffer
//--------------------------------------------------------------------------------------
void SortParticleBuffer( D3DXVECTOR3 vEye, D3DXVECTOR3 vDir )
{
    if( !g_pParticleDepthArray || !g_pCPUParticleIndices )
        return;

    // assume vDir is normalized
    D3DXVECTOR3 vToParticle;
    //init indices and depths
    for( UINT i=0; i<MAX_PARTICLES; i++ )
    {
        g_pCPUParticleIndices[i] = i;
        vToParticle = g_pCPUParticles[i].Pos - vEye;
        g_pParticleDepthArray[i] = D3DXVec3Dot( &vDir, &vToParticle );
    }

    // Sort
    QuickDepthSort(g_pCPUParticleIndices, g_pParticleDepthArray, 0, MAX_PARTICLES-1);
}

//--------------------------------------------------------------------------------------
// Create a VB for particles
//--------------------------------------------------------------------------------------
void AdvanceParticles( ID3D10Device* pd3dDevice, double fTime, float fTimeDelta )
{
    //emit new particles
    static double fLastEmitTime = 0;
    static UINT iLastParticleEmitted = 0;

    if( !g_bAnimateParticles )
    {
        fLastEmitTime = fTime;
        return;
    }

    float fEmitRate = g_fEmitRate;
    float fParticleMaxSize = g_fParticleMaxSize;
    float fParticleMinSize = g_fParticleMinSize;

    if( PT_VOLUME_HARD == g_ParticleTechnique ||
        PT_VOLUME_SOFT == g_ParticleTechnique )
    {
        fEmitRate *= 3.0f;	//emit 1/3 less particles if we're doing volume
        fParticleMaxSize *= 1.5f;	//1.5x the max radius
        fParticleMinSize *= 1.5f;	//1.5x the min radius
    }

    UINT NumParticlesToEmit = (UINT)( (fTime - fLastEmitTime)/fEmitRate );
    if( NumParticlesToEmit > 0 )
    {
        for( UINT i=0; i<NumParticlesToEmit; i++ )
        {
            EmitParticle( &g_pCPUParticles[iLastParticleEmitted] );
            iLastParticleEmitted = (iLastParticleEmitted+1) % MAX_PARTICLES;
        }
        fLastEmitTime = fTime;
    }

    D3DXVECTOR3 vel;
    float lifeSq = 0;
    for( UINT i=0; i<MAX_PARTICLES; i++ )
    {
        if( g_pCPUParticles[i].Life > -1 )
        {
            // squared velocity falloff
            lifeSq = g_pCPUParticles[i].Life*g_pCPUParticles[i].Life;

            // Slow down by 50% as we age
            vel = g_pCPUParticles[i].Vel * (1 - 0.5f*lifeSq);
            vel.y += 0.5f;	//(add some to the up direction, becuase of buoyancy)

            g_pCPUParticles[i].Pos += vel*fTimeDelta;
            g_pCPUParticles[i].Life += fTimeDelta/g_fParticleLifeSpan;
            g_pCPUParticles[i].Size = fParticleMinSize + (fParticleMaxSize-fParticleMinSize) * g_pCPUParticles[i].Life;

            if( g_pCPUParticles[i].Life > 0.99f )
                g_pCPUParticles[i].Life = -1;
        }
    }
}

//--------------------------------------------------------------------------------------
// Update the particle VB using UpdateSubresource
//--------------------------------------------------------------------------------------
void UpdateParticleBuffers( ID3D10Device* pd3dDevice )
{
    pd3dDevice->UpdateSubresource( g_pParticleVB, NULL, NULL, g_pCPUParticles, 0, 0 );
    pd3dDevice->UpdateSubresource( g_pParticleIB, NULL, NULL, g_pCPUParticleIndices, 0, 0 );
}
