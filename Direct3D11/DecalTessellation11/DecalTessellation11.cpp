//--------------------------------------------------------------------------------------
// File: DecalTessellation11.cpp
//
// This sample demonstrates the use of hardware tessellation for deforming geometry
// with displacement map decals.
//
// Developed by AMD Developer Relations team.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <d3dx11.h>
#include "resource.h"
#include "geometry.h"

//--------------------------------------------------------------------------------------
// Macros / Defines
//--------------------------------------------------------------------------------------
// Maximum number of decals per object
#define MAX_DECALS 50

// UI control IDs
#define IDC_TOGGLEFULLSCREEN                1
#define IDC_TOGGLEREF                       3
#define IDC_CHANGEDEVICE                    4

// Sample UI
#define IDC_STATIC_MESH                     5
#define IDC_COMBOBOX_MESH                   6
#define IDC_CHECKBOX_WIREFRAME              7
#define IDC_CHECKBOX_TESSELLATION           8
#define IDC_CHECKBOX_DISPLACEMENT_ADAPTIVE  9
#define IDC_STATIC_TESS_FACTOR              10
#define IDC_SLIDER_TESS_FACTOR              11
#define IDC_STATIC_DIS_SCALE                12
#define IDC_SLIDER_DIS_SCALE                13
#define IDC_STATIC_DIS_BIAS                 14
#define IDC_SLIDER_DIS_BIAS                 15
#define IDC_STATIC_DECAL_RADIUS             16
#define IDC_SLIDER_DECAL_RADIUS             17
#define IDC_BUTTON_CLEAR                    18
#define IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE  19
#define IDC_CHECKBOX_DISPLACE_NORMAL        20
#define IDC_CHECKBOX_BACK_FACE_CULLING      21
#define IDC_COMBOBOX_DECAL                  22
#define IDC_STATIC_DECAL                    23
#define IDC_STATIC_CROSSHAIR                24

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
// UI Constants
const int g_cTessellationSliderMin = 10;
const int g_cTessellationSliderMax = 160;
const int g_cTessellationUIScale = 10;
const int g_cDisplacementScaleSliderMin = -30;
const int g_cDisplacementScaleSliderMax = 30;
const int g_cDisplacementScaleUIScale = 10;
const int g_cDisplacementBiasSliderMin = -20;
const int g_cDisplacementBiasSliderMax = 20;
const int g_cDisplacementBiasUIScale = 10;
const int g_cDecalRadiusSliderMin = 0;
const int g_cDecalRadiusSliderMax = 30;
const int g_cDecalRadiusUIScale = 10;


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

// constants that don't need to be updated every frame
struct INIT_CB_STRUCT
{
    D3DXVECTOR4 vMaterialColor;                     // mesh color
    D3DXVECTOR4 vAmbientColor;                      // mesh ambient color
    D3DXVECTOR4 vSpecularColor;                     // mesh specular highlight color
    D3DXVECTOR4 vScreenSize;                        // x,y = screen width,height
    D3DXVECTOR4 vFlags;                             // miscellaneous flags
};

// constants that need to be updated every frame
struct UPDATE_CB_STRUCT
{
    D3DXMATRIXA16 mWorld;                         // World matrix
    D3DXMATRIXA16 mViewProjection;                // View-Projection matrix
    D3DXMATRIXA16 mWorldViewProjection;           // WVP matrix
    D3DXVECTOR4 vTessellationFactor;              // Edge, inside and minimum tessellation factor
    D3DXVECTOR4 vDisplacementScaleBias;           // Scale and bias of displacement
    D3DXVECTOR4 vLightPosition;                   // 3D light position
    D3DXVECTOR4 vEyePosition;                     // 3D eye position
};

// Constants for storing the tangent space of each hit. Each decal has it's own tangent space.
// The hit location is stored in the unused w component of the normal, binormal and tangent.
struct DAMAGE_CB_STRUCT
{
    D3DXVECTOR4 vNormal[MAX_DECALS];
    D3DXVECTOR4 vBinormal[MAX_DECALS];
    D3DXVECTOR4 vTangent[MAX_DECALS];
    D3DXVECTOR4 vDecalPositonSize[MAX_DECALS];
};

//--------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------
// The different meshes
enum MESH_TYPE
{
    MESH_TYPE_CAR       = 0,
    MESH_TYPE_TEAPOT    = 1,
    MESH_TYPE_BOX       = 2,
    MESH_TYPE_TIGER     = 3,
    MESH_TYPE_USER      = 4,
    MESH_TYPE_MAX       = 5
};

