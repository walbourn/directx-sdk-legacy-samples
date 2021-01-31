//--------------------------------------------------------------------------------------
// File: Magnify.h
//
// Magnify class definition. This class magnifies a region of a given surface, and renders a scaled 
// sprite at the given position on the screen.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef _MAGNIFY_H_
#define _MAGNIFY_H_

class Magnify
{
public:

    // Constructor / destructor
    Magnify();
    ~Magnify();

    // Hooks for the DX SDK Framework
    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice );
    void OnDestroyDevice();
    void OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );

    // Set methods
    void SetPixelRegion( int nPixelRegion );
    void SetScale( int nScale );
    void SetDepthRangeMin( float fDepthRangeMin );
    void SetDepthRangeMax( float fDepthRangeMax );
    void SetSourceResource( ID3D10Resource* pSourceResource, DXGI_FORMAT Format, 
        int nWidth, int nHeight, int nSamples );
    void SetSubSampleIndex( int nSubSampleIndex );

    // Captures a region, at the current cursor position, for magnification
    void Capture();

    // Render the magnified region, at the capture location
    void RenderBackground();
    void RenderMagnifiedRegion();

private:

    // Private methods
    void SetPosition( int nPositionX, int nPositionY );
    void CreateInternalResources();

private:

    // Magnification settings
    int     m_nPositionX;
    int     m_nPositionY;
    int     m_nPixelRegion;
    int     m_nHalfPixelRegion;
    int     m_nScale;
    float   m_fDepthRangeMin;
    float   m_fDepthRangeMax;
    int     m_nBackBufferWidth;
    int     m_nBackBufferHeight;
    int     m_nSubSampleIndex;

    // Helper class for plotting the magnified region
    Sprite  m_Sprite;

    // Source resource data
    ID3D10Resource*             m_pSourceResource;
    ID3D10Texture2D*            m_pResolvedSourceResource;
    ID3D10Texture2D*            m_pCopySourceResource;
    ID3D10ShaderResourceView*   m_pResolvedSourceResourceSRV;
    ID3D10ShaderResourceView*   m_pCopySourceResourceSRV;
    ID3D10ShaderResourceView1*  m_pSourceResourceSRV1;
    DXGI_FORMAT                 m_SourceResourceFormat;
    int                         m_nSourceResourceWidth;
    int                         m_nSourceResourceHeight;
    int                         m_nSourceResourceSamples;
    DXGI_FORMAT                 m_DepthFormat; 
    DXGI_FORMAT                 m_DepthSRVFormat;
    bool                        m_bDepthFormat;
};

#endif // _MAGNIFY_H_

//=================================================================================================================================
// EOF
//=================================================================================================================================
