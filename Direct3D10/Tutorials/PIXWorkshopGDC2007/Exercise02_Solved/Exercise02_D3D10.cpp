//--------------------------------------------------------------------------------------
// Exercise02_D3D10.cpp
// PIX Workshop GDC2007 (D3D10 Codepath)
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#include "Terrain.h"
#include "Player.h"
#include "Sim.h"

#define MAX_SPRITES 200
//#define SWAP( a, b ) { UINT _temp = a; a = b; b = _temp; }
#define NUM_PLAYERS 5000

//--------------------------------------------------------------------------------------
// DXUT Specific variables
//--------------------------------------------------------------------------------------
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CD3DSettingsDlg              g_SettingsDlg;          // Device settings dialog
extern CDXUTTextHelper*             g_pTxtHelper;
extern CDXUTDialog                  g_HUD;                  // dialog for standard controls
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls

// Direct3D 10 resources
ID3DX10Font*                        g_pFont10 = NULL;
ID3DX10Sprite*                      g_pSprite10 = NULL;
ID3D10Effect*                       g_pEffect10 = NULL;
ID3D10InputLayout*                  g_pBasicDecl10 = NULL;
ID3D10InputLayout*                  g_pBallDecl10 = NULL;
ID3D10InputLayout*                  g_pGrassDecl10 = NULL;
ID3D10ShaderResourceView*           g_pHeightTexRV = NULL;
ID3D10ShaderResourceView*           g_pNormalTexRV = NULL;
ID3D10ShaderResourceView*           g_pGrassTexRV = NULL;
ID3D10ShaderResourceView*           g_pDirtTexRV = NULL;
ID3D10ShaderResourceView*           g_pGroundGrassTexRV = NULL;
ID3D10ShaderResourceView*           g_pMaskTexRV = NULL;
ID3D10ShaderResourceView*           g_pShadeNormalTexRV = NULL;
ID3D10Buffer*                       g_pStreamDataVB10 = NULL;
ID3D10Buffer*                       g_pGrassDataVB10 = NULL;

extern CTerrain                     g_Terrain;
extern CPlayer*                     g_pPlayers;
extern CSim                         g_Sim;

extern UINT                         g_NumThreads;
extern bool                         g_bWireframe;
extern bool                         g_bShowTiles;
extern bool                         g_bRenderBalls;
extern bool                         g_bRunSim;
extern bool                         g_bBallCam;
extern bool                         g_bRenderGrass;
extern bool                         g_bEnableCulling;
extern bool                         g_bDepthCullGrass;
extern float                        g_fFOV;
extern CGrowableArray <UINT>        g_VisibleTileArray;
extern CGrowableArray <float>       g_VisibleTileArrayDepth;
extern float                        g_fWorldScale;
extern float                        g_fHeightScale;
extern UINT                         g_SqrtNumTiles;
extern UINT                         g_SidesPerTile;
extern float                        g_fFadeStart;
extern float                        g_fFadeEnd;
extern UINT                         g_NumVisibleBalls;
extern UINT                         g_NumGrassTiles;

//--------------------------------------------------------------------------------------
// Effect variables
//--------------------------------------------------------------------------------------
extern WCHAR g_strEffect[MAX_PATH];
extern FILETIME                     g_fLastWriteTime;
extern double                       g_fLastFileCheck;

ID3D10EffectTechnique*              g_pRenderTerrain = NULL;
ID3D10EffectTechnique*              g_pRenderBall = NULL;
ID3D10EffectTechnique*              g_pRenderGrass = NULL;
ID3D10EffectTechnique*              g_pRenderSky = NULL;
ID3D10EffectMatrixVariable*         g_pmWorldViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmViewProj = NULL;
ID3D10EffectMatrixVariable*         g_pmWorld = NULL;
ID3D10EffectVectorVariable*         g_pvWorldLightDir = NULL;
ID3D10EffectScalarVariable*         g_pfTime = NULL;
ID3D10EffectScalarVariable*         g_pfElapsedTime = NULL;
ID3D10EffectVectorVariable*         g_pvColor = NULL;
ID3D10EffectScalarVariable*         g_pfWorldScale = NULL;
ID3D10EffectScalarVariable*         g_pfHeightScale = NULL;
ID3D10EffectVectorVariable*         g_pvEyePt = NULL;
ID3D10EffectScalarVariable*         g_pfFadeStart = NULL;
ID3D10EffectScalarVariable*         g_pfFadeEnd = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDiffuse = NULL;
ID3D10EffectShaderResourceVariable* g_ptxNormal = NULL;
ID3D10EffectShaderResourceVariable* g_ptxHeight = NULL;
ID3D10EffectShaderResourceVariable* g_ptxDirt = NULL;
ID3D10EffectShaderResourceVariable* g_ptxGrass = NULL;
ID3D10EffectShaderResourceVariable* g_ptxMask = NULL;
ID3D10EffectShaderResourceVariable* g_ptxShadeNormals = NULL;

