//-----------------------------------------------------------------------------
// File: HDRLighting.cpp
//
// Desc: This sample demonstrates some high dynamic range lighting effects 
//       using floating point textures.
//
// The algorithms described in this sample are based very closely on the 
// lighting effects implemented in Masaki Kawase's Rthdribl sample and the tone 
// mapping process described in the whitepaper "Tone Reproduction for Digital 
// Images"
//
// Real-Time High Dynamic Range Image-Based Lighting (Rthdribl)
// Masaki Kawase
// http://www.daionet.gr.jp/~masa/rthdribl/ 
//
// "Photographic Tone Reproduction for Digital Images"
// Erik Reinhard, Mike Stark, Peter Shirley and Jim Ferwerda
// http://www.cs.utah.edu/~reinhard/cdrom/ 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "glaredefd3d.h"
#include <stdio.h>
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 



//-----------------------------------------------------------------------------
// Constants and custom types
//-----------------------------------------------------------------------------
#define MAX_SAMPLES           16      // Maximum number of texture grabs

#define NUM_LIGHTS            2       // Number of lights in the scene

#define EMISSIVE_COEFFICIENT  39.78f  // Emissive color multiplier for each lumen
// of light intensity                                    
#define NUM_TONEMAP_TEXTURES  4       // Number of stages in the 4x4 down-scaling 
// of average luminance textures
#define NUM_STAR_TEXTURES     12      // Number of textures used for the star
// post-processing effect
#define NUM_BLOOM_TEXTURES    3       // Number of textures used for the bloom
// post-processing effect


// Texture coordinate rectangle
struct CoordRect
{
    float fLeftU, fTopV;
    float fRightU, fBottomV;
};


// World vertex format
struct WorldVertex
{
    D3DXVECTOR3 p; // position
    D3DXVECTOR3 n; // normal
    D3DXVECTOR2 t; // texture coordinate

    static const DWORD FVF;
};
const DWORD                 WorldVertex::FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;


// Screen quad vertex format
struct ScreenVertex
{
    D3DXVECTOR4 p; // position
    D3DXVECTOR2 t; // texture coordinate

    static const DWORD FVF;
};
const DWORD                 ScreenVertex::FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;



//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
IDirect3DDevice9*           g_pd3dDevice = NULL;    // D3D Device object
ID3DXFont*                  g_pFont = NULL;         // Font for drawing text
ID3DXSprite*                g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*                g_pEffect = NULL;       // D3DX effect interface
CFirstPersonCamera          g_Camera;               // A model viewing camera
D3DFORMAT                   g_LuminanceFormat;      // Format to use for luminance map
bool                        g_bShowHelp = true;     // If true, it renders the UI control text
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

PDIRECT3DSURFACE9           g_pFloatMSRT = NULL;           // Multi-Sample float render target
PDIRECT3DSURFACE9           g_pFloatMSDS = NULL;           // Depth Stencil surface for the float RT
PDIRECT3DTEXTURE9           g_pTexScene = NULL;            // HDR render target containing the scene
PDIRECT3DTEXTURE9           g_pTexSceneScaled = NULL;      // Scaled copy of the HDR scene
PDIRECT3DTEXTURE9           g_pTexBrightPass = NULL;       // Bright-pass filtered copy of the scene
PDIRECT3DTEXTURE9           g_pTexAdaptedLuminanceCur = NULL;  // The luminance that the user is currenly adapted to
PDIRECT3DTEXTURE9           g_pTexAdaptedLuminanceLast = NULL; // The luminance that the user is currenly adapted to
PDIRECT3DTEXTURE9           g_pTexStarSource = NULL;       // Star effect source texture
PDIRECT3DTEXTURE9           g_pTexBloomSource = NULL;      // Bloom effect source texture

PDIRECT3DTEXTURE9           g_pTexWall = NULL;     // Stone texture for the room walls
PDIRECT3DTEXTURE9           g_pTexFloor = NULL;    // Concrete texture for the room floor
PDIRECT3DTEXTURE9           g_pTexCeiling = NULL;  // Plaster texture for the room ceiling
PDIRECT3DTEXTURE9           g_pTexPainting = NULL; // Texture for the paintings on the wall


PDIRECT3DTEXTURE9 g_apTexBloom[NUM_BLOOM_TEXTURES] = {0};     // Blooming effect working textures
PDIRECT3DTEXTURE9 g_apTexStar[NUM_STAR_TEXTURES] = {0};       // Star effect working textures
PDIRECT3DTEXTURE9 g_apTexToneMap[NUM_TONEMAP_TEXTURES] = {0}; // Log average luminance samples 
// from the HDR render target

LPD3DXMESH                  g_pWorldMesh = NULL;       // Mesh to contain world objects
LPD3DXMESH                  g_pmeshSphere = NULL;      // Representation of point light

CGlareDef                   g_GlareDef;         // Glare defintion
EGLARELIBTYPE               g_eGlareType;       // Enumerated glare type

D3DXVECTOR4 g_avLightPosition[NUM_LIGHTS];    // Light positions in world space
D3DXVECTOR4 g_avLightIntensity[NUM_LIGHTS];   // Light floating point intensities
int g_nLightLogIntensity[NUM_LIGHTS]; // Light intensities on a log scale
int g_nLightMantissa[NUM_LIGHTS];     // Mantissa of the light intensity

DWORD                       g_dwCropWidth;    // Width of the cropped scene texture
DWORD                       g_dwCropHeight;   // Height of the cropped scene texture

float                       g_fKeyValue;              // Middle gray key value for tone mapping

bool                        g_bToneMap;               // True when scene is to be tone mapped            
bool                        g_bDetailedStats;         // True when state variables should be rendered
bool                        g_bDrawHelp;              // True when help instructions are to be drawn
bool                        g_bBlueShift;             // True when blue shift is to be factored in
bool                        g_bAdaptationInvalid;     // True when adaptation level needs refreshing
bool                        g_bUseMultiSampleFloat16 = false; // True when using multisampling on a floating point back buffer
D3DMULTISAMPLE_TYPE         g_MaxMultiSampleType = D3DMULTISAMPLE_NONE;  // Non-Zero when g_bUseMultiSampleFloat16 is true
DWORD                       g_dwMultiSampleQuality = 0; // Non-Zero when we have multisampling on a float backbuffer
bool                        g_bSupportsD16 = false;
bool                        g_bSupportsD32 = false;
bool                        g_bSupportsD24X8 = false;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC              -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_GLARETYPE           5
#define IDC_LIGHT0_LABEL        6
#define IDC_LIGHT1_LABEL        7
#define IDC_LIGHT0              8
#define IDC_LIGHT1              9
#define IDC_KEYVALUE            10
#define IDC_KEYVALUE_LABEL      11
#define IDC_TONEMAP             12
#define IDC_BLUESHIFT           13
#define IDC_RESET               14


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();

// Scene geometry initialization routines
void SetTextureCoords( WorldVertex* pVertex, float u, float v );
HRESULT BuildWorldMesh();
HRESULT BuildColumn( WorldVertex*& pV, float x, float y, float z, float width );


// Post-processing source textures creation
HRESULT Scene_To_SceneScaled();
HRESULT SceneScaled_To_BrightPass();
HRESULT BrightPass_To_StarSource();
HRESULT StarSource_To_BloomSource();


// Post-processing helper functions
HRESULT GetTextureRect( PDIRECT3DTEXTURE9 pTexture, RECT* pRect );
HRESULT GetTextureCoords( PDIRECT3DTEXTURE9 pTexSrc, RECT* pRectSrc, PDIRECT3DTEXTURE9 pTexDest, RECT* pRectDest,
                          CoordRect* pCoords );


// Sample offset calculation. These offsets are passed to corresponding
// pixel shaders.
HRESULT GetSampleOffsets_GaussBlur5x5( DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, D3DXVECTOR2* avTexCoordOffset,
                                       D3DXVECTOR4* avSampleWeights, FLOAT fMultiplier = 1.0f );
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize, float afTexCoordOffset[15], D3DXVECTOR4* avColorWeight,
                                float fDeviation, FLOAT fMultiplier=1.0f );
HRESULT GetSampleOffsets_Star( DWORD dwD3DTexSize, float afTexCoordOffset[15], D3DXVECTOR4* avColorWeight,
                               float fDeviation );
HRESULT GetSampleOffsets_DownScale4x4( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] );
HRESULT GetSampleOffsets_DownScale2x2( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] );


// Tone mapping and post-process lighting effects
HRESULT MeasureLuminance();
HRESULT CalculateAdaptation();
HRESULT RenderStar();
HRESULT RenderBloom();


// Methods to control scene lights
HRESULT AdjustLight( UINT iLight, bool bIncrement );
HRESULT RefreshLights();

HRESULT RenderScene();
void RenderText();
HRESULT ClearTexture( LPDIRECT3DTEXTURE9 pTexture );

VOID ResetOptions();
VOID DrawFullScreenQuad( float fLeftU, float fTopV, float fRightU, float fBottomV );
VOID DrawFullScreenQuad( CoordRect c )
{
    DrawFullScreenQuad( c.fLeftU, c.fTopV, c.fRightU, c.fBottomV );
}

LRESULT MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

