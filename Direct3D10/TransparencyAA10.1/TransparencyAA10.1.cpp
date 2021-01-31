//--------------------------------------------------------------------------------------
// File: TransparencyAA10.1.cpp
//
// This code sample demonstrates the use of DX10.1 fixed MSAA sample patterns, to perform
// sub sample accurate alpha testing - also known as Transparency AA. 
//
// Contributed by AMD Corporation
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

#include "Sprite.h"
#include "Magnify.h"
#include "MagnifyTool.h"

//#define DEBUG_VS   // Uncomment this line to debug D3D9 vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug D3D9 pixel shaders 

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct QuadVertex
{
    D3DXVECTOR3 v3Pos;
    D3DXVECTOR3 v3Normal;
    D3DXVECTOR2 v2TexCoord;
};

typedef enum _TECHNIQUE_TYPE
{
    TECHNIQUE_TYPE_ALPHA_TEST           = 0,
    TECHNIQUE_TYPE_ALPHA_TO_COVERAGE    = 1,
    TECHNIQUE_TYPE_TRANSPARENCY_AA      = 2,
    TECHNIQUE_TYPE_MAX                  = 3,
}TECHNIQUE_TYPE;

typedef enum _ALPHA_TEXTURE_TYPE
{
    ALPHA_TEXTURE_TYPE_LETTERING    = 0,
    ALPHA_TEXTURE_TYPE_FOLIAGE      = 1,
    ALPHA_TEXTURE_TYPE_MAX          = 2,
}ALPHA_TEXTURE_TYPE;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // A model viewing camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

// Direct3D 10 resources
ID3DX10Font*                g_pFont10 = NULL;       
ID3DX10Sprite*              g_pSprite10 = NULL;     
ID3D10Effect*               g_pEffect10 = NULL;     
ID3D10EffectMatrixVariable* g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable* g_pmWorld = NULL;
ID3D10EffectScalarVariable* g_pfTime = NULL;
ID3D10EffectVectorVariable* g_pMaterialAmbientColor = NULL;
ID3D10EffectVectorVariable* g_pMaterialDiffuseColor = NULL;
ID3D10EffectVectorVariable* g_pLightDir = NULL;
ID3D10EffectVectorVariable* g_pLightDiffuse = NULL;
ID3D10EffectScalarVariable* g_pAlphaRef = NULL;

// Magnify class
static Magnify        g_Magnify;
static MagnifyTool    g_MagnifyTool;

// TransparencyAA Data
static ID3D10EffectTechnique*               g_pAlphaTestTechnique = NULL;
static ID3D10EffectTechnique*               g_pAlphaToCoverageTechnique = NULL;
static ID3D10EffectTechnique*               g_pTransparencyAATechnique = NULL;
static QuadVertex                           g_QuadVertices[6];
static ID3D10InputLayout*                   g_pVertexLayout = NULL;
static ID3D10Buffer*                        g_pVertexBuffer = NULL;
static ID3D10ShaderResourceView*            g_pLetteringAlphaTextureView = NULL;
static ID3D10ShaderResourceView*            g_pFoliageAlphaTextureView = NULL;
static ID3D10EffectShaderResourceVariable*  g_pTextureVariable = NULL;
static TECHNIQUE_TYPE                       g_TechniqueType = TECHNIQUE_TYPE_ALPHA_TEST;
static unsigned int                         g_nMSAASamples = 1;
static ALPHA_TEXTURE_TYPE                   g_AlphaTextureType = ALPHA_TEXTURE_TYPE_FOLIAGE;
static float                                g_fAlphaRef = 0.5f;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3

