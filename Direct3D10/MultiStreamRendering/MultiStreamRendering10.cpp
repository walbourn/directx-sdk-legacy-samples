//--------------------------------------------------------------------------------------
// File: MultiStreamRendering10.cpp
//
// Illustrates multi-stream rendering using Direct3D 10
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

enum STREAM_TYPE
{
    ST_VERTEX_POSITION = 0,
    ST_VERTEX_NORMAL,
    ST_VERTEX_TEXTUREUV,
    ST_VERTEX_TEXTUREUV2,

    ST_FEW_VERTEX_POSITION,
    ST_POSITION_INDEX,
    ST_FACE_NORMAL,

    ST_MAX_VB_STREAMS,
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
bool                                g_bRenderFF = false;    // render fixed function or not
bool                                g_bUseAltUV = false;    // render with an alternative UV stream
bool                                g_bUseMultiIndex = false; // render multi-stream multi-index

// Direct3D 9 resources
extern ID3DXFont*                   g_pFont9;
extern ID3DXSprite*                 g_pSprite9;

// Direct3D 10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pVertexLayout_SI = NULL;
ID3D10InputLayout*                  g_pVertexLayout_MI = NULL;
ID3D10ShaderResourceView*           g_pMeshTexRV = NULL;

ID3D10EffectTechnique*              g_pRenderScene_SI = NULL;
ID3D10EffectTechnique*              g_pRenderScene_MI = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectVectorVariable*         g_pLightDir = NULL;
ID3D10EffectVectorVariable*         g_pLightDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_pTexture = NULL;
ID3D10EffectShaderResourceVariable* g_pPosBuffer = NULL;
ID3D10EffectShaderResourceVariable* g_pNormBuffer = NULL;

ID3D10Buffer*                       g_pVBs[ST_MAX_VB_STREAMS] = {NULL};
ID3D10ShaderResourceView*           g_pFewVertexPosRV = NULL;
ID3D10ShaderResourceView*           g_pFaceNormalRV = NULL;
ID3D10Buffer*                       g_pIB = NULL;

// External Mesh Data
extern DWORD                        g_dwNumVertices;
extern DWORD                        g_dwNumFaces;
extern D3DXVECTOR3 g_VertexPositions[];
extern D3DXVECTOR3 g_VertexNormals[];
extern D3DXVECTOR2 g_VertexTextureUV[];
extern D3DXVECTOR2 g_VertexTextureUV2[];
extern DWORD g_Indices[];
extern DWORD                        g_dwFewVertices;
extern D3DXVECTOR4 g_FewVertexPositions[];
extern DWORD g_PositionIndices[];
extern D3DXVECTOR4 g_FaceNormals[];


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_FIXEDFUNC           4
#define IDC_ALTERNATIVE_UV      5
#define IDC_MULTI_INDEX         6
#define IDC_TOGGLEWARP          7


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

extern bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                             bool bWindowed, void* pUserContext );
extern HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice,
                                            const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
extern HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                           void* pUserContext );
extern void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime,
                                        void* pUserContext );