static inline float GaussianDistribution( float x, float y, float rho );




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
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTCreateWindow( L"HDRLighting" );

    // Although multisampling is supported for render target surfaces, those surfaces
    // exist without a parent texture, and must therefore be copied to a texture 
    // surface if they're to be used as a source texture. This sample relies upon 
    // several render targets being used as source textures, and therefore it makes
    // sense to disable multisampling altogether.
    CGrowableArray <D3DMULTISAMPLE_TYPE>* pMultiSampleTypeList =
        DXUTGetD3D9Enumeration()->GetPossibleMultisampleTypeList();
    pMultiSampleTypeList->RemoveAll();
    pMultiSampleTypeList->Add( D3DMULTISAMPLE_NONE );
    DXUTGetD3D9Enumeration()->SetMultisampleQualityMax( 0 );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
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
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_RESET, L"Reset Options (R)", 35, iY += 24, 125, 22, L'R' );

    // Title font for comboboxes
    g_SampleUI.SetFont( 1, L"Arial", 14, FW_BOLD );
    CDXUTElement* pElement = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
    if( pElement )
    {
        pElement->iFont = 1;
        pElement->dwTextFormat = DT_LEFT | DT_BOTTOM;
    }

    CDXUTComboBox* pComboBox = NULL;
    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.AddStatic( IDC_STATIC, L"(G)lare Type", 35, iY += 24, 125, 22 );
    g_SampleUI.AddComboBox( IDC_GLARETYPE, 35, iY += 24, 125, 22, L'G', false, &pComboBox );
    g_SampleUI.AddStatic( IDC_LIGHT0_LABEL, L"Light 0", 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LIGHT0, 35, iY += 24, 100, 22, 0, 63 );
    g_SampleUI.AddStatic( IDC_LIGHT1_LABEL, L"Light 1", 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LIGHT1, 35, iY += 24, 100, 22, 0, 63 );
    g_SampleUI.AddStatic( IDC_KEYVALUE_LABEL, L"Key Value", 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_KEYVALUE, 35, iY += 24, 100, 22, 0, 100 );
    g_SampleUI.AddCheckBox( IDC_TONEMAP, L"(T)one Mapping", 35, iY += 34, 125, 20, true, L'T' );
    g_SampleUI.AddCheckBox( IDC_BLUESHIFT, L"(B)lue Shift", 35, iY += 24, 125, 20, true, L'B' );

    if( pComboBox )
    {
        pComboBox->AddItem( L"Disable", ( void* )GLT_DISABLE );
        pComboBox->AddItem( L"Camera", ( void* )GLT_CAMERA );
        pComboBox->AddItem( L"Natural Bloom", ( void* )GLT_NATURAL );
        pComboBox->AddItem( L"Cheap Lens", ( void* )GLT_CHEAPLENS );
        pComboBox->AddItem( L"Cross Screen", ( void* )GLT_FILTER_CROSSSCREEN );
        pComboBox->AddItem( L"Spectral Cross", ( void* )GLT_FILTER_CROSSSCREEN_SPECTRAL );
        pComboBox->AddItem( L"Snow Cross", ( void* )GLT_FILTER_SNOWCROSS );
        pComboBox->AddItem( L"Spectral Snow", ( void* )GLT_FILTER_SNOWCROSS_SPECTRAL );
        pComboBox->AddItem( L"Sunny Cross", ( void* )GLT_FILTER_SUNNYCROSS );
        pComboBox->AddItem( L"Spectral Sunny", ( void* )GLT_FILTER_SUNNYCROSS_SPECTRAL );
        pComboBox->AddItem( L"Cinema Vertical", ( void* )GLT_CINECAM_VERTICALSLITS );
        pComboBox->AddItem( L"Cinema Horizontal", ( void* )GLT_CINECAM_HORIZONTALSLITS );
    }


    /*
      g_SampleUI.AddComboBox( 19, 35, iY += 24, 125, 22 );
      g_SampleUI.GetComboBox( 19 )->AddItem( L"Text1", NULL );
      g_SampleUI.GetComboBox( 19 )->AddItem( L"Text2", NULL );
      g_SampleUI.GetComboBox( 19 )->AddItem( L"Text3", NULL );
      g_SampleUI.GetComboBox( 19 )->AddItem( L"Text4", NULL );
      g_SampleUI.AddCheckBox( 21, L"Checkbox1", 35, iY += 24, 125, 22 );
      g_SampleUI.AddCheckBox( 11, L"Checkbox2", 35, iY += 24, 125, 22 );
      g_SampleUI.AddRadioButton( 12, 1, L"Radio1G1", 35, iY += 24, 125, 22 );
      g_SampleUI.AddRadioButton( 13, 1, L"Radio2G1", 35, iY += 24, 125, 22 );
      g_SampleUI.AddRadioButton( 14, 1, L"Radio3G1", 35, iY += 24, 125, 22 );
      g_SampleUI.GetRadioButton( 14 )->SetChecked( true ); 
      g_SampleUI.AddButton( 17, L"Button1", 35, iY += 24, 125, 22 );
      g_SampleUI.AddButton( 18, L"Button2", 35, iY += 24, 125, 22 );
      g_SampleUI.AddRadioButton( 15, 2, L"Radio1G2", 35, iY += 24, 125, 22 );
      g_SampleUI.AddRadioButton( 16, 2, L"Radio2G3", 35, iY += 24, 125, 22 );
      g_SampleUI.GetRadioButton( 16 )->SetChecked( true );
      g_SampleUI.AddSlider( 20, 50, iY += 24, 100, 22 );
      g_SampleUI.GetSlider( 20 )->SetRange( 0, 100 );
      g_SampleUI.GetSlider( 20 )->SetValue( 50 );
      //    g_SampleUI.AddEditBox( 20, L"Test", 35, iY += 24, 125, 22 );
     */

    // Set light positions in world space
    g_avLightPosition[0] = D3DXVECTOR4( 4.0f, 2.0f, 18.0f, 1.0f );
    g_avLightPosition[1] = D3DXVECTOR4( 11.0f, 2.0f, 18.0f, 1.0f );

    ResetOptions();
}


//-----------------------------------------------------------------------------
// Name: ResetOptions()
// Desc: Reset all user-controlled options to default values
//-----------------------------------------------------------------------------
void ResetOptions()
{
    g_bDrawHelp = TRUE;
    g_bDetailedStats = TRUE;

    g_SampleUI.EnableNonUserEvents( true );

    g_SampleUI.GetCheckBox( IDC_TONEMAP )->SetChecked( true );
    g_SampleUI.GetCheckBox( IDC_BLUESHIFT )->SetChecked( true );
    g_SampleUI.GetSlider( IDC_LIGHT0 )->SetValue( 52 );
    g_SampleUI.GetSlider( IDC_LIGHT1 )->SetValue( 43 );
    g_SampleUI.GetSlider( IDC_KEYVALUE )->SetValue( 18 );
    g_SampleUI.GetComboBox( IDC_GLARETYPE )->SetSelectedByData( ( void* )GLT_DEFAULT );

    g_SampleUI.EnableNonUserEvents( false );
}


//-----------------------------------------------------------------------------
// Name: AdjustLight
// Desc: Increment or decrement the light at the given index
//-----------------------------------------------------------------------------
HRESULT AdjustLight( UINT iLight, bool bIncrement )
{
    if( iLight >= NUM_LIGHTS )
        return E_INVALIDARG;

    if( bIncrement && g_nLightLogIntensity[iLight] < 7 )
    {
        g_nLightMantissa[iLight]++;
        if( g_nLightMantissa[iLight] > 9 )
        {
            g_nLightMantissa[iLight] = 1;
            g_nLightLogIntensity[iLight]++;
        }
    }

    if( !bIncrement && g_nLightLogIntensity[iLight] > -4 )
    {
        g_nLightMantissa[iLight]--;
        if( g_nLightMantissa[iLight] < 1 )
        {
            g_nLightMantissa[iLight] = 9;
            g_nLightLogIntensity[iLight]--;
        }
    }

    RefreshLights();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RefreshLights
// Desc: Set the light intensities to match the current log luminance
//-----------------------------------------------------------------------------
HRESULT RefreshLights()
{
    CDXUTStatic* pStatic = NULL;
    WCHAR strBuffer[256] = {0};

    for( int i = 0; i < NUM_LIGHTS; i++ )
    {
        g_avLightIntensity[i].x = g_nLightMantissa[i] * ( float )pow( 10.0f, g_nLightLogIntensity[i] );
        g_avLightIntensity[i].y = g_nLightMantissa[i] * ( float )pow( 10.0f, g_nLightLogIntensity[i] );
        g_avLightIntensity[i].z = g_nLightMantissa[i] * ( float )pow( 10.0f, g_nLightLogIntensity[i] );
        g_avLightIntensity[i].w = 1.0f;

        swprintf_s( strBuffer, 255, L"Light %d: %d.0e%d", i, g_nLightMantissa[i], g_nLightLogIntensity[i] );
        pStatic = g_SampleUI.GetStatic( IDC_LIGHT0_LABEL + i );
        pStatic->SetText( strBuffer );

    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // No fallback yet, so need to support D3DFMT_A16B16G16R16F render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal,
                                         pCaps->DeviceType,
                                         AdapterFormat,
                                         D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE,
                                         D3DFMT_A16B16G16R16F ) ) )
    {
        return false;
    }

    // No fallback yet, so need to support D3DFMT_R32F or D3DFMT_R16F render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_R32F ) ) )
    {
        if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                             AdapterFormat, D3DUSAGE_RENDERTARGET,
                                             D3DRTYPE_TEXTURE, D3DFMT_R16F ) ) )
            return false;
    }

    // Need to support post-pixel processing
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat,
                                         D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_SURFACE, BackBufferFormat ) ) )
    {
        return false;
    }


    return true;
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    IDirect3D9* pD3D = DXUTGetD3D9Object();
    HRESULT hr;
    D3DCAPS9 caps;
    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &caps ) );

    // If device doesn't support HW T&L or doesn't support 2.0 vertex shaders in HW 
    // then switch to SWVP.
    if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
        caps.VertexShaderVersion < D3DVS_VERSION( 2, 0 ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
        pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif
#ifdef DEBUG_PS
    pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnDestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    HRESULT hr;


    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );
    // Determine which of D3DFMT_R16F or D3DFMT_R32F to use for luminance texture
    D3DCAPS9 Caps;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( !pD3D )
        return E_FAIL;
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDeviceCaps( &Caps );
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );
    // IsDeviceAcceptable already ensured that one of D3DFMT_R16F or D3DFMT_R32F is available.
    if( FAILED( pD3D->CheckDeviceFormat( Caps.AdapterOrdinal, Caps.DeviceType,
                                         DisplayMode.Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_TEXTURE, D3DFMT_R16F ) ) )
        g_LuminanceFormat = D3DFMT_R32F;
    else
        g_LuminanceFormat = D3DFMT_R16F;

    // Determine whether we can support multisampling on a A16B16G16R16F render target
    g_bUseMultiSampleFloat16 = false;
    g_MaxMultiSampleType = D3DMULTISAMPLE_NONE;
    DXUTDeviceSettings settings = DXUTGetDeviceSettings();
    for( D3DMULTISAMPLE_TYPE imst = D3DMULTISAMPLE_2_SAMPLES; imst <= D3DMULTISAMPLE_16_SAMPLES;
         imst = ( D3DMULTISAMPLE_TYPE )( imst + 1 ) )
    {
        DWORD msQuality = 0;
        if( SUCCEEDED( pD3D->CheckDeviceMultiSampleType( Caps.AdapterOrdinal,
                                                         Caps.DeviceType,
                                                         D3DFMT_A16B16G16R16F,
                                                         settings.d3d9.pp.Windowed,
                                                         imst, &msQuality ) ) )
        {
            g_bUseMultiSampleFloat16 = true;
            g_MaxMultiSampleType = imst;
            if( msQuality > 0 )
                g_dwMultiSampleQuality = msQuality - 1;
            else
                g_dwMultiSampleQuality = msQuality;
        }
    }

    g_bSupportsD16 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D16 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, D3DFMT_A16B16G16R16F,
                                                     D3DFMT_D16 ) ) )
        {
            g_bSupportsD16 = true;
        }
    }
    g_bSupportsD32 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D32 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, D3DFMT_A16B16G16R16F,
                                                     D3DFMT_D32 ) ) )
        {
            g_bSupportsD32 = true;
        }
    }
    g_bSupportsD24X8 = false;
    if( SUCCEEDED( pD3D->CheckDeviceFormat( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                            settings.d3d9.AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) )
    {
        if( SUCCEEDED( pD3D->CheckDepthStencilMatch( settings.d3d9.AdapterOrdinal, settings.d3d9.DeviceType,
                                                     settings.d3d9.AdapterFormat, D3DFMT_A16B16G16R16F,
                                                     D3DFMT_D24X8 ) ) )
        {
            g_bSupportsD24X8 = true;
        }
    }

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the 
    // shader debugger. Debugging vertex shaders requires either REF or software vertex 
    // processing, and debugging pixel shaders requires REF.  The 
    // D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug experience in the 
    // shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile 
    // against the next higher available software target, which ensures that the 
    // unoptimized shaders do not exceed the shader model limitations.  Setting these 
    // flags will cause slower rendering since the shaders will be unoptimized and 
    // forced into software.  See the DirectX documentation for more information about 
    // using the shader debugger.
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"HDRLighting.fx" ) );

    // If this fails, there should be debug output as to 
    // they the .fx file failed to compile
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
                                        NULL, &g_pEffect, NULL ) );

    // Initialize the camera
    D3DXVECTOR3 vFromPt( 7.5f, 1.8f, 2 );
    D3DXVECTOR3 vLookatPt( 7.5f, 1.5f, 10.0f );
    g_Camera.SetViewParams( &vFromPt, &vLookatPt );

    // Set options for the first-person camera
    g_Camera.SetScalers( 0.01f, 15.0f );
    g_Camera.SetDrag( true );
    g_Camera.SetEnableYAxisMovement( false );

    D3DXVECTOR3 vMin = D3DXVECTOR3( 1.5f, 0.0f, 1.5f );
    D3DXVECTOR3 vMax = D3DXVECTOR3( 13.5f, 10.0f, 18.5f );
    g_Camera.SetClipToBoundary( TRUE, &vMin, &vMax );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also optimizes the 
