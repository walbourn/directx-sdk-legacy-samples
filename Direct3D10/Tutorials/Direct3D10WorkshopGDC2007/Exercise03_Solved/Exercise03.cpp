//--------------------------------------------------------------------------------------
// Exercise03.cpp
// Direct3D 10 Shader Model 4.0 Workshop
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include "AudioData.h"

#define MAX_SPRITES 200
#define SWAP( a, b ) { UINT _temp = a; a = b; b = _temp; }
#define MAX_PARTICLES 60000

//--------------------------------------------------------------------------------------
// Vertex Structures
//--------------------------------------------------------------------------------------
struct QUAD_VERTEX
{
    D3DXVECTOR3 pos;
    UINT texx;
    UINT texy;
};

struct SCREENQUAD_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR2 tex;
};

struct STREAM_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    D3DXVECTOR2 tex;
    D3DXVECTOR3 vel;
    UINT bucket;
    UINT type;
    float life;
};

struct PARTICLE_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 vel;
    D3DXVECTOR4 color;
    float Timer;
    UINT Type;
    UINT Bucket;
};

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
// Spectrogram specific globals
UINT                                g_iNumInstances = 128;
UINT                                g_uiTexX = 256;         // This must always be less than 65536
UINT                                g_uiTexY = 1;
UINT                                g_uiFloorX = 1024;
UINT                                g_uiFloorY = 1024;
UINT                                g_uiScreenX = 0;
UINT                                g_uiScreenY = 0;
float                               g_fSpectrogramSize = 100.0f;

ID3D10Effect*                       g_pSpectrogramEffect10 = NULL;
ID3D10InputLayout*                  g_pQuadVertexLayout = NULL;
ID3D10Buffer*                       g_pQuadVB;

ID3D10Texture2D*                    g_pSourceTexture;
ID3D10ShaderResourceView*           g_pSourceTexRV;
ID3D10RenderTargetView*             g_pSourceRTV;
ID3D10Texture2D*                    g_pDestTexture;
ID3D10ShaderResourceView*           g_pDestTexRV;
ID3D10RenderTargetView*             g_pDestRTV;

ID3D10EffectTechnique*              g_pReverse;
ID3D10EffectTechnique*              g_pFFTInner;
ID3D10EffectTechnique*              g_pRenderQuad;
ID3D10EffectShaderResourceVariable* g_ptxSource;
ID3D10EffectVectorVariable*         g_pTextureSize;

ID3D10EffectScalarVariable*         g_pWR;
ID3D10EffectScalarVariable*         g_pWI;
ID3D10EffectScalarVariable*         g_pMMAX;
ID3D10EffectScalarVariable*         g_pM;
ID3D10EffectScalarVariable*         g_pISTEP;

CAudioData                          g_audioData;
HWAVEOUT                            g_hWaveOut;
WAVEHDR                             g_WaveHeader;
bool                                g_bPlaybackStarted = false;
bool                                g_bDrawFromStream = true;
bool                                g_bLine = true;
bool                                g_bRenderPCMData = false;
bool                                g_bScreenEffects = false;
bool                                g_bFireworks = false;
float                               g_fNormalization = 0.85f;
float                               g_fScale = 10.0f;
float                               g_fEffectLimit = 0.6f;
float                               g_fEffectLife = 0.75f;
float                               g_fSampleFrequency = 0.0001f;	// as fast as we can
float                               g_fLastSample = 0.0f;
bool                                g_bRunFFTThisFrame = false;
D3DXVECTOR2*                        g_pvPCMData = NULL;

// particle specific variables
ID3D10Effect*                       g_pParticleEffect = NULL;
ID3D10InputLayout*                  g_pParticleVertexLayout = NULL;
ID3D10Buffer*                       g_pParticleStart = NULL;
ID3D10Buffer*                       g_pParticleStreamTo = NULL;
ID3D10Buffer*                       g_pParticleDrawFrom = NULL;
ID3D10ShaderResourceView*           g_pParticleTexRV = NULL;
ID3D10Texture1D*                    g_pRandomTexture = NULL;
ID3D10ShaderResourceView*           g_pRandomTexRV = NULL;

ID3D10EffectTechnique*              g_pRenderParticles;
ID3D10EffectTechnique*              g_pAdvanceParticles;
ID3D10EffectMatrixVariable*         g_pmParticleWorldViewProj;
ID3D10EffectMatrixVariable*         g_pmInvView;
ID3D10EffectScalarVariable*         g_pfGlobalTime;
ID3D10EffectScalarVariable*         g_pfParticleElapsedTime;
ID3D10EffectShaderResourceVariable* g_pParticleDiffuseTex;
ID3D10EffectShaderResourceVariable* g_pRandomTex;
ID3D10EffectShaderResourceVariable* g_pParticleSpectrogramTex;
ID3D10EffectShaderResourceVariable* g_pParticleGradientTex;
ID3D10EffectScalarVariable*         g_pSecondsPerFirework;
ID3D10EffectScalarVariable*         g_pNumEmber1s;
ID3D10EffectScalarVariable*         g_pMaxEmber2s;
ID3D10EffectVectorVariable*         g_pvFrameGravity;
ID3D10EffectScalarVariable*         g_pfParticleNumInstances;
ID3D10EffectScalarVariable*         g_pfParticleEffectLimit;
ID3D10EffectScalarVariable*         g_pfParticleNormalization;

D3DXVECTOR3                         g_vParticleStartVelocity( 0,15.0f,0 );
bool                                g_bParticleFirst = true;

//--------------------------------------------------------------------------------------
// DXUT Specific variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = NULL;

//--------------------------------------------------------------------------------------
// Button and Text Rendering Variables
//--------------------------------------------------------------------------------------
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;

//--------------------------------------------------------------------------------------
// Mesh Specific Variables
//--------------------------------------------------------------------------------------
CDXUTSDKMesh g_Mesh[2];
ID3D10InputLayout*                  g_pInstVertexLayout = NULL;
ID3D10InputLayout*                  g_pMeshLayout = NULL;
UINT                                g_SizeVB = 0;
UINT                                g_NumIndices = 0;
UINT                                g_VertStride = 0;
ID3D10ShaderResourceView*           g_pMeshTexRV = NULL;
ID3D10Buffer*                       g_pInstanceBuffer = NULL;
ID3D10ShaderResourceView*           g_pGradientTexRV = NULL;
ID3D10Texture2D*                    g_pBobbleTex = NULL;
ID3D10ShaderResourceView*           g_pBobbleTexRV = NULL;

//--------------------------------------------------------------------------------------
// Render Target Variables
//--------------------------------------------------------------------------------------
ID3D10Texture2D*                    g_pFloorRT[2] = { NULL, NULL };
ID3D10RenderTargetView*             g_pFloorRTV[2] = { NULL, NULL };
ID3D10ShaderResourceView*           g_pFloorSRV[2] = { NULL, NULL };
ID3D10Texture2D*                    g_pScreenRT[2] = { NULL, NULL };
ID3D10RenderTargetView*             g_pScreenRTV[2] = { NULL, NULL };
ID3D10ShaderResourceView*           g_pScreenSRV[2] = { NULL, NULL };
ID3D10InputLayout*                  g_pScreenQuadLayout = NULL;
ID3D10Buffer*                       g_pScreenQuadVB = NULL;
UINT                                g_uiFloorTo = 0;
UINT                                g_uiFloorFrom = 1;
UINT                                g_uiScreenTo = 0;
UINT                                g_uiScreenFrom = 1;

//--------------------------------------------------------------------------------------
// Stream Specific Variables
//--------------------------------------------------------------------------------------
ID3D10InputLayout*                  g_pStreamVertexLayout = NULL;
ID3D10Buffer*                       g_pDrawFrom = NULL;
ID3D10Buffer*                       g_pStreamTo = NULL;

//--------------------------------------------------------------------------------------
// Effect variables
//--------------------------------------------------------------------------------------
WCHAR g_strEffect[MAX_PATH];
FILETIME                            g_fLastWriteTime;
double                              g_fLastFileCheck = 0.0;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10EffectTechnique*              g_pRenderInstanced = NULL;
ID3D10EffectTechnique*              g_pRenderToStream = NULL;
ID3D10EffectTechnique*              g_pRenderStream = NULL;
ID3D10EffectTechnique*              g_pHandleEffects = NULL;
ID3D10EffectTechnique*              g_pRenderMesh = NULL;
ID3D10EffectTechnique*              g_pFloorEffect = NULL;
ID3D10EffectTechnique*              g_pScreenEffect = NULL;
ID3D10EffectTechnique*              g_pCopy = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldView = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectVectorVariable*         g_pWorldLightDir = NULL;
ID3D10EffectScalarVariable*         g_pfNumInstances = NULL;
ID3D10EffectScalarVariable*         g_pfScale = NULL;
ID3D10EffectScalarVariable*         g_pfEffectLimit = NULL;
ID3D10EffectScalarVariable*         g_pfEffectLife = NULL;
ID3D10EffectScalarVariable*         g_pfElapsedTime = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectScalarVariable*         g_pfNormalization = NULL;
ID3D10EffectShaderResourceVariable* g_pArrayTex = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex = NULL;
ID3D10EffectShaderResourceVariable* g_pSpectrogramTex = NULL;
ID3D10EffectShaderResourceVariable* g_pGradientTex = NULL;

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void ClearShaderResources( ID3D10Device* pd3dDevice );
HRESULT InitSpectrogramResources( ID3D10Device* pd3dDevice );
void DestroySpectrogramResources();
void CreateSpectrogram( ID3D10Device* pd3dDevice );
HRESULT CreateQuad( ID3D10Device* pd3dDevice, UINT uiTexSizeX, UINT uiTexSizeY );
HRESULT CreateScreenQuad( ID3D10Device* pd3dDevice );
HRESULT CreateBuffers( ID3D10Device* pd3dDevice, UINT uiTexSizeX, UINT uiTexSizeY );
HRESULT CreateFloorTargets( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY );
void DestroyBuffers();
void RenderToTexture( ID3D10Device* pd3dDevice,
                      ID3D10RenderTargetView* pRTV,
                      ID3D10ShaderResourceView* pSRV,
                      bool bClear,
                      ID3D10EffectTechnique* pTechnique,
                      ID3D10EffectShaderResourceVariable* ptxSource,
                      ID3D10InputLayout* pInputLayout,
                      ID3D10Buffer* pIB,
                      UINT uStrides,
                      UINT iRTX,
                      UINT iRTY );
