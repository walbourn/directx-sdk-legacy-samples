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

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
bool                                g_bFirst = true;
bool                                g_bAccumulateWithGS = false;

ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pParticleVertexLayout = NULL;
ID3D10InputLayout*                  g_pSplatVertexLayout = NULL;
ID3D10InputLayout*                  g_pMeshVertexLayout = NULL;

CDXUTSDKMesh                        g_BoidMesh;
CDXUTSDKMesh                        g_FleeMesh;

ID3D10Buffer*                       g_pParticleBuffer = NULL;
ID3D10ShaderResourceView*           g_pParticleTexRV = NULL;
ID3D10Buffer*                       g_pSplatBuffer = NULL;

ID3D10Texture2D*                    g_pParticleDataTextureTo = NULL;
ID3D10ShaderResourceView*           g_pParticleDataTexSRVTo = NULL;
ID3D10RenderTargetView*             g_pParticleDataTexRTVTo = NULL;
ID3D10Texture2D*                    g_pParticleDataTextureFrom = NULL;
ID3D10ShaderResourceView*           g_pParticleDataTexSRVFrom = NULL;
ID3D10RenderTargetView*             g_pParticleDataTexRTVFrom = NULL;
ID3D10Texture2D*                    g_pForceTexture = NULL;
ID3D10ShaderResourceView*           g_pForceTexSRV = NULL;
ID3D10RenderTargetView*             g_pForceTexRTV = NULL;

ID3D10EffectTechnique*              g_pRenderParticles;
ID3D10EffectTechnique*              g_pAccumulateForces;
ID3D10EffectTechnique*              g_pAccumulateForcesGS;
ID3D10EffectTechnique*              g_pAdvanceParticles;
ID3D10EffectTechnique*              g_pRenderBoids;
ID3D10EffectTechnique*              g_pRenderMesh;

ID3D10EffectMatrixVariable*         g_pmWorldViewProj;
ID3D10EffectMatrixVariable*         g_pmWorld;
ID3D10EffectMatrixVariable*         g_pmInvView;
ID3D10EffectScalarVariable*         g_pfGlobalTime;
ID3D10EffectScalarVariable*         g_pfElapsedTime;
ID3D10EffectScalarVariable*         g_piTexSize;
ID3D10EffectScalarVariable*         g_piMaxMip;
ID3D10EffectScalarVariable*         g_pfParticleRad;
ID3D10EffectScalarVariable*         g_pfParticleMass;
ID3D10EffectScalarVariable*         g_pfG;
ID3D10EffectVectorVariable*         g_pvFleePos;
ID3D10EffectScalarVariable*         g_pfBoidScale;

ID3D10EffectShaderResourceVariable* g_pDiffuseTex;
ID3D10EffectShaderResourceVariable* g_pParticleDataTex;
ID3D10EffectShaderResourceVariable* g_pForceTex;

D3DXVECTOR4                         g_vFleePos( 700,0,0,1 );

struct PARTICLE_VERTEX
{
    D3DXVECTOR4 Color;
};

struct SPLAT_VERTEX
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 Tex;
};
#define MAX_PARTICLES 4096
UINT                                g_iTexSize;
UINT                                g_iMaxMip;
float                               g_fParticleMass = 10000.0f * 10000.0f;
float                               g_fGConstant = 6.67300e-11f * 10000.0f;
float                               g_fSpread = 50.0f;
float                               g_fParticleRad = 1.0f;
float                               g_fBoidScale = 3.0f;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWARP          5

#define IDC_ACCUMULATEWITHGS	10
#define IDC_BOIDSCALE_STATIC	11
#define IDC_BOIDSCALE			12


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

