//--------------------------------------------------------------------------------------
// File: MagnifyTool.cpp
//
// MagnifyTool class implementation. This class implements a user interface based upon the DXUT framework,
// for the Magnify class. It exposes all of the functionality. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTgui.h"
#include "Sprite.h"
#include "Magnify.h"
#include "MagnifyTool.h"

//===============================================================================================================================//
//
// Constructor(s) / Destructor(s) Block
//
//===============================================================================================================================//

MagnifyTool::MagnifyTool()
{
    m_pSourceRTResource = NULL;
    m_bReleaseRTOnResize = false;
    m_RTFormat = DXGI_FORMAT_UNKNOWN;
    m_nWidth = 0;
    m_nHeight = 0;
    m_nSamples = 0;

    m_pSourceDepthResource = NULL;
    m_bReleaseDepthOnResize = false;
    m_DepthFormat = DXGI_FORMAT_UNKNOWN;

    m_pDepthStencil = NULL;
    m_pDSV = NULL;
    m_pRState = NULL;

    m_nSubSampleIndex = 0;
}

MagnifyTool::~MagnifyTool()
{
}

//===============================================================================================================================//
//
// Public functions
//
//===============================================================================================================================//

void MagnifyTool::SetSourceResources( ID3D10Resource* pSourceRTResource, DXGI_FORMAT RTFormat, 
        ID3D10Resource* pSourceDepthResource, DXGI_FORMAT DepthFormat, int nWidth, 
        int nHeight, int nSamples )
{
    assert( ( NULL != pSourceRTResource ) || ( NULL != pSourceDepthResource ) );

    SAFE_RELEASE( m_pSourceRTResource );
    SAFE_RELEASE( m_pSourceDepthResource );

    m_bReleaseRTOnResize = false;
    m_bReleaseDepthOnResize = false;

    ID3D10Resource* pTempResource;

    DXUTGetD3D10RenderTargetView()->GetResource( &pTempResource );
    if( pTempResource == pSourceRTResource )
    {
        m_bReleaseRTOnResize = true;
    }
    SAFE_RELEASE( pTempResource );

    if( m_pDepthStencil == pSourceDepthResource )
    {
        m_bReleaseDepthOnResize = true;
    }
    	
    m_pSourceRTResource = pSourceRTResource;
    m_RTFormat = RTFormat;
    m_nWidth = nWidth;
    m_nHeight = nHeight;
    m_nSamples = nSamples;

    m_pSourceDepthResource = pSourceDepthResource;
    m_DepthFormat = DepthFormat;

    if( NULL == m_pSourceRTResource )
    {
        EnableDepthUI();
        ForceDepthUI();
    }

    if( NULL == m_pSourceDepthResource )
    {
        DisableDepthUI();
    }

    ID3D10Device1* pd3dDevice1 = DXUTGetD3D10Device1();
    if( ( NULL == pd3dDevice1 ) && ( m_nSamples > 1 ) )
    {
        DisableDepthUI();
    }
	
    if( m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->GetChecked() )
    {
        if( NULL != pSourceDepthResource )
        {
            m_Magnify.SetSourceResource( m_pSourceDepthResource, m_DepthFormat, m_nWidth, m_nHeight, m_nSamples );
        }
    }
    else
    {
        if( NULL != pSourceRTResource )
        {
            m_Magnify.SetSourceResource( m_pSourceRTResource, m_RTFormat, m_nWidth, m_nHeight, m_nSamples );
        }
    }
}

void MagnifyTool::InitApp( CDXUTDialogResourceManager* pResourceManager )
{
    D3DCOLOR DlgColor = 0x88888888;

    m_MagnifyUI.Init( pResourceManager );

    int iY = 0;

    m_MagnifyUI.SetBackgroundColors( DlgColor );
    m_MagnifyUI.EnableCaption( true );
    m_MagnifyUI.SetCaptionText( L"-- Magnify Tool --" );

    m_MagnifyUI.AddCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE, L"Magnify: RMouse", 5, iY, 140, 24, true );

    m_MagnifyUI.AddStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION, L"Pixel Region : 128", 5, iY += 25, 90, 24 );
    m_MagnifyUI.AddSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION, 15, iY += 25, 140, 24, 16, 256, 128, false );

    m_MagnifyUI.AddStatic( IDC_MAGNIFY_STATIC_SCALE, L"Scale : 5", 5, iY += 25, 50, 24 );
    m_MagnifyUI.AddSlider( IDC_MAGNIFY_SLIDER_SCALE, 15, iY += 25, 140, 24, 2, 10, 5, false );

    m_MagnifyUI.AddCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH, L"View Depth", 5, iY += 25, 140, 24, false );

    m_MagnifyUI.AddStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN, L"Depth Min : 0.0f", 5, iY += 25, 90, 24 );
    m_MagnifyUI.AddSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN, 15, iY += 25, 140, 24, 0, 100, 0, false );

    m_MagnifyUI.AddStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX, L"Depth Max : 1.0f", 5, iY += 25, 90, 24 );
    m_MagnifyUI.AddSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX, 15, iY += 25, 140, 24, 0, 100, 100, false );

    m_MagnifyUI.AddCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES, L"Subsamples: MWheel", 5, iY += 25, 140, 24, false );

    EnableTool();
    EnableUI();
}