HRESULT OpenWaveDevice();
HRESULT CloseWaveDevice();
HRESULT StartPlayback();
HRESULT StopPlayback();
HRESULT LoadAudioData( ID3D10Device* pd3dDevice, LPCTSTR szFileName );
HRESULT UnloadAudioData();
HRESULT UpdateTextureWithAudio( ID3D10Device* pd3dDevice, unsigned long ulSampleOffset );
HRESULT CreateInstanceBuffer( ID3D10Device* pd3dDevice, bool bLine );
HRESULT CreateStreams( ID3D10Device* pd3dDevice );
HRESULT RenderInstancesToStream( ID3D10Device* pd3dDevice );
FILETIME GetFileTime( WCHAR* strFile );
HRESULT LoadEffect( ID3D10Device* pd3dDevice );
HRESULT LoadParticleEffect( ID3D10Device* pd3dDevice );
HRESULT LoadTextureArray( ID3D10Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D10Texture2D** ppTex2D, ID3D10ShaderResourceView** ppSRV );
HRESULT CreateParticleBuffer( ID3D10Device* pd3dDevice, bool bLine );
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice );

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_LOADWAVE			50
#define IDC_STARTPLAYBACK		51
#define IDC_STOPPLAYBACK		52
#define IDC_SCALE_STATIC		53
#define IDC_SCALE				54
#define IDC_EFFECT_LIMIT_STATIC 55
#define IDC_EFFECT_LIMIT		56
#define IDC_EFFECT_LIFE_STATIC  57
#define IDC_EFFECT_LIFE		    58
#define IDC_DRAW_FROM_STREAM	59
#define IDC_DRAW_LINE			60
#define IDC_DRAW_PCM			61
#define IDC_SCREEN_EFFECTS		62
#define IDC_NORMALIZE_FFT_STATIC 63
#define IDC_NORMALIZE_FFT		64
#define IDC_SAMPLE_FREQ_STATIC  65
#define IDC_SAMPLE_FREQ			66
#define IDC_FIREWORKS			67

