//--------------------------------------------------------------------------------------
// File: MagnifyTool.h
//
// MagnifyTool class definition. This class implements a user interface based upon the DXUT framework,
// for the Magnify class. It exposes all of the functionality.
//
// Contributed by AMD Corporation
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#ifndef _MAGNIFYTOOL_H_
#define _MAGNIFYTOOL_H_

#define IDC_MAGNIFY_CHECKBOX_ENABLE         ( 20 + 1024 )
#define IDC_MAGNIFY_STATIC_PIXEL_REGION     ( 21 + 1024 )
#define IDC_MAGNIFY_SLIDER_PIXEL_REGION     ( 22 + 1024 )
#define IDC_MAGNIFY_STATIC_SCALE            ( 23 + 1024 )
#define IDC_MAGNIFY_SLIDER_SCALE            ( 24 + 1024 )
#define IDC_MAGNIFY_CHECKBOX_DEPTH          ( 25 + 1024 )
#define IDC_MAGNIFY_STATIC_DEPTH_MIN        ( 26 + 1024 )
#define IDC_MAGNIFY_SLIDER_DEPTH_MIN        ( 27 + 1024 )
#define IDC_MAGNIFY_STATIC_DEPTH_MAX        ( 28 + 1024 )
#define IDC_MAGNIFY_SLIDER_DEPTH_MAX        ( 29 + 1024 )
#define IDC_MAGNIFY_CHECKBOX_SUB_SAMPLES    ( 30 + 1024 )

class MagnifyTool
{
public:

    // Constructor / destructor
    MagnifyTool();
    ~MagnifyTool();

    // Set methods
    void SetSourceResources( ID3D10Resource* pSourceRTResource, DXGI_FORMAT RTFormat, 
        ID3D10Resource* pSourceDepthResource, DXGI_FORMAT DepthFormat, int nWidth, 
        int nHeight, int nSamples );
    void SetPixelRegion( int nPixelRegion ) { m_Magnify.SetPixelRegion( nPixelRegion ); }
    void SetScale( int nScale ) { m_Magnify.SetScale( nScale ); }
    	
    // Get methods
    CDXUTDialog* GetMagnifyUI() { return &m_MagnifyUI; }
    ID3D10DepthStencilView* GetDepthStencilView() { return m_pDSV; }

    // Hooks for the DX SDK Framework
    void InitApp( CDXUTDialogResourceManager* pResourceManager );
    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice );
    void OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, 
        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext, 
        int nPositionX, int nPositionY );
    void OnDestroyDevice();
    void OnReleasingSwapChain();
    void ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
    void OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

    // Capture
    void Capture()	{ m_Magnify.Capture(); }

    // Render
    void Render();

private:

    // Ensures the depth buffer is bindable as a shader resource
    void CreateDepthStencil( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, 
        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );

    // UI helper methods
    void EnableTool();
    void DisableTool();
    void EnableUI();
    void DisableUI();
    void EnableDepthUI();
    void DisableDepthUI();
    void ForceDepthUI();
    void EnableSubSampleUI();
    void DisableSubSampleUI();

private:

    // The DXUT dialog
    CDXUTDialog m_MagnifyUI;

    // Pointer to the Magnify class
    Magnify m_Magnify;

    // The source resources
    ID3D10Resource* m_pSourceRTResource;
    DXGI_FORMAT     m_RTFormat;
    int             m_nWidth;
    int             m_nHeight;
    int             m_nSamples;
    bool            m_bReleaseRTOnResize;
    ID3D10Resource* m_pSourceDepthResource;
    DXGI_FORMAT     m_DepthFormat;
    bool            m_bReleaseDepthOnResize;

    // The depth buffer
    ID3D10Texture2D*        m_pDepthStencil;
    ID3D10DepthStencilView* m_pDSV;
    ID3D10RasterizerState*  m_pRState;

    // Subsample control
    int m_nSubSampleIndex;
};

#endif // _MAGNIFYTOOL_H_

//=================================================================================================================================
// EOF
//=================================================================================================================================