HRESULT MagnifyTool::OnCreateDevice( ID3D10Device* pd3dDevice )
{
    HRESULT hr;

    assert( NULL != pd3dDevice );

    hr = m_Magnify.OnCreateDevice( pd3dDevice );
    assert( D3D_OK == hr );

    return hr;
}

void MagnifyTool::OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, 
                                        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext, 
                                        int nPositionX, int nPositionY )
{
    CreateDepthStencil( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext );

    m_MagnifyUI.SetLocation( nPositionX, nPositionY );
    m_MagnifyUI.SetSize( 170, 155 );

    m_Magnify.OnResizedSwapChain( DXUTGetDXGIBackBufferSurfaceDesc() );

    if( m_bReleaseRTOnResize )
    {
        D3D10_RENDER_TARGET_VIEW_DESC RTDesc;
        DXUTGetD3D10RenderTargetView()->GetResource( &m_pSourceRTResource );
        DXUTGetD3D10RenderTargetView()->GetDesc( &RTDesc );
        m_RTFormat = RTDesc.Format;
        m_nWidth = DXUTGetDXGIBackBufferSurfaceDesc()->Width;
        m_nHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        m_nSamples = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count;

        if( !m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->GetChecked() )
        {
            m_Magnify.SetSourceResource( m_pSourceRTResource, m_RTFormat, m_nWidth, m_nHeight, m_nSamples );
        }
    }

    if( m_bReleaseDepthOnResize )
    {
        D3D10_DEPTH_STENCIL_VIEW_DESC DSDesc;
        m_pDSV->GetResource( &m_pSourceDepthResource );
        m_pDSV->GetDesc( &DSDesc );
        m_DepthFormat = DSDesc.Format;
        m_nWidth = DXUTGetDXGIBackBufferSurfaceDesc()->Width;
        m_nHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        m_nSamples = DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count;

        if( m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->GetChecked() )
        {
            m_Magnify.SetSourceResource( m_pSourceDepthResource, m_DepthFormat, m_nWidth, m_nHeight, m_nSamples );
        }
    }

    EnableTool();

    if( m_nSamples > 1 )
    {
        EnableSubSampleUI();
    }
    else
    {
        DisableSubSampleUI();
    }

    ID3D10Device1* pd3dDevice1 = DXUTGetD3D10Device1();
    if( ( NULL == pd3dDevice1 ) && ( m_nSamples > 1 ) )
    {
        DisableDepthUI();

        if( NULL == m_pSourceRTResource )
        {
            DisableUI();
            DisableTool();
        }
        else
        {
            m_Magnify.SetSourceResource( m_pSourceRTResource, m_RTFormat, m_nWidth, m_nHeight, m_nSamples );
        }
    }
    else
    {
        if( NULL != m_pSourceDepthResource )
        {
            EnableDepthUI();
        }
    }
}

void MagnifyTool::Render()
{ 
    if( m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetEnabled() &&
        m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetChecked() )
    {
        if( DXUTWasKeyPressed( VK_UP ) )
        {
            m_nSubSampleIndex = ( ( m_nSubSampleIndex ) < ( m_nSamples - 1 ) ) ? ( m_nSubSampleIndex + 1 ) : ( m_nSubSampleIndex );
            m_Magnify.SetSubSampleIndex( m_nSubSampleIndex );
        }

        if( DXUTWasKeyPressed( VK_DOWN ) )
        {
            m_nSubSampleIndex = ( ( m_nSubSampleIndex ) > ( 0 ) ) ? ( m_nSubSampleIndex - 1 ) : ( m_nSubSampleIndex );
            m_Magnify.SetSubSampleIndex( m_nSubSampleIndex );
        }

        m_Magnify.RenderBackground();

        if( DXUTIsMouseButtonDown( VK_RBUTTON ) )
        {
            m_Magnify.Capture();
            m_Magnify.RenderMagnifiedRegion();
        }
    }
}

void MagnifyTool::OnDestroyDevice()
{
    m_Magnify.OnDestroyDevice();
}
	