// mesh for the graphics card's vertex cache, which improves performance by organizing 
// the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh )
{
    ID3DXMesh* pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    // Load the mesh with D3DX and get back a ID3DXMesh*.  For this
    // sample we'll ignore the X file's embedded materials since we know 
    // exactly the model we're loading.  See the mesh samples such as
    // "OptimizedMesh" for a more generic mesh loading example.
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );

    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL, NULL, NULL, NULL, &pMesh ) );

    DWORD* rgdwAdjacency = NULL;

    // Make sure there are normals which are required for lighting
    if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
    {
        ID3DXMesh* pTempMesh;
        V( pMesh->CloneMeshFVF( pMesh->GetOptions(),
                                pMesh->GetFVF() | D3DFVF_NORMAL,
                                pd3dDevice, &pTempMesh ) );
        V( D3DXComputeNormals( pTempMesh, NULL ) );

        SAFE_RELEASE( pMesh );
        pMesh = pTempMesh;
    }

    // Optimize the mesh for this graphics card's vertex cache 
    // so when rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader 
    // on those vertices so it will improve perf.     
    rgdwAdjacency = new DWORD[pMesh->GetNumFaces() * 3];
    if( rgdwAdjacency == NULL )
        return E_OUTOFMEMORY;
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );
    V( pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL ) );
    delete []rgdwAdjacency;

    *ppMesh = pMesh;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// reset, which will happen after a lost device scenario. This is the best location to 
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever 
// the device is lost. Resources created here should be released in the OnLostDevice 
// callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );
    int i = 0; // loop variable

    g_pd3dDevice = pd3dDevice;

    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pEffect )
        V_RETURN( g_pEffect->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    const D3DSURFACE_DESC* pBackBufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();


    // Create the Multi-Sample floating point render target
    D3DFORMAT dfmt = D3DFMT_UNKNOWN;
    if( g_bSupportsD16 )
        dfmt = D3DFMT_D16;
    else if( g_bSupportsD32 )
        dfmt = D3DFMT_D32;
    else if( g_bSupportsD24X8 )
        dfmt = D3DFMT_D24X8;
    else
        g_bUseMultiSampleFloat16 = false;

    if( g_bUseMultiSampleFloat16 )
    {
        hr = g_pd3dDevice->CreateRenderTarget( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                               D3DFMT_A16B16G16R16F,
                                               g_MaxMultiSampleType, g_dwMultiSampleQuality,
                                               FALSE, &g_pFloatMSRT, NULL );
        if( FAILED( hr ) )
            g_bUseMultiSampleFloat16 = false;
        else
        {
            hr = g_pd3dDevice->CreateDepthStencilSurface( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                                          dfmt,
                                                          g_MaxMultiSampleType, g_dwMultiSampleQuality,
                                                          TRUE, &g_pFloatMSDS, NULL );
            if( FAILED( hr ) )
            {
                g_bUseMultiSampleFloat16 = false;
                SAFE_RELEASE( g_pFloatMSRT );
            }
        }
    }

    // Crop the scene texture so width and height are evenly divisible by 8.
    // This cropped version of the scene will be used for post processing effects,
    // and keeping everything evenly divisible allows precise control over
    // sampling points within the shaders.
    g_dwCropWidth = pBackBufferDesc->Width - pBackBufferDesc->Width % 8;
    g_dwCropHeight = pBackBufferDesc->Height - pBackBufferDesc->Height % 8;

    // Create the HDR scene texture
    hr = g_pd3dDevice->CreateTexture( pBackBufferDesc->Width, pBackBufferDesc->Height,
                                      1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
                                      D3DPOOL_DEFAULT, &g_pTexScene, NULL );
    if( FAILED( hr ) )
        return hr;


    // Scaled version of the HDR scene texture
    hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 4, g_dwCropHeight / 4,
                                      1, D3DUSAGE_RENDERTARGET,
                                      D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT,
                                      &g_pTexSceneScaled, NULL );
    if( FAILED( hr ) )
        return hr;


    // Create the bright-pass filter texture. 
    // Texture has a black border of single texel thickness to fake border 
    // addressing using clamp addressing
    hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 4 + 2, g_dwCropHeight / 4 + 2,
                                      1, D3DUSAGE_RENDERTARGET,
                                      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &g_pTexBrightPass, NULL );
    if( FAILED( hr ) )
        return hr;



    // Create a texture to be used as the source for the star effect
    // Texture has a black border of single texel thickness to fake border 
    // addressing using clamp addressing
    hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 4 + 2, g_dwCropHeight / 4 + 2,
                                      1, D3DUSAGE_RENDERTARGET,
                                      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &g_pTexStarSource, NULL );
    if( FAILED( hr ) )
        return hr;



    // Create a texture to be used as the source for the bloom effect
    // Texture has a black border of single texel thickness to fake border 
    // addressing using clamp addressing
    hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 8 + 2, g_dwCropHeight / 8 + 2,
                                      1, D3DUSAGE_RENDERTARGET,
                                      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      &g_pTexBloomSource, NULL );
    if( FAILED( hr ) )
        return hr;




    // Create a 2 textures to hold the luminance that the user is currently adapted
    // to. This allows for a simple simulation of light adaptation.
    hr = g_pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_RENDERTARGET,
                                      g_LuminanceFormat, D3DPOOL_DEFAULT,
                                      &g_pTexAdaptedLuminanceCur, NULL );
    if( FAILED( hr ) )
        return hr;
    hr = g_pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_RENDERTARGET,
                                      g_LuminanceFormat, D3DPOOL_DEFAULT,
                                      &g_pTexAdaptedLuminanceLast, NULL );
    if( FAILED( hr ) )
        return hr;


    // For each scale stage, create a texture to hold the intermediate results
    // of the luminance calculation
    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        int iSampleLen = 1 << ( 2 * i );

        hr = g_pd3dDevice->CreateTexture( iSampleLen, iSampleLen, 1, D3DUSAGE_RENDERTARGET,
                                          g_LuminanceFormat, D3DPOOL_DEFAULT,
                                          &g_apTexToneMap[i], NULL );
        if( FAILED( hr ) )
            return hr;
    }


    // Create the temporary blooming effect textures
    // Texture has a black border of single texel thickness to fake border 
    // addressing using clamp addressing
    for( i = 1; i < NUM_BLOOM_TEXTURES; i++ )
    {
        hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 8 + 2, g_dwCropHeight / 8 + 2,
                                          1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, &g_apTexBloom[i], NULL );
        if( FAILED( hr ) )
            return hr;
    }

    // Create the final blooming effect texture
    hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 8, g_dwCropHeight / 8,
                                      1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &g_apTexBloom[0], NULL );
    if( FAILED( hr ) )
        return hr;


    // Create the star effect textures
    for( i = 0; i < NUM_STAR_TEXTURES; i++ )
    {
        hr = g_pd3dDevice->CreateTexture( g_dwCropWidth / 4, g_dwCropHeight / 4,
                                          1, D3DUSAGE_RENDERTARGET,
                                          D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT,
                                          &g_apTexStar[i], NULL );

        if( FAILED( hr ) )
            return hr;
    }

    // Create a texture to paint the walls
    TCHAR Path[MAX_PATH];
    DXUTFindDXSDKMediaFileCch( Path, MAX_PATH, TEXT( "misc\\env2.bmp" ) );

    hr = D3DXCreateTextureFromFile( g_pd3dDevice, Path, &g_pTexWall );
    if( FAILED( hr ) )
        return hr;


    // Create a texture to paint the floor
    DXUTFindDXSDKMediaFileCch( Path, MAX_PATH, TEXT( "misc\\ground2.bmp" ) );

    hr = D3DXCreateTextureFromFile( g_pd3dDevice, Path, &g_pTexFloor );
    if( FAILED( hr ) )
        return hr;


    // Create a texture to paint the ceiling
    DXUTFindDXSDKMediaFileCch( Path, MAX_PATH, TEXT( "misc\\seafloor.bmp" ) );

    hr = D3DXCreateTextureFromFile( g_pd3dDevice, Path, &g_pTexCeiling );
    if( FAILED( hr ) )
        return hr;


    // Create a texture for the paintings
    DXUTFindDXSDKMediaFileCch( Path, MAX_PATH, TEXT( "misc\\env3.bmp" ) );

    hr = D3DXCreateTextureFromFile( g_pd3dDevice, Path, &g_pTexPainting );
    if( FAILED( hr ) )
        return hr;


    // Textures with borders must be cleared since scissor rect testing will
    // be used to avoid rendering on top of the border
    ClearTexture( g_pTexAdaptedLuminanceCur );
    ClearTexture( g_pTexAdaptedLuminanceLast );
    ClearTexture( g_pTexBloomSource );
    ClearTexture( g_pTexBrightPass );
    ClearTexture( g_pTexStarSource );

    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        ClearTexture( g_apTexBloom[i] );
    }


    // Build the world object
    hr = BuildWorldMesh();
    if( FAILED( hr ) )
        return hr;


    // Create sphere mesh to represent the light
    hr = LoadMesh( g_pd3dDevice, TEXT( "misc\\sphere0.x" ), &g_pmeshSphere );
    if( FAILED( hr ) )
        return hr;

    // Setup the camera's projection parameters
    float fAspectRatio = ( ( FLOAT )g_dwCropWidth ) / g_dwCropHeight;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.2f, 30.0f );
    D3DXMATRIX mProjection = *g_Camera.GetProjMatrix();

    // Set effect file variables
    g_pEffect->SetMatrix( "g_mProjection", &mProjection );
    g_pEffect->SetFloat( "g_fBloomScale", 1.0f );
    g_pEffect->SetFloat( "g_fStarScale", 0.5f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: ClearTexture()
// Desc: Helper function for RestoreDeviceObjects to clear a texture surface
//-----------------------------------------------------------------------------
HRESULT ClearTexture( LPDIRECT3DTEXTURE9 pTexture )
{
    HRESULT hr = S_OK;
    PDIRECT3DSURFACE9 pSurface = NULL;

    hr = pTexture->GetSurfaceLevel( 0, &pSurface );
    if( SUCCEEDED( hr ) )
        g_pd3dDevice->ColorFill( pSurface, NULL, D3DCOLOR_ARGB( 0, 0, 0, 0 ) );

    SAFE_RELEASE( pSurface );
    return hr;
}


//-----------------------------------------------------------------------------
// Name: BuildWorldMesh()
// Desc: Creates the wall, floor, ceiling, columns, and painting mesh
//-----------------------------------------------------------------------------
HRESULT BuildWorldMesh()
{
    HRESULT hr;
    UINT i; // Loop variable

    const FLOAT fWidth = 15.0f;
    const FLOAT fDepth = 20.0f;
    const FLOAT fHeight = 3.0f;

    // Create the room  
    LPD3DXMESH pWorldMeshTemp = NULL;
    hr = D3DXCreateMeshFVF( 48, 96, 0, WorldVertex::FVF, g_pd3dDevice, &pWorldMeshTemp );
    if( FAILED( hr ) )
        goto LCleanReturn;

    WorldVertex* pVertex;
    hr = pWorldMeshTemp->LockVertexBuffer( 0, ( PVOID* )&pVertex );
    if( FAILED( hr ) )
        goto LCleanReturn;

    WorldVertex* pV;
    pV = pVertex;

    // Front wall
    SetTextureCoords( pV, 7.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, fDepth );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, fDepth );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, fDepth );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, fDepth );

    // Right wall
    SetTextureCoords( pV, 10.5f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, fDepth );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, fDepth );

    // Back wall
    SetTextureCoords( pV, 7.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, 0.0f );

    // Left wall
    SetTextureCoords( pV, 10.5f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, fDepth );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, fDepth );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    BuildColumn( pV, 4.0f, fHeight, 7.0f, 0.75f );
    BuildColumn( pV, 4.0f, fHeight, 13.0f, 0.75f );
    BuildColumn( pV, 11.0f, fHeight, 7.0f, 0.75f );
    BuildColumn( pV, 11.0f, fHeight, 13.0f, 0.75f );

    // Floor
    SetTextureCoords( pV, 7.0f, 7.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, fDepth );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, fDepth );
    ( pV++ )->p = D3DXVECTOR3( fWidth, 0.0f, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    // Ceiling
    SetTextureCoords( pV, 7.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, 0.0f );
    ( pV++ )->p = D3DXVECTOR3( fWidth, fHeight, fDepth );
    ( pV++ )->p = D3DXVECTOR3( 0.0f, fHeight, fDepth );

    // Painting 1
    SetTextureCoords( pV, 1.0f, 1.0f );
    ( pV++ )->p = D3DXVECTOR3( 2.0f, fHeight - 0.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 6.0f, fHeight - 0.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 6.0f, fHeight - 2.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 2.0f, fHeight - 2.5f, fDepth - 0.01f );

    // Painting 2
    SetTextureCoords( pV, 1.0f, 1.0f );
    ( pV++ )->p = D3DXVECTOR3( 9.0f, fHeight - 0.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 13.0f, fHeight - 0.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 13.0f, fHeight - 2.5f, fDepth - 0.01f );
    ( pV++ )->p = D3DXVECTOR3( 9.0f, fHeight - 2.5f, fDepth - 0.01f );

    pWorldMeshTemp->UnlockVertexBuffer();

    // Retrieve the indices
    WORD* pIndex;
    hr = pWorldMeshTemp->LockIndexBuffer( 0, ( PVOID* )&pIndex );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( i = 0; i < pWorldMeshTemp->GetNumFaces() / 2; i++ )
    {
        *pIndex++ = (WORD)( ( i * 4 ) + 0 );
        *pIndex++ = (WORD)( ( i * 4 ) + 1 );
        *pIndex++ = (WORD)( ( i * 4 ) + 2 );
        *pIndex++ = (WORD)( ( i * 4 ) + 0 );
        *pIndex++ = (WORD)( ( i * 4 ) + 2 );
        *pIndex++ = (WORD)( ( i * 4 ) + 3 );
    }

    pWorldMeshTemp->UnlockIndexBuffer();

    // Set attribute groups to draw floor, ceiling, walls, and paintings
    // separately, with different shader constants. These group numbers
    // will be used during the calls to DrawSubset().
    DWORD* pAttribs;
    hr = pWorldMeshTemp->LockAttributeBuffer( 0, &pAttribs );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( i = 0; i < 40; i++ )
        *pAttribs++ = 0;

    for( i = 0; i < 2; i++ )
        *pAttribs++ = 1;

    for( i = 0; i < 2; i++ )
        *pAttribs++ = 2;

    for( i = 0; i < 4; i++ )
        *pAttribs++ = 3;

    pWorldMeshTemp->UnlockAttributeBuffer();
    D3DXComputeNormals( pWorldMeshTemp, NULL );

    // Optimize the mesh
    hr = pWorldMeshTemp->CloneMeshFVF( D3DXMESH_VB_WRITEONLY | D3DXMESH_IB_WRITEONLY,
                                       WorldVertex::FVF, g_pd3dDevice, &g_pWorldMesh );
    if( FAILED( hr ) )
        goto LCleanReturn;

    hr = S_OK;

LCleanReturn:
    SAFE_RELEASE( pWorldMeshTemp );
    return hr;
}