HRESULT CreateParticleTextures( ID3D10Device* pd3dDevice );
HRESULT CreateParticleBuffer( ID3D10Device* pd3dDevice );
HRESULT CreateForceTexture( ID3D10Device* pd3dDevice );
HRESULT CreateSplatBuffer( ID3D10Device* pd3dDevice );

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
    DXUTCreateWindow( L"GPUBoids" );
    DXUTCreateDevice( true, 1024, 768 );
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
    g_SampleUI.AddCheckBox( IDC_ACCUMULATEWITHGS, L"Accumulate With GS", 35, iY += 24, 125, 22, g_bAccumulateWithGS );

    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"BoidSize: %.2f", g_fBoidScale );
    g_SampleUI.AddStatic( IDC_BOIDSCALE_STATIC, str, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_BOIDSCALE, 35, iY += 24, 125, 22, 0, 500, ( int )( g_fBoidScale * 100 ) );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    pDeviceSettings->d3d10.SyncInterval = DXGI_SWAP_EFFECT_DISCARD;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

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

    float fMoveAmt = 200.0f * fElapsedTime;
    if( GetAsyncKeyState( 'A' ) & 0x8000 )
        g_vFleePos.x -= fMoveAmt;
    if( GetAsyncKeyState( 'D' ) & 0x8000 )
        g_vFleePos.x += fMoveAmt;

    if( GetAsyncKeyState( 'W' ) & 0x8000 )
        g_vFleePos.z += fMoveAmt;
    if( GetAsyncKeyState( 'S' ) & 0x8000 )
        g_vFleePos.z -= fMoveAmt;

    if( GetAsyncKeyState( 'E' ) & 0x8000 )
        g_vFleePos.y += fMoveAmt;
    if( GetAsyncKeyState( 'Q' ) & 0x8000 )
        g_vFleePos.y -= fMoveAmt;
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_ACCUMULATEWITHGS:
            g_bAccumulateWithGS = !g_bAccumulateWithGS; break;

        case IDC_BOIDSCALE:
        {
            g_fBoidScale = ( float )g_SampleUI.GetSlider( IDC_BOIDSCALE )->GetValue() / 100.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"BoidSize: %.2f", g_fBoidScale );
            g_SampleUI.GetStatic( IDC_BOIDSCALE_STATIC )->SetText( str );
        }
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GPUBoids.fx" ) );
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
    g_pRenderParticles = g_pEffect10->GetTechniqueByName( "RenderParticles" );
    g_pAccumulateForces = g_pEffect10->GetTechniqueByName( "AccumulateForces" );
    g_pAccumulateForcesGS = g_pEffect10->GetTechniqueByName( "AccumulateForcesGS" );
    g_pAdvanceParticles = g_pEffect10->GetTechniqueByName( "AdvanceParticles" );
    g_pRenderBoids = g_pEffect10->GetTechniqueByName( "RenderBoids" );
    g_pRenderMesh = g_pEffect10->GetTechniqueByName( "RenderMesh" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmInvView = g_pEffect10->GetVariableByName( "g_mInvView" )->AsMatrix();
    g_pfGlobalTime = g_pEffect10->GetVariableByName( "g_fGlobalTime" )->AsScalar();
    g_pfElapsedTime = g_pEffect10->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_piTexSize = g_pEffect10->GetVariableByName( "g_iTexSize" )->AsScalar();
    g_piMaxMip = g_pEffect10->GetVariableByName( "g_iMaxMip" )->AsScalar();
    g_pfParticleRad = g_pEffect10->GetVariableByName( "g_fParticleRad" )->AsScalar();
    g_pfParticleMass = g_pEffect10->GetVariableByName( "g_fParticleMass" )->AsScalar();
    g_pfG = g_pEffect10->GetVariableByName( "g_fG" )->AsScalar();
    g_pvFleePos = g_pEffect10->GetVariableByName( "g_vFleePos" )->AsVector();
    g_pfBoidScale = g_pEffect10->GetVariableByName( "g_fBoidScale" )->AsScalar();

    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pParticleDataTex = g_pEffect10->GetVariableByName( "g_txParticleData" )->AsShaderResource();
    g_pForceTex = g_pEffect10->GetVariableByName( "g_txForce" )->AsShaderResource();

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pRenderParticles->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, sizeof( layout ) / sizeof( layout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pParticleVertexLayout ) );

    // Create our splat vertex input layout
    const D3D10_INPUT_ELEMENT_DESC splatlayout[] =
    {
        { "POS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pAccumulateForces->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( splatlayout, sizeof( splatlayout ) / sizeof( splatlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSplatVertexLayout ) );

    // Create our mesh vertex input layout
    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pRenderBoids->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, sizeof( meshlayout ) / sizeof( meshlayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshVertexLayout ) );

    // Create a mesh to avoid
    g_BoidMesh.Create( pd3dDevice, L"GPUBoids\\boid.sdkmesh" );
    g_FleeMesh.Create( pd3dDevice, L"misc\\ball.sdkmesh" );

    // Create the seeding particle
    V_RETURN( CreateParticleTextures( pd3dDevice ) );
    V_RETURN( CreateParticleBuffer( pd3dDevice ) );
    V_RETURN( CreateForceTexture( pd3dDevice ) );
    V_RETURN( CreateSplatBuffer( pd3dDevice ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -g_fSpread * 15.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.5f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_MIDDLE_BUTTON | MOUSE_RIGHT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    g_bFirst = true;
    return hr;
}


//--------------------------------------------------------------------------------------
bool RenderParticles( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pParticleBuffer };
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Set Effects Parameters
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    D3DXMATRIX mInvView;
    D3DXMatrixInverse( &mInvView, NULL, &mView );
    g_pmInvView->SetMatrix( ( float* )&mInvView );
    g_piTexSize->SetInt( g_iTexSize );
    g_piMaxMip->SetInt( g_iMaxMip );
    g_pfParticleRad->SetFloat( g_fParticleRad );

    // Set resources
    g_pParticleDataTex->SetResource( g_pParticleDataTexSRVFrom );
    g_pForceTex->SetResource( g_pForceTexSRV );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderParticles->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pRenderParticles->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( MAX_PARTICLES, 0 );
    }

    ID3D10ShaderResourceView* pNULLs[4] = {0,0,0,0};
    pd3dDevice->VSSetShaderResources( 0, 4, pNULLs );
    pd3dDevice->PSSetShaderResources( 0, 4, pNULLs );

    return true;
}