extern void CALLBACK OnD3D9LostDevice( void* pUserContext );
extern void CALLBACK OnD3D9DestroyDevice( void* pUserContext );

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

    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );

    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"MultiStreamRendering" );
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

    iY += 200;
    g_HUD.AddCheckBox( IDC_FIXEDFUNC, L"Render Fixed Function", 0, iY, 175, 22, g_bRenderFF );
    iY += 50;
    g_HUD.AddCheckBox( IDC_ALTERNATIVE_UV, L"Use Alternative UV Stream", 0, iY, 175, 22, g_bUseAltUV );
    iY += 50;
    g_HUD.AddCheckBox( IDC_MULTI_INDEX, L"Use Multiple Indices", 0, iY, 175, 22, g_bUseMultiIndex );

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

    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

    // Disable the FF checkbox
    CDXUTCheckBox* pCheck = g_HUD.GetCheckBox( IDC_FIXEDFUNC );
    pCheck->SetVisible( false );
    pCheck->SetEnabled( false );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"MultiStreamRendering.fx" ) );
    D3D10_SHADER_MACRO Defines[] =
    {
        { "D3D10", "TRUE" },
        { NULL, NULL }
    };
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    V_RETURN( D3DX10CreateEffectFromFile( str, Defines, NULL, "fx_4_0", dwShaderFlags, 0, pd3dDevice, NULL,
                                              NULL, &g_pEffect10, NULL, NULL ) );

    // Get effects variables
    g_pRenderScene_SI = g_pEffect10->GetTechniqueByName( "RenderScene_SI" );
    g_pRenderScene_MI = g_pEffect10->GetTechniqueByName( "RenderScene_MI" );
    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pLightDir = g_pEffect10->GetVariableByName( "g_LightDir" )->AsVector();
    g_pLightDiffuse = g_pEffect10->GetVariableByName( "g_LightDiffuse" )->AsVector();
    g_pTexture = g_pEffect10->GetVariableByName( "g_MeshTexture" )->AsShaderResource();
    g_pPosBuffer = g_pEffect10->GetVariableByName( "g_posBuffer" )->AsShaderResource();
    g_pNormBuffer = g_pEffect10->GetVariableByName( "g_normBuffer" )->AsShaderResource();

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -5.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    //sizes of various buffers
    DWORD dwPositionSize = g_dwNumVertices * sizeof( D3DXVECTOR3 );
    DWORD dwNormalSize = g_dwNumVertices * sizeof( D3DXVECTOR3 );
    DWORD dwTextureUVSize = g_dwNumVertices * sizeof( D3DXVECTOR2 );
    DWORD dwIndexSize = g_dwNumFaces * 3 * sizeof( DWORD );
    DWORD dwFewPositionSize = g_dwFewVertices * sizeof( D3DXVECTOR4 );
    DWORD dwFaceNormalSize = g_dwNumFaces * sizeof( D3DXVECTOR4 );

    // Create the stream object that will hold vertex positions
    D3D10_BUFFER_DESC bufferDesc;
    bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bufferDesc.ByteWidth = dwPositionSize;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.Usage = D3D10_USAGE_DEFAULT;
    D3D10_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof( D3D10_SUBRESOURCE_DATA ) );
    vbInitData.pSysMem = g_VertexPositions;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_VERTEX_POSITION] ) );

    // Create the stream object that will hold vertex normals
    bufferDesc.ByteWidth = dwNormalSize;
    vbInitData.pSysMem = g_VertexNormals;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_VERTEX_NORMAL] ) );

    // Create the stream object that will hold vertex texture uv coordinates
    bufferDesc.ByteWidth = dwTextureUVSize;
    vbInitData.pSysMem = g_VertexTextureUV;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_VERTEX_TEXTUREUV] ) );

    // Create the stream object that will hold vertex texture uv2 coordinates
    bufferDesc.ByteWidth = dwTextureUVSize;
    vbInitData.pSysMem = g_VertexTextureUV2;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_VERTEX_TEXTUREUV2] ) );

    // Create the stream object that will hold the position indices
    bufferDesc.ByteWidth = dwIndexSize;
    vbInitData.pSysMem = g_PositionIndices;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_POSITION_INDEX] ) );

    // Create the stream object that will hold reduced number of vertex positions coordinates
    bufferDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    bufferDesc.ByteWidth = dwFewPositionSize;
    vbInitData.pSysMem = g_FewVertexPositions;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_FEW_VERTEX_POSITION] ) );

    // Create the stream object that will hold the face normals
    bufferDesc.ByteWidth = dwFaceNormalSize;
    vbInitData.pSysMem = g_FaceNormals;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pVBs[ST_FACE_NORMAL] ) );

    // Create the index buffer
    bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bufferDesc.ByteWidth = dwIndexSize;
    vbInitData.pSysMem = g_Indices;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &g_pIB ) );

    // Create resource views for some of the buffers
    D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof( SRVDesc ) );
    SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
    SRVDesc.Buffer.ElementOffset = 0;
    SRVDesc.Buffer.ElementWidth = g_dwFewVertices;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pVBs[ST_FEW_VERTEX_POSITION], &SRVDesc, &g_pFewVertexPosRV ) );
    SRVDesc.Buffer.ElementWidth = g_dwNumFaces;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pVBs[ST_FACE_NORMAL], &SRVDesc, &g_pFaceNormalRV ) );

    // Create a Input Layout for the MultiStream data.
    // Notice that the 4th parameter is the stream index.  This indicates the
    // VB that the particular data comes from.  In this case, position data
    // comes from stream 0.  Normal data comes from stream 1.  Texture coordinate
    // data comes from stream 2.
    const D3D10_INPUT_ELEMENT_DESC vertlayout_singleindex[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT iNumElements = sizeof( vertlayout_singleindex ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    D3D10_PASS_DESC PassDesc;
    g_pRenderScene_SI->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( vertlayout_singleindex, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout_SI ) );

    // Create a Input Layout for the MultiStream MultiIndex data.
    // 
    const D3D10_INPUT_ELEMENT_DESC vertlayout_multiindex[] =
    {
        { "POSINDEX",    0, DXGI_FORMAT_R32_UINT,        0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    iNumElements = sizeof( vertlayout_multiindex ) / sizeof( D3D10_INPUT_ELEMENT_DESC );
    g_pRenderScene_MI->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( vertlayout_multiindex, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout_MI ) );

    // Load a texture for the mesh
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"misc\\ball.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pMeshTexRV, NULL ) );

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
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    D3DXMATRIX mWorldViewProjection;
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
    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldViewProjection = mWorld * mView * mProj;

    // Update the effect's variables.
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProjection );
    g_pmWorld->SetMatrix( ( float* )&mWorld );
    D3DXVECTOR3 vLightDir( -1, 1, -1 );
    D3DXVec3Normalize( &vLightDir, &vLightDir );
    D3DXVECTOR4 vLightDiffuse( 1, 1, 1, 1 );
    g_pLightDir->SetFloatVector( vLightDir );
    g_pLightDiffuse->SetFloatVector( vLightDiffuse );

    // Init Input Assembler states
    UINT strides[3];
    UINT offsets[3] = {0, 0, 0};

    // Set the parameters for MultiIndex or SingleIndex
    ID3D10Buffer* pBuffers[3];
    ID3D10EffectTechnique* pTech = NULL;
    UINT numVBsSet;
    if( g_bUseMultiIndex )
    {
        pTech = g_pRenderScene_MI;
        pd3dDevice->IASetInputLayout( g_pVertexLayout_MI );

        pBuffers[0] = g_pVBs[ST_POSITION_INDEX];
        if( g_bUseAltUV )
            pBuffers[1] = g_pVBs[ST_VERTEX_TEXTUREUV2];
        else
            pBuffers[1] = g_pVBs[ST_VERTEX_TEXTUREUV];

        strides[0] = sizeof( UINT );
        strides[1] = sizeof( D3DXVECTOR2 );

        g_pPosBuffer->SetResource( g_pFewVertexPosRV );
        g_pNormBuffer->SetResource( g_pFaceNormalRV );

        numVBsSet = 2;
    }
    else
    {
        pTech = g_pRenderScene_SI;
        pd3dDevice->IASetInputLayout( g_pVertexLayout_SI );

        pBuffers[0] = g_pVBs[ST_VERTEX_POSITION];
        pBuffers[1] = g_pVBs[ST_VERTEX_NORMAL];

        if( g_bUseAltUV )
            pBuffers[2] = g_pVBs[ST_VERTEX_TEXTUREUV2];
        else
            pBuffers[2] = g_pVBs[ST_VERTEX_TEXTUREUV];

        strides[0] = sizeof( D3DXVECTOR3 );
        strides[1] = sizeof( D3DXVECTOR3 );
        strides[2] = sizeof( D3DXVECTOR2 );

        numVBsSet = 3;
    }

    pd3dDevice->IASetVertexBuffers( 0, numVBsSet, pBuffers, strides, offsets );
    pd3dDevice->IASetIndexBuffer( g_pIB, DXGI_FORMAT_R32_UINT, 0 );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set Texture
    g_pTexture->SetResource( g_pMeshTexRV );

    // Render the scene using the technique
    D3D10_TECHNIQUE_DESC techDesc;
    pTech->GetDesc( &techDesc );
    for( UINT iPass = 0; iPass < techDesc.Passes; iPass++ )
    {
        pTech->GetPassByIndex( iPass )->Apply( 0 );
        pd3dDevice->DrawIndexed( g_dwNumFaces * 3, 0, 0 );
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
    SAFE_RELEASE( g_pVertexLayout_SI );
    SAFE_RELEASE( g_pVertexLayout_MI );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    for( int i = 0; i < ST_MAX_VB_STREAMS; i++ )
    {
        SAFE_RELEASE( g_pVBs[i] );
    }
    SAFE_RELEASE( g_pFewVertexPosRV );
    SAFE_RELEASE( g_pFaceNormalRV );

    SAFE_RELEASE( g_pIB );

    SAFE_RELEASE( g_pMeshTexRV );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        D3DCAPS9 Caps;
        pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, &Caps );

        // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
        // then switch to SWVP.
        if( ( Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
            Caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_FIXEDFUNC:
            g_bRenderFF = !g_bRenderFF; break;
        case IDC_ALTERNATIVE_UV:
            g_bUseAltUV = !g_bUseAltUV; break;
        case IDC_MULTI_INDEX:
            g_bUseMultiIndex = !g_bUseMultiIndex; break;
    }
}