//-----------------------------------------------------------------------------
// Name: BuildColumn()
// Desc: Helper function for BuildWorldMesh to add column quads to the scene 
//-----------------------------------------------------------------------------
HRESULT BuildColumn( WorldVertex*& pV, float x, float y, float z, float width )
{
    float w = width / 2;

    SetTextureCoords( pV, 1.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( x - w, y, z - w );
    ( pV++ )->p = D3DXVECTOR3( x + w, y, z - w );
    ( pV++ )->p = D3DXVECTOR3( x + w, 0.0f, z - w );
    ( pV++ )->p = D3DXVECTOR3( x - w, 0.0f, z - w );

    SetTextureCoords( pV, 1.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( x + w, y, z - w );
    ( pV++ )->p = D3DXVECTOR3( x + w, y, z + w );
    ( pV++ )->p = D3DXVECTOR3( x + w, 0.0f, z + w );
    ( pV++ )->p = D3DXVECTOR3( x + w, 0.0f, z - w );

    SetTextureCoords( pV, 1.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( x + w, y, z + w );
    ( pV++ )->p = D3DXVECTOR3( x - w, y, z + w );
    ( pV++ )->p = D3DXVECTOR3( x - w, 0.0f, z + w );
    ( pV++ )->p = D3DXVECTOR3( x + w, 0.0f, z + w );

    SetTextureCoords( pV, 1.0f, 2.0f );
    ( pV++ )->p = D3DXVECTOR3( x - w, y, z + w );
    ( pV++ )->p = D3DXVECTOR3( x - w, y, z - w );
    ( pV++ )->p = D3DXVECTOR3( x - w, 0.0f, z - w );
    ( pV++ )->p = D3DXVECTOR3( x - w, 0.0f, z + w );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: SetTextureCoords()
// Desc: Helper function for BuildWorldMesh to set texture coordinates
//       for vertices
//-----------------------------------------------------------------------------
void SetTextureCoords( WorldVertex* pVertex, float u, float v )
{
    ( pVertex++ )->t = D3DXVECTOR2( 0.0f, 0.0f );
    ( pVertex++ )->t = D3DXVECTOR2( u, 0.0f );
    ( pVertex++ )->t = D3DXVECTOR2( u, v );
    ( pVertex++ )->t = D3DXVECTOR2( 0.0f, v );
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    // Set the flag to refresh the user's simulated adaption level.
    // Frame move is not called when the scene is paused or single-stepped. 
    // If the scene is paused, the user's adaptation level needs to remain
    // unchanged.
    g_bAdaptationInvalid = true;

    // Calculate the position of the lights in view space
    D3DXVECTOR4 avLightViewPosition[NUM_LIGHTS];
    for( int iLight = 0; iLight < NUM_LIGHTS; iLight++ )
    {
        D3DXMATRIX mView = *g_Camera.GetViewMatrix();
        D3DXVec4Transform( &avLightViewPosition[iLight], &g_avLightPosition[iLight], &mView );
    }

    // Set frame shader constants
    g_pEffect->SetBool( "g_bEnableToneMap", g_bToneMap );
    g_pEffect->SetBool( "g_bEnableBlueShift", g_bBlueShift );
    g_pEffect->SetValue( "g_avLightPositionView", avLightViewPosition, sizeof( D3DXVECTOR4 ) * NUM_LIGHTS );
    g_pEffect->SetValue( "g_avLightIntensity", g_avLightIntensity, sizeof( D3DXVECTOR4 ) * NUM_LIGHTS );

}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;

    PDIRECT3DSURFACE9 pSurfLDR; // Low dynamic range surface for final output
    PDIRECT3DSURFACE9 pSurfDS;  // Low dynamic range depth stencil surface
    PDIRECT3DSURFACE9 pSurfHDR; // High dynamic range surface to store 
    // intermediate floating point color values

    // Store the old render target
    V( g_pd3dDevice->GetRenderTarget( 0, &pSurfLDR ) );
    V( g_pd3dDevice->GetDepthStencilSurface( &pSurfDS ) );

    // Setup HDR render target
    V( g_pTexScene->GetSurfaceLevel( 0, &pSurfHDR ) );
    if( g_bUseMultiSampleFloat16 )
    {
        V( g_pd3dDevice->SetRenderTarget( 0, g_pFloatMSRT ) );
        V( g_pd3dDevice->SetDepthStencilSurface( g_pFloatMSDS ) );
    }
    else
    {
        V( g_pd3dDevice->SetRenderTarget( 0, pSurfHDR ) );
    }

    // Clear the viewport
    V( g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA( 0, 0, 0, 0 ), 1.0f, 0L ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Render the HDR Scene
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Scene" );
            RenderScene();
        }

        // If using floating point multi sampling, stretchrect to the rendertarget
        if( g_bUseMultiSampleFloat16 )
        {
            V( g_pd3dDevice->StretchRect( g_pFloatMSRT, NULL, pSurfHDR, NULL, D3DTEXF_NONE ) );
            V( g_pd3dDevice->SetRenderTarget( 0, pSurfHDR ) );
            V( g_pd3dDevice->SetDepthStencilSurface( pSurfDS ) );
        }

        // Create a scaled copy of the scene
        Scene_To_SceneScaled();

        // Setup tone mapping technique
        if( g_bToneMap )
            MeasureLuminance();

        // If FrameMove has been called, the user's adaptation level has also changed
        // and should be updated
        if( g_bAdaptationInvalid )
        {
            // Clear the update flag
            g_bAdaptationInvalid = false;

            // Calculate the current luminance adaptation level
            CalculateAdaptation();
        }

        // Now that luminance information has been gathered, the scene can be bright-pass filtered
        // to remove everything except bright lights and reflections.
        SceneScaled_To_BrightPass();

        // Blur the bright-pass filtered image to create the source texture for the star effect
        BrightPass_To_StarSource();

        // Scale-down the source texture for the star effect to create the source texture
        // for the bloom effect
        StarSource_To_BloomSource();

        // Render post-process lighting effects
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Bloom" );
            RenderBloom();
        }
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Star" );
            RenderStar();
        }

        // Draw the high dynamic range scene texture to the low dynamic range
        // back buffer. As part of this final pass, the scene will be tone-mapped
        // using the user's current adapted luminance, blue shift will occur
        // if the scene is determined to be very dark, and the post-process lighting
        // effect textures will be added to the scene.
        UINT uiPassCount, uiPass;

        V( g_pEffect->SetTechnique( "FinalScenePass" ) );
        V( g_pEffect->SetFloat( "g_fMiddleGray", g_fKeyValue ) );

        V( g_pd3dDevice->SetRenderTarget( 0, pSurfLDR ) );
        V( g_pd3dDevice->SetTexture( 0, g_pTexScene ) );
        V( g_pd3dDevice->SetTexture( 1, g_apTexBloom[0] ) );
        V( g_pd3dDevice->SetTexture( 2, g_apTexStar[0] ) );
        V( g_pd3dDevice->SetTexture( 3, g_pTexAdaptedLuminanceCur ) );
        V( g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT ) );
        V( g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT ) );
        V( g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR ) );
        V( g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR ) );
        V( g_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR ) );
        V( g_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR ) );
        V( g_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_POINT ) );
        V( g_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_POINT ) );


        V( g_pEffect->Begin( &uiPassCount, 0 ) );
        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"Final Scene Pass" );

            for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
            {
                V( g_pEffect->BeginPass( uiPass ) );

                DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

                V( g_pEffect->EndPass() );
            }
        }
        V( g_pEffect->End() );

        V( g_pd3dDevice->SetTexture( 1, NULL ) );
        V( g_pd3dDevice->SetTexture( 2, NULL ) );
        V( g_pd3dDevice->SetTexture( 3, NULL ) );

        {
            CDXUTPerfEventGenerator g( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
            RenderText();

            V( g_HUD.OnRender( fElapsedTime ) );
            V( g_SampleUI.OnRender( fElapsedTime ) );
        }

        V( pd3dDevice->EndScene() );
    }

    // Release surfaces
    SAFE_RELEASE( pSurfHDR );
    SAFE_RELEASE( pSurfLDR );
    SAFE_RELEASE( pSurfDS );
}