extern CDXUTSDKMesh                 g_BallMesh;
extern CDXUTSDKMesh                 g_SkyMesh;

//--------------------------------------------------------------------------------------
// Callbacks
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext );
void CALLBACK OnD3D10DestroyDevice( void* pUserContext );
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );

extern void RenderText();
extern void GetCameraData( D3DXMATRIX* pmWorld, D3DXMATRIX* pmView, D3DXMATRIX* pmProj, D3DXVECTOR3* pvEye,
                           D3DXVECTOR3* pvDir );
extern void GetCameraCullPlanes( D3DXVECTOR3* p1Normal, D3DXVECTOR3* p2Normal, float* p1D, float* p2D );
extern void VisibilityCullTiles();
extern FILETIME GetFileTime( WCHAR* strFile );
extern UINT GetSystemHWThreadCount();
extern void ResetBalls();
extern void SetNumVisibleTiles( UINT iVal );
extern void SetNumVisibleGrassTiles( UINT iVal );
extern void SetNumVisibleBalls( UINT iVal );
extern void SetMaxGridBalls( UINT iVal );

HRESULT LoadEffect10( ID3D10Device* pd3dDevice );

//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite10 ) );
    g_pTxtHelper = new CDXUTTextHelper( NULL, NULL, g_pFont10, g_pSprite10, 15 );

#define IDC_WIREFRAME			10
    g_SampleUI.GetCheckBox( IDC_WIREFRAME )->SetVisible( false );

    V_RETURN( LoadEffect10( pd3dDevice ) );

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1.bmp" ) );
    V_RETURN( g_Terrain.LoadTerrain( str, g_SqrtNumTiles, g_SidesPerTile, g_fWorldScale, g_fHeightScale, 1000, 1.0f,
                                     2.0f ) );

    ResetBalls();

    // Create a Vertex Decl for the terrain and basic meshes
    const D3D10_INPUT_ELEMENT_DESC basiclayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC PassDesc;
    g_pRenderTerrain->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( basiclayout, sizeof( basiclayout ) / sizeof( basiclayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pBasicDecl10 ) );

    // Create a Vertex Decl for the ball
    const D3D10_INPUT_ELEMENT_DESC balllayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,  D3D10_INPUT_PER_INSTANCE_DATA, 1 },
    };
    g_pRenderBall->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( balllayout, sizeof( balllayout ) / sizeof( balllayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pBallDecl10 ) );

    // Create a Vertex Decl for the grass
    const D3D10_INPUT_ELEMENT_DESC grasslayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D10_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,  D3D10_INPUT_PER_INSTANCE_DATA, 1 },
    };
    g_pRenderGrass->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( grasslayout, sizeof( grasslayout ) / sizeof( grasslayout[0] ),
                                             PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pGrassDecl10 ) );

    // Load terrain device objects
    V_RETURN( g_Terrain.OnCreateDevice( pd3dDevice ) );

    // Load a mesh
    g_BallMesh.Create( pd3dDevice, L"PIXWorkshop\\lowpolysphere.sdkmesh" );
    g_SkyMesh.Create( pd3dDevice, L"PIXWorkshop\\desertsky.sdkmesh" );

    // Create a VB for the stream data
    D3D10_BUFFER_DESC BufferDesc;
    BufferDesc.ByteWidth = NUM_PLAYERS * sizeof( D3DXVECTOR3 );
    BufferDesc.Usage = D3D10_USAGE_DYNAMIC;
    BufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    BufferDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &g_pStreamDataVB10 ) );

    // Create a VB for the grass instances
    BufferDesc.ByteWidth = g_SqrtNumTiles * g_SqrtNumTiles * sizeof( D3DXVECTOR3 );
    V_RETURN( pd3dDevice->CreateBuffer( &BufferDesc, NULL, &g_pGrassDataVB10 ) );

    // Load a texture for the mesh
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1.bmp" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pHeightTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_Norm.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pNormalTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\grass_v3_dark_tex.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pGrassTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Dirt_Diff.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pDirtTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Grass_Diff.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pGroundGrassTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_Mask.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pMaskTexRV, NULL ) );
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PIXWorkshop\\Terrain1_ShadeNormals.dds" ) );
    V_RETURN( D3DX10CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &g_pShadeNormalTexRV, NULL ) );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 20.0f, -50.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 5.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10SwapChainResized( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( g_fFOV, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 350 );
    g_SampleUI.SetSize( 170, 300 );

    return hr;
}


