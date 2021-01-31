//--------------------------------------------------------------------------------------
// File: MorphTarget.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"

/*
File Structure

MORPH_TARGET_FILE_HEADER
BaseVertices
BaseIndices

for each morph target (start with base MT)
MORPH_TARGET_BLOCK_HEADER
PosDiff
NormDiff
TanDiff

*/
#define MAX_MORPH_NAME 100

struct MESH_VERTEX
{
    UINT DataIndex;
    D3DXVECTOR2 tex;

    D3DXVECTOR4 coeff0;
    D3DXVECTOR4 coeff1;
    D3DXVECTOR4 coeff2;
    D3DXVECTOR4 coeff3;
    D3DXVECTOR2 coeff4;
};

struct FILE_VERTEX
{
    UINT DataIndex;
    D3DXVECTOR2 tex;
};

struct QUAD_VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR2 tex;
};

struct MORPH_TARGET_FILE_HEADER
{
    UINT NumTargets;
    UINT NumBaseVerts;
    UINT NumBaseIndices;
};
struct MORPH_TARGET_BLOCK_HEADER
{
    WCHAR szName[MAX_MORPH_NAME];
    UINT XRes;
    UINT YRes;
    UINT XStart;
    UINT YStart;
    D3DXVECTOR3 vMaxPositionDelta;
    D3DXVECTOR3 vMaxNormalDelta;
    D3DXVECTOR3 vMaxTangentDelta;
};

//--------------------------------------------------------------------------------------
// CMorphTarget
//
// Morph Target Class
//--------------------------------------------------------------------------------------
class CMorphTarget
{
private:
    MORPH_TARGET_BLOCK_HEADER m_Header;
    ID3D10Buffer* m_pVB;
    ID3D10Buffer* m_pIB;
    ID3D10Texture2D* m_pTexture;
    ID3D10ShaderResourceView* m_pTexRV;
    UINT m_XRes;
    UINT m_YRes;

protected:
    HRESULT     CreateTexturesFLOAT( ID3D10Device* pd3dDevice );
    HRESULT     CreateTexturesBIASED( ID3D10Device* pd3dDevice );
    HRESULT     LoadTextureDataFLOAT( ID3D10Device* pd3dDevice, ID3D10Texture2D* pTex, HANDLE hFile );
    HRESULT     LoadTextureDataBIASED( ID3D10Device* pd3dDevice, ID3D10Texture2D* pTex, HANDLE hFile );
    HRESULT     CreateRenderQuad( ID3D10Device* pd3dDevice );

public:
                CMorphTarget();
                ~CMorphTarget();
    ID3D10ShaderResourceView* GetTexRV()
    {
        return m_pTexRV;
    }
    D3DXVECTOR4 GetMaxPositionDelta()
    {
        return D3DXVECTOR4( m_Header.vMaxPositionDelta, 1 );
    }
    D3DXVECTOR4 GetMaxNormalDelta()
    {
        return D3DXVECTOR4( m_Header.vMaxNormalDelta, 1 );
    }
    D3DXVECTOR4 GetMaxTangentDelta()
    {
        return D3DXVECTOR4( m_Header.vMaxTangentDelta, 1 );
    }

    WCHAR* GetName();
    HRESULT     LoadFLOAT( ID3D10Device* pd3dDevice, HANDLE hFile, UINT XRes, UINT YRes );
    HRESULT     LoadBIASED( ID3D10Device* pd3dDevice, HANDLE hFile, UINT XRes, UINT YRes );
    void        Apply( ID3D10Device* pd3dDevice,
                       ID3D10RenderTargetView* pRTV,
                       ID3D10DepthStencilView* pDSV,
                       ID3D10EffectTechnique* pTechnique,
                       ID3D10EffectShaderResourceVariable* pVertData );

    void        RenderToTexture( ID3D10Device* pd3dDevice, ID3D10RenderTargetView* pRTV, ID3D10DepthStencilView* pDSV,
                                 ID3D10EffectTechnique* pTechnique );

};