//-----------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Render the world objects and lights
//-----------------------------------------------------------------------------
HRESULT RenderScene()
{
    HRESULT hr = S_OK;

    UINT uiPassCount, uiPass;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mTrans;
    D3DXMATRIXA16 mRotate;
    D3DXMATRIXA16 mObjectToView;

    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    D3DXMATRIX mView = *g_Camera.GetViewMatrix();

    g_pEffect->SetTechnique( "RenderScene" );
    g_pEffect->SetMatrix( "g_mObjectToView", &mView );

    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        return hr;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Turn off emissive lighting
        D3DXVECTOR4 vNull( 0.0f, 0.0f, 0.0f, 0.0f );
        g_pEffect->SetVector( "g_vEmissive", &vNull );

        // Enable texture
        g_pEffect->SetBool( "g_bEnableTexture", true );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );


        // Render walls and columns
        g_pEffect->SetFloat( "g_fPhongExponent", 5.0f );
        g_pEffect->SetFloat( "g_fPhongCoefficient", 1.0f );
        g_pEffect->SetFloat( "g_fDiffuseCoefficient", 0.5f );
        g_pd3dDevice->SetTexture( 0, g_pTexWall );
        g_pEffect->CommitChanges();
        g_pWorldMesh->DrawSubset( 0 );

        // Render floor
        g_pEffect->SetFloat( "g_fPhongExponent", 50.0f );
        g_pEffect->SetFloat( "g_fPhongCoefficient", 3.0f );
        g_pEffect->SetFloat( "g_fDiffuseCoefficient", 1.0f );
        g_pd3dDevice->SetTexture( 0, g_pTexFloor );
        g_pEffect->CommitChanges();
        g_pWorldMesh->DrawSubset( 1 );

        // Render ceiling
        g_pEffect->SetFloat( "g_fPhongExponent", 5.0f );
        g_pEffect->SetFloat( "g_fPhongCoefficient", 0.3f );
        g_pEffect->SetFloat( "g_fDiffuseCoefficient", 0.3f );
        g_pd3dDevice->SetTexture( 0, g_pTexCeiling );
        g_pEffect->CommitChanges();
        g_pWorldMesh->DrawSubset( 2 );

        // Render paintings
        g_pEffect->SetFloat( "g_fPhongExponent", 5.0f );
        g_pEffect->SetFloat( "g_fPhongCoefficient", 0.3f );
        g_pEffect->SetFloat( "g_fDiffuseCoefficient", 1.0f );
        g_pd3dDevice->SetTexture( 0, g_pTexPainting );
        g_pEffect->CommitChanges();
        g_pWorldMesh->DrawSubset( 3 );

        // Draw the light spheres.
        g_pEffect->SetFloat( "g_fPhongExponent", 5.0f );
        g_pEffect->SetFloat( "g_fPhongCoefficient", 1.0f );
        g_pEffect->SetFloat( "g_fDiffuseCoefficient", 1.0f );
        g_pEffect->SetBool( "g_bEnableTexture", false );

        for( int iLight = 0; iLight < NUM_LIGHTS; iLight++ )
        {
            // Just position the point light -- no need to orient it
            D3DXMATRIXA16 mScale;
            D3DXMatrixScaling( &mScale, 0.05f, 0.05f, 0.05f );

            mView = *g_Camera.GetViewMatrix();
            D3DXMatrixTranslation( &mWorld, g_avLightPosition[iLight].x, g_avLightPosition[iLight].y,
                                   g_avLightPosition[iLight].z );
            mWorld = mScale * mWorld;
            mObjectToView = mWorld * mView;
            g_pEffect->SetMatrix( "g_mObjectToView", &mObjectToView );

            // A light which illuminates objects at 80 lum/sr should be drawn
            // at 3183 lumens/meter^2/steradian, which equates to a multiplier
            // of 39.78 per lumen.
            D3DXVECTOR4 vEmissive = EMISSIVE_COEFFICIENT * g_avLightIntensity[iLight];
            g_pEffect->SetVector( "g_vEmissive", &vEmissive );

            g_pEffect->CommitChanges();
            g_pmeshSphere->DrawSubset( 0 );
        }
        g_pEffect->EndPass();
    }

    g_pEffect->End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: MeasureLuminance()
// Desc: Measure the average log luminance in the scene.
//-----------------------------------------------------------------------------
HRESULT MeasureLuminance()
{
    HRESULT hr = S_OK;
    UINT uiPassCount, uiPass;
    int i, x, y, index;
    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];


    DWORD dwCurTexture = NUM_TONEMAP_TEXTURES - 1;

    // Sample log average luminance
    PDIRECT3DSURFACE9 apSurfToneMap[NUM_TONEMAP_TEXTURES] = {0};

    // Retrieve the tonemap surfaces
    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        hr = g_apTexToneMap[i]->GetSurfaceLevel( 0, &apSurfToneMap[i] );
        if( FAILED( hr ) )
            goto LCleanReturn;
    }

    D3DSURFACE_DESC desc;
    g_apTexToneMap[dwCurTexture]->GetLevelDesc( 0, &desc );


    // Initialize the sample offsets for the initial luminance pass.
    float tU, tV;
    tU = 1.0f / ( 3.0f * desc.Width );
    tV = 1.0f / ( 3.0f * desc.Height );

    index = 0;
    for( x = -1; x <= 1; x++ )
    {
        for( y = -1; y <= 1; y++ )
        {
            avSampleOffsets[index].x = x * tU;
            avSampleOffsets[index].y = y * tV;

            index++;
        }
    }


    // After this pass, the g_apTexToneMap[NUM_TONEMAP_TEXTURES-1] texture will contain
    // a scaled, grayscale copy of the HDR scene. Individual texels contain the log 
    // of average luminance values for points sampled on the HDR texture.
    g_pEffect->SetTechnique( "SampleAvgLum" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

    g_pd3dDevice->SetRenderTarget( 0, apSurfToneMap[dwCurTexture] );
    g_pd3dDevice->SetTexture( 0, g_pTexSceneScaled );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );


    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    dwCurTexture--;

    // Initialize the sample offsets for the iterative luminance passes
    while( dwCurTexture > 0 )
    {
        g_apTexToneMap[dwCurTexture + 1]->GetLevelDesc( 0, &desc );
        GetSampleOffsets_DownScale4x4( desc.Width, desc.Height, avSampleOffsets );


        // Each of these passes continue to scale down the log of average
        // luminance texture created above, storing intermediate results in 
        // g_apTexToneMap[1] through g_apTexToneMap[NUM_TONEMAP_TEXTURES-1].
        g_pEffect->SetTechnique( "ResampleAvgLum" );
        g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

        g_pd3dDevice->SetRenderTarget( 0, apSurfToneMap[dwCurTexture] );
        g_pd3dDevice->SetTexture( 0, g_apTexToneMap[dwCurTexture + 1] );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );


        hr = g_pEffect->Begin( &uiPassCount, 0 );
        if( FAILED( hr ) )
            goto LCleanReturn;

        for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
        {
            g_pEffect->BeginPass( uiPass );

            // Draw a fullscreen quad to sample the RT
            DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

            g_pEffect->EndPass();
        }

        g_pEffect->End();
        dwCurTexture--;
    }

    // Downsample to 1x1
    g_apTexToneMap[1]->GetLevelDesc( 0, &desc );
    GetSampleOffsets_DownScale4x4( desc.Width, desc.Height, avSampleOffsets );


    // Perform the final pass of the average luminance calculation. This pass
    // scales the 4x4 log of average luminance texture from above and performs
    // an exp() operation to return a single texel cooresponding to the average
    // luminance of the scene in g_apTexToneMap[0].
    g_pEffect->SetTechnique( "ResampleAvgLumExp" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

    g_pd3dDevice->SetRenderTarget( 0, apSurfToneMap[0] );
    g_pd3dDevice->SetTexture( 0, g_apTexToneMap[1] );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );


    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

        g_pEffect->EndPass();
    }

    g_pEffect->End();


    hr = S_OK;
LCleanReturn:
    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        SAFE_RELEASE( apSurfToneMap[i] );
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CalculateAdaptation()
// Desc: Increment the user's adapted luminance
//-----------------------------------------------------------------------------
HRESULT CalculateAdaptation()
{
    HRESULT hr = S_OK;
    UINT uiPass, uiPassCount;

    // Swap current & last luminance
    PDIRECT3DTEXTURE9 pTexSwap = g_pTexAdaptedLuminanceLast;
    g_pTexAdaptedLuminanceLast = g_pTexAdaptedLuminanceCur;
    g_pTexAdaptedLuminanceCur = pTexSwap;

    PDIRECT3DSURFACE9 pSurfAdaptedLum = NULL;
    V( g_pTexAdaptedLuminanceCur->GetSurfaceLevel( 0, &pSurfAdaptedLum ) );

    // This simulates the light adaptation that occurs when moving from a 
    // dark area to a bright area, or vice versa. The g_pTexAdaptedLuminance
    // texture stores a single texel cooresponding to the user's adapted 
    // level.
    g_pEffect->SetTechnique( "CalculateAdaptedLum" );
    g_pEffect->SetFloat( "g_fElapsedTime", DXUTGetElapsedTime() );

    g_pd3dDevice->SetRenderTarget( 0, pSurfAdaptedLum );
    g_pd3dDevice->SetTexture( 0, g_pTexAdaptedLuminanceLast );
    g_pd3dDevice->SetTexture( 1, g_apTexToneMap[0] );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );


    V( g_pEffect->Begin( &uiPassCount, 0 ) );

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

        g_pEffect->EndPass();
    }

    g_pEffect->End();


    SAFE_RELEASE( pSurfAdaptedLum );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderStar()