//--------------------------------------------------------------------------------------
// Callbacks
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
    DXUTCreateWindow( L"D3D10 Shader Model 4.0 Workshop GDC2007: Exercise03" );
    DXUTCreateDevice( true, 1024, 768 );
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
    g_SampleUI.AddButton( IDC_LOADWAVE, L"Load Wave", 35, iY, 125, 22 );
    g_SampleUI.AddButton( IDC_STARTPLAYBACK, L"Start Playback", 35, iY += 24, 125, 22 );
    g_SampleUI.AddButton( IDC_STOPPLAYBACK, L"Stop Playback", 35, iY += 24, 125, 22 );

    iY += 24;
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Scaling: %.4f", g_fScale );
    g_SampleUI.AddStatic( IDC_SCALE_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_SCALE, 35, iY += 24, 135, 22, 0, 200, ( int )g_fScale );

    swprintf_s( str, MAX_PATH, L"Effect Limit: %.4f", g_fEffectLimit );
    g_SampleUI.AddStatic( IDC_EFFECT_LIMIT_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_EFFECT_LIMIT, 35, iY += 24, 135, 22, 0, 4000, ( int )( 500 * g_fEffectLimit ) );

    swprintf_s( str, MAX_PATH, L"Effect Life: %.4f", g_fEffectLife );
    g_SampleUI.AddStatic( IDC_EFFECT_LIFE_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_EFFECT_LIFE, 35, iY += 24, 135, 22, 0, 2000, ( int )( 1000 * g_fEffectLife ) );

    swprintf_s( str, MAX_PATH, L"Normalize: %.4f", g_fNormalization );
    g_SampleUI.AddStatic( IDC_NORMALIZE_FFT_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_NORMALIZE_FFT, 35, iY += 24, 135, 22, 0, 1000, ( int )( 1000 * g_fNormalization ) );

    swprintf_s( str, MAX_PATH, L"Sample Frequency: %.4f", g_fSampleFrequency );
    g_SampleUI.AddStatic( IDC_SAMPLE_FREQ_STATIC, str, 25, iY += 24, 135, 22 );
    g_SampleUI.AddSlider( IDC_SAMPLE_FREQ, 35, iY += 24, 135, 22, 0, 10000, ( int )( 5000 * g_fSampleFrequency ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_DRAW_FROM_STREAM, L"Use Stream Out", 35, iY += 24, 135, 22, g_bDrawFromStream );
    g_SampleUI.AddCheckBox( IDC_DRAW_LINE, L"Draw in a Line", 35, iY += 24, 135, 22, g_bLine );
    g_SampleUI.AddCheckBox( IDC_DRAW_PCM, L"Render Raw PCM Data", 35, iY += 24, 135, 22, g_bRenderPCMData );
    g_SampleUI.AddCheckBox( IDC_SCREEN_EFFECTS, L"Screen Effects", 35, iY += 24, 135, 22, g_bScreenEffects );
    g_SampleUI.AddCheckBox( IDC_FIREWORKS, L"Display Fireworks", 35, iY += 24, 135, 22, g_bFireworks );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d10.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Get the wave playback position
    if( g_bPlaybackStarted )
    {
        g_bRunFFTThisFrame = false;
        if( ( float )fTime - g_fLastSample > g_fSampleFrequency )
        {
            MMTIME mmtime;
            mmtime.wType = TIME_SAMPLES;
            HRESULT hr = waveOutGetPosition( g_hWaveOut, &mmtime, sizeof( MMTIME ) );
            if( S_OK == hr )
            {
                if( TIME_SAMPLES == mmtime.wType )
                {
                    UpdateTextureWithAudio( DXUTGetD3D10Device(), mmtime.u.sample );
                }
            }
            g_fLastSample = ( float )fTime;
            g_bRunFFTThisFrame = true;
        }
    }

    // check for fx file reload every 0.5 seconds
    if( fTime - g_fLastFileCheck > 0.5 )
    {
        HRESULT hr = S_OK;
        FILETIME currentTime = GetFileTime( g_strEffect );
        if( CompareFileTime( &g_fLastWriteTime, &currentTime ) )
            V( LoadEffect( DXUTGetD3D10Device() ) );
        g_fLastFileCheck = fTime;
    }

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

        case IDC_LOADWAVE:
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
            ofn.lpstrFilter = L"Wave File\0*.wav\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            BOOL bRet = GetOpenFileName( &ofn );
            SetCurrentDirectory( szCurrentDir );

            if( bRet )
            {
                StopPlayback();
                CloseWaveDevice();
                UnloadAudioData();

                LoadAudioData( DXUTGetD3D10Device(), szFile );
                OpenWaveDevice();
                CreateParticleBuffer( DXUTGetD3D10Device(), g_bLine );
            }
        }
            break;
        case IDC_STARTPLAYBACK:
            StartPlayback();
            break;
        case IDC_STOPPLAYBACK:
            StopPlayback();
            break;
        case IDC_SCALE:
        {
            g_fScale = ( float )g_SampleUI.GetSlider( IDC_SCALE )->GetValue();
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Scaling: %.4f", g_fScale );
            g_SampleUI.GetStatic( IDC_SCALE_STATIC )->SetText( str );

            g_pfScale->SetFloat( g_fScale );
        }
            break;
        case IDC_EFFECT_LIMIT:
        {
            g_fEffectLimit = ( float )g_SampleUI.GetSlider( IDC_EFFECT_LIMIT )->GetValue() / 500.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Effect Limit: %.4f", g_fEffectLimit );
            g_SampleUI.GetStatic( IDC_EFFECT_LIMIT_STATIC )->SetText( str );

            g_pfEffectLimit->SetFloat( g_fEffectLimit );
            g_pfParticleEffectLimit->SetFloat( g_fEffectLimit );
        }
            break;
        case IDC_EFFECT_LIFE:
        {
            g_fEffectLife = ( float )g_SampleUI.GetSlider( IDC_EFFECT_LIFE )->GetValue() / 1000.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Effect Life: %.4f", g_fEffectLife );
            g_SampleUI.GetStatic( IDC_EFFECT_LIFE_STATIC )->SetText( str );

            g_pfEffectLife->SetFloat( g_fEffectLife );
        }
            break;

        case IDC_DRAW_FROM_STREAM:
        {
            g_bDrawFromStream = !g_bDrawFromStream;
        }
            break;
        case IDC_DRAW_LINE:
        {
            g_bLine = !g_bLine;

            HRESULT hr = S_OK;

            V( CreateInstanceBuffer( DXUTGetD3D10Device(), g_bLine ) );
            V( CreateParticleBuffer( DXUTGetD3D10Device(), g_bLine ) );
            V( RenderInstancesToStream( DXUTGetD3D10Device() ) );
        }
            break;
        case IDC_DRAW_PCM:
        {
            g_bRenderPCMData = !g_bRenderPCMData;
        }
            break;
        case IDC_SCREEN_EFFECTS:
        {
            g_bScreenEffects = !g_bScreenEffects;
        }
            break;
        case IDC_NORMALIZE_FFT:
        {
            g_fNormalization = ( float )g_SampleUI.GetSlider( IDC_NORMALIZE_FFT )->GetValue() / 1000.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Normalize: %.4f", g_fNormalization );
            g_SampleUI.GetStatic( IDC_NORMALIZE_FFT_STATIC )->SetText( str );

            g_pfNormalization->SetFloat( g_fNormalization );
            g_pfParticleNormalization->SetFloat( g_fNormalization );
        }
            break;
        case IDC_SAMPLE_FREQ:
        {
            g_fSampleFrequency = ( float )g_SampleUI.GetSlider( IDC_SAMPLE_FREQ )->GetValue() / 5000.0f;
            WCHAR str[MAX_PATH];
            swprintf_s( str, MAX_PATH, L"Sample Frequency: %.4f", g_fSampleFrequency );
            g_SampleUI.GetStatic( IDC_SAMPLE_FREQ_STATIC )->SetText( str );
        }
            break;
        case IDC_FIREWORKS:
        {
            g_bFireworks = !g_bFireworks;
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
    V_RETURN( D3DX10CreateSprite( pd3dDevice, MAX_SPRITES, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D10CreateDevice( pd3dDevice ) );

    // Read the D3DX effect file
    V_RETURN( DXUTFindDXSDKMediaFileCch( g_strEffect, MAX_PATH, L"Exercise03.fx" ) );
    V_RETURN( LoadEffect( pd3dDevice ) );

    // Instanced data vertex layout
    const D3D10_INPUT_ELEMENT_DESC instlayout[] =
    {
        { "POS",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D10_INPUT_PER_VERTEX_DATA,    0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "mTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D10_INPUT_PER_INSTANCE_DATA, 1 },
    };
    UINT numElements = sizeof( instlayout ) / sizeof( instlayout[0] );
    D3D10_PASS_DESC PassDesc;
    g_pRenderInstanced->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( instlayout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pInstVertexLayout ) );

    // Stream data vertex layout
    const D3D10_INPUT_ELEMENT_DESC streamlayout[] =
    {
        { "POS",        0, DXGI_FORMAT_R32G32B32_FLOAT,	   0, 0,  D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "VELOCITY",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "BUCKET",     0, DXGI_FORMAT_R32_UINT,           0, 44, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TYPE",       0, DXGI_FORMAT_R32_UINT,           0, 48, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "LIFE",       0, DXGI_FORMAT_R32_FLOAT,          0, 52, D3D10_INPUT_PER_VERTEX_DATA,   0 },
    };
    numElements = sizeof( streamlayout ) / sizeof( streamlayout[0] );
    g_pHandleEffects->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( streamlayout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pStreamVertexLayout ) );

    // Mesh data vertex layout
    const D3D10_INPUT_ELEMENT_DESC meshlayout[] =
    {
        { "POS",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D10_INPUT_PER_VERTEX_DATA,   0 },
    };
    numElements = sizeof( meshlayout ) / sizeof( meshlayout[0] );
    g_pRenderMesh->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( meshlayout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pMeshLayout ) );

    // Quad data vertex layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
    };

    g_pFloorEffect->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                        PassDesc.pIAInputSignature,
                                        PassDesc.IAInputSignatureSize, &g_pScreenQuadLayout );
    if( FAILED( hr ) )
        return hr;

    // Load two meshes
    g_Mesh[0].Create( pd3dDevice, L"misc\\bobble.sdkmesh" );
    g_Mesh[1].Create( pd3dDevice, L"misc\\flat.sdkmesh" );

    // Create our resources
    WCHAR str[MAX_PATH];
    V_RETURN( CreateInstanceBuffer( pd3dDevice, g_bLine ) );
    V_RETURN( InitSpectrogramResources( pd3dDevice ) );
    V_RETURN( CreateStreams( pd3dDevice ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Wavs\\counting.wav" ) );
    V_RETURN( LoadAudioData( pd3dDevice, str ) );
    V_RETURN( OpenWaveDevice() );
    V_RETURN( RenderInstancesToStream( pd3dDevice ) );
    V_RETURN( CreateFloorTargets( pd3dDevice, g_uiFloorX, g_uiFloorY ) );
    V_RETURN( CreateScreenQuad( pd3dDevice ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\rainbow.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pGradientTexRV, NULL ) );
    V_RETURN( LoadParticleEffect( pd3dDevice ) );
    V_RETURN( CreateParticleBuffer( pd3dDevice, g_bLine ) );
    V_RETURN( CreateRandomTexture( pd3dDevice ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\Particle.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pParticleTexRV, NULL ) );

    // Load the array textures we'll be using
    LPCTSTR szBobbleTextureNames[] =
    {
        L"misc\\bobbletex.dds",
        L"misc\\bobbletex2.dds",
        L"misc\\bobbletex3.dds",
        L"misc\\bobbletex4.dds",
        L"misc\\bobbletex5.dds",
    };
    V_RETURN( LoadTextureArray( pd3dDevice,
                                szBobbleTextureNames,
                                sizeof( szBobbleTextureNames ) / sizeof( szBobbleTextureNames[0] ),
                                &g_pBobbleTex,
                                &g_pBobbleTexRV ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -100.0f );
    D3DXVECTOR3 vecAt( 0.0f,0.0f,0.0f );
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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON | MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 190, pBackBufferSurfaceDesc->Height - 500 );
    g_SampleUI.SetSize( 170, 300 );

    // screen sized render targets
    g_uiScreenX = pBackBufferSurfaceDesc->Width;
    g_uiScreenY = pBackBufferSurfaceDesc->Height;
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = g_uiScreenX;
    dstex.Height = g_uiScreenY;
    dstex.MipLevels = 1;
    dstex.Format = pBackBufferSurfaceDesc->Format;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pScreenRT[0] );
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pScreenRT[1] );
    if( FAILED( hr ) )
        return hr;

    // Create Source and Dest Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    hr = pd3dDevice->CreateShaderResourceView( g_pScreenRT[0], &SRVDesc, &g_pScreenSRV[0] );
    hr = pd3dDevice->CreateShaderResourceView( g_pScreenRT[1], &SRVDesc, &g_pScreenSRV[1] );
    if( FAILED( hr ) )
        return hr;

    // Create RenderTarget Views
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    hr = pd3dDevice->CreateRenderTargetView( g_pScreenRT[0], &DescRT, &g_pScreenRTV[0] );
    hr = pd3dDevice->CreateRenderTargetView( g_pScreenRT[1], &DescRT, &g_pScreenRTV[1] );
    if( FAILED( hr ) )
        return hr;

    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pd3dDevice->ClearRenderTargetView( g_pScreenRTV[0], ClearColor );
    pd3dDevice->ClearRenderTargetView( g_pScreenRTV[1], ClearColor );

    return hr;
}

//--------------------------------------------------------------------------------------
// Render text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetInsertionPos( 2, 40 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( L"Click any button and drag to rotate." );
    g_pTxtHelper->DrawTextLine( L"Scroll mouse wheel to zoom." );

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Render mesh from stream
//--------------------------------------------------------------------------------------
void RenderMeshFromStream( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique )
{
    ID3D10Buffer* pVB[1];
    UINT Strides[1];
    UINT Offsets[1] = {0};

    pVB[0] = g_pDrawFrom;
    Strides[0] = sizeof( STREAM_VERTEX );

    pd3dDevice->IASetInputLayout( g_pStreamVertexLayout );
    pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( NULL, g_Mesh[0].GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;

    pSubset = g_Mesh[0].GetSubset( 0, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pMat = g_Mesh[0].GetMaterial( pSubset->MaterialID );
    if( pMat )
        g_pDiffuseTex->SetResource( pMat->pDiffuseRV10 );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawAuto();
    }
}

//--------------------------------------------------------------------------------------
// Handle effects
//--------------------------------------------------------------------------------------
void HandleEffects( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique )
{
    UINT offset = 0;
    ID3D10Buffer* pVB[1] = {g_pStreamTo};
    pd3dDevice->SOSetTargets( 1, pVB, &offset );
	
    RenderMeshFromStream( pd3dDevice, pTechnique );

    pVB[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pVB, &offset );

    ID3D10Buffer* pTemp = g_pStreamTo;
    g_pStreamTo = g_pDrawFrom;
    g_pDrawFrom = pTemp;
}

//--------------------------------------------------------------------------------------
// Render mesh instanced
//--------------------------------------------------------------------------------------
void RenderMeshInstanced( ID3D10Device* pd3dDevice, ID3D10EffectTechnique* pTechnique )
{
    ID3D10Buffer* pVB[2];
    UINT Strides[2];
    UINT Offsets[2] = {0,0};

    pVB[0] = g_Mesh[0].GetVB10( 0, 0 );
    pVB[1] = g_pInstanceBuffer;
    Strides[0] = ( UINT )g_Mesh[0].GetVertexStride( 0, 0 );
    Strides[1] = sizeof( D3DXMATRIX );

    pd3dDevice->IASetInputLayout( g_pInstVertexLayout );
    pd3dDevice->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dDevice->IASetIndexBuffer( g_Mesh[0].GetIB10( 0 ), g_Mesh[0].GetIBFormat10( 0 ), 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_Mesh[0].GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_Mesh[0].GetSubset( 0, subset );

            PrimType = g_Mesh[0].GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pMat = g_Mesh[0].GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV10 );

            pTechnique->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumInstances,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }
}

