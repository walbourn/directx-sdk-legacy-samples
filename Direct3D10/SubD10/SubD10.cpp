//--------------------------------------------------------------------------------------
// File: SubD10.cpp
//
// This sample shows an implementation of Charles Loop's and Scott Schaefer's Approximate
// Catmull-Clark subdvision paper (http://research.microsoft.com/~cloop/msrtr-2007-44.pdf)
//
// Special thanks to Charles Loop and Peter-Pike Sloan for implementation details.
// Special thanks to Bay Raitt for the monsterfrog and bigguy models.
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
#include "SubDMesh.h"

#define MAX_BUMP 3000                           // Maximum bump amount * 1000 (for UI slider)
#define MAX_DIVS 32                             // Maximum divisions of a patch per side (about 2048 triangles)
#define NUM_BONES 4                             // Number of bones to programmatically create per mesh

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                   // A model viewing camera
CDXUTDirectionWidget                g_LightControl;
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // manages the 3D   
CDXUTDialog                         g_SampleUI;                 // dialog for sample specific controls

// Resources
CDXUTTextHelper*                    g_pTxtHelper = NULL;
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10InputLayout*                  g_pPatchLayout = NULL;
ID3D10InputLayout*                  g_pRegularPatchLayout = NULL;
ID3D10InputLayout*                  g_pPrecalcPatchLayout = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10Effect*                       g_pEffectConvert10 = NULL;
ID3D10Buffer*                       g_pPatchVB[MAX_DIVS-1] = {0};
ID3D10Buffer*                       g_pPatchIB[MAX_DIVS-1] = {0};

// SubD10.fx techniques
ID3D10EffectTechnique*              g_pTechCurrent = NULL;
ID3D10EffectTechnique*              g_pTechRenderScene = NULL;
ID3D10EffectTechnique*              g_pTechRenderSceneTan = NULL;
ID3D10EffectTechnique*              g_pTechRenderSceneBump = NULL;
ID3D10EffectTechnique*              g_pTechRenderSceneBumpTan = NULL;

// SubD10.fx variables
ID3D10EffectShaderResourceVariable* g_ptxHeight = NULL;
ID3D10EffectShaderResourceVariable* g_pbufPatchesB = NULL;
ID3D10EffectShaderResourceVariable* g_pbufPatchesUV = NULL;
ID3D10EffectShaderResourceVariable* g_pbufControlPointsUV = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProjection = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectVectorVariable*         g_pvEyeDir = NULL;
ID3D10EffectScalarVariable*         g_pfHeightAmount = NULL;
ID3D10EffectScalarVariable*         g_pPatchIDOffset = NULL;
ID3D10EffectVectorVariable*         g_pvPatches = NULL;
ID3D10EffectScalarVariable*         g_pPatchSize = NULL;
ID3D10EffectScalarVariable*         g_pInvPatchSize = NULL;
ID3D10EffectScalarVariable*         g_pPatchStride = NULL;
ID3D10EffectScalarVariable*         g_pVerticesPerPatch = NULL;
ID3D10EffectVectorVariable*         g_pPatchOffsets = NULL;

// SubDtoBezier.fx techniques
ID3D10EffectTechnique*              g_pSubDToBezierB = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierUV = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierB_Reg = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierUV_Reg = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierB_Skin = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierUV_Skin = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierB_Reg_Skin = NULL;
ID3D10EffectTechnique*              g_pSubDToBezierUV_Reg_Skin = NULL;

// SubDtoBezier.fx variables
ID3D10EffectScalarVariable*         g_pTanM = NULL;
ID3D10EffectScalarVariable*         g_pfCi = NULL;
ID3D10EffectScalarVariable*         g_pfSkinTime = NULL;
ID3D10EffectMatrixVariable*         g_pmConstBoneWorld = NULL;
ID3D10EffectShaderResourceVariable* g_pbufPoints = NULL;
ID3D10EffectShaderResourceVariable* g_pbufBones = NULL;

// Control variables
int                                 g_iSubdivs = 8;                         // Startup subdivisions per side
bool                                g_bDrawWires = false;                   // Draw the mesh with wireframe overlay
bool                                g_bSeparatePasses = true;               // Do a separate pass for regular and extraordinary patches
bool                                g_bConvertEveryFrame = true;            // Convert from subd to bezier every frame
bool                                g_bAttenuateBasedOnDistance = true;     // Attenuate the tesselation level based upon distance to the camera
bool                                g_bRenderSkinned = false;               // Skin the meshes (GPU only)
float                               g_fHeightAmount = 2.0f;                 // The height amount for displacement mapping
const float                         g_fFarPlane = 500.0f;                   // Distance to the far plane

// Pre-tessellated, pre-computed parametric patch vertex
struct PATCH_VERTEX
{
    D3DXVECTOR2 m_UV;
    D3DXVECTOR4 m_BasisU;
    D3DXVECTOR4 m_BasisV;
    D3DXVECTOR4 m_dBasisU;
    D3DXVECTOR4 m_dBasisV;
};

// Mesh structures
struct SUBDMESH_DESC
{
    WCHAR   m_szFileName[MAX_PATH];
    WCHAR   m_szHeightMap[MAX_PATH];
    D3DXVECTOR3 m_vPosition;
    D3DXVECTOR3 m_vScale;
    D3DXVECTOR3 m_vSpineDir;
};