// The different decals
enum DECAL_TYPE
{
    DECAL_TYPE_METAL    = 0,
    DECAL_TYPE_CRACK    = 1,
    DECAL_TYPE_ROCK     = 2,
    DECAL_TYPE_USER     = 3,
    DECAL_TYPE_MAX      = 4
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
// DXUT resources
CDXUTDialogResourceManager          g_DialogResourceManager;    // Manager for shared resources of dialogs
CModelViewerCamera                  g_Camera[MESH_TYPE_MAX];    // Different camera for each mesh
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // Manages the 3D HUD
CDXUTDialog                         g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = NULL;

// Buffers
ID3D11Buffer*                       g_pInitCB;                  // constant buffer (change rarely)
ID3D11Buffer*                       g_pUpdateCB;                // constant buffer (update every frame)
ID3D11Buffer*                       g_pDamageCB;                // constant buffer (update every hit)
ID3D11Buffer*                       g_pMeshVB;                  // object mesh vertext buffer    

// Input Layouts
ID3D11InputLayout*                  g_pVertexLayout;            // object mesh layout

// Shaders
ID3D11VertexShader*                 g_pNoTessellationVS;        // tessellation off
ID3D11VertexShader*                 g_pDecalTessellationVS;     // tessellation on
ID3D11HullShader*                   g_pDecalTessellationHS;     // tessellation hull shader
ID3D11DomainShader*                 g_pDecalTessellationDS;     // tessellation domain shader
ID3D11PixelShader*                  g_pDecalTessellationPS;     // simple lighting

// Textures
ID3D11ShaderResourceView*           g_pBaseMapSRV;              // render target view of the base texture
ID3D11ShaderResourceView*           g_pDiffuseTextureSRV;       // user defined shader resource view

// States
ID3D11RasterizerState*              g_pRasterizerStateSolid;    // solid fill mode
ID3D11RasterizerState*              g_pRasterizerStateWireframe; // wireframe fill mode
ID3D11DepthStencilState*            g_pDepthStencilState;       // <= comparison depth state
ID3D11BlendState*                   g_pBlendStateNoBlend;       // disable blending
ID3D11SamplerState*                 g_pSamplerStateLinear;      // linear filtering

// The scene meshes 
CDXUTSDKMesh                        g_SceneMesh[MESH_TYPE_MAX];
Model                               g_SceneModel[MESH_TYPE_MAX];
MESH_TYPE                           g_eMeshType = MESH_TYPE_CAR;
D3DXMATRIX                          g_m4x4MeshMatrix[MESH_TYPE_MAX];
D3DXMATRIX                          g_m4x4MeshMatrixInv[MESH_TYPE_MAX];
float                               g_fDecalRadiusScale[MESH_TYPE_MAX];
float                               g_fMaxTessellationFactor[MESH_TYPE_MAX];
D3DXVECTOR4                         g_vMeshColor[MESH_TYPE_MAX];
D3DXVECTOR4                         g_vMeshSpecularColor[MESH_TYPE_MAX];
D3DXVECTOR4                         g_vAmbientColor;

// Decals
DECAL_TYPE                          g_eDecalType = DECAL_TYPE_METAL;
ID3D11ShaderResourceView*           g_pDisplacementMapSRV[DECAL_TYPE_MAX];  // shader view of the object displacement map
ID3D11ShaderResourceView*           g_pNormalMapSRV[DECAL_TYPE_MAX];        // shader view of the object normal map
float                               g_fDecalScale[DECAL_TYPE_MAX];          // default scale value
float                               g_fDecalBias[DECAL_TYPE_MAX];           // default bias value
bool                                g_bUserDecal = false;                   // true if user decal is found

// tessellation parameters
float                               g_TessellationFactor = 16;              // global tessellation factor
float                               g_DisplacementScale = 1.0f;             // displacement is multiplied by scale
float                               g_DisplacementBias = 0.0f;              // bias is added to the displacement after scaling
float                               g_DecalRadius = 1.0f;                   // radius of the damage
bool                                g_bTessellationDisabled = false;        // don't use tessellation if true
bool                                g_bDisplacementAdaptive = false;        // use adaptive tessellation based on displacement if true
bool                                g_bScreenSpaceAdaptive = true;          // use adaptive tessellation based on screen space size of patch if true
bool                                g_bDisplaceNormalDirection = false;     // if true, use the normal for the direction of displacement
bool                                g_bBackFaceCulling = false;

// miscellaneous globals
bool                                g_Shoot = false;                    // shoot button has been pressed
bool                                g_bEnableWireFrame = false;         // if true, draw wireframe
bool                                g_bUserMesh;                        // if true, mesh is a user defined mesh
unsigned                            g_decalIndex = 0;                   // current decal to update
DAMAGE_CB_STRUCT                    g_localDamageCB;                    // local copy of decals
bool                                g_bClearDecals = true;              // if true, clear the decal list
bool                                g_bNewDamageCB = true;              // if true, buffer needs initializing
D3DXVECTOR4                         g_vScreenSize;                      // curent screen width and height


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo, 
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, 
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, 
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                              BOOL bDumpShader = FALSE);
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                  BOOL bDumpShader = FALSE);
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                      BOOL bDumpShader = FALSE);
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                   BOOL bDumpShader = FALSE);
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                     BOOL bDumpShader = FALSE);
BOOL FileExists( WCHAR* pszFilename);

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

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    DXUTSetCallbackKeyboard( OnKeyboard );

    InitApp();
    DXUTInit( true, true );
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DecalTessellation11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );


    g_HUD.AddStatic( IDC_STATIC_CROSSHAIR, L"+", -350, 384, 10, 10 );
    CDXUTStatic* pCrosshair = g_HUD.GetStatic( IDC_STATIC_CROSSHAIR );
    D3DCOLOR crosshairColor = D3DCOLOR_XRGB( 0, 0xff, 0 );
    pCrosshair->SetTextColor( crosshairColor );

    int iY = 10; 
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); 
    
    iY = 0;
    g_SampleUI.AddStatic( IDC_STATIC_MESH, L"Mesh:", 0, iY, 55, 24 );
    CDXUTComboBox* pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_MESH, 0, iY += 25, 125, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 64 );
        pCombo->AddItem( L"Car", NULL );
        pCombo->AddItem( L"Teapot", NULL );
        pCombo->AddItem( L"Box", NULL );
        pCombo->AddItem( L"Tiger", NULL );
        pCombo->AddItem( L"User", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    g_SampleUI.AddStatic( IDC_STATIC_DECAL, L"Decal:", 0, iY += 25, 55, 24 );
    g_SampleUI.AddComboBox( IDC_COMBOBOX_DECAL, 0, iY += 25, 125, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 48 );
        pCombo->AddItem( L"Metal", NULL );
        pCombo->AddItem( L"Crack", NULL );
        pCombo->AddItem( L"Rock", NULL );
        pCombo->AddItem( L"User", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_WIREFRAME, L"Wireframe", 0, iY += 25, 125, 24, false );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_TESSELLATION, L"Tessellation", 0, iY += 25, 125, 24, !g_bTessellationDisabled );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE, L"Screen Space Adaptive", 0, iY += 25, 125, 24, g_bScreenSpaceAdaptive );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_DISPLACEMENT_ADAPTIVE, L"Displacement Adaptive", 0, iY += 25, 125, 24, g_bDisplacementAdaptive );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_DISPLACE_NORMAL, L"Displace Along Normal", 0, iY += 25, 125, 24, g_bDisplaceNormalDirection );
    g_SampleUI.AddCheckBox( IDC_CHECKBOX_BACK_FACE_CULLING, L"Back Face Culling", 0, iY += 25, 125, 24, g_bBackFaceCulling );
    WCHAR szTemp[256];
    swprintf_s( szTemp, L"Tessellation Factor : %f", g_TessellationFactor );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTOR, szTemp, 5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTOR, 0, iY += 25, 140, 24, g_cTessellationSliderMin, 
                          g_cTessellationSliderMax, (int)(g_TessellationFactor * g_cTessellationUIScale ), false );
    swprintf_s( szTemp, L"Displacement Scale : %f", g_DisplacementScale );
    g_SampleUI.AddStatic( IDC_STATIC_DIS_SCALE, szTemp, 5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_DIS_SCALE, 0, iY += 25, 140, 24, g_cDisplacementScaleSliderMin, 
                          g_cDisplacementScaleSliderMax, (int)(g_DisplacementScale * g_cDisplacementScaleUIScale ), false );
    swprintf_s( szTemp, L"Displacement Bias : %f", g_DisplacementBias );
    g_SampleUI.AddStatic( IDC_STATIC_DIS_BIAS, szTemp, 5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_DIS_BIAS, 0, iY += 25, 140, 24, g_cDisplacementBiasSliderMin, 
                          g_cDisplacementBiasSliderMax, (int)(g_DisplacementBias * g_cDisplacementBiasUIScale ), false );
    swprintf_s( szTemp, L"Decal Radius : %f", g_DecalRadius );
    g_SampleUI.AddStatic( IDC_STATIC_DECAL_RADIUS, szTemp, 5, iY += 25, 108, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_DECAL_RADIUS, 0, iY += 25, 140, 24, g_cDecalRadiusSliderMin, 
                          g_cDecalRadiusSliderMax, (int)(g_DecalRadius * g_cDecalRadiusUIScale ), false );
    g_SampleUI.AddButton( IDC_BUTTON_CLEAR, L"Clear Decals", 5, iY += 25, 108, 24 );
}

