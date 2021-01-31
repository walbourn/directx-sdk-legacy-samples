//--------------------------------------------------------------------------------------
// File: BasicHLSL9.cpp
//
// This sample shows a simple example of the Microsoft Direct3D's High-Level 
// Shader Language (HLSL) using the Effect interface. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug D3D9 vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug D3D9 pixel shaders 
#define MAX_LIGHTS 3

#define IDC_NUM_LIGHTS          6
#define IDC_NUM_LIGHTS_STATIC   7
#define IDC_ACTIVE_LIGHT        8

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern CDXUTDialogResourceManager   g_DialogResourceManager; // manager for shared resources of dialogs
extern CModelViewerCamera           g_Camera;               // A model viewing camera
extern CDXUTDirectionWidget g_LightControl[MAX_LIGHTS];
extern CD3DSettingsDlg              g_D3DSettingsDlg;       // Device settings dialog
extern CDXUTDialog                  g_HUD;                  // manages the 3D   
extern CDXUTDialog                  g_SampleUI;             // dialog for sample specific controls
extern D3DXMATRIXA16                g_mCenterMesh;
extern bool                         g_bShowHelp;            // If true, it renders the UI control text

extern float                        g_fLightScale;
extern int                          g_nNumActiveLights;
extern int                          g_nActiveLight;

// Direct3D9 resources
ID3DXFont*                          g_pFont9 = NULL;         // Font for drawing text
ID3DXSprite*                        g_pSprite9 = NULL;       // Sprite for batching draw text calls
ID3DXEffect*                        g_pEffect9 = NULL;       // D3DX effect interface
ID3DXMesh*                          g_pMesh9 = NULL;         // Mesh object
IDirect3DTexture9*                  g_pMeshTexture9 = NULL;  // Mesh texture
extern CDXUTTextHelper*             g_pTxtHelper;