//--------------------------------------------------------------------------------------
// Render Spectrogram to Texture
//--------------------------------------------------------------------------------------
void RenderSpectrogramToTexture( ID3D10Device* pd3dDevice,
                                 double fTime,
                                 float fElapsedTime )
{
    // Store the old viewport
    D3D10_VIEWPORT OldVP;
    UINT cRT = 1;
    pd3dDevice->RSGetViewports( &cRT, &OldVP );

    // Set a new viewport that exactly matches the size of our 2d textures
    D3D10_VIEWPORT PVP;
    PVP.Width = g_uiFloorX;
    PVP.Height = ( UINT )( g_uiFloorY / g_fSpectrogramSize );
    PVP.MinDepth = 0;
    PVP.MaxDepth = 1;
    PVP.TopLeftX = 0;
    PVP.TopLeftY = g_uiFloorY - PVP.Height - 5;
    pd3dDevice->RSSetViewports( 1, &PVP );

    // Store old render target
    ID3D10RenderTargetView* pOldRT = NULL;
    ID3D10DepthStencilView* pOldDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, &pOldRT, &pOldDS );
	
    // Set the render target and a NULL depth/stencil surface
    pd3dDevice->OMSetRenderTargets( 1, &g_pFloorRTV[g_uiFloorTo], NULL );

    // set view
    if( g_pEffect10 )
    {
        D3DXMATRIX mWorld;
        D3DXMATRIX mView;
        D3DXMATRIX mProj;
        D3DXMATRIX mWorldView;
        D3DXMATRIX mWorldViewProj;

        float scale = g_fSpectrogramSize / g_iNumInstances;
        D3DXMatrixIdentity( &mWorld );
        D3DXVECTOR3 vEye( 0,100.0f,-scale * 3.0f );
        D3DXVECTOR3 vLook( 0,0,-scale * 3.0f );
        D3DXVECTOR3 vUp( 0,0,1 );
        D3DXMatrixLookAtLH( &mView, &vEye, &vLook, &vUp );
        D3DXMatrixOrthoLH( &mProj, g_fSpectrogramSize + 2.0f, scale * 2.0f, 0.1f, 500.0f );
        mWorldView = mWorld * mView;
        mWorldViewProj = mWorldView * mProj;

        g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
        g_pmWorldView->SetMatrix( ( float* )&mWorldView );
        g_pmWorld->SetMatrix( ( float* )&mWorld );
        D3DXVECTOR4 lightDir( -1,1,-1,1 );
        D3DXVec3Normalize( ( D3DXVECTOR3* )&lightDir, ( D3DXVECTOR3* )&lightDir );
        g_pWorldLightDir->SetFloatVector( ( float* )&lightDir );
        g_pfElapsedTime->SetFloat( fElapsedTime );
        g_pfTime->SetFloat( ( float )fTime );
        g_pArrayTex->SetResource( g_pBobbleTexRV );
        g_pSpectrogramTex->SetResource( g_pSourceTexRV );
        g_pGradientTex->SetResource( g_pGradientTexRV );

        // render mesh
        if( g_bDrawFromStream )
        {
            RenderMeshFromStream( pd3dDevice, g_pRenderStream );
        }
        else
        {
            RenderMeshInstanced( pd3dDevice, g_pRenderInstanced );
        }
    }

    // Restore the original viewport
    pd3dDevice->RSSetViewports( 1, &OldVP );

    // Restore rendertarget
    pd3dDevice->OMSetRenderTargets( 1, &pOldRT, pOldDS );
    SAFE_RELEASE( pOldRT );
    SAFE_RELEASE( pOldDS );
}

//--------------------------------------------------------------------------------------
bool AdvanceParticles( ID3D10Device* pd3dDevice, float fGlobalTime, float fElapsedTime, D3DXVECTOR4 vGravity )
{
    // Set IA parameters
    ID3D10Buffer* pBuffers[1];
    if( g_bParticleFirst )
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

    // Set Effects Parameters
    g_pfGlobalTime->SetFloat( fGlobalTime );
    g_pfParticleElapsedTime->SetFloat( fElapsedTime );
    vGravity *= fElapsedTime;
    g_pvFrameGravity->SetFloatVector( ( float* )&vGravity );
    g_pRandomTex->SetResource( g_pRandomTexRV );
    g_pSecondsPerFirework->SetFloat( 0.4f );
    g_pNumEmber1s->SetInt( 40 );
    g_pMaxEmber2s->SetFloat( 10.0f );
    g_pParticleSpectrogramTex->SetResource( g_pSourceTexRV );
    g_pParticleGradientTex->SetResource( g_pGradientTexRV );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pAdvanceParticles->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pAdvanceParticles->GetPassByIndex( p )->Apply( 0 );
        if( g_bParticleFirst )
            pd3dDevice->Draw( g_iNumInstances, 0 );
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

    g_bParticleFirst = false;

    return true;
}


//--------------------------------------------------------------------------------------
bool RenderParticles( ID3D10Device* pd3dDevice, D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;

    // Set IA parameters
    ID3D10Buffer* pBuffers[1] = { g_pParticleDrawFrom };
    UINT stride[1] = { sizeof( PARTICLE_VERTEX ) };
    UINT offset[1] = { 0 };
    pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    // Set Effects Parameters
    D3DXMatrixMultiply( &mWorldViewProj, &mView, &mProj );
    g_pmParticleWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pParticleDiffuseTex->SetResource( g_pParticleTexRV );
    D3DXMATRIX mInvView;
    D3DXMatrixInverse( &mInvView, NULL, &mView );
    g_pmInvView->SetMatrix( ( float* )&mInvView );

    // Draw
    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderParticles->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        g_pRenderParticles->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->DrawAuto();
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // Clear the render target
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
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

    // create the spectrogram
    if( !g_bRenderPCMData && g_bRunFFTThisFrame )
        CreateSpectrogram( pd3dDevice );

    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    D3DXMATRIX mWorldView;
    D3DXMATRIX mWorldViewProj;
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldView = mWorld * mView;
    mWorldViewProj = mWorldView * mProj;

    // Set variables
    if( g_pEffect10 )
    {
        // store the original render target and set a new one
        ID3D10RenderTargetView* pOldRTV = NULL;
        ID3D10DepthStencilView* pOldDS = NULL;
        if( g_bScreenEffects )
        {
            pd3dDevice->OMGetRenderTargets( 1, &pOldRTV, &pOldDS );
            pd3dDevice->OMSetRenderTargets( 1, &g_pScreenRTV[g_uiScreenTo], pOldDS );
        }

        // render the other mesh
        pd3dDevice->IASetInputLayout( g_pMeshLayout );
        D3DXMATRIX mScale;
        D3DXMatrixScaling( &mScale, g_fSpectrogramSize / 2.0f + 2.0f, 1.0f, g_fSpectrogramSize / 2.0f );
        D3DXMATRIX mTrans;
        D3DXMatrixTranslation( &mTrans, 0.0f, 0.0f, -g_fSpectrogramSize / 2.0f );
        mWorld = mScale * mTrans;
        mWorldView = mWorld * mView;
        mWorldViewProj = mWorldView * mProj;
        g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
        g_pmWorldView->SetMatrix( ( float* )&mWorldView );
        g_pmWorld->SetMatrix( ( float* )&mWorld );
        g_pDiffuseTex->SetResource( g_pFloorSRV[g_uiFloorFrom] );
        g_Mesh[1].Render( pd3dDevice, g_pRenderMesh );
        ClearShaderResources( pd3dDevice );

        // render to texture operations on the floor texture
        ID3D10RenderTargetView* pPrevRTV = NULL;
        ID3D10DepthStencilView* pPrevDS = NULL;
        pd3dDevice->OMGetRenderTargets( 1, &pPrevRTV, &pPrevDS );
        RenderToTexture( pd3dDevice,
                         g_pFloorRTV[g_uiFloorTo],
                         g_pFloorSRV[g_uiFloorFrom],
                         false,
                         g_pFloorEffect,
                         g_pDiffuseTex,
                         g_pScreenQuadLayout,
                         g_pScreenQuadVB,
                         sizeof( SCREENQUAD_VERTEX ),
                         g_uiFloorX,
                         g_uiFloorY );
        // Restore the original RT and DS
        pd3dDevice->OMSetRenderTargets( 1, &pPrevRTV, pPrevDS );
        SAFE_RELEASE( pPrevRTV );
        SAFE_RELEASE( pPrevDS );

        // render the spectrogram
        mWorld = *g_Camera.GetWorldMatrix();
        mWorldView = mWorld * mView;
        mWorldViewProj = mWorldView * mProj;
        g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
        g_pmWorldView->SetMatrix( ( float* )&mWorldView );
        g_pmWorld->SetMatrix( ( float* )&mWorld );
        D3DXVECTOR4 lightDir( -1,1,-1,1 );
        D3DXVec3Normalize( ( D3DXVECTOR3* )&lightDir, ( D3DXVECTOR3* )&lightDir );
        g_pWorldLightDir->SetFloatVector( ( float* )&lightDir );
        g_pfElapsedTime->SetFloat( fElapsedTime );
        g_pfTime->SetFloat( ( float )fTime );
        g_pArrayTex->SetResource( g_pBobbleTexRV );
        g_pSpectrogramTex->SetResource( g_pSourceTexRV );
        g_pGradientTex->SetResource( g_pGradientTexRV );

        // render mesh
        if( g_bDrawFromStream )
        {
            RenderMeshFromStream( pd3dDevice, g_pRenderStream );
            HandleEffects( pd3dDevice, g_pHandleEffects );
        }
        else
        {
            RenderMeshInstanced( pd3dDevice, g_pRenderInstanced );
        }

        // Set the Vertex Layout
        pd3dDevice->IASetInputLayout( g_pParticleVertexLayout );

        if( g_bFireworks )
        {
            // Advance the Particles
            //D3DXVECTOR4 vGravity(0,-9.8f,0,0);
            D3DXVECTOR4 vGravity( 0,-12.0f,0,0 );
            AdvanceParticles( pd3dDevice, ( float )fTime, fElapsedTime, vGravity );
            // Render the particles
            RenderParticles( pd3dDevice, mView, mProj );
        }

        // do screen-space effects on this image
        if( g_bScreenEffects )
        {
            SWAP( g_uiScreenTo, g_uiScreenFrom );
            ClearShaderResources( pd3dDevice );
            RenderToTexture( pd3dDevice,
                             g_pScreenRTV[g_uiScreenTo],
                             g_pScreenSRV[g_uiScreenFrom],
                             false,
                             g_pScreenEffect,
                             g_pDiffuseTex,
                             g_pScreenQuadLayout,
                             g_pScreenQuadVB,
                             sizeof( SCREENQUAD_VERTEX ),
                             g_uiScreenX,
                             g_uiScreenY );

            // copy the image to the main render target
            ClearShaderResources( pd3dDevice );
            RenderToTexture( pd3dDevice,
                             pOldRTV,
                             g_pScreenSRV[g_uiScreenFrom],
                             false,
                             g_pCopy,
                             g_pDiffuseTex,
                             g_pScreenQuadLayout,
                             g_pScreenQuadVB,
                             sizeof( SCREENQUAD_VERTEX ),
                             g_uiScreenX,
                             g_uiScreenY );

            // restore the original render target and depth
            pd3dDevice->OMSetRenderTargets( 1, &pOldRTV, pOldDS );
            SAFE_RELEASE( pOldRTV );
            SAFE_RELEASE( pOldDS );
        }
    }

    // render the spectrogram into a texture for the next go-around
    RenderSpectrogramToTexture( pd3dDevice,
                                fTime,
                                fElapsedTime );
    // swap floor textures
    SWAP( g_uiFloorTo, g_uiFloorFrom );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();

    ClearShaderResources( pd3dDevice );
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    for( int i = 0; i < 2; i++ )
    {
        SAFE_RELEASE( g_pScreenRT[i] );
        SAFE_RELEASE( g_pScreenRTV[i] );
        SAFE_RELEASE( g_pScreenSRV[i] );
    }
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
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pInstVertexLayout );
    SAFE_RELEASE( g_pMeshLayout );
    for( int i = 0; i < 2; i++ )
        g_Mesh[i].Destroy();
    SAFE_RELEASE( g_pMeshTexRV );
    SAFE_RELEASE( g_pInstanceBuffer );
    SAFE_RELEASE( g_pGradientTexRV );
    SAFE_RELEASE( g_pBobbleTex );
    SAFE_RELEASE( g_pBobbleTexRV );

    SAFE_RELEASE( g_pSpectrogramEffect10 );
    SAFE_RELEASE( g_pQuadVertexLayout );
    SAFE_RELEASE( g_pQuadVB );

    for( int i = 0; i < 2; i++ )
    {
        SAFE_RELEASE( g_pFloorRT[i] );
        SAFE_RELEASE( g_pFloorRTV[i] );
        SAFE_RELEASE( g_pFloorSRV[i] );
    }
    SAFE_RELEASE( g_pScreenQuadLayout );
    SAFE_RELEASE( g_pScreenQuadVB );

    SAFE_RELEASE( g_pStreamVertexLayout );
    SAFE_RELEASE( g_pDrawFrom );
    SAFE_RELEASE( g_pStreamTo );

    // particle stuff
    SAFE_RELEASE( g_pParticleEffect );
    SAFE_RELEASE( g_pParticleVertexLayout );
    SAFE_RELEASE( g_pParticleTexRV );
    SAFE_RELEASE( g_pRandomTexture );
    SAFE_RELEASE( g_pRandomTexRV );

    DestroyBuffers();

    CloseWaveDevice();

    UnloadAudioData();
}