//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera[g_eMeshType].FrameMove( fElapsedTime );
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

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera[g_eMeshType].HandleMessages( hWnd, uMsg, wParam, lParam );

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
            case VK_SPACE:
            g_Shoot = true;
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];

    switch( nControlID )
    {
        // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:  
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEREF:         
            DXUTToggleREF();
            break;
        case IDC_CHANGEDEVICE:      
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;

        // sample controls
        case IDC_SLIDER_TESS_FACTOR:
            g_TessellationFactor = (float)((CDXUTSlider*)pControl)->GetValue() / (float)g_cTessellationUIScale;
            swprintf_s( szTemp, L"Tessellation Factor : %f", g_TessellationFactor );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetText( szTemp );
            break;
        case IDC_CHECKBOX_TESSELLATION:
            g_bTessellationDisabled = !((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_CHECKBOX_WIREFRAME:
            g_bEnableWireFrame = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_CHECKBOX_SCREEN_SPACE_ADAPTIVE:
            g_bScreenSpaceAdaptive = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_CHECKBOX_DISPLACEMENT_ADAPTIVE:
            g_bDisplacementAdaptive = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_CHECKBOX_DISPLACE_NORMAL:
            g_bDisplaceNormalDirection = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_CHECKBOX_BACK_FACE_CULLING:
            g_bBackFaceCulling = ((CDXUTCheckBox*)pControl)->GetChecked();
            break;
        case IDC_SLIDER_DIS_SCALE:
            g_DisplacementScale = (float)((CDXUTSlider*)pControl)->GetValue() / (float)g_cDisplacementScaleUIScale;
            swprintf_s( szTemp, L"Displacement Scale : %f", g_DisplacementScale );
            g_SampleUI.GetStatic( IDC_STATIC_DIS_SCALE )->SetText( szTemp );
            break;
        case IDC_SLIDER_DIS_BIAS:
            g_DisplacementBias = (float)((CDXUTSlider*)pControl)->GetValue() / (float)g_cDisplacementBiasUIScale;
            swprintf_s( szTemp, L"Displacement Bias : %f", g_DisplacementBias );
            g_SampleUI.GetStatic( IDC_STATIC_DIS_BIAS )->SetText( szTemp );
            break;
        case IDC_SLIDER_DECAL_RADIUS:
            g_DecalRadius = (float)((CDXUTSlider*)pControl)->GetValue() / (float)g_cDecalRadiusUIScale;
            swprintf_s( szTemp, L"Decal Radius : %f", g_DecalRadius );
            g_SampleUI.GetStatic( IDC_STATIC_DECAL_RADIUS )->SetText( szTemp );
            break;
        case IDC_COMBOBOX_MESH:
            {
                g_eMeshType = (MESH_TYPE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
                g_bClearDecals = true;
                g_TessellationFactor = g_fMaxTessellationFactor[g_eMeshType];
                swprintf_s( szTemp, L"Tessellation Factor : %f", g_TessellationFactor );
                g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetText( szTemp );
                CDXUTSlider* pSlider = g_SampleUI.GetSlider( IDC_SLIDER_TESS_FACTOR );
                pSlider->SetValue((int)(g_TessellationFactor * g_cTessellationUIScale));
                // set the colors
                INIT_CB_STRUCT init_cb_struct;
                ZeroMemory( &init_cb_struct, sizeof( init_cb_struct ) );
                ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
                init_cb_struct.vMaterialColor = g_vMeshColor[g_eMeshType];
                init_cb_struct.vAmbientColor = g_vAmbientColor;
                init_cb_struct.vSpecularColor = g_vMeshSpecularColor[g_eMeshType];
                init_cb_struct.vScreenSize = g_vScreenSize;
                init_cb_struct.vFlags.x = g_SceneModel[g_eMeshType].textured;
                pd3dImmediateContext->UpdateSubresource( g_pInitCB, 0, NULL, &init_cb_struct, 0, 0 );
            }
            break;
        case IDC_COMBOBOX_DECAL:
            {
                DECAL_TYPE decal = g_eDecalType;
                g_eDecalType = (DECAL_TYPE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
                if( g_eDecalType == DECAL_TYPE_USER && !g_bUserDecal )
                {
                    g_eDecalType = decal; // not a valid decal - restore the previous decal
                }
            }
            break;
        case IDC_BUTTON_CLEAR:
            g_bClearDecals = true;
            break;
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    // Get device context
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    // GUI creation
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    //
    // Compile shaders
    //
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    // decal tessellation shaders
    ID3DBlob* pBlobVS_DecalTessellation = NULL;
    V_RETURN( CreateVertexShaderFromFile( pd3dDevice, L"DecalTessellation11.hlsl", NULL, NULL, "VS_NoTessellation",
                                         "vs_5_0", dwShaderFlags, 0, NULL, &g_pNoTessellationVS ) );
    V_RETURN( CreateVertexShaderFromFile( pd3dDevice, L"DecalTessellation11.hlsl", NULL, NULL, "VS",
                                         "vs_5_0", dwShaderFlags, 0, NULL, &g_pDecalTessellationVS, &pBlobVS_DecalTessellation ) );
    V_RETURN( CreateHullShaderFromFile( pd3dDevice,   L"DecalTessellation11.hlsl", NULL, NULL, "HS",
                                         "hs_5_0", dwShaderFlags, 0, NULL, &g_pDecalTessellationHS ) );
    V_RETURN( CreateDomainShaderFromFile( pd3dDevice, L"DecalTessellation11.hlsl", NULL, NULL, "DS",
                                         "ds_5_0", dwShaderFlags, 0, NULL, &g_pDecalTessellationDS ) );
    V_RETURN( CreatePixelShaderFromFile( pd3dDevice,  L"DecalTessellation11.hlsl", NULL, NULL, "PS",
                                         "ps_5_0", dwShaderFlags, 0, NULL, &g_pDecalTessellationPS ) );

    //
    // Create vertex layout for scene meshes
    //
    const D3D11_INPUT_ELEMENT_DESC vertexElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( vertexElements, ARRAYSIZE( vertexElements ), 
                                             pBlobVS_DecalTessellation->GetBufferPointer(), pBlobVS_DecalTessellation->GetBufferSize(), 
                                             &g_pVertexLayout ) );
    SAFE_RELEASE( pBlobVS_DecalTessellation );
    DXUT_SetDebugName( g_pVertexLayout, "Primary" );

    // Initialize the box mesh
    Vertex* pVBData = NULL;
    CreateBoxModel( 0.0f, 0.0f, 0.0f, 6.0f, 6.0f, 6.0f, &g_SceneModel[MESH_TYPE_BOX], &pVBData );
    g_SceneModel[MESH_TYPE_BOX].textured = true;

    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof( InitData ) );
    InitData.pSysMem = pVBData;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC bd;
    ZeroMemory( &bd, sizeof( bd ) );
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( Vertex ) * g_SceneModel[MESH_TYPE_BOX].triCount * 3;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;

    // Create DX11 vertex buffer specifying initial data
    V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData, &g_pMeshVB ) );
    DXUT_SetDebugName( g_pMeshVB, "bd" );
    delete pVBData;

    // Initialize the rest of the scene meshes ...

    // Setup the camera for each scene   
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    // Tiger
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TIGER].SetViewParams( &vecEye, &vecAt );
    // Teapot
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -4.0f;
    g_Camera[MESH_TYPE_TEAPOT].SetViewParams( &vecEye, &vecAt );
    // User
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -3.0f;
    g_Camera[MESH_TYPE_USER].SetViewParams( &vecEye, &vecAt );
    // Box
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -15.0f;
    g_Camera[MESH_TYPE_BOX].SetViewParams( &vecEye, &vecAt );
    // Car
    vecEye.x = 0.0f; vecEye.y = 0.0f; vecEye.z = -15;
    g_Camera[MESH_TYPE_CAR].SetViewParams( &vecEye, &vecAt );

    // Setup the mesh params for adaptive tessellation
    g_fMaxTessellationFactor[MESH_TYPE_TIGER]  = 8.0f;
    g_fMaxTessellationFactor[MESH_TYPE_TEAPOT] = 8.0f;
    g_fMaxTessellationFactor[MESH_TYPE_BOX]    = 16.0f;
    g_fMaxTessellationFactor[MESH_TYPE_CAR]    = 3.0f;
    g_fMaxTessellationFactor[MESH_TYPE_USER]   = 8.0f;

    g_TessellationFactor = g_fMaxTessellationFactor[g_eMeshType];
    g_SampleUI.GetSlider( IDC_SLIDER_TESS_FACTOR )->SetValue( (int)(g_TessellationFactor * g_cTessellationUIScale) );
    WCHAR szTemp[256];
    swprintf_s( szTemp, L"Tessellation Factor : %f", g_TessellationFactor );
    g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTOR )->SetText( szTemp );

    // Setup the decal radius scale for each mesh (since each mesh is a different size)
    g_fDecalRadiusScale[MESH_TYPE_TIGER]    = 0.3f;
    g_fDecalRadiusScale[MESH_TYPE_TEAPOT]   = 0.25f;
    g_fDecalRadiusScale[MESH_TYPE_BOX]      = 1.0f;
    g_fDecalRadiusScale[MESH_TYPE_CAR]      = 1.0f;
    g_fDecalRadiusScale[MESH_TYPE_USER]     = 1.0f;

    // Setup the matrix for each mesh
    D3DXMATRIXA16 mModelRotationX;
    D3DXMATRIXA16 mModelRotationY;
    D3DXMATRIXA16 mModelTranslation;

    // Tiger
    D3DXMatrixRotationX( &mModelRotationX, -D3DX_PI / 36 ); 
    D3DXMatrixRotationY( &mModelRotationY, D3DX_PI / 4 ); 
    g_m4x4MeshMatrix[MESH_TYPE_TIGER] = mModelRotationX * mModelRotationY;
    D3DXMatrixInverse( &g_m4x4MeshMatrixInv[MESH_TYPE_TIGER], NULL, &g_m4x4MeshMatrix[MESH_TYPE_TIGER] );

    // Teapot
    D3DXMatrixIdentity( &g_m4x4MeshMatrix[MESH_TYPE_TEAPOT] );
    D3DXMatrixIdentity( &g_m4x4MeshMatrixInv[MESH_TYPE_TEAPOT] );

    // Box
    D3DXMatrixIdentity( &g_m4x4MeshMatrix[MESH_TYPE_BOX] );
    D3DXMatrixIdentity( &g_m4x4MeshMatrixInv[MESH_TYPE_BOX] );

    // Car
    D3DXMatrixTranslation( &mModelTranslation, 0.0f, -1.0f, 0.0f );
    g_m4x4MeshMatrix[MESH_TYPE_CAR] = mModelTranslation;
    D3DXMatrixInverse( &g_m4x4MeshMatrixInv[MESH_TYPE_CAR], NULL, &g_m4x4MeshMatrix[MESH_TYPE_CAR] );

    // User
    D3DXMatrixTranslation( &mModelTranslation, 0.0f, -1.0f, 0.0f );
    g_m4x4MeshMatrix[MESH_TYPE_USER] = mModelTranslation;
    D3DXMatrixInverse( &g_m4x4MeshMatrixInv[MESH_TYPE_USER], NULL, &g_m4x4MeshMatrix[MESH_TYPE_USER] );

    // Initialize the Mesh Colors
    // Box
    g_vMeshColor[MESH_TYPE_BOX].x = 1.0;
    g_vMeshColor[MESH_TYPE_BOX].y = 1.0;
    g_vMeshColor[MESH_TYPE_BOX].z = 1.0;
    g_vMeshColor[MESH_TYPE_BOX].w = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_BOX].x = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_BOX].y = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_BOX].z = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_BOX].w = 1.0;
    // Tiger
    g_vMeshColor[MESH_TYPE_TIGER].x = 1.0;
    g_vMeshColor[MESH_TYPE_TIGER].y = 1.0;
    g_vMeshColor[MESH_TYPE_TIGER].z = 1.0;
    g_vMeshColor[MESH_TYPE_TIGER].w = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_TIGER].x = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_TIGER].y = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_TIGER].z = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_TIGER].w = 1.0;
    // Teapot
    g_vMeshColor[MESH_TYPE_TEAPOT].x = 1.0;
    g_vMeshColor[MESH_TYPE_TEAPOT].y = 1.0;
    g_vMeshColor[MESH_TYPE_TEAPOT].z = 0.0;
    g_vMeshColor[MESH_TYPE_TEAPOT].w = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_TEAPOT].x = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_TEAPOT].y = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_TEAPOT].z = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_TEAPOT].w = 1.0;
    // Car
    g_vMeshColor[MESH_TYPE_CAR].x = 1.0;
    g_vMeshColor[MESH_TYPE_CAR].y = 0.0;
    g_vMeshColor[MESH_TYPE_CAR].z = 0.0;
    g_vMeshColor[MESH_TYPE_CAR].w = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_CAR].x = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_CAR].y = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_CAR].z = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_CAR].w = 1.0;
    // User
    g_vMeshColor[MESH_TYPE_USER].x = 1.0;
    g_vMeshColor[MESH_TYPE_USER].y = 1.0;
    g_vMeshColor[MESH_TYPE_USER].z = 1.0;
    g_vMeshColor[MESH_TYPE_USER].w = 1.0;
    g_vMeshSpecularColor[MESH_TYPE_USER].x = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_USER].y = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_USER].z = 0.0;
    g_vMeshSpecularColor[MESH_TYPE_USER].w = 1.0;

    // Load the standard scene meshes
    V_RETURN( g_SceneMesh[MESH_TYPE_TIGER].Create( pd3dDevice, L"tiger\\tiger.sdkmesh" ) );
    ConvertSDKMeshToModel( &g_SceneMesh[MESH_TYPE_TIGER], &g_SceneModel[MESH_TYPE_TIGER] );

    V_RETURN( g_SceneMesh[MESH_TYPE_TEAPOT].Create( pd3dDevice, L"teapot\\teapot.sdkmesh" ) );
    ConvertSDKMeshToModel( &g_SceneMesh[MESH_TYPE_TEAPOT], &g_SceneModel[MESH_TYPE_TEAPOT] );
   
    V_RETURN( g_SceneMesh[MESH_TYPE_CAR].Create( pd3dDevice, L"ExoticCar\\ExoticCar.sdkmesh" ) );
    ConvertSDKMeshToModel( &g_SceneMesh[MESH_TYPE_CAR], &g_SceneModel[MESH_TYPE_CAR] );

    // Load a user mesh and textures if present
    g_bUserMesh = false;
    g_pDiffuseTextureSRV = NULL;
    // The mesh
    if( FileExists( L"media\\user\\user.sdkmesh" ) )
    {
        V_RETURN( g_SceneMesh[MESH_TYPE_USER].Create( pd3dDevice, L"media\\user\\user.sdkmesh" ) );
        g_bUserMesh = true;
        ConvertSDKMeshToModel( &g_SceneMesh[MESH_TYPE_USER], &g_SceneModel[MESH_TYPE_USER] );
    }
    // The user textures
    if( FileExists( L"media\\user\\diffuse.dds" ) )
    {
        V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"media\\user\\diffuse.dds", NULL, NULL, &g_pDiffuseTextureSRV, NULL ) );
        DXUT_SetDebugName( g_pDiffuseTextureSRV, "diffuse.dds" );
    }

    // Create the constant buffers ...
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    // Create the Init constant buffer
    bd.ByteWidth = sizeof( INIT_CB_STRUCT );
    V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &g_pInitCB ) );
    DXUT_SetDebugName( g_pInitCB, "INIT_CB_STRUCT" );

    // Create Update constant buffer
    bd.ByteWidth = sizeof( UPDATE_CB_STRUCT );
    V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &g_pUpdateCB ) );
    DXUT_SetDebugName( g_pUpdateCB, "UPDATE_CB_STRUCT" );

    // Create the damage constant buffer
    bd.ByteWidth = sizeof( DAMAGE_CB_STRUCT );
    V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &g_pDamageCB ) );
    DXUT_SetDebugName( g_pDamageCB, "DAMAGE_CB_STRUCT" );
    g_bNewDamageCB = true;

    // Initialize Constant Buffers ...
    D3DXVECTOR4 vDisplacementMapSize;

    g_vAmbientColor.x = 0.1f;
    g_vAmbientColor.y = 0.1f;
    g_vAmbientColor.z = 0.1f;
    g_vAmbientColor.w = 1.0f;

    g_vScreenSize.x = (float)pBackBufferSurfaceDesc->Width;
    g_vScreenSize.y = (float)pBackBufferSurfaceDesc->Height;

    // initialize the Init constant buffer
    INIT_CB_STRUCT init_cb_struct;
    ZeroMemory( &init_cb_struct, sizeof( init_cb_struct ) );
    init_cb_struct.vMaterialColor = g_vMeshColor[g_eMeshType];
    init_cb_struct.vAmbientColor = g_vAmbientColor;
    init_cb_struct.vSpecularColor = g_vMeshSpecularColor[g_eMeshType];
    init_cb_struct.vScreenSize = g_vScreenSize;
    init_cb_struct.vFlags.x = 1;
    pd3dImmediateContext->UpdateSubresource( g_pInitCB, 0, NULL, &init_cb_struct, 0, 0 );

    // Create Textures
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Base.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pBaseMapSRV, NULL ) );
    DXUT_SetDebugName( g_pBaseMapSRV, "base.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Metal_Displacement.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDisplacementMapSRV[DECAL_TYPE_METAL], NULL ) );
    DXUT_SetDebugName( g_pDisplacementMapSRV[DECAL_TYPE_METAL], "Metal_Displacement.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Metal_Normal.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pNormalMapSRV[DECAL_TYPE_METAL], NULL ) );
    DXUT_SetDebugName( g_pNormalMapSRV[DECAL_TYPE_METAL], "Metal_Normal.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Rock_Displacement.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDisplacementMapSRV[DECAL_TYPE_ROCK], NULL ) );
    DXUT_SetDebugName( g_pDisplacementMapSRV[DECAL_TYPE_ROCK], "Rock_Displacement.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Rock_Normal.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pNormalMapSRV[DECAL_TYPE_ROCK], NULL ) );
    DXUT_SetDebugName( g_pNormalMapSRV[DECAL_TYPE_ROCK], "Rock_Normal.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Crack_Displacement.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDisplacementMapSRV[DECAL_TYPE_CRACK], NULL ) );
    DXUT_SetDebugName( g_pDisplacementMapSRV[DECAL_TYPE_CRACK], "Crack_Displacement.bmp" );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "Misc\\Crack_Normal.bmp" ) ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pNormalMapSRV[DECAL_TYPE_CRACK], NULL ) );
    DXUT_SetDebugName( g_pNormalMapSRV[DECAL_TYPE_CRACK], "Crack_Normal.bmp" );

    if( FileExists( L"media\\user\\Displacement.bmp" ) )
    {
        V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"media\\user\\Displacement.bmp", NULL, NULL, &g_pDisplacementMapSRV[MESH_TYPE_USER], NULL ) );
        DXUT_SetDebugName( g_pDisplacementMapSRV[MESH_TYPE_USER], "User Displacement.bmp" );

        V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, L"media\\user\\Normal.bmp", NULL, NULL, &g_pNormalMapSRV[MESH_TYPE_USER], NULL ) );
        DXUT_SetDebugName( g_pNormalMapSRV[MESH_TYPE_USER], "User Normal.bmop" );

        g_bUserDecal = true;
    }

    // Decal Scale and Bias (from GPUMeshMapper Tool)
    g_fDecalScale[DECAL_TYPE_METAL] = -0.20676535f;
    g_fDecalBias[DECAL_TYPE_METAL] = 0.16655855f;
    g_fDecalScale[DECAL_TYPE_ROCK] = -0.3848734f;
    g_fDecalBias[DECAL_TYPE_ROCK] = 0.1968906f;
    g_fDecalScale[DECAL_TYPE_CRACK] = -0.309316875f;
    g_fDecalBias[DECAL_TYPE_CRACK] = 0.186604625f;
    g_fDecalScale[DECAL_TYPE_USER] = -1.0f;
    g_fDecalBias[DECAL_TYPE_USER] = 0.0f;

    //
    // Create state objects
    //

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof( RasterDesc ) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;
    RasterDesc.DepthClipEnable = TRUE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );
    DXUT_SetDebugName( g_pRasterizerStateSolid, "Solid" );

    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );
    DXUT_SetDebugName( g_pRasterizerStateWireframe, "Wireframe" );

    // Create a less than or equal comparison depthstencil state
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    ZeroMemory( &DSDesc, sizeof( DSDesc ) );
    DSDesc.DepthEnable = TRUE;
    DSDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.StencilEnable = FALSE;
    V_RETURN( pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pDepthStencilState ) );
    DXUT_SetDebugName( g_pDepthStencilState, "DepthStencil" );

    // Disabled blending state
    D3D11_BLEND_DESC BlendState;
    ZeroMemory( &BlendState, sizeof( BlendState ) );
    BlendState.IndependentBlendEnable = FALSE;
    BlendState.RenderTarget[0].BlendEnable = FALSE;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    V_RETURN( pd3dDevice->CreateBlendState( &BlendState, &g_pBlendStateNoBlend ) );
    DXUT_SetDebugName( g_pBlendStateNoBlend, "No Blend" );
    
    // Linear filtering sampler state
    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( SSDesc ) );
    SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SSDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy = 16;
    SSDesc.MinLOD = 0;
    SSDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateLinear) );
    DXUT_SetDebugName( g_pSamplerStateLinear, "Linear" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup each camera's projection parameters
    float fAspectRatio = (FLOAT)pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    for( UINT i = 0; i < MESH_TYPE_MAX; i++ )
    {
        g_Camera[i].SetProjParams( D3DX_PI/4, fAspectRatio, 0.1f, 5000.0f );
        g_Camera[i].SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
        g_Camera[i].SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON );
    }

    // Set GUI size and locations
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 245, pBackBufferSurfaceDesc->Height - 520 );
    g_SampleUI.SetSize( 245, 520 );
    CDXUTStatic* pCrosshair = g_HUD.GetStatic( IDC_STATIC_CROSSHAIR );
    pCrosshair->SetLocation( 170 - (pBackBufferSurfaceDesc->Width / 2), pBackBufferSurfaceDesc->Height / 2 );
    g_vScreenSize.x = (float)pBackBufferSurfaceDesc->Width;
    g_vScreenSize.y = (float)pBackBufferSurfaceDesc->Height;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// RenderMesh Allows the app to render individual meshes of an sdkmesh