//--------------------------------------------------------------------------------------
bool AccumulateForcesGS( ID3D10Device* pd3dDevice )
{
    // Clear the force texture
    float ClearColor[4] = {0,0,0,0};
    pd3dDevice->ClearRenderTargetView( g_pForceTexRTV, ClearColor );

    // Store the old render targets
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // Store the old viewport
    UINT iNumVP = 1;
    D3D10_VIEWPORT OldVP;
    pd3dDevice->RSGetViewports( &iNumVP, &OldVP );

    // Set the new render targets
    pd3dDevice->OMSetRenderTargets( 1, &g_pForceTexRTV, NULL );

    // Set the new viewport
    D3D10_VIEWPORT NewVP;
    NewVP.Width = g_iTexSize;
    NewVP.Height = g_iTexSize;
    NewVP.TopLeftX = 0;
    NewVP.TopLeftY = 0;
    NewVP.MinDepth = 0.0f;
    NewVP.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &NewVP );

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pParticleBuffer };
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Set Effects Parameters
    g_piTexSize->SetInt( g_iTexSize );
    g_piMaxMip->SetInt( g_iMaxMip );
    g_pfParticleMass->SetFloat( g_fParticleMass );
    g_pfG->SetFloat( g_fGConstant );
    g_pfParticleRad->SetFloat( g_fParticleRad );

    // Set resources
    g_pParticleDataTex->SetResource( g_pParticleDataTexSRVFrom );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pAccumulateForcesGS->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pAccumulateForcesGS->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( MAX_PARTICLES, 0 );
    }

    // Restore the old vp
    pd3dDevice->RSSetViewports( 1, &OldVP );

    // Restore the old render targets
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    ID3D10ShaderResourceView* pNULLs[4] = {0,0,0,0};
    pd3dDevice->VSSetShaderResources( 0, 4, pNULLs );
    pd3dDevice->PSSetShaderResources( 0, 4, pNULLs );

    return true;
}


