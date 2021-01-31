//--------------------------------------------------------------------------------------
// File: SHFuncView.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

class CPRTMesh;

class CSHFunctionViewer
{
public:
            CSHFunctionViewer();
            ~CSHFunctionViewer();

    HRESULT OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT OnResetDevice();
    void    OnLostDevice();
    void    OnDestroyDevice();

    HRESULT LoadEffects( IDirect3DDevice9* pd3dDevice, const D3DCAPS9* pDeviceCaps );
    void    GetVertexUnderMouse( CPRTMesh* pPRTMesh, int nAppTechnique, const D3DXMATRIX* pmProj,
                                 const D3DXMATRIX* pmView, const D3DXMATRIX* pmWorld );
    void    ClearActiveVertex( CPRTMesh* pPRTMesh, int nAppTechnique );
    void    UpdateDataForActiveVertex( CPRTMesh* pPRTMesh, int nAppTechnique, bool bUpdateData );
    void    SetAspectRatio( float fAspectRatio )
    {
        m_fAspectRatio = fAspectRatio;
    }
    void    Render( IDirect3DDevice9* pd3dDevice, float fObjectRadius, D3DXMATRIX& mWorld, D3DXMATRIX& mView,
                    D3DXMATRIX& mProj );

protected:
    void    SetCoeffs( float* pCoeffs, unsigned int uOrder, bool bStripe = true );
    void    UpdateTransferVisualizationAtVertex( bool bUpdateData );
    void    CompDerivs( float sT, float cT, float sP, float cP, float retval[2][36] );

    ID3DXMesh* m_pSphereMesh;
    ID3DXEffect* m_pEffect;
    unsigned int m_uNumTheta,m_uNumPhi; // information on spherical tesselation

    float m_fAspectRatio;
    D3DXVECTOR3 m_vVisPos;
    unsigned int m_dwVertexUnderMouse;
    float   m_pVisCoeffs[36];
};