// Desc: Render the blooming effect
//-----------------------------------------------------------------------------
HRESULT RenderStar()
{
    HRESULT hr = S_OK;
    UINT uiPassCount, uiPass;
    int i, d, p, s; // Loop variables

    LPDIRECT3DSURFACE9 pSurfStar = NULL;
    hr = g_apTexStar[0]->GetSurfaceLevel( 0, &pSurfStar );
    if( FAILED( hr ) )
        return hr;

    // Clear the star texture
    g_pd3dDevice->ColorFill( pSurfStar, NULL, D3DCOLOR_ARGB( 0, 0, 0, 0 ) );
    SAFE_RELEASE( pSurfStar );

    // Avoid rendering the star if it's not being used in the current glare
    if( g_GlareDef.m_fGlareLuminance <= 0.0f ||
        g_GlareDef.m_fStarLuminance <= 0.0f )
    {
        return S_OK;
    }

    // Initialize the constants used during the effect
    const CStarDef& starDef = g_GlareDef.m_starDef;
    const float fTanFoV = atanf( D3DX_PI / 8 );
    const D3DXVECTOR4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );
    static const int s_maxPasses = 3;
    static const int nSamples = 8;
    static D3DXVECTOR4 s_aaColor[s_maxPasses][8];
    static const D3DXCOLOR s_colorWhite( 0.63f, 0.63f, 0.63f, 0.0f );

    D3DXVECTOR4 avSampleWeights[MAX_SAMPLES];
    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];

    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    PDIRECT3DSURFACE9 pSurfSource = NULL;
    PDIRECT3DSURFACE9 pSurfDest = NULL;

    // Set aside all the star texture surfaces as a convenience
    PDIRECT3DSURFACE9 apSurfStar[NUM_STAR_TEXTURES] = {0};
    for( i = 0; i < NUM_STAR_TEXTURES; i++ )
    {
        hr = g_apTexStar[i]->GetSurfaceLevel( 0, &apSurfStar[i] );
        if( FAILED( hr ) )
            goto LCleanReturn;
    }

    // Get the source texture dimensions
    hr = g_pTexStarSource->GetSurfaceLevel( 0, &pSurfSource );
    if( FAILED( hr ) )
        goto LCleanReturn;

    D3DSURFACE_DESC desc;
    hr = pSurfSource->GetDesc( &desc );
    if( FAILED( hr ) )
        goto LCleanReturn;

    SAFE_RELEASE( pSurfSource );

    float srcW;
    srcW = ( FLOAT )desc.Width;
    float srcH;
    srcH = ( FLOAT )desc.Height;



    for( p = 0; p < s_maxPasses; p ++ )
    {
        float ratio;
        ratio = ( float )( p + 1 ) / ( float )s_maxPasses;

        for( s = 0; s < nSamples; s ++ )
        {
            D3DXCOLOR chromaticAberrColor;
            D3DXColorLerp( &chromaticAberrColor,
                           &( CStarDef::GetChromaticAberrationColor( s ) ),
                           &s_colorWhite,
                           ratio );

            D3DXColorLerp( ( D3DXCOLOR* )&( s_aaColor[p][s] ),
                           &s_colorWhite, &chromaticAberrColor,
                           g_GlareDef.m_fChromaticAberration );
        }
    }

    float radOffset;
    radOffset = g_GlareDef.m_fStarInclination + starDef.m_fInclination;


    PDIRECT3DTEXTURE9 pTexSource;


    // Direction loop
    for( d = 0; d < starDef.m_nStarLines; d ++ )
    {
        CONST STARLINE& starLine = starDef.m_pStarLine[d];

        pTexSource = g_pTexStarSource;

        float rad;
        rad = radOffset + starLine.fInclination;
        float sn, cs;
        sn = sinf( rad ), cs = cosf( rad );
        D3DXVECTOR2 vtStepUV;
        vtStepUV.x = sn / srcW * starLine.fSampleLength;
        vtStepUV.y = cs / srcH * starLine.fSampleLength;

        float attnPowScale;
        attnPowScale = ( fTanFoV + 0.1f ) * 1.0f *
            ( 160.0f + 120.0f ) / ( srcW + srcH ) * 1.2f;

        // 1 direction expansion loop
        g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        int iWorkTexture;
        iWorkTexture = 1;
        for( p = 0; p < starLine.nPasses; p ++ )
        {

            if( p == starLine.nPasses - 1 )
            {
                // Last pass move to other work buffer
                pSurfDest = apSurfStar[d + 4];
            }
            else
            {
                pSurfDest = apSurfStar[iWorkTexture];
            }

            // Sampling configration for each stage
            for( i = 0; i < nSamples; i ++ )
            {
                float lum;
                lum = powf( starLine.fAttenuation, attnPowScale * i );

                avSampleWeights[i] = s_aaColor[starLine.nPasses - 1 - p][i] *
                    lum * ( p + 1.0f ) * 0.5f;


                // Offset of sampling coordinate
                avSampleOffsets[i].x = vtStepUV.x * i;
                avSampleOffsets[i].y = vtStepUV.y * i;
                if( fabs( avSampleOffsets[i].x ) >= 0.9f ||
                    fabs( avSampleOffsets[i].y ) >= 0.9f )
                {
                    avSampleOffsets[i].x = 0.0f;
                    avSampleOffsets[i].y = 0.0f;
                    avSampleWeights[i] *= 0.0f;
                }

            }


            g_pEffect->SetTechnique( "Star" );
            g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
            g_pEffect->SetVectorArray( "g_avSampleWeights", avSampleWeights, nSamples );

            g_pd3dDevice->SetRenderTarget( 0, pSurfDest );
            g_pd3dDevice->SetTexture( 0, pTexSource );
            g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );


            hr = g_pEffect->Begin( &uiPassCount, 0 );
            if( FAILED( hr ) )
                return hr;

            for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
            {
                g_pEffect->BeginPass( uiPass );

                // Draw a fullscreen quad to sample the RT
                DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

                g_pEffect->EndPass();
            }

            g_pEffect->End();

            // Setup next expansion
            vtStepUV *= nSamples;
            attnPowScale *= nSamples;

            // Set the work drawn just before to next texture source.
            pTexSource = g_apTexStar[iWorkTexture];

            iWorkTexture += 1;
            if( iWorkTexture > 2 )
            {
                iWorkTexture = 1;
            }

        }
    }


    pSurfDest = apSurfStar[0];


    for( i = 0; i < starDef.m_nStarLines; i++ )
    {
        g_pd3dDevice->SetTexture( i, g_apTexStar[i + 4] );
        g_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        g_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

        avSampleWeights[i] = vWhite * 1.0f / ( FLOAT )starDef.m_nStarLines;
    }

    CHAR strTechnique[256];
    sprintf_s( strTechnique, 256, "MergeTextures_%d", starDef.m_nStarLines );

    g_pEffect->SetTechnique( strTechnique );

    g_pEffect->SetVectorArray( "g_avSampleWeights", avSampleWeights, starDef.m_nStarLines );

    g_pd3dDevice->SetRenderTarget( 0, pSurfDest );

    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( 0.0f, 0.0f, 1.0f, 1.0f );

        g_pEffect->EndPass();
    }

    g_pEffect->End();

    for( i = 0; i < starDef.m_nStarLines; i++ )
        g_pd3dDevice->SetTexture( i, NULL );


    hr = S_OK;
LCleanReturn:
    for( i = 0; i < NUM_STAR_TEXTURES; i++ )
    {
        SAFE_RELEASE( apSurfStar[i] );
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: RenderBloom()
// Desc: Render the blooming effect
//-----------------------------------------------------------------------------
HRESULT RenderBloom()
{
    HRESULT hr = S_OK;
    UINT uiPassCount, uiPass;
    int i = 0;


    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];
    FLOAT afSampleOffsets[MAX_SAMPLES];
    D3DXVECTOR4 avSampleWeights[MAX_SAMPLES];

    PDIRECT3DSURFACE9 pSurfScaledHDR;
    g_pTexSceneScaled->GetSurfaceLevel( 0, &pSurfScaledHDR );

    PDIRECT3DSURFACE9 pSurfBloom;
    g_apTexBloom[0]->GetSurfaceLevel( 0, &pSurfBloom );

    PDIRECT3DSURFACE9 pSurfHDR;
    g_pTexScene->GetSurfaceLevel( 0, &pSurfHDR );

    PDIRECT3DSURFACE9 pSurfTempBloom;
    g_apTexBloom[1]->GetSurfaceLevel( 0, &pSurfTempBloom );

    PDIRECT3DSURFACE9 pSurfBloomSource;
    g_apTexBloom[2]->GetSurfaceLevel( 0, &pSurfBloomSource );

    // Clear the bloom texture
    g_pd3dDevice->ColorFill( pSurfBloom, NULL, D3DCOLOR_ARGB( 0, 0, 0, 0 ) );

    if( g_GlareDef.m_fGlareLuminance <= 0.0f ||
        g_GlareDef.m_fBloomLuminance <= 0.0f )
    {
        hr = S_OK;
        goto LCleanReturn;
    }

    RECT rectSrc;
    GetTextureRect( g_pTexBloomSource, &rectSrc );
    InflateRect( &rectSrc, -1, -1 );

    RECT rectDest;
    GetTextureRect( g_apTexBloom[2], &rectDest );
    InflateRect( &rectDest, -1, -1 );

    CoordRect coords;
    GetTextureCoords( g_pTexBloomSource, &rectSrc, g_apTexBloom[2], &rectDest, &coords );

    D3DSURFACE_DESC desc;
    hr = g_pTexBloomSource->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;


    g_pEffect->SetTechnique( "GaussBlur5x5" );

    hr = GetSampleOffsets_GaussBlur5x5( desc.Width, desc.Height, avSampleOffsets, avSampleWeights, 1.0f );

    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfBloomSource );
    g_pd3dDevice->SetTexture( 0, g_pTexBloomSource );
    g_pd3dDevice->SetScissorRect( &rectDest );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }
    g_pEffect->End();
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );

    hr = g_apTexBloom[2]->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Width, afSampleOffsets, avSampleWeights, 3.0f, 2.0f );
    for( i = 0; i < MAX_SAMPLES; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( afSampleOffsets[i], 0.0f );
    }


    g_pEffect->SetTechnique( "Bloom" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfTempBloom );
    g_pd3dDevice->SetTexture( 0, g_apTexBloom[2] );
    g_pd3dDevice->SetScissorRect( &rectDest );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    g_pEffect->Begin( &uiPassCount, 0 );
    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }
    g_pEffect->End();
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );


    hr = g_apTexBloom[1]->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    hr = GetSampleOffsets_Bloom( desc.Height, afSampleOffsets, avSampleWeights, 3.0f, 2.0f );
    for( i = 0; i < MAX_SAMPLES; i++ )
    {
        avSampleOffsets[i] = D3DXVECTOR2( 0.0f, afSampleOffsets[i] );
    }


    GetTextureRect( g_apTexBloom[1], &rectSrc );
    InflateRect( &rectSrc, -1, -1 );

    GetTextureCoords( g_apTexBloom[1], &rectSrc, g_apTexBloom[0], NULL, &coords );


    g_pEffect->SetTechnique( "Bloom" );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfBloom );
    g_pd3dDevice->SetTexture( 0, g_apTexBloom[1] );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );


    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }

    g_pEffect->End();


    hr = S_OK;

LCleanReturn:
    SAFE_RELEASE( pSurfBloomSource );
    SAFE_RELEASE( pSurfTempBloom );
    SAFE_RELEASE( pSurfBloom );
    SAFE_RELEASE( pSurfHDR );
    SAFE_RELEASE( pSurfScaledHDR );

    return hr;
}



//-----------------------------------------------------------------------------
// Name: DrawFullScreenQuad
// Desc: Draw a properly aligned quad covering the entire render target
//-----------------------------------------------------------------------------
void DrawFullScreenQuad( float fLeftU, float fTopV, float fRightU, float fBottomV )
{
    D3DSURFACE_DESC dtdsdRT;
    PDIRECT3DSURFACE9 pSurfRT;

    // Acquire render target width and height
    g_pd3dDevice->GetRenderTarget( 0, &pSurfRT );
    pSurfRT->GetDesc( &dtdsdRT );
    pSurfRT->Release();

    // Ensure that we're directly mapping texels to pixels by offset by 0.5
    // For more info see the doc page titled "Directly Mapping Texels to Pixels"
    FLOAT fWidth5 = ( FLOAT )dtdsdRT.Width - 0.5f;
    FLOAT fHeight5 = ( FLOAT )dtdsdRT.Height - 0.5f;

    // Draw the quad
    ScreenVertex svQuad[4];

    svQuad[0].p = D3DXVECTOR4( -0.5f, -0.5f, 0.5f, 1.0f );
    svQuad[0].t = D3DXVECTOR2( fLeftU, fTopV );

    svQuad[1].p = D3DXVECTOR4( fWidth5, -0.5f, 0.5f, 1.0f );
    svQuad[1].t = D3DXVECTOR2( fRightU, fTopV );

    svQuad[2].p = D3DXVECTOR4( -0.5f, fHeight5, 0.5f, 1.0f );
    svQuad[2].t = D3DXVECTOR2( fLeftU, fBottomV );

    svQuad[3].p = D3DXVECTOR4( fWidth5, fHeight5, 0.5f, 1.0f );
    svQuad[3].t = D3DXVECTOR2( fRightU, fBottomV );

    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    g_pd3dDevice->SetFVF( ScreenVertex::FVF );
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, svQuad, sizeof( ScreenVertex ) );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
}