//--------------------------------------------------------------------------------------
void ClearShaderResources( ID3D10Device* pd3dDevice )
{
    // unload
    ID3D10ShaderResourceView* pNULLS[4] = {0,0,0,0};
    pd3dDevice->PSSetShaderResources( 0, 4, pNULLS );
    pd3dDevice->GSSetShaderResources( 0, 4, pNULLS );
    pd3dDevice->VSSetShaderResources( 0, 4, pNULLS );
}

//--------------------------------------------------------------------------------------
HRESULT InitSpectrogramResources( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    if( FAILED( hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GPUSpectrogram.fx" ) ) )
        return hr;
    char strBuckets[MAX_PATH];
    sprintf_s( strBuckets, MAX_PATH, "%d", g_uiTexX );
    D3D10_SHADER_MACRO Defines[] =
    {
        { "BUCKETS", strBuckets },
        { NULL, NULL }
    };
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    hr = D3DX10CreateEffectFromFile( str, Defines, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                         NULL, &g_pSpectrogramEffect10, NULL, NULL );
    if( FAILED( hr ) )
        return hr;

    // Obtain the technique handles
    g_pReverse = g_pSpectrogramEffect10->GetTechniqueByName( "Reverse" );
    g_pFFTInner = g_pSpectrogramEffect10->GetTechniqueByName( "FFTInner" );
    g_pRenderQuad = g_pSpectrogramEffect10->GetTechniqueByName( "RenderQuad" );
    g_ptxSource = g_pSpectrogramEffect10->GetVariableByName( "g_txSource" )->AsShaderResource();

    g_pWR = g_pSpectrogramEffect10->GetVariableByName( "g_WR" )->AsScalar();
    g_pWI = g_pSpectrogramEffect10->GetVariableByName( "g_WI" )->AsScalar();
    g_pMMAX = g_pSpectrogramEffect10->GetVariableByName( "g_MMAX" )->AsScalar();
    g_pM = g_pSpectrogramEffect10->GetVariableByName( "g_M" )->AsScalar();
    g_pISTEP = g_pSpectrogramEffect10->GetVariableByName( "g_ISTEP" )->AsScalar();
    g_pTextureSize = g_pSpectrogramEffect10->GetVariableByName( "g_TextureSize" )->AsVector();

    g_uiTexY = 1;
    D3DXVECTOR4 vTextureSize( ( float )g_uiTexX, ( float )g_uiTexY, 0.0f, 0.0f );
    g_pTextureSize->SetFloatVector( ( float* )vTextureSize );

    // Create our quad vertex input layout
    const D3D10_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE",  0, DXGI_FORMAT_R32G32_UINT,     0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pReverse->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( quadlayout, sizeof( quadlayout ) / sizeof( quadlayout[0] ),
                                        PassDesc.pIAInputSignature,
                                        PassDesc.IAInputSignatureSize, &g_pQuadVertexLayout );
    if( FAILED( hr ) )
        return hr;

    hr = CreateQuad( pd3dDevice, g_uiTexX, g_uiTexY );
    if( FAILED( hr ) )
        return hr;

    return hr;
}


//--------------------------------------------------------------------------------------
void RenderToTexture( ID3D10Device* pd3dDevice,
                      ID3D10RenderTargetView* pRTV,
                      ID3D10ShaderResourceView* pSRV,
                      bool bClear,
                      ID3D10EffectTechnique* pTechnique,
                      ID3D10EffectShaderResourceVariable* ptxSource,
                      ID3D10InputLayout* pInputLayout,
                      ID3D10Buffer* pVB,
                      UINT iStride,
                      UINT iRTX,
                      UINT iRTY )
{
    // Clear if we need to
    if( bClear )
    {
        float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
        pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    }

    // Store the old viewport
    D3D10_VIEWPORT OldVP;
    UINT cRT = 1;
    pd3dDevice->RSGetViewports( &cRT, &OldVP );

    if( pRTV )
    {
        // Set a new viewport that exactly matches the size of our 2d textures
        D3D10_VIEWPORT PVP;
        PVP.Width = iRTX;
        PVP.Height = iRTY;
        PVP.MinDepth = 0;
        PVP.MaxDepth = 1;
        PVP.TopLeftX = 0;
        PVP.TopLeftY = 0;
        pd3dDevice->RSSetViewports( 1, &PVP );
    }

    // Set input params
    pd3dDevice->IASetInputLayout( pInputLayout );
    UINT offsets = 0;
    UINT uStrides = iStride;
    pd3dDevice->IASetVertexBuffers( 0, 1, &pVB, &uStrides, &offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    // Set the render target and a NULL depth/stencil surface
    if( pRTV )
    {
        ID3D10RenderTargetView* aRTViews[] = { pRTV };
        pd3dDevice->OMSetRenderTargets( 1, aRTViews, NULL );
    }

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        ptxSource->SetResource( pSRV );
        pTechnique->GetPassByIndex( p )->Apply( 0 );
        pd3dDevice->Draw( 4, 0 );
    }

    // Retore the original viewport
    pd3dDevice->RSSetViewports( 1, &OldVP );
}


//--------------------------------------------------------------------------------------
void CreateSpectrogram( ID3D10Device* pd3dDevice )
{
    //store the original rt and ds buffer views
    ID3D10RenderTargetView* apOldRTVs[1] = { NULL };
    ID3D10DepthStencilView* pOldDS = NULL;
    pd3dDevice->OMGetRenderTargets( 1, apOldRTVs, &pOldDS );

    // Reverse Indices
    RenderToTexture( pd3dDevice,
                     g_pDestRTV,
                     g_pSourceTexRV,
                     false,
                     g_pReverse,
                     g_ptxSource,
                     g_pQuadVertexLayout,
                     g_pQuadVB,
                     sizeof( QUAD_VERTEX ),
                     g_uiTexX,
                     g_uiTexY );
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

    // Danielson-Lanczos routine
    UINT iterations = 0;
    float wtemp, wr, wpr, wpi, wi, theta;
    UINT n = g_uiTexX;
    UINT mmax = 1;
    while( n > mmax )
    {
        UINT istep = mmax << 1;
        theta = 6.28318530717959f / ( ( float )mmax * 2.0f );
        wtemp = sin( 0.5f * theta );
        wpr = -2.0f * wtemp * wtemp;
        wpi = sin( theta );
        wr = 1.0f;
        wi = 0.0f;

        for( UINT m = 0; m < mmax; m++ )
        {
            // Inner loop is handled on the GPU
            {
                g_pWR->SetFloat( wr );
                g_pWI->SetFloat( wi );
                g_pMMAX->SetInt( mmax );
                g_pM->SetInt( m );
                g_pISTEP->SetInt( istep );

                // Make sure we immediately unbind the previous RT from the shader pipeline
                ID3D10ShaderResourceView* const pSRV[1] = {NULL};
                pd3dDevice->PSSetShaderResources( 0, 1, pSRV );

                if( 0 == iterations % 2 )
                {
                    RenderToTexture( pd3dDevice,
                                     g_pSourceRTV,
                                     g_pDestTexRV,
                                     false,
                                     g_pFFTInner,
                                     g_ptxSource,
                                     g_pQuadVertexLayout,
                                     g_pQuadVB,
                                     sizeof( QUAD_VERTEX ),
                                     g_uiTexX,
                                     g_uiTexY );
                }
                else
                {
                    RenderToTexture( pd3dDevice,
                                     g_pDestRTV,
                                     g_pSourceTexRV,
                                     false,
                                     g_pFFTInner,
                                     g_ptxSource,
                                     g_pQuadVertexLayout,
                                     g_pQuadVB,
                                     sizeof( QUAD_VERTEX ),
                                     g_uiTexX,
                                     g_uiTexY );
                }
                pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );

                iterations++;
            }

            wtemp = wr;
            wr = wtemp * wpr - wi * wpi + wr;
            wi = wi * wpr + wtemp * wpi + wi;
        }
        mmax = istep;
    }

    // Restore the original RT and DS
    pd3dDevice->OMSetRenderTargets( 1, apOldRTVs, pOldDS );
    SAFE_RELEASE( apOldRTVs[0] );
    SAFE_RELEASE( pOldDS );

    return;
}