SUBDMESH_DESC g_MeshDesc[] =
{
    { L"SubD10\\monsterfrog_mapped_00.obj",  L"SubD10\\monsterfrog.dds", D3DXVECTOR3( 0, 0, 0 ),
        D3DXVECTOR3( 0.5f, 0.5f, 0.5f ), D3DXVECTOR3( 0, 0, 1 ) },
    { L"SubD10\\bigguy_00.obj",              L"",                        D3DXVECTOR3( -50, 0, 0 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 0, 1, 0 ) },
    { L"SubD10\\guy3.obj",                   L"",                        D3DXVECTOR3( 50, 0, 0 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 0, 1, 0 ) },
    { L"SubD10\\subdbox.obj",                L"",                        D3DXVECTOR3( 0, 0, 50 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 0, 0, 1 ) },
    { L"SubD10\\testshape.obj",              L"",                        D3DXVECTOR3( -50, 0, 50 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 0, 1, 0 ) },
    { L"SubD10\\cube.obj",                   L"",                        D3DXVECTOR3( 50, 0, 50 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 0, 1, 0 ) },
    { L"SubD10\\shipquads.obj",              L"",                        D3DXVECTOR3( -50, 0, -50 ),
        D3DXVECTOR3( 1, 1, 1 ),          D3DXVECTOR3( 1, 0, 0 ) },
    { L"SubD10\\tree.obj",                   L"",                        D3DXVECTOR3( 0, 0, -50 ),
        D3DXVECTOR3( 2, 2, 2 ),          D3DXVECTOR3( 0, 1, 0 ) },
    { L"SubD10\\head.obj",                   L"",                        D3DXVECTOR3( 50, 0, -50 ),
        D3DXVECTOR3( 2, 2, 2 ),          D3DXVECTOR3( 0, 1, 0 ) },
};

CSubDMesh g_SubDMesh[ ARRAYSIZE( g_MeshDesc ) ];
UINT                                g_iNumSubDMeshes = ARRAYSIZE( g_MeshDesc );


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN      1
#define IDC_TOGGLEREF             3
#define IDC_CHANGEDEVICE          4