// and override the primitive topology
//--------------------------------------------------------------------------------------
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, 
                 D3D11_PRIMITIVE_TOPOLOGY PrimType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED, 
                 UINT uDiffuseSlot = INVALID_SAMPLER_SLOT, 
                 UINT uNormalSlot = INVALID_SAMPLER_SLOT, 
                 UINT uSpecularSlot = INVALID_SAMPLER_SLOT )
{
    #define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

    assert( NULL != pDXUTMesh );

    if( 0 < pDXUTMesh->GetOutstandingBufferResources() )
    {
        return;
    }

    SDKMESH_MESH* pMesh = pDXUTMesh->GetMesh( uMesh );

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if( pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS )
    {
        return;
    }

    for( UINT64 i = 0; i < pMesh->NumVertexBuffers; i++ )
    {
        pVB[i] = pDXUTMesh->GetVB11( uMesh, (UINT)i );
        Strides[i] = pDXUTMesh->GetVertexStride( uMesh, (UINT)i );
        Offsets[i] = 0;
    }

    ID3D11Buffer* pIB = pDXUTMesh->GetIB11( pMesh->IndexBuffer );
    DXGI_FORMAT ibFormat = pDXUTMesh->GetIBFormat11( pMesh->IndexBuffer );
    
    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    DXUTGetD3D11DeviceContext()->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;

    for( UINT uSubset = 0; uSubset < pMesh->NumSubsets; uSubset++ )
    {
        pSubset = pDXUTMesh->GetSubset( uMesh, uSubset );

        if( D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED == PrimType )
        {
            PrimType = pDXUTMesh->GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        }
        
        DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( PrimType );

        pMat = pDXUTMesh->GetMaterial( pSubset->MaterialID );
        if( uDiffuseSlot != INVALID_SAMPLER_SLOT && pMat->pDiffuseRV11 && !IsErrorResource( pMat->pDiffuseRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uDiffuseSlot, 1, &pMat->pDiffuseRV11 );
        }

        if( uNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uNormalSlot, 1, &pMat->pNormalRV11 );
        }

        if( uSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ) )
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uSpecularSlot, 1, &pMat->pSpecularRV11 );
        }

        UINT IndexCount = ( UINT )pSubset->IndexCount;
        UINT IndexStart = ( UINT )pSubset->IndexStart;
        UINT VertexStart = ( UINT )pSubset->VertexStart;
        
        DXUTGetD3D11DeviceContext()->DrawIndexed( IndexCount, IndexStart, VertexStart );
    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    ID3D11Buffer*               pBuffers[3];
    ID3D11ShaderResourceView*   pSRV[4];
    ID3D11SamplerState*         pSS[1];

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    if( g_bClearDecals )
    {
        // Initialize the Damage constant buffer to all zeros
        ZeroMemory( &g_localDamageCB, sizeof( g_localDamageCB ) );
        pd3dImmediateContext->UpdateSubresource( g_pDamageCB, 0, NULL, &g_localDamageCB, 0, 0 );
        g_decalIndex = 0;
        g_bClearDecals = false;
    }

    // Set common states
    pd3dImmediateContext->OMSetBlendState( g_pBlendStateNoBlend, 0, 0xffffffff );
    pd3dImmediateContext->RSSetState( g_bEnableWireFrame ? g_pRasterizerStateWireframe : g_pRasterizerStateSolid );
    // Set sampler states
    pSS[0] = g_pSamplerStateLinear;
    pd3dImmediateContext->VSSetSamplers( 0, 1, pSS );
    pd3dImmediateContext->HSSetSamplers( 0, 1, pSS );
    pd3dImmediateContext->DSSetSamplers( 0, 1, pSS );
    pd3dImmediateContext->PSSetSamplers( 0, 1, pSS );

    // transform the eye point according to the mesh matrix
    const D3DXVECTOR3* eyePt = g_Camera[g_eMeshType].GetEyePt();
    D3DXVECTOR4 vEyePt;
    vEyePt.x = eyePt->x;
    vEyePt.y = eyePt->y;
    vEyePt.z = eyePt->z;
    vEyePt.w = 1.0f;
    D3DXVECTOR4 tfmEyePt;
    D3DXVec3Transform( &tfmEyePt, eyePt, &g_m4x4MeshMatrixInv[g_eMeshType] );
    D3DXVECTOR4 tfmOrigin;
    D3DXVECTOR3 origin( 0.0f, 0.0f, 0.0f );
    D3DXVec3Transform( &tfmOrigin, &origin, &g_m4x4MeshMatrixInv[g_eMeshType] );
   
    // Update Matrices
    // Projection matrix
    D3DXMATRIX mProj = *g_Camera[g_eMeshType].GetProjMatrix();
    // View matrix
    D3DXMATRIX mView = *g_Camera[g_eMeshType].GetViewMatrix();
    // World Matrix
    D3DXMATRIX mWorld = g_m4x4MeshMatrix[g_eMeshType] * (*g_Camera[g_eMeshType].GetWorldMatrix());
    // View Projection Matrix
    D3DXMATRIX mViewProjection = mView * mProj;    
    // World View Projection Matrix
    D3DXMATRIX mWorldViewProjection = mWorld * mView * mProj;    

    // If gun was fired, find the intersection
    bool hitDetected = false;
    if( g_Shoot )
    {
        // store the bullet trajectory in a ray structure
        Ray bulletRay;    

        // assume the viewing direction is always towards the origin
        bulletRay.origin.x = tfmEyePt.x;
        bulletRay.origin.y = tfmEyePt.y;
        bulletRay.origin.z = tfmEyePt.z;
        bulletRay.direction.x = tfmOrigin.x - tfmEyePt.x;
        bulletRay.direction.y = tfmOrigin.y - tfmEyePt.y;
        bulletRay.direction.z = tfmOrigin.z - tfmEyePt.z;
        D3DXVec3Normalize( &bulletRay.direction, &bulletRay.direction );

        // see if there was an intersection with the geometry
        D3DXVECTOR3 intersection;
        Triangle* pTri = NULL;
        if( RayCastForDecalPosition( bulletRay, g_SceneModel[g_eMeshType], &intersection, &pTri ) )
        {
            hitDetected = true;

            // transform the results to world space
            D3DXVECTOR4 tfmVec;
            D3DXVec3Transform( &tfmVec, &intersection, &mWorld );
            intersection.x = tfmVec.x;
            intersection.y = tfmVec.y;
            intersection.z = tfmVec.z;
            D3DXVec3Transform( &tfmVec, &bulletRay.origin, &mWorld );
            bulletRay.origin.x = tfmVec.x;
            bulletRay.origin.y = tfmVec.y;
            bulletRay.origin.z = tfmVec.z;

            float decalRadius = g_DecalRadius * g_fDecalRadiusScale[g_eMeshType];
            float minRadius = decalRadius;

            // see if this decal will overlap with any existing decals
            for( UINT i = 0; i < MAX_DECALS; i++ )
            {
                if( D3DXVec4Length( &g_localDamageCB.vNormal[i] ) == 0 )
                    break; // the rest of the list is empty
                D3DXVECTOR3 decalLocation;
                decalLocation.x = g_localDamageCB.vDecalPositonSize[i].x;
                decalLocation.y = g_localDamageCB.vDecalPositonSize[i].y;
                decalLocation.z = g_localDamageCB.vDecalPositonSize[i].z;
                float decalSize = g_localDamageCB.vDecalPositonSize[i].w;
                D3DXVECTOR3 vIntToDecal = decalLocation - intersection;
                float distance = D3DXVec3Length( &vIntToDecal );
                if( distance < ((g_DecalRadius * g_fDecalRadiusScale[g_eMeshType]) + decalSize) )
                {
                    // adjust the size of the decal to fit
                    decalRadius = distance - decalSize;
                    decalRadius *= 0.9f;
                    if( decalRadius < minRadius )
                    {
                        minRadius = decalRadius;
                    }
                }
            }

            D3DXVECTOR3    vHitDirection;
            D3DXVECTOR3    vNormal;
            D3DXVECTOR3    vBinormal;
            D3DXVECTOR3    vTangent;

            //compute the hit direction
            if( g_bDisplaceNormalDirection )
            {
                D3DXVec3TransformNormal( &vHitDirection, &pTri->faceNormal, &mWorld );
                vHitDirection = -vHitDirection;
            }
            else
            {
                vHitDirection = intersection - bulletRay.origin;
                D3DXVec3Normalize( &vHitDirection, &vHitDirection );
            }

            // create a tangent space for the damage area
            CreateOrthonormalBasis( &vHitDirection, &vNormal, &vBinormal, &vTangent );

            // Pack the tangent space vectors with the hit location
            D3DXVECTOR4 vNormalConst;
            D3DXVECTOR4 vBinormalConst;
            D3DXVECTOR4 vTangentConst;
            D3DXVECTOR4 vDecalPositionSize;

            vNormalConst.x = vNormal.x;
            vNormalConst.y = vNormal.y;
            vNormalConst.z = vNormal.z;

            vBinormalConst.x = vBinormal.x;
            vBinormalConst.y = vBinormal.y;
            vBinormalConst.z = vBinormal.z;

            vTangentConst.x = vTangent.x;
            vTangentConst.y = vTangent.y;
            vTangentConst.z = vTangent.z;

            vDecalPositionSize.x = intersection.x;
            vDecalPositionSize.y = intersection.y;
            vDecalPositionSize.z = intersection.z;
            vDecalPositionSize.w = minRadius;

            // update the local copy of the decals
            g_localDamageCB.vNormal[g_decalIndex] = vNormalConst;
            g_localDamageCB.vBinormal[g_decalIndex] = vBinormalConst;
            g_localDamageCB.vTangent[g_decalIndex] = vTangentConst;
            g_localDamageCB.vDecalPositonSize[g_decalIndex] = vDecalPositionSize;
            g_decalIndex++;
            if( g_decalIndex >= MAX_DECALS )
            {
                g_decalIndex = 0;    // restart at the beginning of the circular buffer
            }
        }
        g_Shoot = false; // reset shoot boolean
    }

    if( hitDetected || g_bNewDamageCB )
    {
        // update the damage constant buffer
        pd3dImmediateContext->UpdateSubresource( g_pDamageCB, 0, NULL, &g_localDamageCB, 0, 0 );
        g_bNewDamageCB = false;
    }

    // Render the Model

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.00f, 0.00f, 0.00f, 1.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );


    // Transpose matrices before passing to shader stages
    D3DXMatrixTranspose( &mWorld, &mWorld );                    
    D3DXMatrixTranspose( &mViewProjection, &mViewProjection );
    D3DXMatrixTranspose( &mWorldViewProjection, &mWorldViewProjection ); 

    D3DXVECTOR4 vLightPosition;
    vLightPosition.x = 20;
    vLightPosition.y = 20;
    vLightPosition.z = eyePt->z;
    vLightPosition.w = 1.0f;

    float scale = g_DisplacementScale * g_fDecalScale[g_eDecalType];
    float bias = (g_DisplacementScale * g_fDecalBias[g_eDecalType]) + g_DisplacementBias;
 
    // update the constant buffer
    UPDATE_CB_STRUCT update_cb_struct;
    ZeroMemory( &update_cb_struct, sizeof( update_cb_struct ) );
    update_cb_struct.mWorld = mWorld;
    update_cb_struct.mViewProjection = mViewProjection;
    update_cb_struct.mWorldViewProjection = mWorldViewProjection;
    update_cb_struct.vTessellationFactor = 
        D3DXVECTOR4( g_TessellationFactor, g_bScreenSpaceAdaptive, g_bBackFaceCulling, g_bDisplacementAdaptive );
    update_cb_struct.vDisplacementScaleBias = 
        D3DXVECTOR4( scale * g_fDecalRadiusScale[g_eMeshType], bias * g_fDecalRadiusScale[g_eMeshType], 
        g_DecalRadius * g_fDecalRadiusScale[g_eMeshType], 0 );
    update_cb_struct.vLightPosition = vLightPosition;
    update_cb_struct.vEyePosition = vEyePt;
    pd3dImmediateContext->UpdateSubresource( g_pUpdateCB, 0, NULL, &update_cb_struct, 0, 0 );

    // Bind the constant buffers to the device for all stages
    pBuffers[0] = g_pInitCB;
    pBuffers[1] = g_pUpdateCB;
    pBuffers[2] = g_pDamageCB;
    pd3dImmediateContext->VSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->HSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->DSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->GSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->PSSetConstantBuffers( 0, 3, pBuffers );

    // set shader resources
    pSRV[0] = g_pDisplacementMapSRV[g_eDecalType];
    pSRV[1] = g_pNormalMapSRV[g_eDecalType];
    pSRV[2] = g_pBaseMapSRV;
    pd3dImmediateContext->DSSetShaderResources( 0, 3, pSRV );
    pd3dImmediateContext->PSSetShaderResources( 0, 3, pSRV );

    // set the shaders & primitive topology based on if tessellation is enabled
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    if( g_bTessellationDisabled || (0 == g_decalIndex) )
    {
        pd3dImmediateContext->VSSetShader( g_pNoTessellationVS, NULL, 0 );
        pd3dImmediateContext->HSSetShader( NULL, NULL, 0);
        pd3dImmediateContext->DSSetShader( NULL, NULL, 0);
        PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
    else
    {
        pd3dImmediateContext->VSSetShader( g_pDecalTessellationVS, NULL, 0 );
        pd3dImmediateContext->HSSetShader( g_pDecalTessellationHS, NULL, 0);
        pd3dImmediateContext->DSSetShader( g_pDecalTessellationDS, NULL, 0);
        PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    }
    pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
    pd3dImmediateContext->PSSetShader( g_pDecalTessellationPS, NULL, 0 ); 

    // update state
    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthStencilState, 0 );

    // set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &g_pMeshVB, &stride, &offset );

    // set input layout
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    if( g_eMeshType == MESH_TYPE_BOX )
    {
        // Draw the box model
        pd3dImmediateContext->IASetPrimitiveTopology( PrimitiveTopology );
        pd3dImmediateContext->Draw( g_SceneModel[MESH_TYPE_BOX].triCount * 3, 0 );
    }
    else
    {
        // Render the meshes    
        for( UINT iMesh = 0; iMesh < g_SceneMesh[g_eMeshType].GetNumMeshes(); iMesh++ )
        {
            RenderMesh( &g_SceneMesh[g_eMeshType], iMesh, PrimitiveTopology, 2 );
        }
    }

    // Render GUI
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
        
    // Always render text info 
    RenderText();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    // Release Buffers
    SAFE_RELEASE( g_pInitCB );
    SAFE_RELEASE( g_pUpdateCB );
    SAFE_RELEASE( g_pDamageCB );
    SAFE_RELEASE( g_pMeshVB );

    // Release Input Layouts
    SAFE_RELEASE( g_pVertexLayout );

    // Release Shaders
    SAFE_RELEASE( g_pNoTessellationVS );
    SAFE_RELEASE( g_pDecalTessellationVS );
    SAFE_RELEASE( g_pDecalTessellationHS );
    SAFE_RELEASE( g_pDecalTessellationDS );
    SAFE_RELEASE( g_pDecalTessellationPS );

    // Release Textures
    SAFE_RELEASE( g_pBaseMapSRV );
    SAFE_RELEASE( g_pDiffuseTextureSRV );

    // Release States
    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pDepthStencilState );
    SAFE_RELEASE( g_pBlendStateNoBlend );
    SAFE_RELEASE( g_pSamplerStateLinear );

    // Release SceneMeshes
    for( UINT i = 0; i < MESH_TYPE_MAX; i++ )
    {
        g_SceneMesh[i].Destroy();
        SAFE_DELETE( g_SceneModel[i].triArray );
    }

    // Release Decals
    for( UINT i = 0; i < DECAL_TYPE_MAX; i++ )
    {
        SAFE_RELEASE( g_pDisplacementMapSRV[i] );
        SAFE_RELEASE( g_pNormalMapSRV[i] );
    }

}