void MagnifyTool::OnReleasingSwapChain()
{
    if( m_bReleaseRTOnResize )
    {
        SAFE_RELEASE( m_pSourceRTResource );
    }

    if( m_bReleaseDepthOnResize )
    {
        SAFE_RELEASE( m_pSourceDepthResource );
    }

    SAFE_RELEASE( m_pDepthStencil );
    SAFE_RELEASE( m_pDSV );
    SAFE_RELEASE( m_pRState );
}

void MagnifyTool::ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d10.AutoCreateDepthStencil = false;
}

void MagnifyTool::OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];
    int nTemp;
    bool bChecked;
    float fTemp;

    switch( nControlID )
    {
        case IDC_MAGNIFY_CHECKBOX_ENABLE:
            bChecked = ((CDXUTCheckBox*)pControl)->GetChecked();
            if( bChecked )
            {
                EnableUI();

                if( NULL == m_pSourceRTResource )
                {
                    ForceDepthUI();
                }

                if( NULL == m_pSourceDepthResource )
                {
                    DisableDepthUI();
                }

                ID3D10Device1* pd3dDevice1 = DXUTGetD3D10Device1();
                if( ( NULL == pd3dDevice1 ) && ( m_nSamples > 1 ) )
                {
                    DisableDepthUI();
                }

                if( m_nSamples == 1 )
                {
                    DisableSubSampleUI();
                }
            }
            else
            {
                DisableUI();
            }
            break;

        case IDC_MAGNIFY_SLIDER_PIXEL_REGION:
            nTemp = m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION )->GetValue();
            if( nTemp % 2 )
            {
                nTemp++;
            }
            swprintf_s( szTemp, L"Pixel Region : %d", nTemp );
            m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION )->SetText( szTemp );
            m_Magnify.SetPixelRegion( nTemp );
            break;

        case IDC_MAGNIFY_SLIDER_SCALE:
            nTemp = m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_SCALE )->GetValue();
            swprintf_s( szTemp, L"Scale : %d", nTemp );
            m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_SCALE )->SetText( szTemp );
            m_Magnify.SetScale( nTemp );
            break;

        case IDC_MAGNIFY_CHECKBOX_DEPTH:
            bChecked = ((CDXUTCheckBox*)pControl)->GetChecked();
            if( bChecked )
            {
                m_Magnify.SetSourceResource( m_pSourceDepthResource, m_DepthFormat, m_nWidth, m_nHeight, m_nSamples );
            }
            else
            {
                m_Magnify.SetSourceResource( m_pSourceRTResource, m_RTFormat, m_nWidth, m_nHeight, m_nSamples );
            }
            break;

        case IDC_MAGNIFY_SLIDER_DEPTH_MIN:
            fTemp = (float)m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->GetValue();
            fTemp /= 100.0f;
            swprintf_s( szTemp, L"Depth Min : %.2f", fTemp );
            m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetText( szTemp );
            m_Magnify.SetDepthRangeMin( fTemp );
            break;

        case IDC_MAGNIFY_SLIDER_DEPTH_MAX:
            fTemp = (float)m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->GetValue();
            fTemp /= 100.0f;
            swprintf_s( szTemp, L"Depth Max : %.2f", fTemp );
            m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetText( szTemp );
            m_Magnify.SetDepthRangeMax( fTemp );
            break;
	}
}

//===============================================================================================================================//
//
// Protected functions
//
//===============================================================================================================================//

//===============================================================================================================================//
//
// Private functions
//
//===============================================================================================================================//