//--------------------------------------------------------------------------------------
bool AccumulateForces( ID3D10Device* pd3dDevice )
{
    // Clear the force texture
    float ClearColor[4] = {0,0,0,0};
    pd3dDevice->ClearRenderTargetView( g_pForceTexRTV, ClearColor );

    // Store the old render targets
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // Store the old viewport
    UINT iNumVP = 1;
    D3D10_VIEWPORT OldVP;
    pd3dDevice->RSGetViewports( &iNumVP, &OldVP );

    // Set the new render targets
    pd3dDevice->OMSetRenderTargets( 1, &g_pForceTexRTV, NULL );

    // Set the new viewport
    D3D10_VIEWPORT NewVP;
    NewVP.Width = g_iTexSize;
    NewVP.Height = g_iTexSize;
    NewVP.TopLeftX = 0;
    NewVP.TopLeftY = 0;
    NewVP.MinDepth = 0.0f;
    NewVP.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &NewVP );

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pSplatVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pSplatBuffer };
    UINT stride[1] = { sizeof( SPLAT_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set Effects Parameters
    g_piTexSize->SetInt( g_iTexSize );
    g_piMaxMip->SetInt( g_iMaxMip );
    g_pfParticleMass->SetFloat( g_fParticleMass );
    g_pfG->SetFloat( g_fGConstant );
    g_pfParticleRad->SetFloat( g_fParticleRad );

    // Set resources
    g_pParticleDataTex->SetResource( g_pParticleDataTexSRVFrom );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pAccumulateForces->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pAccumulateForces->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( MAX_PARTICLES * 6, 0 );
    }

    // Restore the old vp
    pd3dDevice->RSSetViewports( 1, &OldVP );

    // Restore the old render targets
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    ID3D10ShaderResourceView* pNULLs[4] = {0,0,0,0};
    pd3dDevice->VSSetShaderResources( 0, 4, pNULLs );
    pd3dDevice->PSSetShaderResources( 0, 4, pNULLs );

    return true;
}


//--------------------------------------------------------------------------------------
bool AdvanceParticles( ID3D10Device* pd3dDevice, float fElapsedTime )
{
    // Store the old render targets
    ID3D10RenderTargetView* pOldRTV;
    ID3D10DepthStencilView* pOldDSV;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDSV );

    // Store the old viewport
    UINT iNumVP = 1;
    D3D10_VIEWPORT OldVP;
    pd3dDevice->RSGetViewports( &iNumVP, &OldVP );

    // Set the new render targets
    pd3dDevice->OMSetRenderTargets( 1, &g_pParticleDataTexRTVTo, NULL );

    // Set the new viewport
    D3D10_VIEWPORT NewVP;
    NewVP.Width = g_iTexSize;
    NewVP.Height = g_iTexSize;
    NewVP.TopLeftX = 0;
    NewVP.TopLeftY = 0;
    NewVP.MinDepth = 0.0f;
    NewVP.MaxDepth = 1.0f;
    pd3dDevice->RSSetViewports( 1, &NewVP );

    // Set the Vertex Layout
    pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pParticleBuffer };
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Set Effects Parameters
    g_piTexSize->SetInt( g_iTexSize );
    g_piMaxMip->SetInt( g_iMaxMip );
    g_pfParticleMass->SetFloat( g_fParticleMass );
    g_pfElapsedTime->SetFloat( fElapsedTime );

    // Set resources
    g_pParticleDataTex->SetResource( g_pParticleDataTexSRVFrom );
    g_pForceTex->SetResource( g_pForceTexSRV );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pAdvanceParticles->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pAdvanceParticles->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 1, 0 );
    }

    // Restore the old vp
    pd3dDevice->RSSetViewports( 1, &OldVP );

    // Restore the old render targets
    pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDSV );
    SAFE_RELEASE( pOldRTV );
    SAFE_RELEASE( pOldDSV );

    ID3D10ShaderResourceView* pNULLs[4] = {0,0,0,0};
    pd3dDevice->VSSetShaderResources( 0, 4, pNULLs );
    pd3dDevice->PSSetShaderResources( 0, 4, pNULLs );

    return true;
}

//--------------------------------------------------------------------------------------
void RenderMeshInstanced( ID3D10Device* pd3dDevice,
                          CDXUTSDKMesh* pMesh,
                          ID3D10EffectTechnique* pTechnique,
                          D3DXMATRIX mView,
                          D3DXMATRIX mProj )
{
    ID3D10Buffer* pVB[1];
    UINT Strides[1];
    UINT Offsets[1] = {0};

    pd3dDevice->IASetInputLayout( g_pMeshVertexLayout );
    // Render the mesh instanced
    pVB[0] = pMesh->GetVB10( 0, 0 );
    Strides[0] = ( UINT )pMesh->GetVertexStride( 0, 0 );

    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( pMesh->GetIB10( 0 ), pMesh->GetIBFormat10( 0 ), 0 );

    // Set Effects Parameters
    D3DXMATRIX mWorldViewProj;
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_piTexSize->SetInt( g_iTexSize );
    g_piMaxMip->SetInt( g_iMaxMip );
    g_pfBoidScale->SetFloat( g_fBoidScale );

    // Set resources
    g_pParticleDataTex->SetResource( g_pParticleDataTexSRVFrom );

    SDKMESH_SUBSET* pSubset = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < pMesh->GetNumSubsets( 0 ); ++subset )
        {
            pSubset = pMesh->GetSubset( 0, subset );

            PrimType = pMesh->GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, MAX_PARTICLES,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

}