//-----------------------------------------------------------------------------
// Name: GetTextureRect()
// Desc: Get the dimensions of the texture
//-----------------------------------------------------------------------------
HRESULT GetTextureRect( PDIRECT3DTEXTURE9 pTexture, RECT* pRect )
{
    HRESULT hr = S_OK;

    if( pTexture == NULL || pRect == NULL )
        return E_INVALIDARG;

    D3DSURFACE_DESC desc;
    hr = pTexture->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    pRect->left = 0;
    pRect->top = 0;
    pRect->right = desc.Width;
    pRect->bottom = desc.Height;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetTextureCoords()
// Desc: Get the texture coordinates to use when rendering into the destination
//       texture, given the source and destination rectangles
//-----------------------------------------------------------------------------
HRESULT GetTextureCoords( PDIRECT3DTEXTURE9 pTexSrc, RECT* pRectSrc,
                          PDIRECT3DTEXTURE9 pTexDest, RECT* pRectDest, CoordRect* pCoords )
{
    HRESULT hr = S_OK;
    D3DSURFACE_DESC desc;
    float tU, tV;

    // Validate arguments
    if( pTexSrc == NULL || pTexDest == NULL || pCoords == NULL )
        return E_INVALIDARG;

    // Start with a default mapping of the complete source surface to complete 
    // destination surface
    pCoords->fLeftU = 0.0f;
    pCoords->fTopV = 0.0f;
    pCoords->fRightU = 1.0f;
    pCoords->fBottomV = 1.0f;

    // If not using the complete source surface, adjust the coordinates
    if( pRectSrc != NULL )
    {
        // Get destination texture description
        hr = pTexSrc->GetLevelDesc( 0, &desc );
        if( FAILED( hr ) )
            return hr;

        // These delta values are the distance between source texel centers in 
        // texture address space
        tU = 1.0f / desc.Width;
        tV = 1.0f / desc.Height;

        pCoords->fLeftU += pRectSrc->left * tU;
        pCoords->fTopV += pRectSrc->top * tV;
        pCoords->fRightU -= ( desc.Width - pRectSrc->right ) * tU;
        pCoords->fBottomV -= ( desc.Height - pRectSrc->bottom ) * tV;
    }

    // If not drawing to the complete destination surface, adjust the coordinates
    if( pRectDest != NULL )
    {
        // Get source texture description
        hr = pTexDest->GetLevelDesc( 0, &desc );
        if( FAILED( hr ) )
            return hr;

        // These delta values are the distance between source texel centers in 
        // texture address space
        tU = 1.0f / desc.Width;
        tV = 1.0f / desc.Height;

        pCoords->fLeftU -= pRectDest->left * tU;
        pCoords->fTopV -= pRectDest->top * tV;
        pCoords->fRightU += ( desc.Width - pRectDest->right ) * tU;
        pCoords->fBottomV += ( desc.Height - pRectDest->bottom ) * tV;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Scene_To_SceneScaled()
// Desc: Scale down g_pTexScene by 1/4 x 1/4 and place the result in 
//       g_pTexSceneScaled
//-----------------------------------------------------------------------------
HRESULT Scene_To_SceneScaled()
{
    HRESULT hr = S_OK;
    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];


    // Get the new render target surface
    PDIRECT3DSURFACE9 pSurfScaledScene = NULL;
    hr = g_pTexSceneScaled->GetSurfaceLevel( 0, &pSurfScaledScene );
    if( FAILED( hr ) )
        goto LCleanReturn;

    const D3DSURFACE_DESC* pBackBufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();

    // Create a 1/4 x 1/4 scale copy of the HDR texture. Since bloom textures
    // are 1/8 x 1/8 scale, border texels of the HDR texture will be discarded 
    // to keep the dimensions evenly divisible by 8; this allows for precise 
    // control over sampling inside pixel shaders.
    g_pEffect->SetTechnique( "DownScale4x4" );

    // Place the rectangle in the center of the back buffer surface
    RECT rectSrc;
    rectSrc.left = ( pBackBufferDesc->Width - g_dwCropWidth ) / 2;
    rectSrc.top = ( pBackBufferDesc->Height - g_dwCropHeight ) / 2;
    rectSrc.right = rectSrc.left + g_dwCropWidth;
    rectSrc.bottom = rectSrc.top + g_dwCropHeight;

    // Get the texture coordinates for the render target
    CoordRect coords;
    GetTextureCoords( g_pTexScene, &rectSrc, g_pTexSceneScaled, NULL, &coords );

    // Get the sample offsets used within the pixel shader
    GetSampleOffsets_DownScale4x4( pBackBufferDesc->Width, pBackBufferDesc->Height, avSampleOffsets );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

    g_pd3dDevice->SetRenderTarget( 0, pSurfScaledScene );
    g_pd3dDevice->SetTexture( 0, g_pTexScene );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    UINT uiPassCount, uiPass;
    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }

    g_pEffect->End();


    hr = S_OK;
LCleanReturn:
    SAFE_RELEASE( pSurfScaledScene );
    return hr;
}




//-----------------------------------------------------------------------------
// Name: SceneScaled_To_BrightPass
// Desc: Run the bright-pass filter on g_pTexSceneScaled and place the result
//       in g_pTexBrightPass
//-----------------------------------------------------------------------------
HRESULT SceneScaled_To_BrightPass()
{
    HRESULT hr = S_OK;

    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];
    D3DXVECTOR4 avSampleWeights[MAX_SAMPLES];


    // Get the new render target surface
    PDIRECT3DSURFACE9 pSurfBrightPass;
    hr = g_pTexBrightPass->GetSurfaceLevel( 0, &pSurfBrightPass );
    if( FAILED( hr ) )
        goto LCleanReturn;


    D3DSURFACE_DESC desc;
    g_pTexSceneScaled->GetLevelDesc( 0, &desc );

    // Get the rectangle describing the sampled portion of the source texture.
    // Decrease the rectangle to adjust for the single pixel black border.
    RECT rectSrc;
    GetTextureRect( g_pTexSceneScaled, &rectSrc );
    InflateRect( &rectSrc, -1, -1 );

    // Get the destination rectangle.
    // Decrease the rectangle to adjust for the single pixel black border.
    RECT rectDest;
    GetTextureRect( g_pTexBrightPass, &rectDest );
    InflateRect( &rectDest, -1, -1 );

    // Get the correct texture coordinates to apply to the rendered quad in order 
    // to sample from the source rectangle and render into the destination rectangle
    CoordRect coords;
    GetTextureCoords( g_pTexSceneScaled, &rectSrc, g_pTexBrightPass, &rectDest, &coords );

    // The bright-pass filter removes everything from the scene except lights and
    // bright reflections
    g_pEffect->SetTechnique( "BrightPassFilter" );

    g_pd3dDevice->SetRenderTarget( 0, pSurfBrightPass );
    g_pd3dDevice->SetTexture( 0, g_pTexSceneScaled );
    g_pd3dDevice->SetTexture( 1, g_pTexAdaptedLuminanceCur );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    g_pd3dDevice->SetScissorRect( &rectDest );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    UINT uiPass, uiPassCount;
    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad to sample the RT
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );

    hr = S_OK;
LCleanReturn:
    SAFE_RELEASE( pSurfBrightPass );
    return hr;
}




//-----------------------------------------------------------------------------
// Name: BrightPass_To_StarSource
// Desc: Perform a 5x5 gaussian blur on g_pTexBrightPass and place the result
//       in g_pTexStarSource. The bright-pass filtered image is blurred before
//       being used for star operations to avoid aliasing artifacts.
//-----------------------------------------------------------------------------
HRESULT BrightPass_To_StarSource()
{
    HRESULT hr = S_OK;

    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];
    D3DXVECTOR4 avSampleWeights[MAX_SAMPLES];

    // Get the new render target surface
    PDIRECT3DSURFACE9 pSurfStarSource;
    hr = g_pTexStarSource->GetSurfaceLevel( 0, &pSurfStarSource );
    if( FAILED( hr ) )
        goto LCleanReturn;


    // Get the destination rectangle.
    // Decrease the rectangle to adjust for the single pixel black border.
    RECT rectDest;
    GetTextureRect( g_pTexStarSource, &rectDest );
    InflateRect( &rectDest, -1, -1 );

    // Get the correct texture coordinates to apply to the rendered quad in order 
    // to sample from the source rectangle and render into the destination rectangle
    CoordRect coords;
    GetTextureCoords( g_pTexBrightPass, NULL, g_pTexStarSource, &rectDest, &coords );

    // Get the sample offsets used within the pixel shader
    D3DSURFACE_DESC desc;
    hr = g_pTexBrightPass->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;


    GetSampleOffsets_GaussBlur5x5( desc.Width, desc.Height, avSampleOffsets, avSampleWeights );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );
    g_pEffect->SetValue( "g_avSampleWeights", avSampleWeights, sizeof( avSampleWeights ) );

    // The gaussian blur smooths out rough edges to avoid aliasing effects
    // when the star effect is run
    g_pEffect->SetTechnique( "GaussBlur5x5" );

    g_pd3dDevice->SetRenderTarget( 0, pSurfStarSource );
    g_pd3dDevice->SetTexture( 0, g_pTexBrightPass );
    g_pd3dDevice->SetScissorRect( &rectDest );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    UINT uiPassCount, uiPass;
    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );


    hr = S_OK;
LCleanReturn:
    SAFE_RELEASE( pSurfStarSource );
    return hr;
}