void MagnifyTool::CreateDepthStencil( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, 
		const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    DXGI_FORMAT DepthFormat;
    HRESULT hr;

    SAFE_RELEASE( m_pDepthStencil );
    SAFE_RELEASE( m_pDSV );
    SAFE_RELEASE( m_pRState );

    switch( DXUTGetDeviceSettings().d3d10.AutoDepthStencilFormat )
    {
        case DXGI_FORMAT_D32_FLOAT:
            DepthFormat = DXGI_FORMAT_R32_TYPELESS;
            break;

        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            DepthFormat = DXGI_FORMAT_R24G8_TYPELESS;
            break;

        case DXGI_FORMAT_D16_UNORM:
            DepthFormat = DXGI_FORMAT_R16_TYPELESS;
            break;

        default:
            DepthFormat = DXGI_FORMAT_UNKNOWN;
            break;
    }
    assert( DepthFormat != DXGI_FORMAT_UNKNOWN );

    // Create depth stencil texture
    D3D10_TEXTURE2D_DESC descDepth;
    descDepth.Width = pBackBufferSurfaceDesc->Width;
    descDepth.Height = pBackBufferSurfaceDesc->Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DepthFormat;
    descDepth.SampleDesc.Count = DXUTGetDeviceSettings().d3d10.sd.SampleDesc.Count;
    descDepth.SampleDesc.Quality = DXUTGetDeviceSettings().d3d10.sd.SampleDesc.Quality;
    descDepth.Usage = D3D10_USAGE_DEFAULT;
    descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;

    if( ( descDepth.SampleDesc.Count == 1 ) || DXUTGetD3D10Device1() )
    {
        descDepth.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
    }

    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = pd3dDevice->CreateTexture2D( &descDepth, NULL, &m_pDepthStencil );
    assert( D3D_OK == hr );
       
    // Create the depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
    descDSV.Format = DXUTGetDeviceSettings().d3d10.AutoDepthStencilFormat;
    if( descDepth.SampleDesc.Count > 1 )
    {
        descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    }
    else
    {
        descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    }
    descDSV.Texture2D.MipSlice = 0;

    hr = pd3dDevice->CreateDepthStencilView( m_pDepthStencil, &descDSV, &m_pDSV );
    assert( D3D_OK == hr );
      
    // Create a default rasterizer state that enables MSAA
    D3D10_RASTERIZER_DESC RSDesc;
    RSDesc.FillMode = D3D10_FILL_SOLID;
    RSDesc.CullMode = D3D10_CULL_BACK;
    RSDesc.FrontCounterClockwise = FALSE;
    RSDesc.DepthBias = 0;
    RSDesc.SlopeScaledDepthBias = 0.0f;
    RSDesc.DepthBias = 0;
    RSDesc.DepthClipEnable = TRUE;
    RSDesc.ScissorEnable = FALSE;
    RSDesc.AntialiasedLineEnable = FALSE;
    if( descDepth.SampleDesc.Count > 1 )
    {
        RSDesc.MultisampleEnable = TRUE;
    }
    else
    {
        RSDesc.MultisampleEnable = FALSE;
    }

    hr = pd3dDevice->CreateRasterizerState( &RSDesc, &m_pRState );
    assert( D3D_OK == hr );

    pd3dDevice->RSSetState( m_pRState );

    // Get the current render and depth targets, so we can later revert to these.
    ID3D10RenderTargetView *pRenderTargetView;
    DXUTGetD3D10Device()->OMGetRenderTargets( 1, &pRenderTargetView, NULL );

    // Bind our render target view to the OM stage.
    DXUTGetD3D10Device()->OMSetRenderTargets( 1, (ID3D10RenderTargetView* const*)&pRenderTargetView, m_pDSV );

    // Decrement the counter on these resources
    SAFE_RELEASE( pRenderTargetView );
}

void MagnifyTool::DisableTool()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->SetEnabled( false );
}

void MagnifyTool::EnableTool()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->SetEnabled( true );
}

void MagnifyTool::DisableUI()
{
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION )->SetEnabled( false );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION )->SetEnabled( false );	
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_SCALE )->SetEnabled( false );		
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_SCALE )->SetEnabled( false );		
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetEnabled( false );		
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetEnabled( false );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetEnabled( false );	
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetEnabled( false );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetEnabled( false );
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetEnabled( false );	
}

void MagnifyTool::EnableUI()
{
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION )->SetEnabled( true );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION )->SetEnabled( true );	
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_SCALE )->SetEnabled( true );		
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_SCALE )->SetEnabled( true );		
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetEnabled( true );		
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetEnabled( true );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetEnabled( true );	
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetEnabled( true );	
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetEnabled( true );	
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetEnabled( true );
}

void MagnifyTool::DisableDepthUI()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetEnabled( false );
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetEnabled( false );
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetEnabled( false );
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetEnabled( false );
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetEnabled( false );
}

void MagnifyTool::EnableDepthUI()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetEnabled( true );
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MIN )->SetEnabled( true );
    m_MagnifyUI.GetStatic( IDC_MAGNIFY_STATIC_DEPTH_MAX )->SetEnabled( true );
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MIN )->SetEnabled( true );
    m_MagnifyUI.GetSlider( IDC_MAGNIFY_SLIDER_DEPTH_MAX )->SetEnabled( true );
}

void MagnifyTool::ForceDepthUI()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetChecked( true );
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_DEPTH )->SetEnabled( false );
}

void MagnifyTool::DisableSubSampleUI()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetEnabled( false );
}

void MagnifyTool::EnableSubSampleUI()
{
    m_MagnifyUI.GetCheckBox( IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES )->SetEnabled( true );
}

//=================================================================================================================================
// EOF.
//=================================================================================================================================




