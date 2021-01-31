//--------------------------------------------------------------------------------------
// File: DepthOfField10.1.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxut.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include <basetsd.h>

typedef enum _RenderType {

    enRenderUsingDepth,
    enRenderUsingNonMSAADepth

} RenderType;

//--------------------------------------------------------------------------------------
//  Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;                   //  A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager;    //  manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;           //  Device settings dialog
CDXUTDialog                         g_HUD;                      //  manages the 3D UI
CDXUTDialog                         g_SampleUI;                 //  dialog for sample specific controls
UINT                                g_iWidth;                   //  width of the backbuffer
UINT                                g_iHeight;                  //  height of the backbuffer
DXGI_FORMAT                         g_ColorFormat;              //  format of the backbuffer

#define NUM_MICROSCOPE_INSTANCES 6
ID3DX10Font*                        g_pFont10                   = NULL;       
ID3DX10Sprite*                      g_pSprite10                 = NULL;
CDXUTTextHelper*                    g_pTxtHelper                = NULL;
ID3D10Effect*                       g_pEffect10                 = NULL;
ID3D10InputLayout*                  g_pVertexLayout             = NULL;

ID3D10Buffer*                       g_pFullScreenVertexBuffer   = NULL;

ID3D10Texture2D*                    g_pColorTexture             = NULL;
ID3D10RenderTargetView*             g_pColorRTView              = NULL;
ID3D10ShaderResourceView*           g_pColorSRView              = NULL;

ID3D10Texture2D*                    g_pDepthStencilTexture      = NULL;
ID3D10DepthStencilView*             g_pDepthStencilDSView       = NULL;
ID3D10ShaderResourceView*           g_pDepthStencilSRView       = NULL;

ID3D10Texture2D*                    g_pMSAAColorTexture         = NULL;
ID3D10RenderTargetView*             g_pMSAAColorRTView          = NULL;
ID3D10ShaderResourceView*           g_pMSAAColorSRView          = NULL;

ID3D10Texture2D*                    g_pDepthColorTexture        = NULL;
ID3D10RenderTargetView*             g_pDepthColorRTV            = NULL;
ID3D10ShaderResourceView*           g_pDepthColorSRV            = NULL;

CDXUTSDKMesh                        g_CityMesh;
CDXUTSDKMesh                        g_HeavyMesh;
CDXUTSDKMesh                        g_ColumnMesh;

ID3D10EffectTechnique*              g_pRenderTextured           = NULL;
ID3D10EffectTechnique*              g_pRenderDepth              = NULL;
ID3D10EffectTechnique*              g_pRenderVerticiesQuad      = NULL;
ID3D10EffectTechnique*              g_pRenderVerticiesQuadMSAA = NULL;

ID3D10EffectMatrixVariable*         g_pmWorldViewProj           = NULL;
ID3D10EffectMatrixVariable*         g_mInvProj                  = NULL;

ID3D10EffectVectorVariable*         g_pvDepthInfo               = NULL;
ID3D10EffectShaderResourceVariable* g_pDiffuseTex               = NULL;
ID3D10EffectShaderResourceVariable* g_pDepthTex                 = NULL;
ID3D10EffectShaderResourceVariable* g_pDepthMSAATex             = NULL;

//
//  The focal plane depth is in View space depth from the camera.
//
FLOAT                               g_fFocalPlaneVSDepth        = 5.f;
DXGI_SAMPLE_DESC                    g_MSAASettings              = { 1, 0 };
RenderType                          g_RenderType                = enRenderUsingDepth;

typedef struct _FSVertex {

    D3DXVECTOR3         Pos;
    D3DXVECTOR3         Norm;  //  un-used
    D3DXVECTOR2         Tex;

} FSVertex;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                                   1
#define IDC_TOGGLEREF                                          3
#define IDC_CHANGEDEVICE                                       4
#define IDC_FOCALPLANE                                         5
#define IDC_TECHNIQUE_LABEL                                    6
#define IDC_TECHNIQUE                                          7
#define IDC_FOCALPLANE_LABEL                                   8
#define IDC_MULTISAMPLE_TYPE_LABEL                               9
#define IDC_MULTISAMPLE_TYPE                                   10
#define IDC_MULTISAMPLE_QUALITY_LABEL                           11 
#define IDC_MULTISAMPLE_QUALITY                                   12
#define IDC_TOGGLEWARP                                         13
//--------------------------------------------------------------------------------------
//  Called right before creating a D3D9 or D3D10 device, allowing the app to modify the 
//  device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK 
ModifyDeviceSettings( 
    DXUTDeviceSettings* pDeviceSettings, 
    void* pUserContext )
{
    //
    //  For the first device created if its a REF device, optionally display a warning dialog box
    //
    static bool s_bFirstTime = true;

    if( s_bFirstTime ) {

        s_bFirstTime = false;

        if( (DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
            (DXUT_D3D10_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE) ) {

            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    pDeviceSettings->d3d10.SyncInterval = 0;

    //
    //  Disable MSAA settings in device ui since we use render targets.  the swapchain should always
    //  be non-msaa.  To support msaa, we create the intermediate rendertargets as msaa.
    //
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT )->SetEnabled(false);
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY )->SetEnabled(false);
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL )->SetEnabled(false);
    g_D3DSettingsDlg.GetDialogControl()->GetStatic( DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL )->SetEnabled(false);

    return true;
}