//--------------------------------------------------------------------------------------
// RenderTerrain
//--------------------------------------------------------------------------------------
void RenderTerrain( ID3D10Device* pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    pd3dDevice->IASetInputLayout( g_pBasicDecl10 );

    g_pmWorldViewProj->SetMatrix( ( float* )&mWVP );
    g_pmWorld->SetMatrix( ( float* )&mWorld );

    g_ptxNormal->SetResource( g_pNormalTexRV );
    g_ptxDirt->SetResource( g_pDirtTexRV );
    g_ptxGrass->SetResource( g_pGroundGrassTexRV );
    g_ptxMask->SetResource( g_pMaskTexRV );

    if( !g_bShowTiles )
    {
        D3DXVECTOR4 color( 1,1,1,1 );
        g_pvColor->SetFloatVector( ( float* )&color );
    }

    pd3dDevice->IASetIndexBuffer( g_Terrain.GetTerrainIB10(), DXGI_FORMAT_R16_UINT, 0 );

    D3D10_TECHNIQUE_DESC techDesc;
    g_pRenderTerrain->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        // Render front to back
        UINT NumTiles = g_VisibleTileArray.GetSize();
        SetNumVisibleTiles( NumTiles );
        for( UINT i = 0; i < NumTiles; i++ )
        {
            TERRAIN_TILE* pTile = g_Terrain.GetTile( g_VisibleTileArray.GetAt( i ) );

            if( g_bShowTiles )
            {
                g_pvColor->SetFloatVector( ( float* )&pTile->Color );
            }
            g_pRenderTerrain->GetPassByIndex( p )->Apply( 0 );

            g_Terrain.RenderTile( pTile );
        }
    }
}


//--------------------------------------------------------------------------------------
// RenderBalls
//--------------------------------------------------------------------------------------
void RenderBalls( ID3D10Device* pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    g_pmWorldViewProj->SetMatrix( ( float* )&mWVP );
    g_pmWorld->SetMatrix( ( float* )&mWorld );

    pd3dDevice->IASetInputLayout( g_pBallDecl10 );

    ID3D10EffectTechnique* pTechnique = g_pRenderBall;
	
    // set vb streams
    ID3D10Buffer* pBuffers[2];
    pBuffers[0] = g_BallMesh.GetVB10( 0, 0 );
    pBuffers[1] = g_pStreamDataVB10;
    UINT strides[2];
    strides[0] = g_BallMesh.GetVertexStride( 0, 0 );
    strides[1] = sizeof( D3DXVECTOR3 );
    UINT offsets[2] = {0,0};
    pd3dDevice->IASetVertexBuffers( 0, 2, pBuffers, strides, offsets );

    SetNumVisibleBalls( g_NumVisibleBalls );

    // Set our index buffer as well
    pd3dDevice->IASetIndexBuffer( g_BallMesh.GetIB10( 0 ), g_BallMesh.GetIBFormat10( 0 ), 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    D3D10_PRIMITIVE_TOPOLOGY PrimType;
    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_BallMesh.GetNumSubsets( 0 ); subset++ )
        {
            pSubset = g_BallMesh.GetSubset( 0, subset );

            PrimType = g_BallMesh.GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dDevice->IASetPrimitiveTopology( PrimType );

            pTechnique->GetPassByIndex( p )->Apply( 0 );

            UINT IndexCount = ( UINT )pSubset->IndexCount;
            UINT IndexStart = ( UINT )pSubset->IndexStart;
            UINT VertexStart = ( UINT )pSubset->VertexStart;
            //UINT VertexCount = (UINT)pSubset->VertexCount;

            pd3dDevice->DrawIndexedInstanced( IndexCount, g_NumVisibleBalls, IndexStart, VertexStart, 0 );
        }
    }

    pBuffers[0] = NULL;
    pBuffers[1] = NULL;
    pd3dDevice->IASetVertexBuffers( 0, 2, pBuffers, strides, offsets );
}