//--------------------------------------------------------------------------------------
void SwapParticleTextures()
{
    ID3D10Texture2D* Temp1 = g_pParticleDataTextureTo;
    ID3D10ShaderResourceView* Temp2 = g_pParticleDataTexSRVTo;
    ID3D10RenderTargetView* Temp3 = g_pParticleDataTexRTVTo;

    g_pParticleDataTextureTo = g_pParticleDataTextureFrom;
    g_pParticleDataTexSRVTo = g_pParticleDataTexSRVFrom;
    g_pParticleDataTexRTVTo = g_pParticleDataTexRTVFrom;

    g_pParticleDataTextureFrom = Temp1;
    g_pParticleDataTexSRVFrom = Temp2;
    g_pParticleDataTexRTVFrom = Temp3;
}

//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 1.0f, 1.0f, 1.0f, 0.0 };

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    g_pvFleePos->SetFloatVector( ( float* )g_vFleePos );

    // Average the velocities and positions
    pd3dDevice->GenerateMips( g_pParticleDataTexSRVFrom );

    // Accumulate forces
    if( g_bAccumulateWithGS )
        AccumulateForcesGS( pd3dDevice );
    else
        AccumulateForces( pd3dDevice );

    // Apply forces to particles
    AdvanceParticles( pd3dDevice, fElapsedTime );

    // Swap the ping-pong buffers
    SwapParticleTextures();

    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    // Get the projection & view matrix from the camera class
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    // Render the boids
    RenderMeshInstanced( pd3dDevice,
                         &g_BoidMesh,
                         g_pRenderBoids,
                         mView,
                         mProj );

    // render the mesh
    pd3dDevice->IASetInputLayout( g_pMeshVertexLayout );
    D3DXMATRIX mWorld;
    D3DXMATRIX mWorldViewProj;
    D3DXMatrixTranslation( &mWorld, g_vFleePos.x, g_vFleePos.y, g_vFleePos.z );
    D3DXMATRIX mScale;
    float fScale = 50.0f;
    D3DXMatrixScaling( &mScale, fScale, fScale, fScale );
    mWorldViewProj = mScale * mWorld * mView * mProj;
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_FleeMesh.Render( pd3dDevice, g_pRenderMesh, g_pDiffuseTex );

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
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.5f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( L"Use A,D,S,W,Q,E keys to move the ball" );
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
    SAFE_RELEASE( g_pParticleVertexLayout );
    SAFE_RELEASE( g_pSplatVertexLayout );
    SAFE_RELEASE( g_pMeshVertexLayout );
    SAFE_RELEASE( g_pParticleBuffer );
    SAFE_RELEASE( g_pSplatBuffer );

    SAFE_RELEASE( g_pParticleDataTextureTo );
    SAFE_RELEASE( g_pParticleDataTexSRVTo );
    SAFE_RELEASE( g_pParticleDataTexRTVTo );
    SAFE_RELEASE( g_pParticleDataTextureFrom );
    SAFE_RELEASE( g_pParticleDataTexSRVFrom );
    SAFE_RELEASE( g_pParticleDataTexRTVFrom );
    SAFE_RELEASE( g_pForceTexture );
    SAFE_RELEASE( g_pForceTexSRV );
    SAFE_RELEASE( g_pForceTexRTV );

    g_BoidMesh.Destroy();
    g_FleeMesh.Destroy();
}


//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = ( float )( ( rand() % 10000 ) - 5000 );
    return ret / 5000.0f;
}


