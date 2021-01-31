//--------------------------------------------------------------------------------------
// File: Skinning10.cpp
//
// Basic starting point for new Direct3D 10 samples
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

#define MAX_BONE_MATRICES 255
#define MAX_SPRITES 500
#define MAX_INSTANCES 500

enum FETCH_TYPE
{
    FT_CONSTANTBUFFER = 0,
    FT_TEXTUREBUFFER,
    FT_TEXTURE,
    FT_BUFFER,
};

struct STREAM_OUT_VERTEX
{
    D3DXVECTOR4 Pos;
    D3DXVECTOR3 Norm;
    D3DXVECTOR2 Tex;
    D3DXVECTOR3 Tangent;
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = NULL;
CDXUTDialog                         g_HUD;                  // dialog for standard controls
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls  

CDXUTSDKMesh                        g_SkinnedMesh;			// The skinned mesh
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pSkinnedVertexLayout = NULL;
ID3D10InputLayout*                  g_pTransformedVertexLayout = NULL;

ID3D10EffectTechnique*              g_pRenderConstantBuffer = NULL;
ID3D10EffectTechnique*              g_pRenderTextureBuffer = NULL;
ID3D10EffectTechnique*              g_pRenderTexture = NULL;
ID3D10EffectTechnique*              g_pRenderBuffer = NULL;

ID3D10EffectTechnique*              g_pRenderConstantBuffer_SO = NULL;
ID3D10EffectTechnique*              g_pRenderTextureBuffer_SO = NULL;
ID3D10EffectTechnique*              g_pRenderTexture_SO = NULL;
ID3D10EffectTechnique*              g_pRenderBuffer_SO = NULL;

ID3D10EffectTechnique*              g_pRenderPostTransformed = NULL;

ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectVectorVariable*         g_pvLightPos = NULL;
ID3D10EffectVectorVariable*         g_pvEyePt = NULL;
ID3D10EffectMatrixVariable*         g_pmConstBoneWorld = NULL;
ID3D10EffectMatrixVariable*         g_pmTexBoneWorld = NULL;
ID3D10EffectShaderResourceVariable* g_pBoneBufferVar = NULL;
ID3D10EffectShaderResourceVariable* g_ptxBoneTexture = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxNormal = NULL;

ID3D10Buffer*                       g_pBoneBuffer = NULL;
ID3D10ShaderResourceView*           g_pBoneBufferRV = NULL;
ID3D10Texture1D*                    g_pBoneTexture = NULL;
ID3D10ShaderResourceView*           g_pBoneTextureRV = NULL;

ID3D10Buffer**                      g_ppTransformedVBs = NULL;

D3DXVECTOR3                         g_vLightPos = D3DXVECTOR3( 159.47f, 74.23f, 103.60f );
FETCH_TYPE                          g_FetchType = FT_CONSTANTBUFFER;
bool                                g_bUseStreamOut = true;
UINT                                g_iInstances = 10;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC             -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_FETCHTYPE           4
#define IDC_STREAMOUT           5
#define IDC_INSTANCES_STATIC    6
#define IDC_INSTANCES           7
#define IDC_TOGGLEWARP          8

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );

void InitApp();
void RenderText();
void SetBoneMatrices( FETCH_TYPE ft, UINT iMesh );

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
    DXUTCreateWindow( L"Skinning10" );
    DXUTCreateDevice( true, 640, 480 );
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

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    CDXUTComboBox* pComboBox = NULL;
    iY = 60;
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Soldiers: %d", g_iInstances );
    g_SampleUI.AddStatic( IDC_INSTANCES_STATIC, str, 5, iY += 24, 150, 22 );
    g_SampleUI.AddSlider( IDC_INSTANCES, 15, iY += 24, 135, 22, 0, MAX_INSTANCES, ( int )g_iInstances );

    g_SampleUI.AddStatic( IDC_STATIC, L"(F)etch Type", 0, iY += 24, 105, 25 );
    g_SampleUI.AddComboBox( IDC_FETCHTYPE, 0, iY += 24, 140, 24, 'F', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 50 );
    pComboBox->AddItem( L"Constant Buffer", IntToPtr( FT_CONSTANTBUFFER ) );
    pComboBox->AddItem( L"Texture Buffer", IntToPtr( FT_TEXTUREBUFFER ) );
    pComboBox->AddItem( L"Texture", IntToPtr( FT_TEXTURE ) );
    pComboBox->AddItem( L"Buffer", IntToPtr( FT_BUFFER ) );

    iY = 220;
    g_SampleUI.AddCheckBox( IDC_STREAMOUT, L"Use Stream Out", 0, iY, 175, 22, g_bUseStreamOut );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
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
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
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

    V_RETURN( D3DX10CreateSprite( pd3dDevice, MAX_SPRITES, &g_pSprite10 ) );
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Skinning10.fx" ) );
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

    // Get effects techniques
    g_pRenderConstantBuffer = g_pEffect10->GetTechniqueByName( "RenderConstantBuffer" );
    g_pRenderTextureBuffer = g_pEffect10->GetTechniqueByName( "RenderTextureBuffer" );
    g_pRenderTexture = g_pEffect10->GetTechniqueByName( "RenderTexture" );
    g_pRenderBuffer = g_pEffect10->GetTechniqueByName( "RenderBuffer" );

    g_pRenderConstantBuffer_SO = g_pEffect10->GetTechniqueByName( "RenderConstantBuffer_SO" );
    g_pRenderTextureBuffer_SO = g_pEffect10->GetTechniqueByName( "RenderTextureBuffer_SO" );
    g_pRenderTexture_SO = g_pEffect10->GetTechniqueByName( "RenderTexture_SO" );
    g_pRenderBuffer_SO = g_pEffect10->GetTechniqueByName( "RenderBuffer_SO" );

    g_pRenderPostTransformed = g_pEffect10->GetTechniqueByName( "RenderPostTransformed" );

    // Get effects variables
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pvLightPos = g_pEffect10->GetVariableByName( "g_vLightPos" )->AsVector();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_pmConstBoneWorld = g_pEffect10->GetVariableByName( "g_mConstBoneWorld" )->AsMatrix();
    g_pmTexBoneWorld = g_pEffect10->GetVariableByName( "g_mTexBoneWorld" )->AsMatrix();
    g_pBoneBufferVar = g_pEffect10->GetVariableByName( "g_BufferBoneWorld" )->AsShaderResource();
    g_ptxBoneTexture = g_pEffect10->GetVariableByName( "g_txTexBoneWorld" )->AsShaderResource();
    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxNormal = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();

    // Define our vertex data layout for skinned objects
    const D3D10_INPUT_ELEMENT_DESC skinnedlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,    0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,      12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0,         16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,      20, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,       32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,     40, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    int numElements = sizeof( skinnedlayout ) / sizeof( skinnedlayout[0] );
    D3D10_PASS_DESC PassDesc;
    g_pRenderConstantBuffer->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( skinnedlayout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSkinnedVertexLayout ) );

    // Define our vertex data layout for post-transformed objects
    const D3D10_INPUT_ELEMENT_DESC transformedlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,      16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,       28, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,     36, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    numElements = sizeof( transformedlayout ) / sizeof( transformedlayout[0] );
    g_pRenderPostTransformed->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( transformedlayout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pTransformedVertexLayout ) );

    // Load the animated mesh
    V_RETURN( g_SkinnedMesh.Create( pd3dDevice, L"Soldier\\soldier.sdkmesh", true ) );
    V_RETURN( g_SkinnedMesh.LoadAnimation( L"Soldier\\soldier.sdkmesh_anim" ) );
    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity( &mIdentity );
    g_SkinnedMesh.TransformBindPose( &mIdentity );

    // Create a bone texture
    // It will be updated more than once per frame (in a typical game) so make it dynamic
    D3D10_TEXTURE1D_DESC dstex;
    dstex.Width = MAX_BONE_MATRICES * 4;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D10_USAGE_DYNAMIC;
    dstex.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateTexture1D( &dstex, NULL, &g_pBoneTexture ) );

    // Create the resource view for the bone texture
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
    SRVDesc.Texture2D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pBoneTexture, &SRVDesc, &g_pBoneTextureRV ) );

    // Create a bone matrix buffer
    // It will be updated more than once per frame (in a typical game) so make it dynamic
    D3D10_BUFFER_DESC vbdesc =
    {
        MAX_BONE_MATRICES * sizeof( D3DXMATRIX ),
        D3D10_USAGE_DYNAMIC,
        D3D10_BIND_SHADER_RESOURCE,
        D3D10_CPU_ACCESS_WRITE,
        0
    };
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &g_pBoneBuffer ) );

    // Again, we need a resource view to use it in the shader
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = MAX_BONE_MATRICES * 4;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pBoneBuffer, &SRVDesc, &g_pBoneBufferRV ) );

    // Create VBs that will hold all of the skinned vertices that need to be streamed out
    g_ppTransformedVBs = new ID3D10Buffer*[ g_SkinnedMesh.GetNumMeshes() ];
    if( !g_ppTransformedVBs )
        return E_OUTOFMEMORY;
    for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
    {
        UINT iStreamedVertexCount = ( UINT )g_SkinnedMesh.GetNumVertices( m, 0 );
        D3D10_BUFFER_DESC vbdescSO =
        {
            iStreamedVertexCount * sizeof( STREAM_OUT_VERTEX ),
            D3D10_USAGE_DEFAULT,
            D3D10_BIND_VERTEX_BUFFER | D3D10_BIND_STREAM_OUTPUT,
            0,
            0
        };
        V_RETURN( pd3dDevice->CreateBuffer( &vbdescSO, NULL, &g_ppTransformedVBs[m] ) );
    }

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 2.0f, 0.5f, 2.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.5f, -1.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

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
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Get the world matrix for this particular mesh instance
//--------------------------------------------------------------------------------------
void GetInstanceWorldMatrix( UINT iInstance, D3DXMATRIX* pmWorld )
{
    int iSide = ( UINT )sqrtf( ( float )g_iInstances );

    int iX = iInstance % iSide;
    int iZ = iInstance / iSide;

    float xStart = -iSide * 0.25f;
    float zStart = -iSide * 0.25f;

    D3DXMatrixTranslation( pmWorld, xStart + iX * 0.5f, 0, zStart + iZ * 0.5f );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIX mWorldViewProj;
    D3DXMATRIX mViewProj;
    D3DXMATRIX mWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );

    // Clear the depth stencil
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Get the projection & view matrix from the camera class
    D3DXMatrixIdentity( &mWorld );
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mViewProj = mView * mProj;

    // Set general effect values
    g_pvLightPos->SetFloatVector( ( float* )&g_vLightPos );
    D3DXVECTOR3 vEye = *g_Camera.GetEyePt();
    g_pvEyePt->SetFloatVector( ( float* )&vEye );

    // Set technique
    ID3D10EffectTechnique* pTech = NULL;
    if( g_bUseStreamOut )
    {
        switch( g_FetchType )
        {
            case FT_CONSTANTBUFFER:
                pTech = g_pRenderConstantBuffer_SO;
                break;
            case FT_TEXTUREBUFFER:
                pTech = g_pRenderTextureBuffer_SO;
                break;
            case FT_TEXTURE:
                pTech = g_pRenderTexture_SO;
                break;
            case FT_BUFFER:
                pTech = g_pRenderBuffer_SO;
                break;
        };
    }
    else
    {
        switch( g_FetchType )
        {
            case FT_CONSTANTBUFFER:
                pTech = g_pRenderConstantBuffer;
                break;
            case FT_TEXTUREBUFFER:
                pTech = g_pRenderTextureBuffer;
                break;
            case FT_TEXTURE:
                pTech = g_pRenderTexture;
                break;
            case FT_BUFFER:
                pTech = g_pRenderBuffer;
                break;
        };
    }

    ID3D10Buffer* pBuffers[1];
    UINT stride[1];
    UINT offset[1] = { 0 };

    if( g_bUseStreamOut )
    {
        //
        // Skin the vertices and stream them out
        //
        for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
        {
            // Turn on stream out
            pBuffers[0] = g_ppTransformedVBs[m];
            pd3dDevice->SOSetTargets( 1, pBuffers, offset );

            // Set vertex Layout
            pd3dDevice->IASetInputLayout( g_pSkinnedVertexLayout );

            // Set source vertex buffer
            pBuffers[0] = g_SkinnedMesh.GetVB10( m, 0 );
            stride[0] = g_SkinnedMesh.GetVertexStride( m, 0 );
            pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
            pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );	// When skinning vertices, we don't care about topology.
            // Treat the entire vertex buffer as list of points.

            // Set the bone matrices
            SetBoneMatrices( g_FetchType, m );

            // Render the vertices as an array of points
            D3D10_TECHNIQUE_DESC techDesc;
            pTech->GetDesc( &techDesc );
            for( UINT p = 0; p < techDesc.Passes; ++p )
            {
                pTech->GetPassByIndex( p )->Apply( 0 );
                pd3dDevice->Draw( ( UINT )g_SkinnedMesh.GetNumVertices( m, 0 ), 0 );
            }
        }

        // Turn off stream out
        pBuffers[0] = NULL;
        pd3dDevice->SOSetTargets( 1, pBuffers, offset );


        //
        // Render the transformed mesh from the streamed out vertices
        //

        for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
        {
            // Set IA parameters
            pBuffers[0] = g_ppTransformedVBs[m];
            stride[0] = sizeof( STREAM_OUT_VERTEX );
            pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
            pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            pd3dDevice->IASetIndexBuffer( g_SkinnedMesh.GetIB10( m ), g_SkinnedMesh.GetIBFormat10( m ), 0 );

            // Set vertex Layout
            pd3dDevice->IASetInputLayout( g_pTransformedVertexLayout );

            // Set materials
            SDKMESH_SUBSET* pSubset = NULL;
            SDKMESH_MATERIAL* pMat = NULL;

            D3D10_TECHNIQUE_DESC techDesc;
            g_pRenderPostTransformed->GetDesc( &techDesc );

            for( UINT iInstance = 0; iInstance < g_iInstances; iInstance++ )
            {
                // Set effect variables
                GetInstanceWorldMatrix( iInstance, &mWorld );
                mWorldViewProj = mWorld * mViewProj;
                g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
                g_pmWorld->SetMatrix( ( float* )&mWorld );

                for( UINT p = 0; p < techDesc.Passes; ++p )
                {
                    for( UINT subset = 0; subset < g_SkinnedMesh.GetNumSubsets( m ); subset++ )
                    {
                        pSubset = g_SkinnedMesh.GetSubset( m, subset );

                        pMat = g_SkinnedMesh.GetMaterial( pSubset->MaterialID );
                        if( pMat )
                        {
                            g_ptxDiffuse->SetResource( pMat->pDiffuseRV10 );
                            g_ptxNormal->SetResource( pMat->pNormalRV10 );
                        }

                        g_pRenderPostTransformed->GetPassByIndex( p )->Apply( 0 );
                        pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                                 ( UINT )pSubset->VertexStart );
                    }
                }
            }
        }

        //clear out the vb bindings for the next pass
        pBuffers[0] = NULL;
        pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
    }
    else
    {
        for( UINT iInstance = 0; iInstance < g_iInstances; iInstance++ )
        {
            // Set effect variables
            GetInstanceWorldMatrix( iInstance, &mWorld );
            mWorldViewProj = mWorld * mViewProj;
            g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
            g_pmWorld->SetMatrix( ( float* )&mWorld );

            // Set vertex Layout
            pd3dDevice->IASetInputLayout( g_pSkinnedVertexLayout );

            // Render the meshes
            for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
            {
                pBuffers[0] = g_SkinnedMesh.GetVB10( m, 0 );
                stride[0] = ( UINT )g_SkinnedMesh.GetVertexStride( m, 0 );
                offset[0] = 0;

                pd3dDevice->IASetVertexBuffers( 0, 1, pBuffers, stride, offset );
                pd3dDevice->IASetIndexBuffer( g_SkinnedMesh.GetIB10( m ), g_SkinnedMesh.GetIBFormat10( m ), 0 );

                D3D10_TECHNIQUE_DESC techDesc;
                pTech->GetDesc( &techDesc );
                SDKMESH_SUBSET* pSubset = NULL;
                SDKMESH_MATERIAL* pMat = NULL;
                D3D10_PRIMITIVE_TOPOLOGY PrimType;

                // Set the bone matrices
                SetBoneMatrices( g_FetchType, m );

                for( UINT p = 0; p < techDesc.Passes; ++p )
                {
                    for( UINT subset = 0; subset < g_SkinnedMesh.GetNumSubsets( m ); subset++ )
                    {
                        pSubset = g_SkinnedMesh.GetSubset( m, subset );

                        PrimType = g_SkinnedMesh.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )
                                                                     pSubset->PrimitiveType );
                        pd3dDevice->IASetPrimitiveTopology( PrimType );

                        pMat = g_SkinnedMesh.GetMaterial( pSubset->MaterialID );
                        if( pMat )
                        {
                            g_ptxDiffuse->SetResource( pMat->pDiffuseRV10 );
                            g_ptxNormal->SetResource( pMat->pNormalRV10 );
                        }

                        pTech->GetPassByIndex( p )->Apply( 0 );
                        pd3dDevice->DrawIndexed( ( UINT )pSubset->IndexCount, ( UINT )pSubset->IndexStart,
                                                 ( UINT )pSubset->VertexStart );
                    }
                }
            }//nummeshes
        }
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
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
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pSkinnedVertexLayout );
    SAFE_RELEASE( g_pTransformedVertexLayout );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pBoneBuffer );
    SAFE_RELEASE( g_pBoneBufferRV );
    SAFE_RELEASE( g_pBoneTexture );
    SAFE_RELEASE( g_pBoneTextureRV );
    for( UINT m = 0; m < g_SkinnedMesh.GetNumMeshes(); m++ )
        SAFE_RELEASE( g_ppTransformedVBs[m] );
    SAFE_DELETE_ARRAY( g_ppTransformedVBs );
    g_SkinnedMesh.Destroy();
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
        if( ( DXUT_D3D10_DEVICE == pDeviceSettings->ver &&
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

    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity( &mIdentity );
    g_SkinnedMesh.TransformMesh( &mIdentity, fTime );
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
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_STREAMOUT:
            g_bUseStreamOut = !g_bUseStreamOut; break;
        case IDC_FETCHTYPE:
        {
            CDXUTComboBox* pComboBox = ( CDXUTComboBox* )pControl;
            g_FetchType = ( FETCH_TYPE )( int )PtrToInt( pComboBox->GetSelectedData() );
        }
            break;
        case IDC_INSTANCES:
        {
            WCHAR str[MAX_PATH] = {0};
            g_iInstances = g_SampleUI.GetSlider( IDC_INSTANCES )->GetValue();

            swprintf_s( str, MAX_PATH, L"Soldlers: %d", g_iInstances );
            g_SampleUI.GetStatic( IDC_INSTANCES_STATIC )->SetText( str );
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// SetBoneMatrices
//
// This function handles the various ways of sending bone matrices to the shader.
//		FT_CONSTANTBUFFER:
//			With this approach, the bone matrices are stored in a constant buffer.
//			The shader will index into the constant buffer to grab the correct
//			transformation matrices for each vertex.
//		FT_TEXTUREBUFFER:
//			This approach shows the differences between constant buffers and tbuffers.
//			tbuffers are special constant buffers that are accessed like textures.
//			They should give better random access performance.
//		FT_TEXTURE:
//			When FT_TEXTURE is specified, the matrices are loaded into a 1D texture.
//			This is different from a tbuffer in that an actual texture fetch is used
//			instead of a lookup into a constant buffer.
//		FT_BUFFER:
//			This loads the matrices into a buffer that is bound to the shader.  The
//			shader calls Load on the buffer object to load the different matrices from
//			the stream.  This should give better linear access performance.
//--------------------------------------------------------------------------------------
void SetBoneMatrices( FETCH_TYPE ft, UINT iMesh )
{
    switch( ft )
    {
        case FT_CONSTANTBUFFER:
            for( UINT i = 0; i < g_SkinnedMesh.GetNumInfluences( iMesh ); i++ )
            {
                const D3DXMATRIX* pMat = g_SkinnedMesh.GetMeshInfluenceMatrix( iMesh, i );
                g_pmConstBoneWorld->SetMatrixArray( ( float* )pMat, i, 1 );
            }
            break;
        case FT_TEXTUREBUFFER:
            for( UINT i = 0; i < g_SkinnedMesh.GetNumInfluences( iMesh ); i++ )
            {
                const D3DXMATRIX* pMat = g_SkinnedMesh.GetMeshInfluenceMatrix( iMesh, i );
                g_pmTexBoneWorld->SetMatrixArray( ( float* )pMat, i, 1 );
            }
            break;
        case FT_TEXTURE:
        {
            D3DXMATRIX* pMatrices;
            HRESULT hr = S_OK;
            V( g_pBoneTexture->Map( 0, D3D10_MAP_WRITE_DISCARD, 0, ( void** )&pMatrices ) );
            for( UINT i = 0; i < g_SkinnedMesh.GetNumInfluences( iMesh ); i++ )
            {
                pMatrices[i] = *g_SkinnedMesh.GetMeshInfluenceMatrix( iMesh, i );
            }
            g_pBoneTexture->Unmap( 0 );

            g_ptxBoneTexture->SetResource( g_pBoneTextureRV );
        }
            break;
        case FT_BUFFER:
        {
            D3DXMATRIX* pMatrices;
            HRESULT hr = S_OK;
            V( g_pBoneBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, ( void** )&pMatrices ) );
            for( UINT i = 0; i < g_SkinnedMesh.GetNumInfluences( iMesh ); i++ )
            {
                pMatrices[i] = *g_SkinnedMesh.GetMeshInfluenceMatrix( iMesh, i );
            }
            g_pBoneBuffer->Unmap();

            g_pBoneBufferVar->SetResource( g_pBoneBufferRV );
        }
            break;
    };
}