//--------------------------------------------------------------------------------------
// RenderGrass
//--------------------------------------------------------------------------------------
void RenderGrass( ID3D10Device* pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXMatrixIdentity( &mWorld );

    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    D3DXMATRIX mWVP = mCamWorld * mView * mProj;

    // set vb streams
    ID3D10Buffer* pBuffers[1];
    pBuffers[0] = g_pGrassDataVB10;
    UINT strides[1];
    strides[0] = sizeof( D3DXVECTOR3 );
    UINT offsets[1] = {0};
    pd3dDevice->IASetVertexBuffers( 1, 1, pBuffers, strides, offsets );

    SetNumVisibleGrassTiles( g_NumGrassTiles );

    // set effect variables
    g_pmWorldViewProj->SetMatrix( ( float* )&mWVP );
    g_pfWorldScale->SetFloat( g_fWorldScale );
    g_pfHeightScale->SetFloat( g_fHeightScale );
    D3DXVECTOR4 vEye4( vEye, 1 );
    g_pvEyePt->SetFloatVector( ( float* )&vEye4 );
    g_pfFadeStart->SetFloat( g_fFadeStart );
    g_pfFadeEnd->SetFloat( g_fFadeEnd );

    g_ptxDiffuse->SetResource( g_pGrassTexRV );
    g_ptxHeight->SetResource( g_pHeightTexRV );
    g_ptxMask->SetResource( g_pMaskTexRV );
    g_ptxShadeNormals->SetResource( g_pShadeNormalTexRV );

    ID3D10EffectTechnique* pTechnique = g_pRenderGrass;

    pd3dDevice->IASetInputLayout( g_pGrassDecl10 );

    D3D10_TECHNIQUE_DESC techDesc;
    pTechnique->GetDesc( &techDesc );

    for( UINT p = 0; p < techDesc.Passes; p++ )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0 );

        g_Terrain.RenderGrass( &vDir, g_NumGrassTiles );
    }

    pBuffers[0] = NULL;
    pd3dDevice->IASetVertexBuffers( 1, 1, pBuffers, strides, offsets );
}


//--------------------------------------------------------------------------------------
// RenderSky
//--------------------------------------------------------------------------------------
void RenderSky( ID3D10Device* pd3dDevice )
{
    D3DXMATRIX mWorld;
    D3DXVECTOR3 vEye;
    D3DXVECTOR3 vDir;
    D3DXMATRIX mCamWorld;
    D3DXMATRIX mView;
    D3DXMATRIX mProj;

    D3DXMatrixRotationY( &mWorld, -D3DX_PI / 2.5f );
    GetCameraData( &mCamWorld, &mView, &mProj, &vEye, &vDir );
    mView._41 = mView._42 = mView._43 = 0.0f;
    D3DXMATRIX mWVP = mWorld * mCamWorld * mView * mProj;

    g_pmWorldViewProj->SetMatrix( ( float* )&mWVP );
    g_pmWorld->SetMatrix( ( float* )&mWorld );

    pd3dDevice->IASetInputLayout( g_pBasicDecl10 );
    g_SkyMesh.Render( pd3dDevice, g_pRenderSky, g_ptxDiffuse );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;

    float ClearColor[4] = { 0,0,0,0 };
    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView( pRTV, ClearColor );
    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    pd3dDevice->ClearDepthStencilView( pDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Render the scene
    {
        D3DXVECTOR4 vLightDir( -1,1,-1,1 );
        D3DXVec4Normalize( &vLightDir, &vLightDir );
        g_pvWorldLightDir->SetFloatVector( ( float* )&vLightDir );
        g_pfTime->SetFloat( ( float )fTime );
        g_pfElapsedTime->SetFloat( fElapsedTime );

        VisibilityCullTiles();

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Sky" );
        RenderSky( pd3dDevice );
        DXUT_EndPerfEvent();

        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Terrain" );
        RenderTerrain( pd3dDevice );
        DXUT_EndPerfEvent();

        if( g_bRenderBalls )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Balls" );
            RenderBalls( pd3dDevice );
            DXUT_EndPerfEvent();
        }

        if( g_bRenderGrass )
        {
            DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"Render Grass" );
            RenderGrass( pd3dDevice );
            DXUT_EndPerfEvent();
        }
		
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
        RenderText();
        V( g_HUD.OnRender( fElapsedTime ) );
        V( g_SampleUI.OnRender( fElapsedTime ) );
        DXUT_EndPerfEvent();
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10SwapChainReleasing( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_SettingsDlg.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    g_Terrain.OnDestroyDevice();
    g_BallMesh.Destroy();
    g_SkyMesh.Destroy();

    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pBasicDecl10 );
    SAFE_RELEASE( g_pBallDecl10 );
    SAFE_RELEASE( g_pGrassDecl10 );

    SAFE_RELEASE( g_pHeightTexRV );
    SAFE_RELEASE( g_pNormalTexRV );
    SAFE_RELEASE( g_pGrassTexRV );
    SAFE_RELEASE( g_pDirtTexRV );
    SAFE_RELEASE( g_pGroundGrassTexRV );
    SAFE_RELEASE( g_pMaskTexRV );
    SAFE_RELEASE( g_pShadeNormalTexRV );
    SAFE_RELEASE( g_pStreamDataVB10 );
    SAFE_RELEASE( g_pGrassDataVB10 );
}