//--------------------------------------------------------------------------------------
// This helper function loads a group of particles
//--------------------------------------------------------------------------------------
void LoadParticles( D3DXVECTOR4* pPositions, D3DXVECTOR4* pVelocities, D3DXVECTOR4* pUps,
                    D3DXVECTOR3 Center, D3DXVECTOR4 Velocity, float Spread, UINT NumParticles )
{
    for( UINT i = 0; i < NumParticles; i++ )
    {
        D3DXVECTOR3 Delta( Spread, Spread, Spread );

        while( D3DXVec3LengthSq( &Delta ) > Spread * Spread )
        {
            Delta.x = RPercent() * Spread;
            Delta.y = RPercent() * Spread;
            Delta.z = RPercent() * Spread;
        }

        pPositions[i].x = Center.x + Delta.x;
        pPositions[i].y = Center.y + Delta.y;
        pPositions[i].z = Center.z + Delta.z;
        pPositions[i].w = 1.0f;

        pVelocities[i] = Velocity;

        pUps[i] = D3DXVECTOR4( 0, 1, 0, 1 );
    }
}

//--------------------------------------------------------------------------------------
// This helper function creates the texture array that will store all of the particle
// data.  Position, Velocity.
//--------------------------------------------------------------------------------------
HRESULT CreateParticleTextures( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    UINT SideSize = ( UINT )sqrtf( MAX_PARTICLES ) + 1;
    g_iTexSize = SideSize;
    UINT ArraySize = 3;

    // Create the texture
    UINT MipLevels = ( UINT )( ( logf( ( float )SideSize ) / logf( 2 ) ) + 1 );
    g_iMaxMip = MipLevels - 1;
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = SideSize;
    dstex.Height = SideSize;
    dstex.MipLevels = MipLevels;
    dstex.ArraySize = ArraySize;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;

    // Initialize the data in the buffers
    D3DXVECTOR4** ppData = new D3DXVECTOR4*[3 * MipLevels];
    UINT subResource = 0;
    for( UINT i = 0; i < 3; i++ )
    {
        UINT Size = SideSize;
        for( UINT m = 0; m < MipLevels; m++ )
        {
            UINT MaxTextureParticles = Size * Size;

            ppData[subResource] = new D3DXVECTOR4[ MaxTextureParticles ];
            if( !ppData[subResource] )
                return E_OUTOFMEMORY;

            subResource ++;
            Size = Size >> 1;
        }
    }

    srand( timeGetTime() );

    UINT MaxTextureParticles = SideSize * SideSize;
    // Two clumps
    LoadParticles( ppData[0], ppData[1 * MipLevels], ppData[2 * MipLevels],
                   D3DXVECTOR3( g_fSpread, 0, 0 ), D3DXVECTOR4( 0, 0, -1, 0 ),
                   g_fSpread, MaxTextureParticles / 2 );
    LoadParticles( &ppData[0][MaxTextureParticles / 2], &ppData[1 * MipLevels][MaxTextureParticles / 2], &ppData[2 *
                   MipLevels][MaxTextureParticles / 2],
                   D3DXVECTOR3( -g_fSpread, 0, 0 ), D3DXVECTOR4( 0, 0, 1, 0 ),
                   g_fSpread, MaxTextureParticles / 2 );

    D3D10_SUBRESOURCE_DATA* pInitData = new D3D10_SUBRESOURCE_DATA[3 * MipLevels];

    subResource = 0;
    for( UINT i = 0; i < 3; i++ )
    {
        UINT Size = SideSize;
        for( UINT m = 0; m < MipLevels; m++ )
        {
            pInitData[subResource].pSysMem = ppData[subResource];
            pInitData[subResource].SysMemPitch = sizeof( D3DXVECTOR4 ) * Size;
            pInitData[subResource].SysMemSlicePitch = sizeof( D3DXVECTOR4 ) * Size * Size;

            Size = Size >> 1;
            subResource ++;
        }
    }

    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, pInitData, &g_pParticleDataTextureTo ) );
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, pInitData, &g_pParticleDataTextureFrom ) );
    for( UINT i = 0; i < 3 * MipLevels; i++ )
        SAFE_DELETE_ARRAY( ppData[i] );
    SAFE_DELETE_ARRAY( ppData );
    SAFE_DELETE_ARRAY( pInitData );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.ArraySize = ArraySize;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.MipLevels = MipLevels;
    SRVDesc.Texture2DArray.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pParticleDataTextureTo, &SRVDesc, &g_pParticleDataTexSRVTo ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pParticleDataTextureFrom, &SRVDesc,
                                                    &g_pParticleDataTexSRVFrom ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    ZeroMemory( &RTVDesc, sizeof( RTVDesc ) );
    RTVDesc.Format = dstex.Format;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
    RTVDesc.Texture2DArray.ArraySize = ArraySize;
    RTVDesc.Texture2DArray.FirstArraySlice = 0;
    RTVDesc.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pParticleDataTextureTo, &RTVDesc, &g_pParticleDataTexRTVTo ) );
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pParticleDataTextureFrom, &RTVDesc, &g_pParticleDataTexRTVFrom ) );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CreateForceTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    UINT SideSize = ( UINT )sqrtf( MAX_PARTICLES ) + 1;
    g_iTexSize = SideSize;
    UINT ArraySize = 1;

    // Create the texture
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = SideSize;
    dstex.Height = SideSize;
    dstex.MipLevels = 1;
    dstex.ArraySize = ArraySize;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;

    // Initialize the data in the buffers
    UINT MaxTextureParticles = SideSize * SideSize;
    D3DXVECTOR4* pData = new D3DXVECTOR4[ MaxTextureParticles ];
    if( !pData )
        return E_OUTOFMEMORY;

    for( UINT i = 0; i < MaxTextureParticles; i++ )
    {
        pData[i] = D3DXVECTOR4( 0, 0, 0, 1 );
    }
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pData;
    InitData.SysMemPitch = sizeof( D3DXVECTOR4 ) * SideSize;
    InitData.SysMemSlicePitch = sizeof( D3DXVECTOR4 ) * MaxTextureParticles;
    V_RETURN( pd3dDevice->CreateTexture2D( &dstex, &InitData, &g_pForceTexture ) );
    SAFE_DELETE_ARRAY( pData );

    // Create the resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2DArray.ArraySize = ArraySize;
    SRVDesc.Texture2DArray.FirstArraySlice = 0;
    SRVDesc.Texture2DArray.MipLevels = 1;
    SRVDesc.Texture2DArray.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pForceTexture, &SRVDesc, &g_pForceTexSRV ) );

    // Create the render target view
    D3D10_RENDER_TARGET_VIEW_DESC RTVDesc;
    ZeroMemory( &RTVDesc, sizeof( RTVDesc ) );
    RTVDesc.Format = dstex.Format;
    RTVDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2DArray.ArraySize = ArraySize;
    RTVDesc.Texture2DArray.FirstArraySlice = 0;
    RTVDesc.Texture2DArray.MipSlice = 0;
    V_RETURN( pd3dDevice->CreateRenderTargetView( g_pForceTexture, &RTVDesc, &g_pForceTexRTV ) );

    return hr;
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

    PARTICLE_VERTEX* pVertices = new PARTICLE_VERTEX[ MAX_PARTICLES ];
    if( !pVertices )
        return E_OUTOFMEMORY;

    for( UINT i = 0; i < MAX_PARTICLES; i++ )
    {
        pVertices[i].Color = D3DXVECTOR4( 1, 1, 0.2f, 1 );
    }

    vbInitData.pSysMem = pVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &vbInitData, &g_pParticleBuffer ) );
    SAFE_DELETE_ARRAY( pVertices );

    return hr;
}