#define IDC_STATIC_TECHNIQUE    10
#define IDC_COMBOBOX_TECHNIQUE  11
#define IDC_STATIC_TEXTURE      12
#define IDC_COMBOBOX_TEXTURE    13
#define IDC_STATIC_ALPHA_REF    14
#define IDC_SLIDER_ALPHA_REF    15

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void    CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void    CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void    CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool    CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool    CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void    CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void    CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void    InitApp();
void    RenderText();


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
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"Transparency AA - Direct3D 10.1" );
    DXUTCreateDevice( true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
    
    D3DCOLOR DlgColor = 0x88888888;

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10; 
    g_HUD.SetBackgroundColors( DlgColor );
    g_HUD.EnableCaption( true );
    g_HUD.SetCaptionText( L"-- Direct3D --" );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 10, iY, 150, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 10, iY += 24, 150, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 10, iY += 24, 150, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
    g_SampleUI.SetBackgroundColors( DlgColor );
    g_SampleUI.EnableCaption( true );
    g_SampleUI.SetCaptionText( L"-- Rendering Settings --" );
    
    g_SampleUI.AddStatic( IDC_STATIC_TECHNIQUE, L"Technique:", 5, iY, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_TECHNIQUE, 5, iY += 25, 160, 24, 0, false, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 30 );
        pCombo->AddItem( L"Alpha Test", (LPVOID)0x11111111 );
        pCombo->AddItem( L"Alpha To Coverage", (LPVOID)0x12121212 );
    }
    g_SampleUI.SetControlEnabled( IDC_COMBOBOX_TECHNIQUE, true );
    
    g_SampleUI.AddStatic( IDC_STATIC_TEXTURE, L"Alpha Texture:", 5, iY += 25, 75, 24 );
    g_SampleUI.AddComboBox( IDC_COMBOBOX_TEXTURE, 5, iY += 25, 160, 24, 0, false, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 35 );
        pCombo->AddItem( L"Lettering", (LPVOID)0x11111111 );
        pCombo->AddItem( L"Foliage", (LPVOID)0x12121212 );
        pCombo->SetSelectedByIndex( 1 );
    }
    g_SampleUI.SetControlEnabled( IDC_COMBOBOX_TEXTURE, true );

    g_SampleUI.AddStatic( IDC_STATIC_ALPHA_REF, L"Alpha Ref : 0.50f", 5, iY += 25, 85, 24 );
    g_SampleUI.AddSlider( IDC_SLIDER_ALPHA_REF, 15, iY += 25, 140, 24, 0, 100, 50, false );

    // Init the magnify tool UI
    g_MagnifyTool.InitApp( &g_DialogResourceManager );
    g_MagnifyTool.GetMagnifyUI()->SetCallback( OnGUIEvent );

    // This sample doesn't need all functionality from the MagnifyTool
    g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetVisible( false );        
    g_MagnifyTool.GetMagnifyUI()->GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetVisible( false );    
    g_MagnifyTool.GetMagnifyUI()->GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetVisible( false );
    g_MagnifyTool.GetMagnifyUI()->GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetVisible( false );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );

    if( NULL != DXUTGetD3D10Device1() ) 
    {
        g_pTxtHelper->DrawTextLine( L"Direct3D 10.1 Device Detected" );  
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Please Run On Direct3D 10.1 Hardware To Access All Features Of This Sample!!" );  
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    }

    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );  
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
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
    LPCSTR pszTarget;
    D3D10_SHADER_MACRO* pShaderMacros = NULL;
    D3D10_SHADER_MACRO ShaderMacros[3];

    V_RETURN( D3DX10CreateSprite( pd3dDevice, 500, &g_pSprite10 ) );
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                L"Arial", &g_pFont10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Check to see if we have a DX10.1 device
    if( NULL == DXUTGetD3D10Device1() ) 
    {
        pszTarget = "fx_4_0";
        pShaderMacros = NULL;
    }
    else
    {
        pszTarget = "fx_4_1";
        ShaderMacros[0].Name = "DX10_1_ENABLED";
        ShaderMacros[0].Definition = "1";
        ShaderMacros[1].Name = "MSAA_SAMPLES";
        switch( g_nMSAASamples )
        {
            case 1:
                ShaderMacros[1].Definition = "1";
                break;
            case 2:
                ShaderMacros[1].Definition = "2";
                break;
            case 4:
                ShaderMacros[1].Definition = "4";
                break;
            case 8:
                ShaderMacros[1].Definition = "8";
                break;
        }
        ShaderMacros[2].Name = NULL;
        pShaderMacros = ShaderMacros;

        CDXUTComboBox *pCombo = g_SampleUI.GetComboBox( IDC_COMBOBOX_TECHNIQUE );
        if( pCombo )
        {
            if( !pCombo->ContainsItem( L"Transparency AA" ) )
            {
                pCombo->AddItem( L"Transparency AA", (LPVOID)0x13131313 );
            }
        }
    }
    
    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"TransparencyAA10.1.fx" ) );
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    V_RETURN( D3DX10CreateEffectFromFile( str, pShaderMacros, NULL, pszTarget, dwShaderFlags, 0, pd3dDevice, NULL, NULL, &g_pEffect10, NULL, NULL ) );

    // Get effects variables
    g_pAlphaTestTechnique = g_pEffect10->GetTechniqueByName( "RenderAlphaTest" );
    g_pAlphaToCoverageTechnique = g_pEffect10->GetTechniqueByName( "RenderAlphaToCoverage" );
    g_pTransparencyAATechnique = g_pEffect10->GetTechniqueByName( "RenderTransparencyAA" );
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_m4x4WorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_m4x4World" )->AsMatrix();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pTextureVariable = g_pEffect10->GetVariableByName( "g_AlphaTexture" )->AsShaderResource();
    g_pMaterialAmbientColor = g_pEffect10->GetVariableByName( "g_v4MaterialAmbientColor" )->AsVector();     
    g_pMaterialDiffuseColor = g_pEffect10->GetVariableByName( "g_v4MaterialDiffuseColor" )->AsVector();     
    g_pLightDir = g_pEffect10->GetVariableByName( "g_v3LightDir" )->AsVector();                 
    g_pLightDiffuse = g_pEffect10->GetVariableByName( "g_v4LightDiffuse" )->AsVector();        
    g_pAlphaRef = g_pEffect10->GetVariableByName( "g_fAlphaRef" )->AsScalar();

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye(0.0f, 0.0f, -3.0f);
    D3DXVECTOR3 vecAt (0.0f, 0.0f, -0.0f);
    g_Camera.SetViewParams( &vecEye, &vecAt );

    // Call the magnify tool hook function
    g_MagnifyTool.OnCreateDevice( pd3dDevice );

    // Define the input layout
    D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof(layout) / sizeof(layout[0]);

    // Create the input layout
    D3D10_PASS_DESC PassDesc;
    g_pAlphaTestTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    hr = pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pVertexLayout );
    assert( D3D_OK == hr );

    // Fill out a unit quad
    g_QuadVertices[0].v3Pos = D3DXVECTOR3( -1.0f, -1.0f, 0.0f );
    g_QuadVertices[0].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[0].v2TexCoord = D3DXVECTOR2( 0.0f, 1.0f );
    g_QuadVertices[1].v3Pos = D3DXVECTOR3( -1.0f, 1.0f, 0.0f );
    g_QuadVertices[1].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[1].v2TexCoord = D3DXVECTOR2( 0.0f, 0.0f );
    g_QuadVertices[2].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.0f );
    g_QuadVertices[2].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[2].v2TexCoord = D3DXVECTOR2( 1.0f, 1.0f );
    g_QuadVertices[3].v3Pos = D3DXVECTOR3( -1.0f, 1.0f, 0.0f );
    g_QuadVertices[3].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[3].v2TexCoord = D3DXVECTOR2( 0.0f, 0.0f );
    g_QuadVertices[4].v3Pos = D3DXVECTOR3( 1.0f, 1.0f, 0.0f );
    g_QuadVertices[4].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[4].v2TexCoord = D3DXVECTOR2( 1.0f, 0.0f );
    g_QuadVertices[5].v3Pos = D3DXVECTOR3( 1.0f, -1.0f, 0.0f );
    g_QuadVertices[5].v3Normal = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    g_QuadVertices[5].v2TexCoord = D3DXVECTOR2( 1.0f, 1.0f );

    // Create the vertex buffer
    D3D10_BUFFER_DESC BD;
    BD.Usage = D3D10_USAGE_DEFAULT;
    BD.ByteWidth = sizeof( QuadVertex ) * 6;
    BD.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = 0;
    BD.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = g_QuadVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &g_pVertexBuffer );
    assert( D3D_OK == hr );

    // Load the alpha texture, and bind it to a shader resource view
    WCHAR strPath[MAX_PATH];

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"TransparencyAA\\DX10_1_TransparencyAA.dds" ) );
    hr = D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, NULL, NULL, &g_pLetteringAlphaTextureView, NULL );
    assert( D3D_OK == hr );

    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"TransparencyAA\\Foliage.dds" ) );
    hr = D3DX10CreateShaderResourceViewFromFile( pd3dDevice, strPath, NULL, NULL, &g_pFoliageAlphaTextureView, NULL );
    assert( D3D_OK == hr );
    
    // Setup lighting variables.
    D3DXCOLOR MaterialAmbientColor( 0.3f, 0.3f, 0.3f, 1.0f );
    D3DXCOLOR MaterialDiffuseColor( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXVECTOR3 LightDir( 0.0f, 0.0f, -1.0f );
    D3DXCOLOR LightDiffuseColor( 1.0f, 1.0f, 1.0f, 1.0f );
    hr = g_pMaterialAmbientColor->SetFloatVector( (float*)&MaterialAmbientColor );
    assert( D3D_OK == hr );
    hr = g_pMaterialDiffuseColor->SetFloatVector( (float*)&MaterialDiffuseColor );
    assert( D3D_OK == hr );
    hr = g_pLightDir->SetFloatVector( (float*)&LightDir );
    assert( D3D_OK == hr );
    hr = g_pLightDiffuse->SetFloatVector( (float*)&LightDiffuseColor );
    assert( D3D_OK == hr );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI/4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width-180, 10 );
    g_HUD.SetSize( 170, 120 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width-180, 135 );
    g_SampleUI.SetSize( 170, 180 );

    // Call the MagnifyTool hook function 
    g_MagnifyTool.OnResizedSwapChain( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext, 
        pBackBufferSurfaceDesc->Width-180, 320 );

    // Pass the back buffer and primary depth buffer to the MagnifyTool,
    // this is a once only operation, as the tool uses the hook functions to 
    // keep up with changing devices, and back buffers etc.
    static bool s_bFirstPass = true;
    if( s_bFirstPass )
    {
        D3D10_RENDER_TARGET_VIEW_DESC RTDesc;
        ID3D10Resource* pTempRTResource;
        DXUTGetD3D10RenderTargetView()->GetResource( &pTempRTResource );
        DXUTGetD3D10RenderTargetView()->GetDesc( &RTDesc );
        
        D3D10_DEPTH_STENCIL_VIEW_DESC DSDesc;
        ID3D10Resource* pTempDepthResource;
        g_MagnifyTool.GetDepthStencilView()->GetResource( &pTempDepthResource );
        g_MagnifyTool.GetDepthStencilView()->GetDesc( &DSDesc );

        // Both
        g_MagnifyTool.SetSourceResources( pTempRTResource, RTDesc.Format, pTempDepthResource, DSDesc.Format,
                    DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height,
                    DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count );
        
        g_MagnifyTool.SetPixelRegion( 64 );
        g_MagnifyTool.SetScale( 5 );

        s_bFirstPass = false;
    }

    
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIX  mWorldViewProjection;
    D3DXMATRIX  mWorld;
    D3DXMATRIX  mView;
    D3DXMATRIX  mProj;
    HRESULT hr;

    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // Clear the depth stencil
    ID3D10DepthStencilView* pDSV = g_MagnifyTool.GetDepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Get the projection & view matrix from the camera class
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldViewProjection = mWorld * mView * mProj;

    // Update the effect's variables.  Instead of using strings, it would 
    // be more efficient to cache a handle to the parameter by calling 
    // ID3DXEffect::GetParameterByName
    g_pmWorldViewProj->SetMatrix( (float*)&mWorldViewProjection );
    g_pmWorld->SetMatrix( (float*)&mWorld );
    g_pfTime->SetFloat( (float)fTime );

    // Set the alpha ref value
    g_pAlphaRef->SetFloat( g_fAlphaRef );

    // Set the texture
    switch( g_AlphaTextureType )
    {
        case ALPHA_TEXTURE_TYPE_LETTERING:
            hr = g_pTextureVariable->SetResource( g_pLetteringAlphaTextureView );
            break;
        case ALPHA_TEXTURE_TYPE_FOLIAGE:
            hr = g_pTextureVariable->SetResource( g_pFoliageAlphaTextureView );
            break;
        default:
            hr = E_FAIL;
            break;
    }
    assert( D3D_OK == hr );

    // Set vertex Layout
    pd3dDevice->IASetInputLayout( g_pVertexLayout );

    // Set vertex buffer
    UINT Stride = sizeof( QuadVertex );
    UINT Offset = 0;
    pd3dDevice->IASetVertexBuffers( 0, 1, &g_pVertexBuffer, &Stride, &Offset );

    // Set primitive topology
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Apply technique
    switch( g_TechniqueType )
    {
        case TECHNIQUE_TYPE_ALPHA_TEST:
            g_pAlphaTestTechnique->GetPassByIndex( 0 )->Apply( 0 );
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Alpha Test" );
            break;
        case TECHNIQUE_TYPE_ALPHA_TO_COVERAGE:
            g_pAlphaToCoverageTechnique->GetPassByIndex( 0 )->Apply( 0 );
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Alpha To Coverage" );
            break;
        case TECHNIQUE_TYPE_TRANSPARENCY_AA:    
            g_pTransparencyAATechnique->GetPassByIndex( 0 )->Apply( 0 );
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Transparency AA" );
            break;
    }
    
    // Render
    pd3dDevice->Draw( 6, 0 );

    DXUT_EndPerfEvent();
    
       DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Magnify Tool" );
   
    g_MagnifyTool.Render();
    
    DXUT_EndPerfEvent();

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

    RenderText();
    g_HUD.OnRender( fElapsedTime ); 
    g_SampleUI.OnRender( fElapsedTime );
    g_MagnifyTool.GetMagnifyUI()->OnRender( fElapsedTime );    

    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();

    g_MagnifyTool.OnReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    g_MagnifyTool.OnDestroyDevice();

    SAFE_RELEASE( g_pVertexBuffer );
    SAFE_RELEASE( g_pLetteringAlphaTextureView );
    SAFE_RELEASE( g_pFoliageAlphaTextureView );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9 *pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( (Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION(1,1) )
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
    }

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( (DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
            (DXUT_D3D10_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE) )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );

        // Enable MSAA
        pDeviceSettings->d3d10.sd.SampleDesc.Count = 4;
    }

    g_MagnifyTool.ModifyDeviceSettings( pDeviceSettings, pUserContext );

    g_nMSAASamples = pDeviceSettings->d3d10.sd.SampleDesc.Count;

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
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    *pbNoFurtherProcessing = g_MagnifyTool.GetMagnifyUI()->MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

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
    int nSelectedIndex = 0;
    WCHAR szTemp[256];
    
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN: 
            DXUTToggleFullScreen(); 
            break;
        
        case IDC_TOGGLEREF:        
            DXUTToggleREF(); 
            break;

        case IDC_CHANGEDEVICE:     
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); 
            break;

        case IDC_COMBOBOX_TECHNIQUE:
            nSelectedIndex = ((CDXUTComboBox*)pControl)->GetSelectedIndex();
            g_TechniqueType = (TECHNIQUE_TYPE)nSelectedIndex;
            g_SampleUI.GetStatic( IDC_STATIC_ALPHA_REF )->SetEnabled( ( g_TechniqueType != TECHNIQUE_TYPE_ALPHA_TO_COVERAGE ) );
            g_SampleUI.GetSlider( IDC_SLIDER_ALPHA_REF )->SetEnabled( ( g_TechniqueType != TECHNIQUE_TYPE_ALPHA_TO_COVERAGE ) );
            break;

        case IDC_COMBOBOX_TEXTURE:
            nSelectedIndex = ((CDXUTComboBox*)pControl)->GetSelectedIndex();
            g_AlphaTextureType = (ALPHA_TEXTURE_TYPE)nSelectedIndex;
            break;

        case IDC_SLIDER_ALPHA_REF:
            g_fAlphaRef = (float)g_SampleUI.GetSlider( IDC_SLIDER_ALPHA_REF )->GetValue();
            g_fAlphaRef /= 100.0f;
            swprintf_s( szTemp, L"Alpha Ref : %.2f", g_fAlphaRef );
            g_SampleUI.GetStatic( IDC_STATIC_ALPHA_REF )->SetText( szTemp );
            break;
    }

    // Call the MagnifyTool gui event handler
    g_MagnifyTool.OnGUIEvent( nEvent, nControlID, pControl, pUserContext );
}

//--------------------------------------------------------------------------------------
// EOF.
//--------------------------------------------------------------------------------------
