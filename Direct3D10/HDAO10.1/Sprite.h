//--------------------------------------------------------------------------------------
// File: Sprite.h
//
// Sprite class definition. This class provides functionality to render sprites, at a given position and scale. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef _SPRITE_H_
#define _SPRITE_H_

class Sprite
{
public:

    Sprite();
    ~Sprite();

    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice );
    void OnDestroyDevice();
    void OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );

    HRESULT RenderSprite( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, bool bAlpha, bool bBordered );

    HRESULT RenderSpriteMS( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight, 
        bool bAlpha, bool bBordered, int nSampleIndex );

    HRESULT RenderSpriteAsDepth( ID3D10ShaderResourceView* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, bool bBordered, float fDepthRangeMin, 
        float fDepthRangeMax );
	
    HRESULT RenderSpriteAsDepthMS( ID3D10ShaderResourceView1* pTextureView, int nStartPosX,
        int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight, 
        bool bBordered, float fDepthRangeMin, float fDepthRangeMax, int nSampleIndex );

    void SetBorderColor( D3DXCOLOR Color );
    void SetUVs( float fU1, float fV1, float fU2, float fV2 );

private:

	void RenderBorder();
	void Render();

	// VBs
	ID3D10InputLayout*  m_pVertexLayout;
	ID3D10Buffer*       m_pVertexBuffer;
	ID3D10InputLayout*  m_pBorderVertexLayout;
	ID3D10Buffer*       m_pBorderVertexBuffer;

	// Effect and techniques
	ID3D10Effect*           m_pEffect;
	ID3D10EffectTechnique*  m_pSpriteTechnique;
	ID3D10EffectTechnique*  m_pSpriteMSTechnique;
	ID3D10EffectTechnique*  m_pSpriteAsDepthTechnique;
	ID3D10EffectTechnique*  m_pSpriteAsDepthMSTechnique;
	ID3D10EffectTechnique*  m_pAlphaSpriteTechnique;
	ID3D10EffectTechnique*  m_pBorderTechnique;

	// Shader resources
	ID3D10EffectShaderResourceVariable* m_pSpriteTextureVar;
	ID3D10EffectShaderResourceVariable* m_pSpriteTextureMSVar;
	ID3D10EffectShaderResourceVariable* m_pSpriteDepthTextureMSVar;
	
	// Shader variables
	ID3D10EffectScalarVariable* m_pViewportWidthVar;
	ID3D10EffectScalarVariable* m_pViewportHeightVar;
	ID3D10EffectScalarVariable* m_pStartPosXVar;
    ID3D10EffectScalarVariable* m_pStartPosYVar;
    ID3D10EffectScalarVariable* m_pWidthVar;
	ID3D10EffectScalarVariable* m_pHeightVar;
	ID3D10EffectVectorVariable* m_pBorderColorVar;
	ID3D10EffectScalarVariable* m_pTextureWidthVar;
	ID3D10EffectScalarVariable* m_pTextureHeightVar;
	ID3D10EffectScalarVariable* m_pDepthRangeMinVar;
	ID3D10EffectScalarVariable* m_pDepthRangeMaxVar;
	ID3D10EffectScalarVariable* m_pSampleIndexVar;
};

#endif // _SPRITE_H_

//=================================================================================================================================
// EOF
//=================================================================================================================================