D3DXHANDLE                          g_hLightDir;
D3DXHANDLE                          g_hLightDiffuse;
D3DXHANDLE                          g_hmWorldViewProjection;
D3DXHANDLE                          g_hmWorld;
D3DXHANDLE                          g_hMaterialDiffuseColor;
D3DXHANDLE                          g_hfTime;
D3DXHANDLE                          g_hnNumLights;
D3DXHANDLE                          g_hRenderSceneWithTexture1Light;
D3DXHANDLE                          g_hRenderSceneWithTexture2Light;
D3DXHANDLE                          g_hRenderSceneWithTexture3Light;

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext );
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D9LostDevice( void* pUserContext );
void CALLBACK OnD3D9DestroyDevice( void* pUserContext );
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
extern void RenderText();


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
                                      D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // No fallback defined by this app, so reject any device that doesn't support at least ps2.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
        return false;

    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont9 ) );

    // Load the mesh
    V_RETURN( LoadMesh( pd3dDevice, L"tiny\\tiny.x", &g_pMesh9 ) );

    g_SampleUI.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetVisible( true );
    g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->SetVisible( true );
    g_SampleUI.GetButton( IDC_ACTIVE_LIGHT )->SetVisible( true );

    D3DXVECTOR3* pData;
    D3DXVECTOR3 vCenter;
    FLOAT fObjectRadius;
    V( g_pMesh9->LockVertexBuffer( 0, ( LPVOID* )&pData ) );
    V( D3DXComputeBoundingSphere( pData, g_pMesh9->GetNumVertices(),
                                  D3DXGetFVFVertexSize( g_pMesh9->GetFVF() ), &vCenter, &fObjectRadius ) );
    V( g_pMesh9->UnlockVertexBuffer() );

    D3DXMatrixTranslation( &g_mCenterMesh, -vCenter.x, -vCenter.y, -vCenter.z );
    D3DXMATRIXA16 m;
    D3DXMatrixRotationY( &m, D3DX_PI );
    g_mCenterMesh *= m;
    D3DXMatrixRotationX( &m, D3DX_PI / 2.0f );
    g_mCenterMesh *= m;

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D9CreateDevice( pd3dDevice ) );
    for( int i = 0; i < MAX_LIGHTS; i++ )
        g_LightControl[i].SetRadius( fObjectRadius );

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXSHADER_NO_PRESHADER | D3DXFX_LARGEADDRESSAWARE;
#ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
#ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"BasicHLSL.fx" ) );
    V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect9, NULL ) );

    // Create the mesh texture from a file
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"tiny\\tiny_skin.dds" ) );

    V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                           D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                           D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                           NULL, NULL, &g_pMeshTexture9 ) );

    // Set effect variables as needed
    D3DXCOLOR colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
    D3DXCOLOR colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );

    D3DXHANDLE hMaterialAmbientColor = g_pEffect9->GetParameterByName( NULL, "g_MaterialAmbientColor" );
    D3DXHANDLE hMaterialDiffuseColor = g_pEffect9->GetParameterByName( NULL, "g_MaterialDiffuseColor" );
    D3DXHANDLE hMeshTexture = g_pEffect9->GetParameterByName( NULL, "g_MeshTexture" );

    V_RETURN( g_pEffect9->SetValue( hMaterialAmbientColor, &colorMtrlAmbient, sizeof( D3DXCOLOR ) ) );
    V_RETURN( g_pEffect9->SetValue( hMaterialDiffuseColor, &colorMtrlDiffuse, sizeof( D3DXCOLOR ) ) );
    V_RETURN( g_pEffect9->SetTexture( hMeshTexture, g_pMeshTexture9 ) );

    g_hLightDir = g_pEffect9->GetParameterByName( NULL, "g_LightDir" );
    g_hLightDiffuse = g_pEffect9->GetParameterByName( NULL, "g_LightDiffuse" );
    g_hmWorldViewProjection = g_pEffect9->GetParameterByName( NULL, "g_mWorldViewProjection" );
    g_hmWorld = g_pEffect9->GetParameterByName( NULL, "g_mWorld" );
    g_hMaterialDiffuseColor = g_pEffect9->GetParameterByName( NULL, "g_MaterialDiffuseColor" );
    g_hfTime = g_pEffect9->GetParameterByName( NULL, "g_fTime" );
    g_hnNumLights = g_pEffect9->GetParameterByName( NULL, "g_nNumLights" );
    g_hRenderSceneWithTexture1Light = g_pEffect9->GetTechniqueByName( "RenderSceneWithTexture1Light" );
    g_hRenderSceneWithTexture2Light = g_pEffect9->GetTechniqueByName( "RenderSceneWithTexture2Light" );
    g_hRenderSceneWithTexture3Light = g_pEffect9->GetTechniqueByName( "RenderSceneWithTexture3Light" );

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -15.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

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
        V( pMesh->CloneMeshFVF( pMesh->GetOptions(), pMesh->GetFVF() | D3DFVF_NORMAL, pd3dDevice, &pTempMesh ) );
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
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice,
                                    const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_D3DSettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont9 ) V_RETURN( g_pFont9->OnResetDevice() );
    if( g_pEffect9 ) V_RETURN( g_pEffect9->OnResetDevice() );

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite9 ) );
    g_pTxtHelper = new CDXUTTextHelper( g_pFont9, g_pSprite9, NULL, NULL, 15 );

    for( int i = 0; i < MAX_LIGHTS; i++ )
        g_LightControl[i].OnD3D9ResetDevice( pBackBufferSurfaceDesc );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 2.0f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXVECTOR3 vLightDir[MAX_LIGHTS];
    D3DXCOLOR vLightDiffuse[MAX_LIGHTS];
    UINT iPass, cPasses;
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR( 0.0f, 0.25f, 0.25f, 0.55f ), 1.0f,
                          0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Get the projection & view matrix from the camera class
        mWorld = g_mCenterMesh * *g_Camera.GetWorldMatrix();
        mProj = *g_Camera.GetProjMatrix();
        mView = *g_Camera.GetViewMatrix();

        mWorldViewProjection = mWorld * mView * mProj;

        // Render the light arrow so the user can visually see the light dir
        for( int i = 0; i < g_nNumActiveLights; i++ )
        {
            D3DXCOLOR arrowColor = ( i == g_nActiveLight ) ? D3DXCOLOR( 1, 1, 0, 1 ) : D3DXCOLOR( 1, 1, 1, 1 );
            V( g_LightControl[i].OnRender9( arrowColor, &mView, &mProj, g_Camera.GetEyePt() ) );
            vLightDir[i] = g_LightControl[i].GetLightDirection();
            vLightDiffuse[i] = g_fLightScale * D3DXCOLOR( 1, 1, 1, 1 );
        }

        V( g_pEffect9->SetValue( g_hLightDir, vLightDir, sizeof( D3DXVECTOR3 ) * MAX_LIGHTS ) );
        V( g_pEffect9->SetValue( g_hLightDiffuse, vLightDiffuse, sizeof( D3DXVECTOR4 ) * MAX_LIGHTS ) );

        // Update the effect's variables.  Instead of using strings, it would 
        // be more efficient to cache a handle to the parameter by calling 
        // ID3DXEffect::GetParameterByName
        V( g_pEffect9->SetMatrix( g_hmWorldViewProjection, &mWorldViewProjection ) );
        V( g_pEffect9->SetMatrix( g_hmWorld, &mWorld ) );

        D3DXCOLOR vWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( g_pEffect9->SetValue( g_hMaterialDiffuseColor, &vWhite, sizeof( D3DXCOLOR ) ) );
        V( g_pEffect9->SetFloat( g_hfTime, ( float )fTime ) );
        V( g_pEffect9->SetInt( g_hnNumLights, g_nNumActiveLights ) );

        // Render the scene with this technique as defined in the .fx file
        switch( g_nNumActiveLights )
        {
            case 1:
                V( g_pEffect9->SetTechnique( g_hRenderSceneWithTexture1Light ) ); break;
            case 2:
                V( g_pEffect9->SetTechnique( g_hRenderSceneWithTexture2Light ) ); break;
            case 3:
                V( g_pEffect9->SetTechnique( g_hRenderSceneWithTexture3Light ) ); break;
        }

        // Apply the technique contained in the effect and render the mesh
        V( g_pEffect9->Begin( &cPasses, 0 ) );
        for( iPass = 0; iPass < cPasses; iPass++ )
        {
            V( g_pEffect9->BeginPass( iPass ) );
            V( g_pMesh9->DrawSubset( 0 ) );
            V( g_pEffect9->EndPass() );
        }
        V( g_pEffect9->End() );

        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );

        RenderText();

        V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_D3DSettingsDlg.OnD3D9LostDevice();
    CDXUTDirectionWidget::StaticOnD3D9LostDevice();
    if( g_pFont9 ) g_pFont9->OnLostDevice();
    if( g_pEffect9 ) g_pEffect9->OnLostDevice();
    SAFE_RELEASE( g_pSprite9 );
    SAFE_DELETE( g_pTxtHelper );

}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_D3DSettingsDlg.OnD3D9DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D9DestroyDevice();
    SAFE_RELEASE( g_pEffect9 );
    SAFE_RELEASE( g_pFont9 );
    SAFE_RELEASE( g_pMesh9 );
    SAFE_RELEASE( g_pMeshTexture9 );
}