//--------------------------------------------------------------------------------------
// Helper function to create a shader from the specified filename
// This function is called by the shader-specific versions of this
// function located after the body of this function.
//--------------------------------------------------------------------------------------
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob, 
                              BOOL bDumpShader)
{
    HRESULT   hr = D3D_OK;
    ID3DBlob* pShaderBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    WCHAR     wcFullPath[256];
    
    DXUTFindDXSDKMediaFileCch( wcFullPath, 256, pSrcFile );
    // Compile shader into binary blob
    hr = D3DX11CompileFromFile( wcFullPath, pDefines, pInclude, pFunctionName, pProfile, 
                                Flags1, Flags2, pPump, &pShaderBlob, &pErrorBlob, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    
    // Create shader from binary blob
    if ( ppShader )
    {
        hr = E_FAIL;
        if ( strstr( pProfile, "vs" ) )
        {
            hr = pd3dDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)ppShader );
        }
        else if ( strstr( pProfile, "hs" ) )
        {
            hr = pd3dDevice->CreateHullShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11HullShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ds" ) )
        {
            hr = pd3dDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11DomainShader**)ppShader );
        }
        else if ( strstr( pProfile, "gs" ) )
        {
            hr = pd3dDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ps" ) )
        {
            hr = pd3dDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "cs" ) )
        {
            hr = pd3dDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)ppShader );
        }
        if ( FAILED( hr ) )
        {
            OutputDebugString( L"Shader creation failed\n" );
            SAFE_RELEASE( pErrorBlob );
            SAFE_RELEASE( pShaderBlob );
            return hr;
        }
    }

    // If blob was requested then pass it otherwise release it
    if ( ppShaderBlob )
    {
        *ppShaderBlob = pShaderBlob;
    }
    else
    {
        pShaderBlob->Release();
    }

    DXUT_SetDebugName( *ppShader, pFunctionName );

    // Return error code
    return hr;
}


//--------------------------------------------------------------------------------------
// Create a vertex shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a hull shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob, 
                                  BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}
//--------------------------------------------------------------------------------------
// Create a domain shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a geometry shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob, 
                                      BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a pixel shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob, 
                                   BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a compute shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob, 
                                     BOOL bDumpShader)
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Helper function that returns TRUE if the specified filename exists on disk
//--------------------------------------------------------------------------------------
BOOL FileExists( WCHAR* pszFilename )
{
    FILE* f = NULL;
    
    if( _wfopen_s( &f, pszFilename, L"rb" ) == 0 )
    {
        fclose( f );
        return TRUE;
    }
    
    return FALSE;
}