//-----------------------------------------------------------------------------
// Name: StarSource_To_BloomSource
// Desc: Scale down g_pTexStarSource by 1/2 x 1/2 and place the result in 
//       g_pTexBloomSource
//-----------------------------------------------------------------------------
HRESULT StarSource_To_BloomSource()
{
    HRESULT hr = S_OK;

    D3DXVECTOR2 avSampleOffsets[MAX_SAMPLES];

    // Get the new render target surface
    PDIRECT3DSURFACE9 pSurfBloomSource;
    hr = g_pTexBloomSource->GetSurfaceLevel( 0, &pSurfBloomSource );
    if( FAILED( hr ) )
        goto LCleanReturn;


    // Get the rectangle describing the sampled portion of the source texture.
    // Decrease the rectangle to adjust for the single pixel black border.
    RECT rectSrc;
    GetTextureRect( g_pTexStarSource, &rectSrc );
    InflateRect( &rectSrc, -1, -1 );

    // Get the destination rectangle.
    // Decrease the rectangle to adjust for the single pixel black border.
    RECT rectDest;
    GetTextureRect( g_pTexBloomSource, &rectDest );
    InflateRect( &rectDest, -1, -1 );

    // Get the correct texture coordinates to apply to the rendered quad in order 
    // to sample from the source rectangle and render into the destination rectangle
    CoordRect coords;
    GetTextureCoords( g_pTexStarSource, &rectSrc, g_pTexBloomSource, &rectDest, &coords );

    // Get the sample offsets used within the pixel shader
    D3DSURFACE_DESC desc;
    hr = g_pTexBrightPass->GetLevelDesc( 0, &desc );
    if( FAILED( hr ) )
        return hr;

    GetSampleOffsets_DownScale2x2( desc.Width, desc.Height, avSampleOffsets );
    g_pEffect->SetValue( "g_avSampleOffsets", avSampleOffsets, sizeof( avSampleOffsets ) );

    // Create an exact 1/2 x 1/2 copy of the source texture
    g_pEffect->SetTechnique( "DownScale2x2" );

    g_pd3dDevice->SetRenderTarget( 0, pSurfBloomSource );
    g_pd3dDevice->SetTexture( 0, g_pTexStarSource );
    g_pd3dDevice->SetScissorRect( &rectDest );
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    UINT uiPassCount, uiPass;
    hr = g_pEffect->Begin( &uiPassCount, 0 );
    if( FAILED( hr ) )
        goto LCleanReturn;

    for( uiPass = 0; uiPass < uiPassCount; uiPass++ )
    {
        g_pEffect->BeginPass( uiPass );

        // Draw a fullscreen quad
        DrawFullScreenQuad( coords );

        g_pEffect->EndPass();
    }

    g_pEffect->End();
    g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );



    hr = S_OK;
LCleanReturn:
    SAFE_RELEASE( pSurfBloomSource );
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale4x4
// Desc: Get the texture coordinate offsets to be used inside the DownScale4x4
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale4x4( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwWidth;
    float tV = 1.0f / dwHeight;

    // Sample from the 16 surrounding points. Since the center point will be in
    // the exact center of 16 texels, a 0.5f offset is needed to specify a texel
    // center.
    int index = 0;
    for( int y = 0; y < 4; y++ )
    {
        for( int x = 0; x < 4; x++ )
        {
            avSampleOffsets[ index ].x = ( x - 1.5f ) * tU;
            avSampleOffsets[ index ].y = ( y - 1.5f ) * tV;

            index++;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_DownScale2x2
// Desc: Get the texture coordinate offsets to be used inside the DownScale2x2
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_DownScale2x2( DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[] )
{
    if( NULL == avSampleOffsets )
        return E_INVALIDARG;

    float tU = 1.0f / dwWidth;
    float tV = 1.0f / dwHeight;

    // Sample from the 4 surrounding points. Since the center point will be in
    // the exact center of 4 texels, a 0.5f offset is needed to specify a texel
    // center.
    int index = 0;
    for( int y = 0; y < 2; y++ )
    {
        for( int x = 0; x < 2; x++ )
        {
            avSampleOffsets[ index ].x = ( x - 0.5f ) * tU;
            avSampleOffsets[ index ].y = ( y - 0.5f ) * tV;

            index++;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_GaussBlur5x5
// Desc: Get the texture coordinate offsets to be used inside the GaussBlur5x5
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_GaussBlur5x5( DWORD dwD3DTexWidth,
                                       DWORD dwD3DTexHeight,
                                       D3DXVECTOR2* avTexCoordOffset,
                                       D3DXVECTOR4* avSampleWeight,
                                       FLOAT fMultiplier )
{
    float tu = 1.0f / ( float )dwD3DTexWidth;
    float tv = 1.0f / ( float )dwD3DTexHeight;

    D3DXVECTOR4 vWhite( 1.0f, 1.0f, 1.0f, 1.0f );

    float totalWeight = 0.0f;
    int index = 0;
    for( int x = -2; x <= 2; x++ )
    {
        for( int y = -2; y <= 2; y++ )
        {
            // Exclude pixels with a block distance greater than 2. This will
            // create a kernel which approximates a 5x5 kernel using only 13
            // sample points instead of 25; this is necessary since 2.0 shaders
            // only support 16 texture grabs.
            if( abs( x ) + abs( y ) > 2 )
                continue;

            // Get the unscaled Gaussian intensity for this offset
            avTexCoordOffset[index] = D3DXVECTOR2( x * tu, y * tv );
            avSampleWeight[index] = vWhite * GaussianDistribution( ( float )x, ( float )y, 1.0f );
            totalWeight += avSampleWeight[index].x;

            index++;
        }
    }

    // Divide the current weight by the total weight of all the samples; Gaussian
    // blur kernels add to 1.0f to ensure that the intensity of the image isn't
    // changed when the blur occurs. An optional multiplier variable is used to
    // add or remove image intensity during the blur.
    for( int i = 0; i < index; i++ )
    {
        avSampleWeight[i] /= totalWeight;
        avSampleWeight[i] *= fMultiplier;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom
// Desc: Get the texture coordinate offsets to be used inside the Bloom
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Bloom( DWORD dwD3DTexSize,
                                float afTexCoordOffset[15],
                                D3DXVECTOR4* avColorWeight,
                                float fDeviation,
                                float fMultiplier )
{
    int i = 0;
    float tu = 1.0f / ( float )dwD3DTexSize;

    // Fill the center texel
    float weight = fMultiplier * GaussianDistribution( 0, 0, fDeviation );
    avColorWeight[0] = D3DXVECTOR4( weight, weight, weight, 1.0f );

    afTexCoordOffset[0] = 0.0f;

    // Fill the first half
    for( i = 1; i < 8; i++ )
    {
        // Get the Gaussian intensity for this offset
        weight = fMultiplier * GaussianDistribution( ( float )i, 0, fDeviation );
        afTexCoordOffset[i] = i * tu;

        avColorWeight[i] = D3DXVECTOR4( weight, weight, weight, 1.0f );
    }

    // Mirror to the second half
    for( i = 8; i < 15; i++ )
    {
        avColorWeight[i] = avColorWeight[i - 7];
        afTexCoordOffset[i] = -afTexCoordOffset[i - 7];
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSampleOffsets_Bloom
// Desc: Get the texture coordinate offsets to be used inside the Bloom
//       pixel shader.
//-----------------------------------------------------------------------------
HRESULT GetSampleOffsets_Star( DWORD dwD3DTexSize,
                               float afTexCoordOffset[15],
                               D3DXVECTOR4* avColorWeight,
                               float fDeviation )
{
    int i = 0;
    float tu = 1.0f / ( float )dwD3DTexSize;

    // Fill the center texel
    float weight = 1.0f * GaussianDistribution( 0, 0, fDeviation );
    avColorWeight[0] = D3DXVECTOR4( weight, weight, weight, 1.0f );

    afTexCoordOffset[0] = 0.0f;

    // Fill the first half
    for( i = 1; i < 8; i++ )
    {
        // Get the Gaussian intensity for this offset
        weight = 1.0f * GaussianDistribution( ( float )i, 0, fDeviation );
        afTexCoordOffset[i] = i * tu;

        avColorWeight[i] = D3DXVECTOR4( weight, weight, weight, 1.0f );
    }

    // Mirror to the second half
    for( i = 8; i < 15; i++ )
    {
        avColorWeight[i] = avColorWeight[i - 7];
        afTexCoordOffset[i] = -afTexCoordOffset[i - 7];
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GaussianDistribution
// Desc: Helper function for GetSampleOffsets function to compute the 
//       2 parameter Gaussian distrubution using the given standard deviation
//       rho
//-----------------------------------------------------------------------------
float GaussianDistribution( float x, float y, float rho )
{
    float g = 1.0f / sqrtf( 2.0f * D3DX_PI * rho * rho );
    g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

    return g;
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( g_pSprite, strMsg, -1, &rc, DT_NOCLIP, g_clr );
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

    WCHAR glareType[255];
    swprintf_s(glareType, L"Glare type: %s", g_GlareDef.m_strGlareName);
    txtHelper.DrawTextLine( glareType );

    if( g_bUseMultiSampleFloat16 )
    {
        txtHelper.DrawTextLine( L"Using MultiSample Render Target" );
        txtHelper.DrawFormattedTextLine( L"Number of Samples: %d", ( int )g_MaxMultiSampleType );
        txtHelper.DrawFormattedTextLine( L"Quality: %d", g_dwMultiSampleQuality );
    }

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

        txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Look: Left drag mouse\n"
                                L"Move: A,W,S,D or Arrow Keys\n"
                                L"Quit: ESC" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
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
    CDXUTStatic* pStatic = NULL;
    CDXUTComboBox* pComboBox = NULL;
    CDXUTSlider* pSlider = NULL;
    CDXUTCheckBox* pCheckBox = NULL;
    WCHAR strBuffer[256] = {0};

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_RESET:
            ResetOptions(); break;

        case IDC_TONEMAP:
            pCheckBox = ( CDXUTCheckBox* )pControl;
            g_bToneMap = pCheckBox->GetChecked();
            break;

        case IDC_BLUESHIFT:
            pCheckBox = ( CDXUTCheckBox* )pControl;
            g_bBlueShift = pCheckBox->GetChecked();
            break;

        case IDC_GLARETYPE:
            pComboBox = ( CDXUTComboBox* )pControl;
            g_eGlareType = ( EGLARELIBTYPE )( INT_PTR )pComboBox->GetSelectedData();
            g_GlareDef.Initialize( g_eGlareType );
            break;

        case IDC_KEYVALUE:
            pSlider = ( CDXUTSlider* )pControl;
            g_fKeyValue = pSlider->GetValue() / 100.0f;

            swprintf_s( strBuffer, 255, L"Key Value: %.2f", g_fKeyValue );
            pStatic = g_SampleUI.GetStatic( IDC_KEYVALUE_LABEL );
            pStatic->SetText( strBuffer );
            break;

        case IDC_LIGHT0:
        case IDC_LIGHT1:
            {
                int iLight = 0;

                if( nControlID == IDC_LIGHT0 )
                    iLight = 0;
                else if( nControlID == IDC_LIGHT1 )
                    iLight = 1;

                pSlider = ( CDXUTSlider* )pControl;
                g_nLightMantissa[iLight] = 1 + ( pSlider->GetValue() ) % 9;
                g_nLightLogIntensity[iLight] = -4 + ( pSlider->GetValue() ) / 9;
                RefreshLights();

                break;
            }


    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all 
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for 
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    int i = 0;

    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();


    SAFE_RELEASE( g_pTextSprite );

    SAFE_RELEASE( g_pWorldMesh );
    SAFE_RELEASE( g_pmeshSphere );

    SAFE_RELEASE( g_pFloatMSRT );
    SAFE_RELEASE( g_pFloatMSDS );
    SAFE_RELEASE( g_pTexScene );
    SAFE_RELEASE( g_pTexSceneScaled );
    SAFE_RELEASE( g_pTexWall );
    SAFE_RELEASE( g_pTexFloor );
    SAFE_RELEASE( g_pTexCeiling );
    SAFE_RELEASE( g_pTexPainting );
    SAFE_RELEASE( g_pTexAdaptedLuminanceCur );
    SAFE_RELEASE( g_pTexAdaptedLuminanceLast );
    SAFE_RELEASE( g_pTexBrightPass );
    SAFE_RELEASE( g_pTexBloomSource );
    SAFE_RELEASE( g_pTexStarSource );

    for( i = 0; i < NUM_TONEMAP_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexToneMap[i] );
    }

    for( i = 0; i < NUM_STAR_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexStar[i] );
    }

    for( i = 0; i < NUM_BLOOM_TEXTURES; i++ )
    {
        SAFE_RELEASE( g_apTexBloom[i] );
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
}