//--------------------------------------------------------------------------------------
//  Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK 
OnFrameMove( 
    double fTime, 
    float fElapsedTime, 
    void* pUserContext )
{
    //
    //  Update the camera's position based on user input 
    //
    g_Camera.FrameMove( fElapsedTime );
}

//--------------------------------------------------------------------------------------
//  Create a vertex buffer which contains a full-screen quad in view space.
//  The buffer contains a texture coordinate ranging [0,1] accross the quad.
//  tThe normal is unused but is there to re-use the normal vertex declaration.
//--------------------------------------------------------------------------------------
HRESULT
SetupFullScreenQuadVertexBuffer(
    ID3D10Device* pd3dDevice )
{
    
    D3D10_BUFFER_DESC       BufDesc;
    D3D10_SUBRESOURCE_DATA  SRData;
    FSVertex                Verticies[ 4 ];
    HRESULT                 hr;

    //  
    //  setup full-screen quad vertex buffer
    //
    Verticies[ 0 ].Pos.x        = -1;
    Verticies[ 0 ].Pos.y        = -1;
    Verticies[ 0 ].Pos.z        =  1;

    Verticies[ 0 ].Tex.x        =  0;
    Verticies[ 0 ].Tex.y        =  1;

    Verticies[ 1 ].Pos.x        = -1;
    Verticies[ 1 ].Pos.y        =  1;
    Verticies[ 1 ].Pos.z        =  1;

    Verticies[ 1 ].Tex.x        =  0;
    Verticies[ 1 ].Tex.y        =  0;

    Verticies[ 2 ].Pos.x        =  1;
    Verticies[ 2 ].Pos.y        = -1;
    Verticies[ 2 ].Pos.z        =  1;

    Verticies[ 2 ].Tex.x        =  1;
    Verticies[ 2 ].Tex.y        =  1;

    Verticies[ 3 ].Pos.x        =  1;
    Verticies[ 3 ].Pos.y        =  1;
    Verticies[ 3 ].Pos.z        =  1;

    Verticies[ 3 ].Tex.x        =  1;
    Verticies[ 3 ].Tex.y        =  0;


    BufDesc.ByteWidth           = sizeof( FSVertex ) * 4;
    BufDesc.Usage               = D3D10_USAGE_DEFAULT;
    BufDesc.BindFlags           = D3D10_BIND_VERTEX_BUFFER;
    BufDesc.CPUAccessFlags      = 0;
    BufDesc.MiscFlags           = 0;

    SRData.pSysMem              = Verticies;
    SRData.SysMemPitch          = 0;
    SRData.SysMemSlicePitch     = 0;

    hr = pd3dDevice->CreateBuffer(
        &BufDesc,
        &SRData,
        &g_pFullScreenVertexBuffer );

   return hr;
}