//--------------------------------------------------------------------------------------
// Load an effect file
//--------------------------------------------------------------------------------------
HRESULT LoadEffect10( ID3D10Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    SAFE_RELEASE( g_pEffect10 );

    g_fLastWriteTime = GetFileTime( g_strEffect );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
    dwShaderFlags |= D3DXSHADER_DEBUG;

    UINT uFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY | D3D10_SHADER_DEBUG;
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Exercise02.fx" ) );
    V_RETURN( D3DX10CreateEffectFromFile( str, NULL, NULL, "fx_4_0", uFlags, 0, pd3dDevice, NULL, NULL, &g_pEffect10,
                                          NULL, NULL ) );

    // Get the effect variable handles
    g_pRenderTerrain = g_pEffect10->GetTechniqueByName( "RenderTerrain10" );
    g_pRenderBall = g_pEffect10->GetTechniqueByName( "RenderBall10" );
    g_pRenderGrass = g_pEffect10->GetTechniqueByName( "RenderGrass10" );
    g_pRenderSky = g_pEffect10->GetTechniqueByName( "RenderSky10" );

    g_pmWorldViewProj = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmViewProj = g_pEffect10->GetVariableByName( "g_mViewProj" )->AsMatrix();
    g_pmWorld = g_pEffect10->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pvWorldLightDir = g_pEffect10->GetVariableByName( "g_vWorldLightDir" )->AsVector();
    g_pfTime = g_pEffect10->GetVariableByName( "g_fTime" )->AsScalar();
    g_pfElapsedTime = g_pEffect10->GetVariableByName( "g_fElapsedTime" )->AsScalar();
    g_pvColor = g_pEffect10->GetVariableByName( "g_vColor" )->AsVector();
    g_pfWorldScale = g_pEffect10->GetVariableByName( "g_fWorldScale" )->AsScalar();
    g_pfHeightScale = g_pEffect10->GetVariableByName( "g_fHeightScale" )->AsScalar();
    g_pvEyePt = g_pEffect10->GetVariableByName( "g_vEyePt" )->AsVector();
    g_pfFadeStart = g_pEffect10->GetVariableByName( "g_fFadeStart" )->AsScalar();
    g_pfFadeEnd = g_pEffect10->GetVariableByName( "g_fFadeEnd" )->AsScalar();

    g_ptxDiffuse = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_ptxNormal = g_pEffect10->GetVariableByName( "g_txNormal" )->AsShaderResource();
    g_ptxHeight = g_pEffect10->GetVariableByName( "g_txHeight" )->AsShaderResource();
    g_ptxDirt = g_pEffect10->GetVariableByName( "g_txDirt" )->AsShaderResource();
    g_ptxGrass = g_pEffect10->GetVariableByName( "g_txGrass" )->AsShaderResource();
    g_ptxMask = g_pEffect10->GetVariableByName( "g_txMask" )->AsShaderResource();
    g_ptxShadeNormals = g_pEffect10->GetVariableByName( "g_txShadeNormals" )->AsShaderResource();

    return hr;
}