HRESULT CreateSplatBuffer( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    D3D10_BUFFER_DESC vbdesc =
    {
        MAX_PARTICLES * 6 * sizeof( SPLAT_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );

    SPLAT_VERTEX* pVertices = new SPLAT_VERTEX[ MAX_PARTICLES * 6 ];
    if( !pVertices )
        return E_OUTOFMEMORY;

    UINT index = 0;
    for( UINT i = 0; i < MAX_PARTICLES; i++ )
    {
        pVertices[index].Pos = D3DXVECTOR3( -1, 1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 0, 0 ); index++;
        pVertices[index].Pos = D3DXVECTOR3( 1, 1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 1, 0 ); index++;
        pVertices[index].Pos = D3DXVECTOR3( -1, -1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 0, 1 ); index++;

        pVertices[index].Pos = D3DXVECTOR3( -1, -1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 0, 1 ); index++;
        pVertices[index].Pos = D3DXVECTOR3( 1, 1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 1, 0 ); index++;
        pVertices[index].Pos = D3DXVECTOR3( 1, -1, 0 );
        pVertices[index].Tex = D3DXVECTOR2( 1, 1 ); index++;

    }

    vbInitData.pSysMem = pVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &vbInitData, &g_pSplatBuffer ) );
    SAFE_DELETE_ARRAY( pVertices );

    return hr;
}