//--------------------------------------------------------------------------------------
// CMorphTargetManager
//
// Morph Target Manager Class
//--------------------------------------------------------------------------------------
class CMorphTargetManager
{
private:
    ID3D10Buffer* m_pMeshVB;  //mesh vertex buffer
    ID3D10Buffer* m_pMeshIB;  //mesh index buffer
    ID3D10InputLayout* m_pMeshLayout;
    ID3D10InputLayout* m_pQuadLayout;
    ID3D10Texture2D* m_pTexture;
    ID3D10ShaderResourceView* m_pTexRV;
    ID3D10RenderTargetView* m_pRTV;
    ID3D10Texture2D* m_pDepth;
    ID3D10DepthStencilView* m_pDSV;
    UINT m_XRes;
    UINT m_YRes;

    MORPH_TARGET_FILE_HEADER m_fileHeader;
    CMorphTarget* m_pMorphTargets; //morph target array

protected:
    HRESULT SetLDPRTData( MESH_VERTEX* pVertices, ID3DXPRTBuffer* pLDPRTBuff );
    HRESULT CreateVB( ID3D10Device* pd3dDevice, HANDLE hFile, ID3DXPRTBuffer* pLDPRTBuff );
    HRESULT CreateIB( ID3D10Device* pd3dDevice, HANDLE hFile );
    HRESULT CreateTextures( ID3D10Device* pd3dDevice, UINT uiXRes, UINT uiYRes );

public:
            CMorphTargetManager();
            ~CMorphTargetManager();

    HRESULT Create( ID3D10Device* pd3dDevice, WCHAR* szMeshFile, WCHAR* szLDPRTFile );
    void    Destroy();

    void    ResetToBase( ID3D10Device* pd3dDevice,
                         ID3D10EffectTechnique* pTechnique,
                         ID3D10EffectShaderResourceVariable* pVertData,
                         ID3D10EffectScalarVariable* pBlendAmt,
                         ID3D10EffectVectorVariable* pMaxDeltas );
    void    ApplyMorph( ID3D10Device* pd3dDevice,
                        WCHAR* szMorph,
                        float fAmount,
                        ID3D10EffectTechnique* pTechnique,
                        ID3D10EffectShaderResourceVariable* pVertData,
                        ID3D10EffectScalarVariable* pBlendAmt,
                        ID3D10EffectVectorVariable* pMaxDeltas );
    void    ApplyMorph( ID3D10Device* pd3dDevice,
                        UINT iMorph,
                        float fAmount,
                        ID3D10EffectTechnique* pTechnique,
                        ID3D10EffectShaderResourceVariable* pVertData,
                        ID3D10EffectScalarVariable* pBlendAmt,
                        ID3D10EffectVectorVariable* pMaxDeltas );

    void    Render( ID3D10Device* pd3dDevice,
                    ID3D10EffectTechnique* pTechnique,
                    ID3D10EffectShaderResourceVariable* pVertData,
                    ID3D10EffectShaderResourceVariable* pVertDataOrig,
                    ID3D10EffectScalarVariable* pDataTexSize );

    void    SetVertexLayouts( ID3D10InputLayout* pMeshLayout, ID3D10InputLayout* pQuadLayout );

    void    ShowMorphTexture( ID3D10Device* pd3dDevice,
                              ID3D10EffectTechnique* pTechnique,
                              ID3D10EffectShaderResourceVariable* pVertData,
                              ID3D10EffectScalarVariable* pRT,
                              ID3D10EffectScalarVariable* pBlendAmt,
                              int iRT,
                              int iTarget );
    void    ShowMorphTexture( ID3D10Device* pd3dDevice,
                              ID3D10EffectTechnique* pTechnique,
                              ID3D10EffectShaderResourceVariable* pVertData,
                              ID3D10EffectScalarVariable* pRT,
                              ID3D10EffectScalarVariable* pBlendAmt,
                              int iRT,
                              WCHAR* szMorph );
};
