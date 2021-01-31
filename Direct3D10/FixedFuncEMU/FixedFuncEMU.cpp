//--------------------------------------------------------------------------------------
// File: FixedFuncEMU.cpp
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

#define FOGMODE_NONE    0
#define FOGMODE_LINEAR  1
#define FOGMODE_EXP     2
#define FOGMODE_EXP2    3
#define DEG2RAD( a ) ( a * D3DX_PI / 180.f )
#define MAX_BALLS 10

struct SCENE_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR2 tex;
};

struct SCENE_LIGHT
{
    D3DXVECTOR4 Position;
    D3DXVECTOR4 Diffuse;
    D3DXVECTOR4 Specular;
    D3DXVECTOR4 Ambient;
    D3DXVECTOR4 Atten;
};

struct BALL
{
    double dStartTime;
    D3DXMATRIX mWorld;
    D3DXVECTOR3 velStart;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
D3DXMATRIX                          g_mBlackHole;
D3DXMATRIX                          g_mLightView;
D3DXMATRIX                          g_mLightProj;
SCENE_LIGHT g_lights[8];
D3DXVECTOR4 g_clipplanes[3];
BALL g_balls[ MAX_BALLS ];
double                              g_fLaunchInterval = 0.3f;
float                               g_fRotateSpeed = 70.0f;

ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pVertexLayout = NULL;
ID3D10Buffer*                       g_pScreenQuadVB = NULL;

ID3D10ShaderResourceView*           g_pScreenTexRV = NULL;
ID3D10ShaderResourceView*           g_pProjectedTexRV = NULL;

CDXUTSDKMesh                        g_ballMesh;
CDXUTSDKMesh                        g_roomMesh;
CDXUTSDKMesh                        g_holeMesh;

ID3D10EffectTechnique*              g_pRenderSceneGouraudTech = NULL;
ID3D10EffectTechnique*              g_pRenderSceneFlatTech = NULL;
ID3D10EffectTechnique*              g_pRenderScenePointTech = NULL;
ID3D10EffectTechnique*              g_pRenderScreenSpaceAlphaTestTech = NULL;

ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmView = NULL;
ID3D10EffectMatrixVariable*         g_pmProj = NULL;
ID3D10EffectMatrixVariable*         g_pmInvProj = NULL;
ID3D10EffectMatrixVariable*         g_pmLightViewProj = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectShaderResourceVariable* g_pProjectedTex = NULL;
ID3D10EffectVariable*               g_pSceneLights = NULL;
ID3D10EffectVectorVariable*         g_pClipPlanes = NULL;
ID3D10EffectScalarVariable*         g_pViewportHeight = NULL;
ID3D10EffectScalarVariable*         g_pViewportWidth = NULL;
ID3D10EffectScalarVariable*         g_pNearPlane = NULL;
ID3D10EffectScalarVariable*         g_pPointSize = NULL;
ID3D10EffectScalarVariable*         g_pEnableLighting = NULL;
ID3D10EffectScalarVariable*         g_pEnableClipping = NULL;
ID3D10EffectScalarVariable*         g_pFogMode = NULL;
ID3D10EffectScalarVariable*         g_pFogStart = NULL;
ID3D10EffectScalarVariable*         g_pFogEnd = NULL;
ID3D10EffectScalarVariable*         g_pFogDensity = NULL;
ID3D10EffectVectorVariable*         g_pFogColor = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWARP          5

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
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void InitApp();
void RenderText();


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
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10SwapChainReleasing );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"FixedFuncEMU" );
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
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    g_HUD.SetCallback( OnGUIEvent ); iY = 10;
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
    g_Camera.FrameMove( fElapsedTime );

    float fBlackHoleRads = ( float )fTime * DEG2RAD( g_fRotateSpeed );
    D3DXMatrixRotationY( &g_mBlackHole, fBlackHoleRads );

    // Rotate the clip planes to align with the black holes
    g_clipplanes[0] = D3DXVECTOR4( 0, 1.0f, 0, -0.8f );
    D3DXVECTOR3 v3Plane1( 0.707f, 0.707f, 0 );
    D3DXVECTOR3 v3Plane2( -0.707f, 0.707f, 0 );
    D3DXVec3TransformNormal( ( D3DXVECTOR3* )&g_clipplanes[1], &v3Plane1, &g_mBlackHole );
    D3DXVec3TransformNormal( ( D3DXVECTOR3* )&g_clipplanes[2], &v3Plane2, &g_mBlackHole );
    g_clipplanes[1].w = 0.70f;
    g_clipplanes[2].w = 0.70f;
    g_pClipPlanes->SetFloatVectorArray( ( float* )g_clipplanes, 0, 3 );

    D3DXVECTOR3 ballLaunch( 2.1f, 8.1f, 0 );
    D3DXVECTOR3 ballStart( 0,0.45f,0 );
    D3DXVECTOR3 ballGravity( 0,-9.8f, 0 );
    D3DXVECTOR3 ballNow;

    float fBall_Life = 3.05f / ballLaunch.x;

    // Move existing balls
    for( int i = 0; i < MAX_BALLS; i++ )
    {
        float T = ( float )( fTime - g_balls[i].dStartTime );
        if( T < fBall_Life + 0.5f ) // Live 1/2 second longer to fully show off clipping
        {
            // Use the equation X = Xo + VoT + 1/2AT^2
            ballNow = ballStart + g_balls[i].velStart * T + 0.5f * ballGravity * T * T;

            // Create a world matrix
            D3DXMatrixTranslation( &g_balls[i].mWorld, ballNow.x, ballNow.y, ballNow.z );
        }
        else
        {
            g_balls[i].dStartTime = -1.0;
        }
    }

    // Launch a ball if it's time
    D3DXMATRIX wLaunchMatrix;
    bool bFound = false;
    static double dLastLaunch = -g_fLaunchInterval - 1;
    if( ( fTime - dLastLaunch ) > g_fLaunchInterval )
    {
        for( int i = 0; i < MAX_BALLS && !bFound; i++ )
        {
            if( g_balls[i].dStartTime < 0.0 )
            {
                // Found a free ball
                g_balls[i].dStartTime = fTime;
                D3DXMatrixRotationY( &wLaunchMatrix, ( i % 2 ) * DEG2RAD(180.0f) + fBlackHoleRads +
                                     DEG2RAD( fBall_Life*g_fRotateSpeed ) );
                D3DXVec3TransformNormal( &g_balls[i].velStart, &ballLaunch, &wLaunchMatrix );
                D3DXMatrixTranslation( &g_balls[i].mWorld, ballStart.x, ballStart.y, ballStart.z );
                bFound = true;
            }
        }
        dLastLaunch = fTime;
    }

    // Rotate the cookie matrix
    D3DXMATRIX mLightRot;
    D3DXMatrixRotationY( &mLightRot, DEG2RAD( 50.0f ) * ( float )fTime );
    D3DXVECTOR3 vLightEye( 0, 5.65f, 0 );
    D3DXVECTOR3 vLightAt( 0, 0, 0 );
    D3DXVECTOR3 vUp( 0,0,1 );
    D3DXMatrixLookAtLH( &g_mLightView, &vLightEye, &vLightAt, &vUp );
    g_mLightView = mLightRot * g_mLightView;
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
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
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"FixedFuncEMU.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderSceneGouraudTech = g_pEffect10->GetTechniqueByName( "RenderSceneGouraud" );
    g_pRenderSceneFlatTech = g_pEffect10->GetTechniqueByName( "RenderSceneFlat" );
    g_pRenderScenePointTech = g_pEffect10->GetTechniqueByName( "RenderScenePoint" );
    g_pRenderScreenSpaceAlphaTestTech = g_pEffect10->GetTechniqueByName( "RenderScreenSpaceAlphaTest" );

    // Obtain the parameter handles
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmView = g_pEffect10->GetVariableByName( "g_mView" )->AsMatrix();
    g_pmProj = g_pEffect10->GetVariableByName( "g_mProj" )->AsMatrix();
    g_pmInvProj = g_pEffect10->GetVariableByName( "g_mInvProj" )->AsMatrix();
    g_pmLightViewProj = g_pEffect10->GetVariableByName( "g_mLightViewProj" )->AsMatrix();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pProjectedTex = g_pEffect10->GetVariableByName( "g_txProjected" )->AsShaderResource();
    g_pSceneLights = g_pEffect10->GetVariableByName( "g_lights" );
    g_pClipPlanes = g_pEffect10->GetVariableByName( "g_clipplanes" )->AsVector();
    g_pViewportHeight = g_pEffect10->GetVariableByName( "g_viewportHeight" )->AsScalar();
    g_pViewportWidth = g_pEffect10->GetVariableByName( "g_viewportWidth" )->AsScalar();
    g_pNearPlane = g_pEffect10->GetVariableByName( "g_nearPlane" )->AsScalar();
    g_pPointSize = g_pEffect10->GetVariableByName( "g_pointSize" )->AsScalar();
    g_pEnableLighting = g_pEffect10->GetVariableByName( "g_bEnableLighting" )->AsScalar();
    g_pEnableClipping = g_pEffect10->GetVariableByName( "g_bEnableClipping" )->AsScalar();
    g_pFogMode = g_pEffect10->GetVariableByName( "g_fogMode" )->AsScalar();
    g_pFogStart = g_pEffect10->GetVariableByName( "g_fogStart" )->AsScalar();
    g_pFogEnd = g_pEffect10->GetVariableByName( "g_fogEnd" )->AsScalar();
    g_pFogDensity = g_pEffect10->GetVariableByName( "g_fogDensity" )->AsScalar();
    g_pFogColor = g_pEffect10->GetVariableByName( "g_fogColor" )->AsVector();

    //set constant variables
    g_pPointSize->SetFloat( 3.0f );
    g_pFogMode->SetInt( FOGMODE_LINEAR );
    g_pFogStart->SetFloat( 12.0f );
    g_pFogEnd->SetFloat( 22.0f );
    g_pFogDensity->SetFloat( 0.05f );
    D3DXVECTOR4 v4FogColor( 0.7f,1.0f,1.0f,1 );
    g_pFogColor->SetFloatVector( ( float* )&v4FogColor );

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pRenderSceneGouraudTech->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Load the meshes
    V_RETURN( g_ballMesh.Create( pd3dDevice, L"misc\\ball.sdkmesh", true ) );
    V_RETURN( g_roomMesh.Create( pd3dDevice, L"BlackHoleRoom\\BlackHoleRoom.sdkmesh", true ) );
    V_RETURN( g_holeMesh.Create( pd3dDevice, L"BlackHoleRoom\\BlackHole.sdkmesh", true ) );
    D3DXMatrixIdentity( &g_mBlackHole );

    // Initialize the balls
    for( int i = 0; i < MAX_BALLS; i++ )
        g_balls[i].dStartTime = -1.0;

    // Setup the Lights
    ZeroMemory( g_lights, 8 * sizeof( SCENE_LIGHT ) );
    int iLight = 0;
    for( int y = 0; y < 3; y++ )
    {
        for( int x = 0; x < 3; x++ )
        {
            if( x != 1 || y != 1 )
            {
                g_lights[iLight].Position = D3DXVECTOR4( 3.0f * ( -1.0f + x ), 5.65f, 5.0f * ( -1.0f + y ), 1 );
                if( 0 == iLight % 2 )
                {
                    g_lights[iLight].Diffuse = D3DXVECTOR4( 0.20f, 0.20f, 0.20f, 1.0f );
                    g_lights[iLight].Specular = D3DXVECTOR4( 0.5f, 0.5f, 0.5f, 1.0f );
                    g_lights[iLight].Ambient = D3DXVECTOR4( 0.03f, 0.03f, 0.03f, 0.0f );
                }
                else
                {
                    g_lights[iLight].Diffuse = D3DXVECTOR4( 0.0f, 0.15f, 0.20f, 1.0f );
                    g_lights[iLight].Specular = D3DXVECTOR4( 0.15f, 0.25f, 0.3f, 1.0f );
                    g_lights[iLight].Ambient = D3DXVECTOR4( 0.00f, 0.02f, 0.03f, 0.0f );
                }
                g_lights[iLight].Atten.x = 1.0f;

                iLight ++;
            }
        }
    }
    g_pSceneLights->SetRawValue( g_lights, 0, 8 * sizeof( SCENE_LIGHT ) );

    D3DXMatrixPerspectiveFovLH( &g_mLightProj, DEG2RAD(90.0f), 1.0f, 0.1f, 100.0f );

    // Create the screenspace quad VB
    // This gets initialized in OnD3D10SwapChainResized
    D3D10_BUFFER_DESC vbdesc =
    {
        4 * sizeof( SCENE_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pScreenQuadVB ) );

    // Load the HUD and Cookie Textures
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\hud.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pScreenTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\cookie.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pProjectedTexRV, NULL ) );
    g_pProjectedTex->SetResource( g_pProjectedTexRV );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 2.3f, -8.5f );
    D3DXVECTOR3 vecAt ( 0.0f, 2.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( 9.0f, 1.0f, 15.0f );

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

    float fWidth = static_cast< float >( pBackBufferSurfaceDesc->Width );
    float fHeight = static_cast< float >( pBackBufferSurfaceDesc->Height );

    // Set the viewport width/height
    g_pViewportWidth->SetFloat( fWidth );
    g_pViewportHeight->SetFloat( fHeight );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 100.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_pNearPlane->SetFloat( 0.1f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    // Update our Screen-space quad
    SCENE_VERTEX aVerts[] =
    {
        { D3DXVECTOR3( 0, 0, 0.5 ), D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR2( 0, 0 ) },
        { D3DXVECTOR3( fWidth, 0, 0.5 ), D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR2( 1, 0 ) },
        { D3DXVECTOR3( 0, fHeight, 0.5 ), D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR2( 0, 1 ) },
        { D3DXVECTOR3( fWidth, fHeight, 0.5 ), D3DXVECTOR3( 0, 0, 0 ), D3DXVECTOR2( 1, 1 ) },
    };

    pd3dDevice->UpdateSubresource( g_pScreenQuadVB, 0, NULL, aVerts, 0, 0 );

    return hr;
}


//--------------------------------------------------------------------------------------
// Render a screen quad
//--------------------------------------------------------------------------------------
void RenderScreenQuad( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique )
{
    UINT uStride = sizeof( SCENE_VERTEX );
    UINT offsets = 0;
    ID3D10Buffer* pBuffers[1] = { g_pScreenQuadVB };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, &uStride, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    g_pDiffuseTex->SetResource( g_pScreenTexRV );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Setup the view matrices
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mInvProj;
    D3DXMATRIX mLightViewProj;
    D3DXMatrixIdentity( &mWorld );
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();

    mLightViewProj = g_mLightView * g_mLightProj;
    g_pmLightViewProj->SetMatrix( ( float* )&mLightViewProj );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    g_pmView->SetMatrix( ( float* )&mView );
    g_pmProj->SetMatrix( ( float* )&mProj );
    D3DXMatrixInverse( &mInvProj, NULL, &mProj );
    g_pmInvProj->SetMatrix( ( float* )&mInvProj );

    // Render the room and the blackholes
    g_pEnableClipping->SetBool( false );
    g_pEnableLighting->SetBool( false );
    g_roomMesh.Render( pd3dDevice, g_pRenderSceneGouraudTech, g_pDiffuseTex );
    g_pmWorld->SetMatrix( ( float* )&g_mBlackHole );
    g_holeMesh.Render( pd3dDevice, g_pRenderSceneGouraudTech, g_pDiffuseTex );

    // Render the balls
    g_pEnableClipping->SetBool( true );
    g_pEnableLighting->SetBool( true );
    for( int i = 0; i < MAX_BALLS; i++ )
    {
        if( g_balls[i].dStartTime > -1.0 )
        {
            g_pmWorld->SetMatrix( ( float* )&g_balls[i].mWorld );

            if( i % 3 == 0 )
                g_ballMesh.Render( pd3dDevice, g_pRenderSceneGouraudTech, g_pDiffuseTex );
            else if( i % 3 == 1 )
                g_ballMesh.Render( pd3dDevice, g_pRenderSceneFlatTech, g_pDiffuseTex );
            else
                g_ballMesh.Render( pd3dDevice, g_pRenderScenePointTech, g_pDiffuseTex );
        }
    }

    g_pEnableClipping->SetBool( false );
    g_pEnableLighting->SetBool( false );
    RenderScreenQuad( pd3dDevice, g_pRenderScreenSpaceAlphaTestTech );

    // Render the HUD
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
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pScreenTexRV );
    SAFE_RELEASE( g_pProjectedTexRV );
    SAFE_RELEASE( g_pScreenQuadVB );
    g_ballMesh.Destroy();
    g_roomMesh.Destroy();
    g_holeMesh.Destroy();
}