//--------------------------------------------------------------------------------------
//  Populate the sample ui with the msaa options for the current device.
//--------------------------------------------------------------------------------------
HRESULT
SetupMSAAOptions()
{
    DXUTDeviceSettings                  ds;
    CD3D10Enumeration*                  pD3DEnum                    = NULL;
    CD3D10EnumDeviceSettingsCombo*      pDeviceSettingsCombo        = NULL;
    CDXUTComboBox*                      pMultisampleCountCombo      = NULL;
    CDXUTComboBox*                      pMultisampleQualityCombo    = NULL;
    UINT                                uMaxQuality;

    ID3D10Device1 *pdev = DXUTGetD3D10Device1();
    if( !pdev )
        return S_OK;

    //
    //  get device enumeration information.
    //   
    pD3DEnum = DXUTGetD3D10Enumeration();
    assert( pD3DEnum != NULL );
    ds = DXUTGetDeviceSettings();

    pDeviceSettingsCombo = pD3DEnum->GetDeviceSettingsCombo( 
        ds.d3d10.AdapterOrdinal,
        ds.d3d10.DriverType,
        ds.d3d10.Output,
        ds.d3d10.sd.BufferDesc.Format,
        (ds.d3d10.sd.Windowed == TRUE) );

    if( pDeviceSettingsCombo == NULL ) {
        return E_FAIL;
    }

    uMaxQuality = 0;
    pMultisampleCountCombo = g_SampleUI.GetComboBox( IDC_MULTISAMPLE_TYPE );
    pMultisampleCountCombo->RemoveAllItems();

    for( UINT uCount = 0; uCount < (UINT)pDeviceSettingsCombo->multiSampleCountList.GetSize(); ++uCount ) {

        UINT uMSAACount = pDeviceSettingsCombo->multiSampleCountList.GetAt( uCount );

        UINT r16Qual = 0;
        if ( FAILED( pdev->CheckMultisampleQualityLevels( DXGI_FORMAT_R16_UNORM, uMSAACount, &r16Qual ) ) || r16Qual == 0 )
        {
            // This sample needs any MSAA settings to be supported by DXGI_FORMAT_R16_UNORM as well as the backbuffer
            continue;
        }
        
        WCHAR str[50];
        swprintf_s( str, 50, L"%u", uMSAACount );

        if( !pMultisampleCountCombo->ContainsItem( str ) ) {
            pMultisampleCountCombo->AddItem( str, ULongToPtr( uMSAACount ) );
        }

        if ( uMSAACount == g_MSAASettings.Count )
        {
            pMultisampleCountCombo->SetSelectedByIndex( uCount );
			uMaxQuality = min( pDeviceSettingsCombo->multiSampleQualityList.GetAt( uCount ), r16Qual );
        }
    }

    pMultisampleQualityCombo = g_SampleUI.GetComboBox( IDC_MULTISAMPLE_QUALITY );
    pMultisampleQualityCombo->RemoveAllItems();

    bool set = false;
    for( UINT uQuality = 0; uQuality < uMaxQuality; ++uQuality ) {
        WCHAR strQuality[50];
        swprintf_s( strQuality, 50, L"%d", uQuality );

        if( !pMultisampleQualityCombo->ContainsItem( strQuality ) ) {
            pMultisampleQualityCombo->AddItem( strQuality, ULongToPtr(uQuality) );
        }

        if ( uQuality == g_MSAASettings.Quality )
        {
            set = true;
            pMultisampleQualityCombo->SetSelectedByIndex( uQuality );
        }
    }

    if ( !set )
    {
        pMultisampleQualityCombo->SetSelectedByIndex( 0 );
        g_MSAASettings.Quality = PtrToUlong( pMultisampleQualityCombo->GetSelectedData() );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
//  Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK 
OnD3D10CreateDevice( 
    ID3D10Device*               pd3dDevice, 
    const DXGI_SURFACE_DESC*    pBackBufferSurfaceDesc, 
    void*                       pUserContext )
{
    D3D10_PASS_DESC             PassDesc;
    WCHAR                       str[ MAX_PATH ];
    DWORD                       dwShaderFlags       = D3D10_SHADER_ENABLE_STRICTNESS;
    LPCSTR                      pszTarget;

    HRESULT                     hr;
    
    hr = g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice );
    V_RETURN( hr );

    hr = g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice );
    V_RETURN( hr );

    hr = D3DX10CreateFont( 
        pd3dDevice, 
        15, 
        0, 
        FW_BOLD, 
        1, 
        FALSE, 
        DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, 
        DEFAULT_QUALITY, 
        DEFAULT_PITCH | FF_DONTCARE, 
        L"Arial", 
        &g_pFont10 );

    V_RETURN( hr );

    hr = D3DX10CreateSprite( 
        pd3dDevice, 
        512, 
        &g_pSprite10);
    
    V_RETURN( hr );

    g_pTxtHelper = new CDXUTTextHelper( 
        NULL, 
        NULL, 
        g_pFont10, 
        g_pSprite10, 
        15 );

    //
    //  Read the D3DX effect file
    //
    #if defined( DEBUG ) || defined( _DEBUG )
        //
        //  Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
        //  Setting this flag improves the shader debugging experience, but still allows 
        //  the shaders to be optimized and to run exactly the way they will run in 
        //  the release configuration of this program.
        //
        dwShaderFlags |= D3D10_SHADER_DEBUG;
    #endif
   
    hr = DXUTFindDXSDKMediaFileCch( 
        str, 
        MAX_PATH, 
        L"DepthOfField10.1.fx" );
    
    V_RETURN( hr );

    //
    //  Check to see if we are on SP1 and have a dx10.1 device.
    //
    if( NULL == DXUTGetD3D10Device1() ) 
    {
        pszTarget = "fx_4_1";

        g_SampleUI.GetStatic( IDC_MULTISAMPLE_TYPE_LABEL )->SetEnabled( false );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_TYPE_LABEL )->SetVisible( false );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_TYPE )->SetEnabled( false );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_TYPE )->SetVisible( false );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_QUALITY_LABEL )->SetEnabled( false );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_QUALITY_LABEL )->SetVisible( false );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_QUALITY )->SetEnabled( false );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_QUALITY )->SetVisible( false );

    } 
    else 
    {
        pszTarget = "fx_4_1";

        g_SampleUI.GetStatic( IDC_MULTISAMPLE_TYPE_LABEL )->SetEnabled( true );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_TYPE_LABEL )->SetVisible( true );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_TYPE )->SetEnabled( true );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_TYPE )->SetVisible( true );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_QUALITY_LABEL )->SetEnabled( true );
        g_SampleUI.GetStatic( IDC_MULTISAMPLE_QUALITY_LABEL )->SetVisible( true );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_QUALITY )->SetEnabled( true );
        g_SampleUI.GetComboBox( IDC_MULTISAMPLE_QUALITY )->SetVisible( true );
    }
    ID3D10Blob* pErrorBlob;
    hr = D3DX10CreateEffectFromFile( 
        str, 
        NULL, 
        NULL, 
        pszTarget, 
        dwShaderFlags, 
        0, 
        pd3dDevice, 
        NULL, 
        NULL, 
        &g_pEffect10, 
        &pErrorBlob, 
        NULL );

    if( FAILED( hr ) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
    } 
    else 
    {
        SAFE_RELEASE( pErrorBlob );
    }
    V_RETURN( hr );
    
    //
    //  Obtain the technique handles
    //
    g_pRenderTextured           = g_pEffect10->GetTechniqueByName( "RenderTextured" );
    g_pRenderDepth              = g_pEffect10->GetTechniqueByName( "RenderDepth" );
    g_pRenderVerticiesQuad      = g_pEffect10->GetTechniqueByName( "RenderQuad" );
    g_pRenderVerticiesQuadMSAA  = g_pEffect10->GetTechniqueByName( "RenderQuadMSAA" );

    //
    //  Obtain the parameter handles
    //
    g_pmWorldViewProj           = g_pEffect10->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_mInvProj                  = g_pEffect10->GetVariableByName( "g_mInvProj" )->AsMatrix();
    g_pvDepthInfo               = g_pEffect10->GetVariableByName( "g_vDepth" )->AsVector();
    g_pDiffuseTex               = g_pEffect10->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pDepthTex                 = g_pEffect10->GetVariableByName( "g_txDepth" )->AsShaderResource();
    g_pDepthMSAATex             = g_pEffect10->GetVariableByName( "g_txDepthMSAA" )->AsShaderResource();

    //
    //  Define our vertex data layout
    //
    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_pRenderTextured->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    
    hr = pd3dDevice->CreateInputLayout( 
        layout, 
        3, 
        PassDesc.pIAInputSignature, 
        PassDesc.IAInputSignatureSize, 
        &g_pVertexLayout );

    V_RETURN( hr );

    //
    //  Load the Meshes
    //
    g_CityMesh.Create( 
        pd3dDevice, 
        L"MicroscopeCity\\occcity.sdkmesh" );

    g_HeavyMesh.Create( 
        pd3dDevice, 
        L"MicroscopeCity\\scanner.sdkmesh" );
    
    g_ColumnMesh.Create( 
        pd3dDevice, 
        L"MicroscopeCity\\column.sdkmesh" );

    //
    //  Setup the camera's view parameters
    //
    D3DXVECTOR3 vecEye( 0.0f, 0.8f, -2.3f );
    D3DXVECTOR3 vecAt( 0.0f, 0.0f, 0.0f );

    g_Camera.SetViewParams( 
        &vecEye, 
        &vecAt );

    //
    //  Create the full-screen vertex buffer
    //
    hr = SetupFullScreenQuadVertexBuffer( pd3dDevice );
    V_RETURN( hr );

    //
    //  Re-populate the MSAA ui based on the current device
    //
    hr = SetupMSAAOptions();
    V_RETURN( hr );

    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK 