#define IDC_PATCH_SUBDIVS         5
#define IDC_PATCH_SUBDIVS_STATIC  6
#define IDC_BUMP_HEIGHT	          7
#define IDC_BUMP_HEIGHT_STATIC    8
#define IDC_TOGGLE_LINES          9
#define IDC_TECH_STATIC           10
#define IDC_TECH                  11
#define IDC_SEPARATE_PASSES       12
#define IDC_CONVERT_PER_FRAME     13
#define IDC_DIST_ATTEN            14
#define IDC_SKIN                  15
#define IDC_TOGGLEWARP            16
//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();
UINT SetPatchConstants( ID3D10Device* pd3dDevice, UINT iSubdivs );
void UpdateSubDPatches( ID3D10Device* pd3dDevice, CSubDMesh* pMesh );
void ConvertFromSubDToBezier( ID3D10Device* pd3dDevice, CSubDMesh* pMesh );
void FillTables();
HRESULT CreatePatchVBsIBs( ID3D10Device* pd3dDevice );

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
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SubD10" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_LightControl.SetLightDirection( D3DXVECTOR3( 0, 0, -1 ) );

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
    swprintf_s( sz, 100, L"Patch Divisions: %d", g_iSubdivs );
    g_SampleUI.AddStatic( IDC_PATCH_SUBDIVS_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_PATCH_SUBDIVS, 50, iY += 24, 100, 22, 1, MAX_DIVS - 1, g_iSubdivs );

    swprintf_s( sz, 100, L"BumpHeight: %.4f", g_fHeightAmount );
    g_SampleUI.AddStatic( IDC_BUMP_HEIGHT_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_BUMP_HEIGHT, 50, iY += 24, 100, 22, 0, MAX_BUMP, ( int )( 1000.0f * g_fHeightAmount ) );

    iY += 24;
    g_SampleUI.AddStatic( IDC_TECH_STATIC, L"Technique", 35, iY += 24, 125, 22 );
    CDXUTComboBox* pComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_TECH, 0, iY += 24, 180, 22, 0, false, &pComboBox );
    pComboBox->SetDropHeight( 120 );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_LINES, L"Toggle Wires", 35, iY += 24, 125, 22, g_bDrawWires );
    g_SampleUI.AddCheckBox( IDC_SEPARATE_PASSES, L"Separate Reg/Extra", 35, iY += 24, 125, 22, g_bSeparatePasses );
    g_SampleUI.AddCheckBox( IDC_CONVERT_PER_FRAME, L"Convert Every Frame", 35, iY += 24, 125, 22,
                            g_bConvertEveryFrame );
    g_SampleUI.AddCheckBox( IDC_DIST_ATTEN, L"Distance Attenuate", 35, iY += 24, 125, 22,
                            g_bAttenuateBasedOnDistance );
    g_SampleUI.AddCheckBox( IDC_SKIN, L"Skin Meshes", 35, iY += 24, 125, 22, g_bRenderSkinned );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( -70.0f, 20.0f, 70.0f );
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
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    // Randomly animate the skinned meshes
    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        for( UINT b = 0; b < NUM_BONES; b++ )
        {
            D3DXVECTOR3 vRotate(0,0,0);

            vRotate.x = D3DXToRadian( 20.0f ) * sinf( ( float )fTime * 0.5f * ( i + 1 ) );
            vRotate.y = D3DXToRadian( 15.0f ) * sinf( ( float )fTime * 0.75f * ( i + 1 ) );

            vRotate.x *= ( b ) / 2.0f;
            vRotate.y *= ( float )( b % 2 );

            if( i % 2 == 1 )
            {
                float temp = vRotate.x;
                vRotate.x = vRotate.y;
                vRotate.y = temp;
            }

            g_SubDMesh[i].RotateBone( b, vRotate );
        }
        g_SubDMesh[i].CreateSkinningMatrices();
    }
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
            // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
            // Custom app controls
        case IDC_PATCH_SUBDIVS:
        {
            g_iSubdivs = g_SampleUI.GetSlider( IDC_PATCH_SUBDIVS )->GetValue();

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Patch Divisions: %d", g_iSubdivs );
            g_SampleUI.GetStatic( IDC_PATCH_SUBDIVS_STATIC )->SetText( sz );
        }
            break;
        case IDC_BUMP_HEIGHT:
        {
            g_fHeightAmount = ( float )g_SampleUI.GetSlider( IDC_BUMP_HEIGHT )->GetValue() / 1000.0f;

            WCHAR sz[100];
            swprintf_s( sz, 100, L"BumpHeight: %.4f", g_fHeightAmount );
            g_SampleUI.GetStatic( IDC_BUMP_HEIGHT_STATIC )->SetText( sz );

            g_pfHeightAmount->SetFloat( g_fHeightAmount );
        }
            break;
        case IDC_TOGGLE_LINES:
        {
            g_bDrawWires = g_SampleUI.GetCheckBox( IDC_TOGGLE_LINES )->GetChecked();
        }
            break;
        case IDC_TECH:
        {
            CDXUTComboBox* pComboBox = ( CDXUTComboBox* )pControl;
            g_pTechCurrent = ( ID3D10EffectTechnique* )pComboBox->GetSelectedData();
        }
            break;
        case IDC_SEPARATE_PASSES:
        {
            g_bSeparatePasses = !g_bSeparatePasses;
        }
            break;
        case IDC_CONVERT_PER_FRAME:
        {
            g_bConvertEveryFrame = !g_bConvertEveryFrame;
        }
            break;
        case IDC_DIST_ATTEN:
        {
            g_bAttenuateBasedOnDistance = !g_bAttenuateBasedOnDistance;
        }
            break;
        case IDC_SKIN:
        {
            g_bRenderSkinned = !g_bRenderSkinned;
        }
            break;

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

    // Setup our defines so that the FX file is in sync with the .cpp file
    char strPoints[MAX_PATH];
    sprintf_s( strPoints, ARRAYSIZE( strPoints ), "%d", MAX_POINTS );
    char strStride[MAX_PATH];
    sprintf_s( strStride, ARRAYSIZE( strPoints ), "%d", PATCH_STRIDE );

    D3D10_SHADER_MACRO macros[] =
    {
        { "MAX_POINTS", strPoints },
        { "PATCH_STRIDE", strStride },
        { NULL, NULL }
    };

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SubDtoBezier.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, macros, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffectConvert10, NULL, NULL ) );

    // Setup our defines so that the FX file is in sync with the .cpp file
    D3D10_SHADER_MACRO macros2[] =
    {
        { "PATCH_STRIDE", strStride },
        { NULL, NULL }
    };

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SubD10.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, macros2, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                          NULL, &g_pEffect10, NULL, NULL ) );


    // Obtain technique objects
    g_pTechRenderScene = g_pEffect10->GetTechniqueByName( "RenderScene" );
    g_pTechRenderSceneTan = g_pEffect10->GetTechniqueByName( "RenderSceneTan" );
    g_pTechRenderSceneBump = g_pEffect10->GetTechniqueByName( "RenderSceneBump" );
    g_pTechRenderSceneBumpTan = g_pEffect10->GetTechniqueByName( "RenderSceneBumpTan" );

    CDXUTComboBox* pComboBox = g_SampleUI.GetComboBox( IDC_TECH );
    pComboBox->AddItem( L"RenderPatch", ( void* )g_pTechRenderScene );
    pComboBox->AddItem( L"RenderPatchTan", ( void* )g_pTechRenderSceneTan );
    pComboBox->AddItem( L"RenderBump", ( void* )g_pTechRenderSceneBump );
    pComboBox->AddItem( L"RenderBumpTan", ( void* )g_pTechRenderSceneBumpTan );

    g_pTechCurrent = g_pTechRenderSceneBumpTan;
    pComboBox->SetSelectedByIndex( 3 );

    // Obtain variables for sub10.fx
    g_ptxHeight = g_pEffect10->GetVariableByName( "g_txHeight" )->AsShaderResource();
    g_pbufPatchesB = g_pEffect10->GetVariableByName( "g_bufPatchesB" )->AsShaderResource();
    g_pbufPatchesUV = g_pEffect10->GetVariableByName( "g_bufPatchesUV" )->AsShaderResource();
    g_pbufControlPointsUV = g_pEffect10->GetVariableByName( "g_bufControlPointsUV" )->AsShaderResource();
    g_pmWorldViewProjection = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pvEyeDir = g_pEffect10->GetVariableByName( "g_vEyeDir" )->AsVector();
    g_pfHeightAmount = g_pEffect10->GetVariableByName( "g_fHeightAmount" )->AsScalar();
    g_pPatchIDOffset = g_pEffect10->GetVariableByName( "g_PatchIDOffset" )->AsScalar();

    g_pPatchSize = g_pEffect10->GetVariableByName( "g_PatchSize" )->AsScalar();
    g_pInvPatchSize = g_pEffect10->GetVariableByName( "g_InvPatchSize" )->AsScalar();
    g_pPatchStride = g_pEffect10->GetVariableByName( "g_PatchStride" )->AsScalar();
    g_pVerticesPerPatch = g_pEffect10->GetVariableByName( "g_VerticesPerPatch" )->AsScalar();
    g_pPatchOffsets = g_pEffect10->GetVariableByName( "g_PatchOffsets" )->AsVector();
    g_pvPatches = g_pEffect10->GetVariableByName( "g_Patches" )->AsVector();

    // Obtain variables for subdtobezier.fx
    g_pSubDToBezierB = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierB_Extra" );
    g_pSubDToBezierUV = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierUV_Extra" );
    g_pSubDToBezierB_Reg = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierB_Reg" );
    g_pSubDToBezierUV_Reg = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierUV_Reg" );

    g_pSubDToBezierB_Skin = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierB_Extra_Skin" );
    g_pSubDToBezierUV_Skin = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierUV_Extra_Skin" );
    g_pSubDToBezierB_Reg_Skin = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierB_Reg_Skin" );
    g_pSubDToBezierUV_Reg_Skin = g_pEffectConvert10->GetTechniqueByName( "SubDToBezierUV_Reg_Skin" );

    g_pbufPoints = g_pEffectConvert10->GetVariableByName( "g_bufPoints" )->AsShaderResource();
    g_pbufBones = g_pEffectConvert10->GetVariableByName( "g_bufBones" )->AsShaderResource();

    g_pTanM = g_pEffectConvert10->GetVariableByName( "g_TanM" )->AsScalar();
    g_pfCi = g_pEffectConvert10->GetVariableByName( "g_fCi" )->AsScalar();
    g_pfSkinTime = g_pEffectConvert10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pmConstBoneWorld = g_pEffectConvert10->GetVariableByName( "g_mConstBoneWorld" )->AsMatrix();

    // Using pre-tesselated patches.  Create them here.
    V_RETURN( CreatePatchVBsIBs( pd3dDevice ) );

    // Create out precalculated patch layout
    const D3D10_INPUT_ELEMENT_DESC precalcpatchlayout[] =
    {
        { "PATCHUV",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BASISU",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BASISV",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "DBASISU",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "DBASISV",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pTechRenderScene->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( precalcpatchlayout, ARRAYSIZE( precalcpatchlayout ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pPrecalcPatchLayout ) );

    // Create our vertex input layouts
    const D3D10_INPUT_ELEMENT_DESC patchlayout[] =
    {
        // Control Points
        { "CONTROLPOINTS",  0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 0,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  1, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  2, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  3, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  4, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
#if (MAX_POINTS > 20 )
        { "CONTROLPOINTS",  5, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
#if (MAX_POINTS > 24 )
        { "CONTROLPOINTS",  6, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
#if (MAX_POINTS > 28 )
        { "CONTROLPOINTS",  7, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
#endif
#endif
#endif
        // Valences and Prefixes
        { "VALENCES",       0, DXGI_FORMAT_R8G8B8A8_UINT,     0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "PREFIXES",       0, DXGI_FORMAT_R8G8B8A8_UINT,     0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pSubDToBezierB->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( patchlayout, ARRAYSIZE( patchlayout ), PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pPatchLayout ) );

    const D3D10_INPUT_ELEMENT_DESC regularpatchlayout[] =
    {
        // Control Points
        { "CONTROLPOINTS",  0,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  1,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  2,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  3,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  4,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  5,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  6,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  7,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  8,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  9,  DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  10, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  11, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  12, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  13, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  14, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "CONTROLPOINTS",  15, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT,
            D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };
    g_pSubDToBezierB_Reg->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( regularpatchlayout, ARRAYSIZE( regularpatchlayout ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pRegularPatchLayout ) );

    // Fill our helper/temporary tables
    FillTables();

    // Load meshes
    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        // Load the subd mesh from an obj file
        V_RETURN( g_SubDMesh[i].LoadSubDFromObj( g_MeshDesc[i].m_szFileName ) );

        // Much of what follows (conditioning, creating tangents, etc) should be handled offline in the content
        // pipeline.  It's handled here to facilitate easily loading obj files into the sample.

        // Condition the mesh by creating a 1-ring neighborhood around each patch
        if( !g_SubDMesh[i].ConditionMesh() )
            return E_FAIL;

        // Compute a UV tangent frame for the mesh
        g_SubDMesh[i].ComputeTangents();

        // Programmatically add bones and rig the mesh
        g_SubDMesh[i].AddBones( NUM_BONES, g_MeshDesc[i].m_vSpineDir, 1.0f );

        // Sort the patches by size so that we can separate regular and extraordinary patches
        g_SubDMesh[i].SortPatchesBySize();

        // Load any height textures for displacement
        if( g_MeshDesc[i].m_szHeightMap[0] != L'' )
        V_RETURN( g_SubDMesh[i].LoadHeightTexture( pd3dDevice, g_MeshDesc[i].m_szHeightMap ) );

        // Create GPU resources
        V_RETURN( g_SubDMesh[i].CreatePatchesBuffer( pd3dDevice ) );
        V_RETURN( g_SubDMesh[i].CreateBuffersFromSubDMesh( pd3dDevice ) );
    }

    g_pfHeightAmount->SetFloat( g_fHeightAmount );

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
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, g_fFarPlane );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 190, pBackBufferSurfaceDesc->Height - 650 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Use the gpu to convert from subds to cubic bezier patches using stream out
//--------------------------------------------------------------------------------------
void ConvertFromSubDToBezier( ID3D10Device* pd3dDevice, CSubDMesh* pMesh )
{
    // Set shader resources
    g_pmConstBoneWorld->SetMatrixArray( ( float* )pMesh->GetTransformMatrices(), 0, pMesh->GetNumTransformMatrices() );
    g_pbufPoints->SetResource( pMesh->GetControlPointSRV() );
    g_pbufBones->SetResource( pMesh->GetControlPointBonesSRV() );

    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );

    D3D10_TECHNIQUE_DESC Desc;

    // Do regular and extraordinary patches in separate passes if g_bSeparatePasses is true.
    // Rendering them in separate passes is actually faster than doing it all at once.  
    // The reasoning is that regular patches make up the majority of the meshes patches.
    // Because of the fixed valences of regular patches, the regular patch path can be 
    // optimized well beyond the extraordinary patch path.  When g_bSeparatePasses is true
    // the regular patches go through the fast path and only the extraordinary patches
    // go through the full (and slow) path.
    int NumExtraPatches = pMesh->GetNumExtraordinaryPatches();
    int NumPasses = 2;
    if( !g_bSeparatePasses )
    {
        NumExtraPatches = pMesh->GetNumPatches();
        NumPasses = 1;
    }

    // Why use arrays of [1] here?  IASetVertexBuffers expects array input.  We could just use define UINT Stride, and use that,
    // but it's good to get into the habit of binding arrays.  It also saves API calls when binding multiple resources.
    UINT stride[1];
    ID3D10Buffer* pBuffers[1];
    UINT zeroOffset[1] = {0};

    // Do up to 2 passes.  The first pass is the extraordinary patches.
    for( int iPassIndex = 0; iPassIndex < NumPasses; iPassIndex++ )
    {
        UINT offset[1];
        offset[0] = iPassIndex * NumExtraPatches * PATCH_STRIDE * sizeof( D3DXVECTOR4 );

        ID3D10EffectTechnique* pTechB = g_pSubDToBezierB;
        ID3D10EffectTechnique* pTechUV = g_pSubDToBezierUV;

        if( g_bRenderSkinned )
        {
            pTechB = g_pSubDToBezierB_Skin;
            pTechUV = g_pSubDToBezierUV_Skin;
        }

        ID3D10InputLayout* pLayout = g_pPatchLayout;
        int NumToDraw = NumExtraPatches;
        stride[0] = sizeof( SUBDPATCH );
        ID3D10Buffer* pBuffers[1];
        pBuffers[0] = pMesh->GetSubDPatchVB();

        // If this is the second pass, then we're doing the regular patches.
        // Setup the techniques appropriately.
        if( iPassIndex == 1 )
        {
            pTechB = g_pSubDToBezierB_Reg;
            pTechUV = g_pSubDToBezierUV_Reg;

            if( g_bRenderSkinned )
            {
                pTechB = g_pSubDToBezierB_Reg_Skin;
                pTechUV = g_pSubDToBezierUV_Reg_Skin;
            }

            NumToDraw = pMesh->GetNumRegularPatches();
            if( NumToDraw == 0 )
                continue;

            pLayout = g_pRegularPatchLayout;
            stride[0] = sizeof( SUBDPATCHREGULAR );
            pBuffers[0] = pMesh->GetSubDPatchRegVB();
        }

        // Set IA data
        pd3dDevice->IASetInputLayout( pLayout );

        // Set patch vb
        pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, zeroOffset );

        // Stream to BezierB buffer
        pBuffers[0] = pMesh->GetPatchesBufferB();
        pd3dDevice->SOSetTargets( 1, pBuffers, offset );
        pTechB->GetDesc( &Desc );
        for( UINT p = 0; p < Desc.Passes; ++p )
        {
            pTechB->GetPassByIndex( p )->Apply( 0 );
            pd3dDevice->Draw( NumToDraw, 0 );
        }

        // Don't do UV patches for regular patches becuase we don't need the fixup for the normals.
        // We could do UV patches for the regular patches, but it would just waste time.
        if( iPassIndex != 1 )
        {
            if( g_pTechCurrent == g_pTechRenderSceneTan ||
                g_pTechCurrent == g_pTechRenderSceneBumpTan )
            {
                // Stream to BezierUV buffer
                pBuffers[0] = pMesh->GetPatchesBufferUV();
                pd3dDevice->SOSetTargets( 1, pBuffers, offset );
                pTechUV->GetDesc( &Desc );
                for( UINT p = 0; p < Desc.Passes; ++p )
                {
                    pTechUV->GetPassByIndex( p )->Apply( 0 );
                    pd3dDevice->Draw( NumToDraw, 0 );
                }
            }
        }
    }

    // Get back to normal
    pBuffers[0] = NULL;
    pd3dDevice->SOSetTargets( 1, pBuffers, zeroOffset );

}

//--------------------------------------------------------------------------------------
// Helper to get the number of vertices to draw with respect to the subdivison
// amount requested.
//--------------------------------------------------------------------------------------
UINT GetNumVerticesForSubdivisionLevel( UINT iSubDivs )
{
    return ( ( ( iSubDivs + 1 ) * 2 + 2 ) * iSubDivs ) - 2;
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

    // Do the conversion from subd to bicubic patches
    if( g_bConvertEveryFrame )
    {
        for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
        {
            ConvertFromSubDToBezier( pd3dDevice, &g_SubDMesh[i] );
        }
    }

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    D3DXMATRIX mWorldViewProjection;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    D3DXVECTOR3 vEyePt = *g_Camera.GetEyePt();
    D3DXVECTOR3 vDir = *g_Camera.GetLookAtPt() - vEyePt;
    D3DXVECTOR4 vEyeVector( vDir,1 );

    V( g_pfSkinTime->SetFloat( ( float )fTime ) );
    V( g_pfTime->SetFloat( ( float )fTime ) );
    V( g_pvEyeDir->SetFloatVector( ( float* )&vEyeVector ) );

    // Store the old rasterizer state
    ID3D10RasterizerState* pRSState = NULL;
    pd3dDevice->RSGetState( &pRSState );

    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        // Get the projection & view matrix from the camera class
        mWorld = *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        CSubDMesh* pMesh = &g_SubDMesh[i];

        D3DXMATRIX mTrans;
        D3DXMatrixTranslation( &mTrans, g_MeshDesc[i].m_vPosition.x, g_MeshDesc[i].m_vPosition.y,
                               g_MeshDesc[i].m_vPosition.z );
        D3DXMATRIX mScale;
        D3DXMatrixScaling( &mScale, g_MeshDesc[i].m_vScale.x, g_MeshDesc[i].m_vScale.y, g_MeshDesc[i].m_vScale.z );
        mWorld = mScale * mWorld * mTrans;
        mWorldViewProjection = mWorld * mView * mProj;

        // Set effect variables
        V( g_pmWorldViewProjection->SetMatrix( ( float* )&mWorldViewProjection ) );
        V( g_pmWorld->SetMatrix( ( float* )&mWorld ) );

        // Set all of the buffers needed such as the patches buffers
        V( g_ptxHeight->SetResource( pMesh->GetHeightSRV() ) );
        V( g_pbufPatchesB->SetResource( pMesh->GetPatchesBufferBSRV() ) );
        V( g_pbufPatchesUV->SetResource( pMesh->GetPatchesBufferUVSRV() ) );
        V( g_pbufControlPointsUV->SetResource( pMesh->GetControlPointUVSRV() ) );

        D3DXVECTOR3 vToMesh = vEyePt - g_MeshDesc[i].m_vPosition;
        float DistToCamera = D3DXVec3Length( &vToMesh );
        int iSubDivs = g_iSubdivs;

        // We're attenuating (changing the subdivision level) on a per-object basis if this is enabled.
        // Objects farther away don't need to be subdivided as much as objects closer to the camera.
        if( g_bAttenuateBasedOnDistance )
        {
            const int minSubD = 1;
            const float fNear = 10.0f;

            float fDistAtten = ( DistToCamera - fNear ) / ( g_fFarPlane / 4.0f );

            float fSubDivs = g_iSubdivs + fDistAtten * ( minSubD - g_iSubdivs );

            iSubDivs = ( UINT )ceil( fSubDivs );
            iSubDivs = min( g_iSubdivs, iSubDivs );
            iSubDivs = max( minSubD, iSubDivs );
        }

        // Figure out how many vertices are in the patch
        UINT VerticesToDraw = GetNumVerticesForSubdivisionLevel( iSubDivs );

        // Render the scene with this technique as defined in the .fx file
        ID3D10EffectTechnique* pRenderTechnique = g_pTechCurrent;

        if( pRenderTechnique == g_pTechRenderSceneBump && g_MeshDesc[i].m_szHeightMap[0] == L'' )
        pRenderTechnique = g_pTechRenderScene;
        else if( pRenderTechnique == g_pTechRenderSceneBumpTan && g_MeshDesc[i].m_szHeightMap[0] == L'' )
        pRenderTechnique = g_pTechRenderSceneTan;

        // Render regular and extraordinary patches separately since they will have different shaders
        UINT Strides[1];
        UINT Offsets[1];
        ID3D10Buffer* pVB[1];
        ID3D10Buffer* pIB;

        // Setup the input assembler
        pd3dDevice->IASetInputLayout( g_pPrecalcPatchLayout );
        pVB[0] = g_pPatchVB[iSubDivs - 1];
        pIB = g_pPatchIB[iSubDivs - 1];
        Strides[0] = sizeof( PATCH_VERTEX );
        Offsets[0] = 0;
        pd3dDevice->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
        pd3dDevice->IASetIndexBuffer( pIB, DXGI_FORMAT_R16_UINT, 0 );
        pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

        // One pass for extraordinary, one for regular
        ID3D10EffectTechnique* pPatchPassTech = pRenderTechnique;
        UINT NumPatchPasses = 2;

        // Unless, of course, we've told it not to do the tangent fixup
        if( pPatchPassTech == g_pTechRenderScene ||
            pPatchPassTech == g_pTechRenderSceneBump )
        {
            NumPatchPasses = 1;
        }

        for( UINT patchpass = 0; patchpass < NumPatchPasses; patchpass++ )
        {
            UINT NumPatches;
            UINT StartInstance;

            if( NumPatchPasses == 1 ) // Just regular patches for everything
            {
                NumPatches = pMesh->GetNumPatches();
                StartInstance = 0;
                pPatchPassTech = pRenderTechnique;
            }
            else if( patchpass == 0 ) // Extraordinary patches
            {
                NumPatches = pMesh->GetNumExtraordinaryPatches();
                StartInstance = 0;
                pPatchPassTech = pRenderTechnique;
            }
            else // regular patches
            {
                NumPatches = pMesh->GetNumRegularPatches();
                StartInstance = pMesh->GetNumExtraordinaryPatches();

                // Regular patches don't require tangent information.  For these,
                // fallback to the Regular patch version of the technique
                if( pRenderTechnique == g_pTechRenderSceneTan )
                    pPatchPassTech = g_pTechRenderScene;
                else if( pRenderTechnique == g_pTechRenderSceneBumpTan )
                    pPatchPassTech = g_pTechRenderSceneBump;
            }

            g_pPatchIDOffset->SetInt( StartInstance );

            //Render
            D3D10_TECHNIQUE_DESC techDesc;
            pPatchPassTech->GetDesc( &techDesc );

            // Add another pass if we're adding a wireframe overlay
            UINT maxpasses = g_bDrawWires ? 2 : 1;
            for( UINT p = 0; p < maxpasses; p++ )
            {
                pPatchPassTech->GetPassByIndex( p )->Apply( 0 );
                pd3dDevice->DrawIndexedInstanced( VerticesToDraw, NumPatches, 0, 0, 0 );

            }
        }
    }

    // Restore the old rasterizer state
    pd3dDevice->RSSetState( pRSState );
    if( pRSState )
        pRSState->Release();

    // Clear out the PS, VS, and GS views so that we don't have any D3DX debug messages about
    // surfaces being bound to both output and input.
    ID3D10ShaderResourceView* pViews[8] = {0,0,0,0,0,0,0,0};
    pd3dDevice->PSSetShaderResources( 0, 8, pViews );
    pd3dDevice->VSSetShaderResources( 0, 8, pViews );
    pd3dDevice->GSSetShaderResources( 0, 8, pViews );

    // Render the HUD
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
    SAFE_RELEASE( g_pEffectConvert10 );
    SAFE_RELEASE( g_pPatchLayout );
    SAFE_RELEASE( g_pRegularPatchLayout );
    SAFE_RELEASE( g_pPrecalcPatchLayout );

    for( UINT i = 0; i < MAX_DIVS - 1; i++ )
    {
        SAFE_RELEASE( g_pPatchVB[i] );
        SAFE_RELEASE( g_pPatchIB[i] );
    }

    for( UINT i = 0; i < g_iNumSubDMeshes; i++ )
    {
        g_SubDMesh[i].Destroy();
    }
}

//--------------------------------------------------------------------------------------
// Fill the TanM and Ci precalculated tables.  This function precalculates part of the
// stencils (weights) used when calculating UV patches.  We precalculate a lot of the
// values here and just pass them in as shader constants.
//--------------------------------------------------------------------------------------
float g_TanM[MAX_VALENCE][64];
float g_fCi[MAX_VALENCE];
void FillTables()
{
    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        int a = 0;
        float CosfPIV = cosf( D3DX_PI / v );
        float VSqrtTerm = ( v * sqrtf( 4.0f + CosfPIV * CosfPIV ) );

        for( UINT i = 0; i < 32; i++ )
        {
            g_TanM[v][i * 2 + a] = ( ( 1.0f / v ) + CosfPIV / VSqrtTerm ) * cosf( ( 2 * D3DX_PI * i ) / ( float )v );
        }
        a = 1;
        for( UINT i = 0; i < 32; i++ )
        {
            g_TanM[v][i * 2 + a] = ( 1.0f / VSqrtTerm ) * cosf( ( 2 * D3DX_PI * i + D3DX_PI ) / ( float )v );
        }
    }

    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        g_fCi[v] = cosf( ( 2.0f * D3DX_PI ) / ( v + 3.0f ) );
    }

    g_pTanM->SetFloatArray( ( float* )g_TanM, 0, MAX_VALENCE * 64 );
    g_pfCi->SetFloatArray( g_fCi, 0, MAX_VALENCE );
}

//--------------------------------------------------------------------------------------
// Bernstein basis functions
//--------------------------------------------------------------------------------------
float Basis0( float t )
{
    float invT = 1.0f - t;
    return invT * invT * invT;
}
float Basis1( float t )
{
    float invT = 1.0f - t;
    return 3.0f * t * invT * invT;
}
float Basis2( float t )
{
    float invT = 1.0f - t;
    return 3.0f * t * t * invT;
}
float Basis3( float t )
{
    return t * t * t;
}

//--------------------------------------------------------------------------------------
// Derivative Bernstein basis functions
//--------------------------------------------------------------------------------------
float dBasis0( float t )
{
    float invT = 1.0f - t;
    return -3 * invT * invT;
}
float dBasis1( float t )
{
    float invT = 1.0f - t;
    return 3 * invT * invT - 6 * t * invT;
}
float dBasis2( float t )
{
    float invT = 1.0f - t;
    return 6 * t * invT - 3 * t * t;
}
float dBasis3( float t )
{
    return 3 * t * t;
}

//--------------------------------------------------------------------------------------
// Create an index buffer for a patch of a certain size
//--------------------------------------------------------------------------------------
HRESULT FillIBForPatch( ID3D10Device* pd3dDevice, UINT NumDivisionsPerSide, ID3D10Buffer** ppBuffer )
{
    HRESULT hr = S_OK;

    UINT NumIndices = GetNumVerticesForSubdivisionLevel( NumDivisionsPerSide );
    SHORT* pIndices = new SHORT[NumIndices];
    if( !pIndices )
        return E_OUTOFMEMORY;

    SHORT vIndex = 0;
    UINT iIndex = 0;
    for( UINT v = 0; v < NumDivisionsPerSide; v++ )
    {
        vIndex = ( SHORT )( ( NumDivisionsPerSide + 1 ) * ( NumDivisionsPerSide - v ) );
        for( UINT u = 0; u < NumDivisionsPerSide + 1; u++ )
        {
            pIndices[iIndex] = vIndex;
            iIndex++;
            pIndices[iIndex] = vIndex - ( SHORT )( NumDivisionsPerSide + 1 );
            iIndex++;
            vIndex++;
        }
        if( v != NumDivisionsPerSide - 1 )
        {
            // add a degenerate tri
            pIndices[iIndex] = pIndices[iIndex - 1];
            iIndex++;
            pIndices[iIndex] = ( SHORT )( ( NumDivisionsPerSide + 1 ) * ( NumDivisionsPerSide - ( v + 1 ) ) );
            iIndex++;
        }
    }

    // Create the d3d10 buffer
    D3D10_BUFFER_DESC Desc;
    Desc.ByteWidth = NumIndices * sizeof( SHORT );
    Desc.Usage = D3D10_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pIndices;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &Desc, &InitData, ppBuffer ) );

    SAFE_DELETE_ARRAY( pIndices );
    return hr;
}