//--------------------------------------------------------------------------------------
void DestroyResources()
{
    SAFE_RELEASE( g_pQuadVertexLayout );

    SAFE_RELEASE( g_pQuadVB );
    SAFE_RELEASE( g_pSpectrogramEffect10 );
    SAFE_RELEASE( g_pSourceTexture );
    SAFE_RELEASE( g_pSourceTexRV );
    SAFE_RELEASE( g_pSourceRTV );
    SAFE_RELEASE( g_pDestTexture );
    SAFE_RELEASE( g_pDestTexRV );
    SAFE_RELEASE( g_pDestRTV );
}

//--------------------------------------------------------------------------------------
HRESULT CreateQuad( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY )
{
    HRESULT hr = S_OK;

    // First create space for the vertices
    UINT uiVertBufSize = 4 * sizeof( QUAD_VERTEX );
    QUAD_VERTEX* pVerts = new QUAD_VERTEX[ uiVertBufSize ];
    if( !pVerts )
        return E_OUTOFMEMORY;

    int index = 0;
    pVerts[index].pos = D3DXVECTOR3( -1, -1, 0 );
    pVerts[index].texx = 0;
    pVerts[index].texy = 0;

    index++;
    pVerts[index].pos = D3DXVECTOR3( -1, 1, 0 );
    pVerts[index].texx = 0;
    pVerts[index].texy = uiTexY;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, -1, 0 );
    pVerts[index].texx = uiTexX;
    pVerts[index].texy = 0;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, 1, 0 );
    pVerts[index].texx = uiTexX;
    pVerts[index].texy = uiTexY;

    D3D10_BUFFER_DESC vbdesc =
    {
        uiVertBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVerts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    hr = pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pQuadVB );
    SAFE_DELETE_ARRAY( pVerts );
    if( FAILED( hr ) )
        return hr;

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CreateScreenQuad( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // First create space for the vertices
    UINT uiVertBufSize = 4 * sizeof( SCREENQUAD_VERTEX );
    SCREENQUAD_VERTEX* pVerts = new SCREENQUAD_VERTEX[ uiVertBufSize ];
    if( !pVerts )
        return E_OUTOFMEMORY;

    int index = 0;
    pVerts[index].pos = D3DXVECTOR3( -1, -1, 0 );
    pVerts[index].tex.x = 0;
    pVerts[index].tex.y = 1.0f;

    index++;
    pVerts[index].pos = D3DXVECTOR3( -1, 1, 0 );
    pVerts[index].tex.x = 0;
    pVerts[index].tex.y = 0.0f;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, -1, 0 );
    pVerts[index].tex.x = 1.0f;
    pVerts[index].tex.y = 1.0f;

    index++;
    pVerts[index].pos = D3DXVECTOR3( 1, 1, 0 );
    pVerts[index].tex.x = 1.0f;
    pVerts[index].tex.y = 0.0f;

    D3D10_BUFFER_DESC vbdesc =
    {
        uiVertBufSize,
        D3D10_USAGE_IMMUTABLE,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVerts;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    hr = pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pScreenQuadVB );
    SAFE_DELETE_ARRAY( pVerts );
    if( FAILED( hr ) )
        return hr;

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT CreateBuffers( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY )
{
    HRESULT hr = S_OK;

    // Create Source and Dest textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = uiTexX;
    dstex.Height = uiTexY;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32_FLOAT;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pSourceTexture );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pDestTexture );
    if( FAILED( hr ) )
        return hr;

    // Create Source and Dest Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    hr = pd3dDevice->CreateShaderResourceView( g_pSourceTexture, &SRVDesc, &g_pSourceTexRV );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateShaderResourceView( g_pDestTexture, &SRVDesc, &g_pDestTexRV );
    if( FAILED( hr ) )
        return hr;

    // Create Position and Velocity RenderTarget Views
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    hr = pd3dDevice->CreateRenderTargetView( g_pSourceTexture, &DescRT, &g_pSourceRTV );
    if( FAILED( hr ) )
        return hr;
    hr = pd3dDevice->CreateRenderTargetView( g_pDestTexture, &DescRT, &g_pDestRTV );
    if( FAILED( hr ) )
        return hr;

    return hr;
}

HRESULT CreateFloorTargets( ID3D10Device* pd3dDevice, UINT uiTexX, UINT uiTexY )
{
    HRESULT hr = S_OK;
    // Create Source and Dest textures
    D3D10_TEXTURE2D_DESC dstex;
    dstex.Width = uiTexX;
    dstex.Height = uiTexY;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Usage = D3D10_USAGE_DEFAULT;
    dstex.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pFloorRT[0] );
    hr = pd3dDevice->CreateTexture2D( &dstex, NULL, &g_pFloorRT[1] );
    if( FAILED( hr ) )
        return hr;

    // Create Source and Dest Resource Views
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;
    hr = pd3dDevice->CreateShaderResourceView( g_pFloorRT[0], &SRVDesc, &g_pFloorSRV[0] );
    hr = pd3dDevice->CreateShaderResourceView( g_pFloorRT[1], &SRVDesc, &g_pFloorSRV[1] );
    if( FAILED( hr ) )
        return hr;

    // Create RenderTarget Views
    D3D10_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = dstex.Format;
    DescRT.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    DescRT.Texture2D.MipSlice = 0;
    hr = pd3dDevice->CreateRenderTargetView( g_pFloorRT[0], &DescRT, &g_pFloorRTV[0] );
    hr = pd3dDevice->CreateRenderTargetView( g_pFloorRT[1], &DescRT, &g_pFloorRTV[1] );
    if( FAILED( hr ) )
        return hr;

    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pd3dDevice->ClearRenderTargetView( g_pFloorRTV[0], ClearColor );
    pd3dDevice->ClearRenderTargetView( g_pFloorRTV[1], ClearColor );

    return hr;
}

void DestroyBuffers()
{
    SAFE_RELEASE( g_pSourceTexture );
    SAFE_RELEASE( g_pSourceTexRV );
    SAFE_RELEASE( g_pSourceRTV );
    SAFE_RELEASE( g_pDestTexture );
    SAFE_RELEASE( g_pDestTexRV );
    SAFE_RELEASE( g_pDestRTV );

    SAFE_RELEASE( g_pParticleStart );
    SAFE_RELEASE( g_pParticleStreamTo );
    SAFE_RELEASE( g_pParticleDrawFrom );
}