OnD3D10SwapChainResized( 
    ID3D10Device*               pd3dDevice, 
    IDXGISwapChain*             pSwapChain, 
    const DXGI_SURFACE_DESC*    pBackBufferSurfaceDesc, 
    void*                       pUserContext )
{
    ID3D10Device1*                      pd3dDevice1;
    D3D10_TEXTURE2D_DESC                TexDesc;
    D3D10_RENDER_TARGET_VIEW_DESC       RTDesc;
    D3D10_SHADER_RESOURCE_VIEW_DESC     SRDesc;
    D3D10_DEPTH_STENCIL_VIEW_DESC       DSDesc;
    FLOAT                               fAspectRatio;

    HRESULT                             hr = S_OK;

    if ( !pd3dDevice )
        return S_OK;

    //
    //  Get the dx10.1 device, if it exists.
    //
    pd3dDevice1 = DXUTGetD3D10Device1();

    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( 
            pd3dDevice, 
            pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( 
        pd3dDevice, 
        pBackBufferSurfaceDesc ) );

    g_iWidth        = pBackBufferSurfaceDesc->Width;
    g_iHeight       = pBackBufferSurfaceDesc->Height;
    g_ColorFormat   = pBackBufferSurfaceDesc->Format;

    //
    //  Setup the camera's projection parameters
    //
    fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    
    g_Camera.SetProjParams( 
        D3DX_PI / 4, 
        fAspectRatio, 
        0.05f, 
        500.0f );

    g_Camera.SetWindow( 
        pBackBufferSurfaceDesc->Width, 
        pBackBufferSurfaceDesc->Height );

    g_Camera.SetButtonMasks( 
        MOUSE_LEFT_BUTTON, 
        MOUSE_WHEEL, 
        MOUSE_MIDDLE_BUTTON );

    //
    //  Reset the ui positions based on the current swapchain size.
    //
    g_HUD.SetLocation( 
        pBackBufferSurfaceDesc->Width - 170, 
        0 );

    g_HUD.SetSize( 
        170, 
        170 );

    g_SampleUI.SetLocation( 
        pBackBufferSurfaceDesc->Width - 170, 
        pBackBufferSurfaceDesc->Height - 400 );

    g_SampleUI.SetSize( 
        170, 
        400 );

    //
    //  Create full-screen render target and its views
    //

    TexDesc.Width                   = g_iWidth;
    TexDesc.Height                  = g_iHeight;
    TexDesc.MipLevels               = 0;
    TexDesc.ArraySize               = 1;
    TexDesc.Format                  = pBackBufferSurfaceDesc->Format;
    TexDesc.SampleDesc.Count        = 1;
    TexDesc.SampleDesc.Quality      = 0;
    TexDesc.Usage                   = D3D10_USAGE_DEFAULT;
    TexDesc.BindFlags               = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    TexDesc.CPUAccessFlags          = 0;
    TexDesc.MiscFlags               = D3D10_RESOURCE_MISC_GENERATE_MIPS;

    hr = pd3dDevice->CreateTexture2D(
        &TexDesc,
        NULL,
        &g_pColorTexture );

    V_RETURN( hr );

    if( 1 != g_MSAASettings.Count ) {
        
        //
        //  if we are using msaa, create the msaa render target
        //
        TexDesc.MipLevels           = 1;
        TexDesc.SampleDesc          = g_MSAASettings;    
        TexDesc.MiscFlags           = 0;

        hr = pd3dDevice->CreateTexture2D(
            &TexDesc,
            NULL,
            &g_pMSAAColorTexture );

        V_RETURN( hr );
    }

    if( enRenderUsingNonMSAADepth == g_RenderType ) {

        TexDesc.Format              = DXGI_FORMAT_R16_TYPELESS;
        TexDesc.SampleDesc.Count    = 1;
        TexDesc.SampleDesc.Quality  = 0;

        hr = pd3dDevice->CreateTexture2D(
            &TexDesc,
            NULL,
            &g_pDepthColorTexture );

        V_RETURN( hr );
    }

    RTDesc.Format = pBackBufferSurfaceDesc->Format;
    RTDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
    RTDesc.Texture2D.MipSlice = 0;

    hr = pd3dDevice->CreateRenderTargetView(
        g_pColorTexture,
        &RTDesc,
        &g_pColorRTView );

    V_RETURN( hr );

    SRDesc.Format = pBackBufferSurfaceDesc->Format;
    SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    SRDesc.Texture2D.MostDetailedMip = 0;
    SRDesc.Texture2D.MipLevels = 6;

    hr = pd3dDevice->CreateShaderResourceView(
        g_pColorTexture,
        &SRDesc,
        &g_pColorSRView );

    V_RETURN( hr );

    if( 1 != g_MSAASettings.Count ) {

        RTDesc.Format = pBackBufferSurfaceDesc->Format;
        RTDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2DMS;
        RTDesc.Texture2D.MipSlice = 0;

        hr = pd3dDevice->CreateRenderTargetView(
            g_pMSAAColorTexture,
            &RTDesc,
            &g_pMSAAColorRTView );

        V_RETURN( hr );

        SRDesc.Format = pBackBufferSurfaceDesc->Format;
        SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
        SRDesc.Texture2D.MostDetailedMip = 0;
        SRDesc.Texture2D.MipLevels = 1;

        hr = pd3dDevice->CreateShaderResourceView(
            g_pMSAAColorTexture,
            &SRDesc,
            &g_pMSAAColorSRView );

        V_RETURN( hr );
    }

    if( enRenderUsingNonMSAADepth == g_RenderType ) {

        RTDesc.Format = DXGI_FORMAT_R16_FLOAT;
        RTDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
        RTDesc.Texture2D.MipSlice = 0;
         
        hr = pd3dDevice->CreateRenderTargetView(
            g_pDepthColorTexture,
            &RTDesc,
            &g_pDepthColorRTV );

        V_RETURN( hr );

        SRDesc.Format = DXGI_FORMAT_R16_FLOAT;
        SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        SRDesc.Texture2D.MostDetailedMip = 0;
        SRDesc.Texture2D.MipLevels = 1;

        hr = pd3dDevice->CreateShaderResourceView(
            g_pDepthColorTexture,
            &SRDesc,
            &g_pDepthColorSRV );
        
        V_RETURN( hr );
    }

    //
    //  Create depth/stencil view or depth color buffer.
    //    
    TexDesc.Width               = g_iWidth;
    TexDesc.Height              = g_iHeight;
    TexDesc.MipLevels           = 1;
    TexDesc.ArraySize           = 1;
    TexDesc.Format              = DXGI_FORMAT_R16_TYPELESS;
    TexDesc.SampleDesc          = g_MSAASettings;
    TexDesc.Usage               = D3D10_USAGE_DEFAULT;
    TexDesc.CPUAccessFlags      = 0;
    TexDesc.MiscFlags           = 0;

    if( enRenderUsingDepth == g_RenderType ) { 
        TexDesc.BindFlags       = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;
    } else {
        TexDesc.BindFlags       = D3D10_BIND_DEPTH_STENCIL;
    }

    hr = pd3dDevice->CreateTexture2D(
        &TexDesc,
        NULL,
        &g_pDepthStencilTexture );

    V_RETURN( hr );

    DSDesc.Format = DXGI_FORMAT_D16_UNORM;

    if( 1 == g_MSAASettings.Count ) {
        DSDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    } else {
        DSDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    }

    DSDesc.Texture2D.MipSlice = 0;

    hr = pd3dDevice->CreateDepthStencilView(
        g_pDepthStencilTexture,
        &DSDesc,
        &g_pDepthStencilDSView );

    V_RETURN( hr );
 
    //
    //  Create a shader view if we are using the depth buffer directly.
    //
    if( enRenderUsingDepth == g_RenderType ) {

        if( 1 == g_MSAASettings.Count ) 
        {    
            SRDesc.Format = DXGI_FORMAT_R16_UNORM;
            SRDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
            SRDesc.Texture2D.MostDetailedMip = 0;
            SRDesc.Texture2D.MipLevels = 1;

            hr = pd3dDevice->CreateShaderResourceView(
                g_pDepthStencilTexture,
                &SRDesc,
                &g_pDepthStencilSRView );
            
            V_RETURN( hr );

        } 
        else if( NULL != pd3dDevice1 ) 
        {
            //
            //  the depth/stencil buffer is msaa so we need dx10.1 to allow
            //  us to create a shader resource view.
            //
            D3D10_SHADER_RESOURCE_VIEW_DESC1    SRDesc1;
            ID3D10ShaderResourceView1*          pSRView1 = NULL;              
                
            SRDesc1.Format = DXGI_FORMAT_R16_UNORM;
            SRDesc1.ViewDimension = D3D10_1_SRV_DIMENSION_TEXTURE2DMS;

            hr = pd3dDevice1->CreateShaderResourceView1(
                g_pDepthStencilTexture,
                &SRDesc1,
                &pSRView1 );

            V_RETURN( hr );

            //
            //  Get the original shader resource view interface from the dx10.1 interface
            //
            hr = pSRView1->QueryInterface(
                __uuidof( ID3D10ShaderResourceView ),
                (LPVOID*)&g_pDepthStencilSRView );

            pSRView1->Release();

            V_RETURN( hr );
        } 
        else 
        {
            //
            //  We cannot create a shader resource view in dx10.0 so we cannot
            //  render using the depth information directly.
            //
            V_RETURN( E_FAIL );
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void 
RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );  
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
//  Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK 
OnD3D10FrameRender( 
    ID3D10Device*   pd3dDevice, 
    DOUBLE          fTime, 
    FLOAT           fElapsedTime, 
    void*           pUserContext )
{
    //
    // get the back-buffer target views
    ID3D10RenderTargetView*     pRTView             = DXUTGetD3D10RenderTargetView();
    ID3D10DepthStencilView*     pDSView             = DXUTGetD3D10DepthStencilView();
    
    const FLOAT                 fClearColor[ 4 ]    = { 0.9569f, 0.9569f, 1.0f, 0.0f };
    const FLOAT                 fDepthInfo[ 4 ]     = { 0., (FLOAT)g_iWidth, (FLOAT)g_iHeight, g_fFocalPlaneVSDepth };

    const UINT                  uOffset             = 0;
    const UINT                  uStride             = sizeof( FSVertex );

    D3DXMATRIX                  mWorld;
    D3DXMATRIX                  mView;
    D3DXMATRIX                  mProj;
    D3DXMATRIX                  mInvProj;
    D3DXMATRIX                  mWorldViewProj;
    
    //
    //  If the settings dialog is being shown, then render it instead of rendering the app's scene
    //
    if( g_D3DSettingsDlg.IsActive() ) {

        //
        //  Clear and Set the backbuffer rendertarget.
        //
        pd3dDevice->ClearRenderTargetView( 
            pRTView, 
            fClearColor );

        pd3dDevice->OMSetRenderTargets(
            1,
            &pRTView,
            NULL );

        g_D3DSettingsDlg.OnRender( fElapsedTime );
        
        return;
    }
 
    //
    //  Clear and set the render targets based on the user selections
    //
    ID3D10ShaderResourceView *pNullView = NULL; 
    pd3dDevice->PSSetShaderResources( 1, 1, &pNullView  );
    if( 1 == g_MSAASettings.Count ) 
    {
        pd3dDevice->ClearRenderTargetView( 
            g_pColorRTView, 
            fClearColor );

        pd3dDevice->ClearDepthStencilView( 
            g_pDepthStencilDSView, 
            D3D10_CLEAR_DEPTH, 
            1.0, 
            0 );

        pd3dDevice->OMSetRenderTargets(
            1,
            &g_pColorRTView,
            g_pDepthStencilDSView );

    } 
    else 
    {
        pd3dDevice->ClearRenderTargetView( 
            g_pMSAAColorRTView, 
            fClearColor );

        pd3dDevice->ClearDepthStencilView( 
            g_pDepthStencilDSView, 
            D3D10_CLEAR_DEPTH, 
            1.0, 
            0 );

        pd3dDevice->OMSetRenderTargets(
            1,
            &g_pMSAAColorRTView,
            g_pDepthStencilDSView );
    }

    mWorld = *g_Camera.GetWorldMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mView = *g_Camera.GetViewMatrix();
    mWorldViewProj = mWorld*mView*mProj;

    D3DXMatrixInverse(
        &mInvProj,
        NULL,
        &mProj );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    g_pmWorldViewProj->SetMatrix( (float*)&mWorldViewProj );
    g_mInvProj->SetMatrix( (float*)&mInvProj );

    //
    //  Render the city
    //
    g_CityMesh.Render( 
        pd3dDevice, 
        g_pRenderTextured, 
        g_pDiffuseTex );

    g_ColumnMesh.Render( 
        pd3dDevice, 
        g_pRenderTextured, 
        g_pDiffuseTex );

    //
    //  Render the microscopes themselves if the bounding volumes rendered
    //
    for( UINT uIdx = 0; uIdx < NUM_MICROSCOPE_INSTANCES; ++uIdx ) 
    {
        D3DXMATRIX mMatRot;
        D3DXMATRIX mWVP;
        
        D3DXMatrixRotationY( 
            &mMatRot, 
            uIdx * ( D3DX_PI / 3.0f ) );
        
        mWVP = mMatRot*mWorldViewProj;
        g_pmWorldViewProj->SetMatrix( (float*)&mWVP );

        g_HeavyMesh.Render( 
            pd3dDevice, 
            g_pRenderTextured, 
            g_pDiffuseTex );
    }

    if( 1 != g_MSAASettings.Count ) 
    {
    
        //
        //  Resolve the color buffer to use as the color source for the 
        //  final, blurred result
        //
        pd3dDevice->ResolveSubresource(
            g_pColorTexture,
            0,
            g_pMSAAColorTexture,
            0,
            g_ColorFormat );
    } 

    if( enRenderUsingNonMSAADepth == g_RenderType ) 
    {
        //
        //  Re-render the scene writing the depth value to the depth texture.
        //  
        const FLOAT fClearColor2[ 4 ] = { 1.f, 1.f, 1.f, 1.f };

        pd3dDevice->ClearRenderTargetView(
            g_pDepthColorRTV,
            fClearColor2 );

        pd3dDevice->ClearDepthStencilView( 
            pDSView, 
            D3D10_CLEAR_DEPTH, 
            1.0, 
            0 );

        pd3dDevice->OMSetRenderTargets(
            1,
            &g_pDepthColorRTV,
            pDSView );

        g_pmWorldViewProj->SetMatrix( (float*)&mWorldViewProj );

        //
        //  Render the city
        //
        g_CityMesh.Render( 
            pd3dDevice, 
            g_pRenderDepth, 
            g_pDiffuseTex );

        g_ColumnMesh.Render( 
            pd3dDevice, 
            g_pRenderDepth, 
            g_pDiffuseTex );

        //
        //  Render the microscopes themselves if the bounding volumes rendered
        //
        for( UINT uIdx = 0; uIdx < NUM_MICROSCOPE_INSTANCES; ++uIdx ) 
        {
            D3DXMATRIX mMatRot;
            D3DXMATRIX mWVP;
            
            D3DXMatrixRotationY( 
                &mMatRot, 
                uIdx * ( D3DX_PI / 3.0f ) );
            
            mWVP = mMatRot*mWorldViewProj;
            g_pmWorldViewProj->SetMatrix( (float*)&mWVP );

            g_HeavyMesh.Render( 
                pd3dDevice, 
                g_pRenderDepth, 
                g_pDiffuseTex );
        }
    }

    //
    //  Set swap chain render targets to draw to the screen
    //
    pd3dDevice->OMSetRenderTargets(
        1,
        &pRTView,
        pDSView );

    //
    //  Create the mips so we can sample the levels based on distance from
    //  the viewer to approximate a blur effect.
    //
    pd3dDevice->GenerateMips(
        g_pColorSRView );

    g_pDiffuseTex->SetResource(
        g_pColorSRView );

     
    g_pvDepthInfo->SetFloatVector( (FLOAT*)fDepthInfo );

    pd3dDevice->IASetVertexBuffers(
        0,
        1,
        &g_pFullScreenVertexBuffer,
        &uStride,
        &uOffset );

    if( enRenderUsingNonMSAADepth == g_RenderType ) 
    {
        g_pDepthTex->SetResource( g_pDepthColorSRV );
    } 
    else
    {
        //
        //  Set the depth surface into the appropriate 
        //
        if( 1 == g_MSAASettings.Count )
            g_pDepthTex->SetResource( g_pDepthStencilSRView );
        else
            g_pDepthMSAATex->SetResource( g_pDepthStencilSRView );
    }
   
    //
    //  Apply the post-processing effect
    //
    if( 1 == g_MSAASettings.Count )
        g_pRenderVerticiesQuad->GetPassByIndex( 0 )->Apply( 0 );
    else
        g_pRenderVerticiesQuadMSAA->GetPassByIndex( 0 )->Apply( 0 );
    
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pd3dDevice->Draw(
        4,
        0 );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_SampleUI.OnRender( fElapsedTime );
    g_HUD.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}


//--------------------------------------------------------------------------------------
//  Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK 
OnD3D10ReleasingSwapChain( 
    void* pUserContext )
{
    SAFE_RELEASE( g_pColorRTView );
    SAFE_RELEASE( g_pColorSRView );

    SAFE_RELEASE( g_pColorRTView );
    SAFE_RELEASE( g_pColorSRView );
    SAFE_RELEASE( g_pColorTexture );

    SAFE_RELEASE( g_pDepthStencilTexture );
    SAFE_RELEASE( g_pDepthStencilDSView );
    SAFE_RELEASE( g_pDepthStencilSRView );

    SAFE_RELEASE( g_pMSAAColorTexture );
    SAFE_RELEASE( g_pMSAAColorRTView );
    SAFE_RELEASE( g_pMSAAColorSRView );

    SAFE_RELEASE( g_pDepthColorTexture );
    SAFE_RELEASE( g_pDepthColorRTV );
    SAFE_RELEASE( g_pDepthColorSRV );

    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
//  Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK 
OnD3D10DestroyDevice( 
    void* pUserContext )
{
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pFont10 );
    SAFE_RELEASE( g_pSprite10 );
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect10 );
    SAFE_RELEASE( g_pVertexLayout );

    SAFE_RELEASE( g_pFullScreenVertexBuffer );

    g_CityMesh.Destroy();
    g_HeavyMesh.Destroy();
    g_ColumnMesh.Destroy();
}

//--------------------------------------------------------------------------------------
//  Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK 
OnGUIEvent( 
    UINT            nEvent, 
    INT             nControlID, 
    CDXUTControl*   pControl, 
    void*           pUserContext )
{   
    BOOL bResetDevice = FALSE;

    switch( nControlID )
    {
        case IDC_TOGGLEWARP: DXUTToggleWARP(); break;
        case IDC_TOGGLEFULLSCREEN: DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:        DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:     g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_FOCALPLANE: {

            CDXUTSlider* pSlider = (CDXUTSlider*)pControl;

            g_fFocalPlaneVSDepth = (FLOAT)pSlider->GetValue() / 100.f;
            
            break;
        }
        case IDC_MULTISAMPLE_TYPE: {

            CDXUTComboBox* pBox = (CDXUTComboBox*)pControl;
            g_MSAASettings.Count = PtrToUlong( pBox->GetSelectedData() );

            bResetDevice = TRUE;
            break;
        }
        case IDC_MULTISAMPLE_QUALITY: {

            CDXUTComboBox* pBox = (CDXUTComboBox*)pControl;
            g_MSAASettings.Quality = PtrToUlong( pBox->GetSelectedData() );

            bResetDevice = TRUE;
            break;
        }
        case IDC_TECHNIQUE: {

            CDXUTComboBox* pBox = (CDXUTComboBox*)pControl;
            g_RenderType = (RenderType)PtrToUlong( pBox->GetSelectedData() );

            bResetDevice = TRUE;
            break;
        }
    }    

    //
    //  If any options affecting our render targets formats have occured, recreate them.
    //
    if( bResetDevice )
    {
        SetupMSAAOptions();

        OnD3D10ReleasingSwapChain( NULL );
        
        OnD3D10SwapChainResized(
            DXUTGetD3D10Device(),
            DXUTGetDXGISwapChain(),
            DXUTGetDXGIBackBufferSurfaceDesc(),
            NULL );
    }
}

//--------------------------------------------------------------------------------------
//  Initialize the app.
//  Setup the gui call backs and add our custom sample controls.
//--------------------------------------------------------------------------------------
void 
InitApp()
{
    CDXUTComboBox*  pTechniqueBox   = NULL;

    int iY = 10;

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); 
    
    //
    //  Add default HUD buttons.
    //
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 35, iY += 24, 125, 22, VK_F4 );

    //
    //  Create the sample ui controls to update our sample parameters.
    //
    iY = 0;    

    g_SampleUI.AddStatic( 
        IDC_FOCALPLANE_LABEL, 
        L"Focal Plane Depth", 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddSlider( 
        IDC_FOCALPLANE, 
        35, 
        iY += 24, 
        125, 
        22, 
        0, 
        (INT)g_fFocalPlaneVSDepth * 200, 
        (INT)g_fFocalPlaneVSDepth * 100 );

    g_SampleUI.AddStatic( 
        IDC_MULTISAMPLE_TYPE_LABEL, 
        L"Multisample Type", 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddComboBox( 
        IDC_MULTISAMPLE_TYPE, 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddStatic( 
        IDC_MULTISAMPLE_QUALITY_LABEL, 
        L"Multisample Quality", 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddComboBox( 
        IDC_MULTISAMPLE_QUALITY, 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddStatic( 
        IDC_TECHNIQUE_LABEL, 
        L"Render Technique", 
        35, 
        iY += 24, 
        125, 
        22 );

    g_SampleUI.AddComboBox( 
        IDC_TECHNIQUE, 
        35, 
        iY += 24, 
        125, 
        22, 
        0, 
        false, 
        &pTechniqueBox );

    pTechniqueBox->AddItem(
        L"Bind Depth Buffer",
        ULongToPtr( enRenderUsingDepth ) );

    pTechniqueBox->AddItem(
        L"Depth Color Buffer",
        ULongToPtr( enRenderUsingNonMSAADepth ) );
    
    g_SampleUI.SetCallback( OnGUIEvent );  
}


//--------------------------------------------------------------------------------------
//  Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK 
MsgProc( 
    HWND        hWnd, 
    UINT        uMsg, 
    WPARAM      wParam, 
    LPARAM      lParam, 
    bool*       pbNoFurtherProcessing, 
    void*       pUserContext )
{
    //
    //  Pass messages to dialog resource manager calls so GUI state is updated correctly
    //
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing ) {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() ) {

        g_D3DSettingsDlg.MsgProc( 
            hWnd, 
            uMsg, 
            wParam, 
            lParam );

        return 0;
    }

    //
    //  Give the dialogs a chance to handle the message first
    //
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing ) {
        return 0;
    }

    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing ) {
        return 0;
    }

    //
    //  Pass all remaining windows messages to camera so it can respond to user input
    //
    g_Camera.HandleMessages( 
        hWnd, 
        uMsg, 
        wParam, 
        lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
//  Entry point to the program. Initializes everything and goes into a message 
//  processing loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI 
wWinMain( 
    HINSTANCE   hInstance, 
    HINSTANCE   hPrevInstance, 
    LPWSTR      lpCmdLine, 
    int         nCmdShow )
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
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10SwapChainResized );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DepthOfField10.1" );
    DXUTCreateDevice( true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}