//--------------------------------------------------------------------------------------
// Create an vertex buffer for a patch of a certain size.
// This patch will contain precalculated UVs and bernstein polynomial coefficients.
//--------------------------------------------------------------------------------------
HRESULT FillVBForPatch( ID3D10Device* pd3dDevice, UINT NumDivisionsPerSide, ID3D10Buffer** ppBuffer )
{
    HRESULT hr = S_OK;

    UINT NumVertices = ( NumDivisionsPerSide + 1 ) * ( NumDivisionsPerSide + 1 );
    PATCH_VERTEX* pVertices = new PATCH_VERTEX[ NumVertices ];
    if( !pVertices )
        return E_OUTOFMEMORY;

    UINT iVertex = 0;
    float vStart = 0.0f;
    float uDelta = 1.0f / ( float )NumDivisionsPerSide;
    float vDelta = 1.0f / ( float )NumDivisionsPerSide;

    // Loop through terrain vertices and get height from the heightmap
    for( UINT v = 0; v < NumDivisionsPerSide + 1; v++ )
    {
        float uStart = 0.0f;
        for( UINT u = 0; u < NumDivisionsPerSide + 1; u++ )
        {
            D3DXVECTOR2 uv( uStart, vStart );

            pVertices[iVertex].m_UV = uv;

            // Regular basis functions
            pVertices[iVertex].m_BasisU.x = Basis0( uStart );
            pVertices[iVertex].m_BasisU.y = Basis1( uStart );
            pVertices[iVertex].m_BasisU.z = Basis2( uStart );
            pVertices[iVertex].m_BasisU.w = Basis3( uStart );

            pVertices[iVertex].m_BasisV.x = Basis0( vStart );
            pVertices[iVertex].m_BasisV.y = Basis1( vStart );
            pVertices[iVertex].m_BasisV.z = Basis2( vStart );
            pVertices[iVertex].m_BasisV.w = Basis3( vStart );

            // Derivative basis functions
            pVertices[iVertex].m_dBasisU.x = dBasis0( uStart );
            pVertices[iVertex].m_dBasisU.y = dBasis1( uStart );
            pVertices[iVertex].m_dBasisU.z = dBasis2( uStart );
            pVertices[iVertex].m_dBasisU.w = dBasis3( uStart );

            pVertices[iVertex].m_dBasisV.x = dBasis0( vStart );
            pVertices[iVertex].m_dBasisV.y = dBasis1( vStart );
            pVertices[iVertex].m_dBasisV.z = dBasis2( vStart );
            pVertices[iVertex].m_dBasisV.w = dBasis3( vStart );

            iVertex ++;
            uStart += uDelta;
        }
        vStart += vDelta;
    }

    // Create the d3d10 buffer
    D3D10_BUFFER_DESC Desc;
    Desc.ByteWidth = NumVertices * sizeof( PATCH_VERTEX );
    Desc.Usage = D3D10_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertices;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    V_RETURN( pd3dDevice->CreateBuffer( &Desc, &InitData, ppBuffer ) );

    SAFE_DELETE_ARRAY( pVertices );

    return hr;
}

//--------------------------------------------------------------------------------------
// Create precomputed UV patches for every level of subdivision we're likely to need
//--------------------------------------------------------------------------------------
HRESULT CreatePatchVBsIBs( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;
    for( UINT i = 1; i < MAX_DIVS; i++ )
    {
        V_RETURN( FillIBForPatch( pd3dDevice, i, &g_pPatchIB[i - 1] ) );
        V_RETURN( FillVBForPatch( pd3dDevice, i, &g_pPatchVB[i - 1] ) );
    }
    return hr;
}