#define MM_RETURN(x)    { hr = (x); if( S_OK != hr ) { return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
HRESULT OpenWaveDevice()
{
    // Get the number of audio output devices on the system
    UINT numDevs = waveOutGetNumDevs();

    // query the caps of each device
    for( UINT i = 0; i < numDevs; i++ )
    {
        WAVEOUTCAPS woc;
        waveOutGetDevCaps( i, &woc, sizeof( WAVEOUTCAPS ) );
    }

    // try to open one
    WAVEFORMATEX waveFormat = g_audioData.GetFormat();
    UINT uDeviceID = WAVE_MAPPER;
    DWORD fdwOpen = CALLBACK_NULL;

    HRESULT hr = waveOutOpen( &g_hWaveOut,
                              uDeviceID,
                              &waveFormat,
                              ( DWORD_PTR )NULL,
                              NULL,
                              fdwOpen );
    MM_RETURN(hr);

    // Prepare the output header
    ZeroMemory( &g_WaveHeader, sizeof( WAVEHDR ) );
    g_WaveHeader.dwBufferLength = g_audioData.GetBitsSize();
    g_WaveHeader.dwBytesRecorded = 0;
    g_WaveHeader.lpData = ( LPSTR )g_audioData.GetRawBits();
    g_WaveHeader.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    g_WaveHeader.dwLoops = ( DWORD )-1;
    hr = waveOutPrepareHeader( g_hWaveOut, &g_WaveHeader, sizeof( WAVEHDR ) );
    MM_RETURN(hr);

    // Pause playback
    hr = waveOutPause( g_hWaveOut );
    MM_RETURN(hr);

    // Pop the data into a buffer
    hr = waveOutWrite( g_hWaveOut, &g_WaveHeader, sizeof( WAVEHDR ) );
    MM_RETURN(hr);

    return S_OK;
}

HRESULT CloseWaveDevice()
{
    HRESULT hr = StopPlayback();
    MM_RETURN(hr);

    hr = waveOutReset( g_hWaveOut );
    MM_RETURN(hr);

    hr = waveOutClose( g_hWaveOut );
    MM_RETURN(hr);

    return S_OK;
}

HRESULT StartPlayback()
{
    if( g_bPlaybackStarted )
        return S_OK;

    HRESULT hr = waveOutRestart( g_hWaveOut );
    MM_RETURN(hr);

    g_bPlaybackStarted = true;

    return S_OK;
}

HRESULT StopPlayback()
{
    if( !g_bPlaybackStarted )
        return S_OK;

    // pause
    HRESULT hr = waveOutPause( g_hWaveOut );
    MM_RETURN(hr);

    g_bPlaybackStarted = false;

    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT LoadAudioData( ID3D10Device* pd3dDevice, LPCTSTR szFileName )
{
    HRESULT hr = S_OK;

    // Load the wave file
    if( !g_audioData.LoadWaveFile( ( TCHAR* )szFileName ) )
        return E_FAIL;

    // Normalize the data
    g_audioData.NormalizeData();

    // Create a texture large enough to hold our data
    hr = CreateBuffers( pd3dDevice, g_uiTexX, g_uiTexY );
    if( FAILED( hr ) )
        return hr;

    // Create temp storage with space for imaginary data
    unsigned long size = g_uiTexX * g_uiTexY;
    g_pvPCMData = new D3DXVECTOR2[ size ];
    if( !g_pvPCMData )
        return E_OUTOFMEMORY;

    return hr;
}

HRESULT UnloadAudioData()
{
    DestroyBuffers();
    g_audioData.Cleanup();
    SAFE_DELETE_ARRAY( g_pvPCMData );
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT UpdateTextureWithAudio( ID3D10Device* pd3dDevice, unsigned long ulSampleOffset )
{
    //float* pDataPtr = audioData.GetChannelPtr( 0 );
    float* pSamples1;
    float* pSamples2;
    unsigned long ulSampleSize1;
    unsigned long ulSampleSize2;

    g_audioData.GetChannelData( 0,
                                ulSampleOffset, g_uiTexX,
                                &pSamples1, &ulSampleSize1,
                                &pSamples2, &ulSampleSize2 );

    UINT index = 0;
    for( UINT s = 0; s < ulSampleSize1; s++ )
    {
        g_pvPCMData[index].x = pSamples1[s];
        g_pvPCMData[index].y = 0.0f;
        index++;
    }

    for( UINT s = 0; s < ulSampleSize2; s++ )
    {
        g_pvPCMData[index].x = pSamples2[s];
        g_pvPCMData[index].y = 0.0f;
        index++;
    }

    // Update the texture with this information
    pd3dDevice->UpdateSubresource( g_pSourceTexture, D3D10CalcSubresource( 0, 0, 1 ), NULL,
                                   g_pvPCMData, g_uiTexX * sizeof( D3DXVECTOR2 ), 0 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
void CreateSpectrogramInstances( D3DXMATRIX* pInstances, UINT NumInstances, bool bLine )
{
    float scale = g_fSpectrogramSize / ( g_iNumInstances * 2.0f );
    for( int i = 0; i < ( int )g_iNumInstances; i++ )
    {
        if( bLine )
        {
            D3DXMatrixTranslation( &pInstances[i], ( -g_fSpectrogramSize / 2.0f + scale * i * 2 ), 0.0f, 0.0f );
            D3DXMATRIX mScale;
            D3DXMatrixScaling( &mScale, scale, scale, scale );
            pInstances[i] = mScale * pInstances[i];
        }
        else
        {
            D3DXMatrixTranslation( &pInstances[i], -g_fSpectrogramSize / 2.0f, 0.0f, 0.0f );
            D3DXMATRIX mRot;
            D3DXMatrixRotationY( &mRot, ( D3DX_PI / g_iNumInstances ) * ( float )i );
            D3DXMATRIX mScale;
            D3DXMatrixScaling( &mScale, scale, scale, scale );
            pInstances[i] = mScale * pInstances[i] * mRot;
        }
    }

}

//--------------------------------------------------------------------------------------
HRESULT CreateInstanceBuffer( ID3D10Device* pd3dDevice, bool bLine )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pInstanceBuffer );

    // Create a resource with the input matrices
    // We're creating this buffer as dynamic because in a game, the instance data could be dynamic... aka
    // we could have moving trees.
    D3D10_BUFFER_DESC bufferDesc =
    {
        g_iNumInstances * sizeof( D3DXMATRIX ),
        D3D10_USAGE_DYNAMIC,
        D3D10_BIND_VERTEX_BUFFER,
        D3D10_CPU_ACCESS_WRITE,
        0
    };
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, NULL, &g_pInstanceBuffer ) );

    D3DXMATRIX* pInstances = new D3DXMATRIX[g_iNumInstances];
    CreateSpectrogramInstances( pInstances, g_iNumInstances, bLine );

    D3DXMATRIX* pMatrices = NULL;
    g_pInstanceBuffer->Map( D3D10_MAP_WRITE_DISCARD, NULL, ( void** )&pMatrices );
    memcpy( pMatrices, pInstances, g_iNumInstances * sizeof( D3DXMATRIX ) );
    g_pInstanceBuffer->Unmap();

    SAFE_DELETE_ARRAY( pInstances );
    return hr;
}

HRESULT CreateStreams( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    D3D10_BUFFER_DESC vbdesc =
    {
        g_iNumInstances * sizeof( STREAM_VERTEX ) * ( UINT )g_Mesh[0].GetNumIndices( 0 ) * 2,
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT,
        0,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pDrawFrom ) );
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pStreamTo ) );

    return hr;
}

HRESULT RenderInstancesToStream( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    UINT offset = 0;
    ID3D10Buffer* pVB[1] = {g_pStreamTo};
    pd3dDevice->SOSetTargets( 1, pVB, &offset );
    RenderMeshInstanced( pd3dDevice, g_pRenderToStream );
    pVB[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pVB, &offset );

    ID3D10Buffer* pTemp = g_pStreamTo;
    g_pStreamTo = g_pDrawFrom;
    g_pDrawFrom = pTemp;

    return hr;
}

FILETIME GetFileTime( WCHAR* strFile )
{
    WIN32_FIND_DATA FindData;
    HANDLE hFile = FindFirstFile( strFile, &FindData );
    FindClose( hFile );
    return FindData.ftLastWriteTime;
}

HRESULT LoadEffect( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pEffect10 );

    g_fLastWriteTime = GetFileTime( g_strEffect );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    V_RETURN( D3DX10CreateEffectFromFile( g_strEffect, NULL, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderInstanced = g_pEffect10->GetTechniqueByName( "RenderInstanced" );
    g_pRenderToStream = g_pEffect10->GetTechniqueByName( "RenderToStream" );
    g_pRenderStream = g_pEffect10->GetTechniqueByName( "RenderStream" );
    g_pHandleEffects = g_pEffect10->GetTechniqueByName( "HandleEffects" );
    g_pRenderMesh = g_pEffect10->GetTechniqueByName( "RenderMesh" );
    g_pFloorEffect = g_pEffect10->GetTechniqueByName( "FloorEffect" );
    g_pScreenEffect = g_pEffect10->GetTechniqueByName( "ScreenEffect" );
    g_pCopy = g_pEffect10->GetTechniqueByName( "Copy" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect10->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pWorldLightDir = g_pEffect10->GetVariableByName( "g_WorldLightDir" )->AsVector();
    g_pfNumInstances = g_pEffect10->GetVariableByName( "g_fNumInstances" )->AsScalar();
    g_pfScale = g_pEffect10->GetVariableByName( "g_fScale" )->AsScalar();
    g_pfEffectLimit = g_pEffect10->GetVariableByName( "g_fEffectLimit" )->AsScalar();
    g_pfEffectLife = g_pEffect10->GetVariableByName( "g_fEffectLife" )->AsScalar();
    g_pfElapsedTime = g_pEffect10->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pfNormalization = g_pEffect10->GetVariableByName( "g_fNormalization" )->AsScalar();
    g_pArrayTex = g_pEffect10->GetVariableByName( "g_txArray" )->AsShaderResource();
    g_pDiffuseTex = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pSpectrogramTex = g_pEffect10->GetVariableByName( "g_txSpectrogram" )->AsShaderResource();
    g_pGradientTex = g_pEffect10->GetVariableByName( "g_txGradient" )->AsShaderResource();
	
    // set rarely used data
    g_pfNumInstances->SetFloat( ( float )g_iNumInstances );
    g_pfScale->SetFloat( g_fScale );
    g_pfEffectLimit->SetFloat( g_fEffectLimit );
    g_pfEffectLife->SetFloat( g_fEffectLife );
    g_pfNormalization->SetFloat( g_fNormalization );

    return hr;
}

HRESULT LoadParticleEffect( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ParticlesGS.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, pd3dDevice,
                                          NULL, NULL, &g_pParticleEffect, NULL, NULL ) );

    // Obtain the technique handles
    g_pRenderParticles = g_pParticleEffect->GetTechniqueByName( "RenderParticles" );
    g_pAdvanceParticles = g_pParticleEffect->GetTechniqueByName( "AdvanceParticles" );

    // Obtain the parameter handles
    g_pmParticleWorldViewProj = g_pParticleEffect->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmInvView = g_pParticleEffect->GetVariableByName( "g_mInvView" )->AsMatrix();
    g_pfGlobalTime = g_pParticleEffect->GetVariableByName( "g_fGlobalTime" )->AsScalar();
    g_pfParticleElapsedTime = g_pParticleEffect->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_pParticleDiffuseTex = g_pParticleEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pRandomTex = g_pParticleEffect->GetVariableByName( "g_txRandom" )->AsShaderResource();
    g_pParticleSpectrogramTex = g_pParticleEffect->GetVariableByName( "g_txSpectrogram" )->AsShaderResource();
    g_pParticleGradientTex = g_pParticleEffect->GetVariableByName( "g_txGradient" )->AsShaderResource();
    g_pSecondsPerFirework = g_pParticleEffect->GetVariableByName( "g_fSecondsPerFirework" )->AsScalar();
    g_pNumEmber1s = g_pParticleEffect->GetVariableByName( "g_iNumEmber1s" )->AsScalar();
    g_pMaxEmber2s = g_pParticleEffect->GetVariableByName( "g_fMaxEmber2s" )->AsScalar();
    g_pvFrameGravity = g_pParticleEffect->GetVariableByName( "g_vFrameGravity" )->AsVector();
    g_pfParticleNumInstances = g_pParticleEffect->GetVariableByName( "g_fNumInstances" )->AsScalar();
    g_pfParticleEffectLimit = g_pParticleEffect->GetVariableByName( "g_fEffectLimit" )->AsScalar();
    g_pfParticleNormalization = g_pParticleEffect->GetVariableByName( "g_fNormalization" )->AsScalar();

    g_pfParticleNumInstances->SetFloat( ( float )g_iNumInstances );
    g_pfParticleEffectLimit->SetFloat( g_fEffectLimit );
    g_pfParticleNormalization->SetFloat( g_fNormalization );

    // Create our vertex input layout
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TIMER",    0, DXGI_FORMAT_R32_FLOAT,          0, 40, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TYPE",     0, DXGI_FORMAT_R32_UINT,           0, 44, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BUCKET",   0, DXGI_FORMAT_R32_UINT,           0, 48, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3D10_PASS_DESC PassDesc;
    g_pAdvanceParticles->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, sizeof( layout ) / sizeof( layout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pParticleVertexLayout ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// LoadTextureArray loads a texture array and associated view from a series
// of textures on disk.
//--------------------------------------------------------------------------------------
HRESULT LoadTextureArray( ID3D10Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D10Texture2D** ppTex2D, ID3D10ShaderResourceView** ppSRV )
{
    HRESULT hr = S_OK;
    D3D10_TEXTURE2D_DESC desc;
    ZeroMemory( &desc, sizeof( D3D10_TEXTURE2D_DESC ) );

    WCHAR str[MAX_PATH];
    for( int i = 0; i < iNumTextures; i++ )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szTextureNames[i] ) );

        ID3D10Resource* pRes = NULL;
        D3DX10_IMAGE_LOAD_INFO loadInfo;
        ZeroMemory( &loadInfo, sizeof( D3DX10_IMAGE_LOAD_INFO ) );
        loadInfo.Width = D3DX_FROM_FILE;
        loadInfo.Height = D3DX_FROM_FILE;
        loadInfo.Depth = D3DX_FROM_FILE;
        loadInfo.FirstMipLevel = 0;
        loadInfo.MipLevels = 1;
        loadInfo.Usage = D3D10_USAGE_STAGING;
        loadInfo.BindFlags = 0;
        loadInfo.CpuAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;
        loadInfo.MiscFlags = 0;
        loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        loadInfo.Filter = D3DX10_FILTER_NONE;
        loadInfo.MipFilter = D3DX10_FILTER_NONE;

        V_RETURN( D3DX10CreateTextureFromFile( pd3dDevice, str, &loadInfo, NULL, &pRes, NULL ) );
        if( pRes )
        {
            ID3D10Texture2D* pTemp;
            pRes->QueryInterface( __uuidof( ID3D10Texture2D ), ( LPVOID* )&pTemp );
            pTemp->GetDesc( &desc );

            D3D10_MAPPED_TEXTURE2D mappedTex2D;
            if( DXGI_FORMAT_R8G8B8A8_UNORM != desc.Format )   //make sure we're R8G8B8A8
                return false;

            if( desc.MipLevels > 4 )
                desc.MipLevels -= 4;
            if( !( *ppTex2D ) )
            {
                desc.Usage = D3D10_USAGE_DEFAULT;
                desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
                desc.CPUAccessFlags = 0;
                desc.ArraySize = iNumTextures;
                V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, ppTex2D ) );
            }

            for( UINT iMip = 0; iMip < desc.MipLevels; iMip++ )
            {
                pTemp->Map( iMip, D3D10_MAP_READ, 0, &mappedTex2D );

                pd3dDevice->UpdateSubresource( ( *ppTex2D ),
                                               D3D10CalcSubresource( iMip, i, desc.MipLevels ),
                                               NULL,
                                               mappedTex2D.pData,
                                               mappedTex2D.RowPitch,
                                               0 );

                pTemp->Unmap( iMip );
            }

            SAFE_RELEASE( pRes );
            SAFE_RELEASE( pTemp );
        }
        else
        {
            return false;
        }
    }

    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
    SRVDesc.Texture2DArray.ArraySize = iNumTextures;
    V_RETURN( pd3dDevice->CreateShaderResourceView( *ppTex2D, &SRVDesc, ppSRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// This helper function creates 3 vertex buffers.  The first is used to seed the
// particle system.  The second two are used as output and intput buffers alternatively
// for the GS particle system.  Since a buffer cannot be both an input to the GS and an
// output of the GS, we must ping-pong between the two.
//--------------------------------------------------------------------------------------
HRESULT CreateParticleBuffer( ID3D10Device* pd3dDevice, bool bLine )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pParticleStart );
    SAFE_RELEASE( g_pParticleDrawFrom );
    SAFE_RELEASE( g_pParticleStreamTo );

    D3D10_BUFFER_DESC vbdesc =
    {
        g_iNumInstances * sizeof( PARTICLE_VERTEX ),
        D3D10_USAGE_DEFAULT,
        D3D10_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );
	
    PARTICLE_VERTEX* pVertices = new PARTICLE_VERTEX[g_iNumInstances];
    D3DXMATRIX* pInstances = new D3DXMATRIX[g_iNumInstances];
    CreateSpectrogramInstances( pInstances, g_iNumInstances, bLine );
    for( UINT i = 0; i < g_iNumInstances; i++ )
    {
        D3DXVECTOR3 origin( 0,0,0 );
        D3DXVECTOR4 transPos;
        D3DXVec3Transform( &transPos, &origin, &pInstances[i] );
        pVertices[i].pos = ( D3DXVECTOR3 )transPos;
        pVertices[i].vel = g_vParticleStartVelocity;
        pVertices[i].color = D3DXVECTOR4( 1, 0.1f, 0.1f, 1 );
        pVertices[i].Timer = 0;
        pVertices[i].Type = 0;
        pVertices[i].Bucket = i;
    }

    vbInitData.pSysMem = pVertices;
    vbInitData.SysMemPitch = g_iNumInstances * sizeof( PARTICLE_VERTEX );
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &vbInitData, &g_pParticleStart ) );

    SAFE_DELETE_ARRAY( pVertices );
    SAFE_DELETE_ARRAY( pInstances );

    vbdesc.ByteWidth = MAX_PARTICLES * sizeof( PARTICLE_VERTEX );
    vbdesc.BindFlags |= D3D10_BIND_STREAM_OUTPUT;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleDrawFrom ) );
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pParticleStreamTo ) );

    g_bParticleFirst = true;

    return hr;
}


//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors.  The shader uses
// the current time value to index into this texture to get a random vector.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTexture( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    int iNumRandValues = 1024;
    srand( timeGetTime() );
    //create the data
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new float[iNumRandValues * 3];
    if( !InitData.pSysMem )
        return E_OUTOFMEMORY;
    InitData.SysMemPitch = iNumRandValues * 3 * sizeof( float );
    InitData.SysMemSlicePitch = iNumRandValues * 3 * sizeof( float );
    for( int i = 0; i < iNumRandValues * 3; i++ )
    {
        ( ( float* )InitData.pSysMem )[i] = float( ( rand() % 10000 ) - 5000 );
    }

    // Create the texture
    D3D10_TEXTURE1D_DESC dstex;
    dstex.Width = iNumRandValues;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32_FLOAT;
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
