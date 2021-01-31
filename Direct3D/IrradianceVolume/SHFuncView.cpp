//--------------------------------------------------------------------------------------
// File: SHFunView.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "SHFuncView.h"
#include "PRTMesh.h"
#include <stdio.h>


//--------------------------------------------------------------------------------------
CSHFunctionViewer::CSHFunctionViewer()
{
    m_pSphereMesh = NULL;
    m_pEffect = NULL;
    m_uNumTheta = 41;
    m_uNumPhi = 60;

    ZeroMemory( m_vVisPos, sizeof( D3DXVECTOR3 ) );
    m_dwVertexUnderMouse = 0;
    m_fAspectRatio = 1.0f;
    ZeroMemory( m_pVisCoeffs, sizeof( float ) * 36 );
    m_pVisCoeffs[0] = 1.0f;
}


//--------------------------------------------------------------------------------------
CSHFunctionViewer::~CSHFunctionViewer()
{
    SAFE_RELEASE( m_pSphereMesh );
    SAFE_RELEASE( m_pEffect );
}


//--------------------------------------------------------------------------------------
HRESULT CSHFunctionViewer::OnCreateDevice( LPDIRECT3DDEVICE9 pd3dDevice )
{
    unsigned short* pIBuff;
    char* pRawVB = NULL;
    D3DXVECTOR3* pTmp;
    D3DCOLOR* pClr;
    unsigned int szVB = sizeof( D3DXVECTOR3 ) * 2 + sizeof( D3DCOLOR );
    unsigned int vi, fi, rvi = 0, rfi = 0;

    D3DXCreateMeshFVF( m_uNumPhi * 2 + ( m_uNumTheta - 3 ) * ( m_uNumPhi * 2 ), 2 + m_uNumPhi * ( m_uNumTheta - 2 ),
                       D3DXMESH_MANAGED, D3DFVF_DIFFUSE | D3DFVF_NORMAL | D3DFVF_XYZ, pd3dDevice, &m_pSphereMesh );

    m_pSphereMesh->LockIndexBuffer( 0, ( VOID** )&pIBuff );
    m_pSphereMesh->LockVertexBuffer( 0, ( VOID** )&pRawVB );

    // 1st create the "top cap"
    for( fi = 0; fi < m_uNumPhi; fi++ )
    {
        pIBuff[fi * 3 + 0] = 0;
        pIBuff[fi * 3 + 1] = ( USHORT )fi + 1;
        pIBuff[fi * 3 + 2] = ( USHORT )( ( fi + 1 ) % m_uNumPhi ) + 1;
        rfi++;
    }

    // then fill in the "spans"
    for( vi = 1; vi < m_uNumTheta - 2; vi++ )
    {
        for( fi = 0; fi < m_uNumPhi; fi++ )
        {
            pIBuff[rfi * 3 + 0] = ( USHORT )( 1 + ( vi - 1 ) * m_uNumPhi + fi );
            pIBuff[rfi * 3 + 1] = ( USHORT )( 1 + ( vi )*m_uNumPhi + fi );
            pIBuff[rfi * 3 + 2] = ( USHORT )( 1 + ( vi )*m_uNumPhi + ( ( fi + 1 ) % m_uNumPhi ) );

            rfi++;

            pIBuff[rfi * 3 + 0] = ( USHORT )( 1 + ( vi - 1 ) * m_uNumPhi + fi );
            pIBuff[rfi * 3 + 1] = ( USHORT )( 1 + ( vi )*m_uNumPhi + ( ( fi + 1 ) % m_uNumPhi ) );
            pIBuff[rfi * 3 + 2] = ( USHORT )( 1 + ( vi - 1 ) * m_uNumPhi + ( ( fi + 1 ) % m_uNumPhi ) );
            rfi++;
        }
    }

    // then create the "bottom cap"

    for( fi = 0; fi < m_uNumPhi; fi++ )
    {
        pIBuff[rfi * 3 + 0] = ( USHORT )( 1 + m_uNumPhi * ( m_uNumTheta - 3 ) + fi );
        pIBuff[rfi * 3 + 1] = ( USHORT )( 1 + m_uNumPhi * ( m_uNumTheta - 2 ) );
        pIBuff[rfi * 3 + 2] = ( USHORT )( 1 + m_uNumPhi * ( m_uNumTheta - 3 ) + ( ( fi + 1 ) % m_uNumPhi ) );
        rfi++;
    }

    // then create all of the vertices...
    for( vi = 0; vi < m_uNumTheta; vi++ )
    {
        float fTheta = vi * D3DX_PI / ( m_uNumTheta - 1.0f );
        float cT = cosf( fTheta );
        float sT = sinf( fTheta );

        const float fZScale = 1.0f / 4.0f;

        if( vi && ( vi < m_uNumTheta - 1 ) )
        {
            for( fi = 0; fi < m_uNumPhi; fi++ )
            {
                float fPhi = fi * 2.0f * D3DX_PI / m_uNumPhi;
                float cP = cosf( fPhi );
                float sP = sinf( fPhi );

                D3DXVECTOR3 vPos( cP * sT * fZScale,sP * sT,cT ); // scale mesh in Z...

                pTmp = ( D3DXVECTOR3* )( pRawVB + rvi * szVB );
                *pTmp = vPos;
                ++pTmp; // bump to the normal
                vPos = D3DXVECTOR3( cP * sT / fZScale, sP * sT, cT );
                D3DXVec3Normalize( &vPos, &vPos ); // generate xformed normal...
                *pTmp = vPos;
                pClr = ( D3DCOLOR* )( pTmp + 1 );
                *pClr = D3DCOLOR_ARGB( 0, 255, 0, 0 );
                rvi++;
            }
        }
        else
        {
            pTmp = ( D3DXVECTOR3* )( pRawVB + rvi * szVB );
            *pTmp = D3DXVECTOR3( 0.0f, 0.0f, cT );
            ++pTmp; // bump to the normal
            *pTmp = D3DXVECTOR3( 0.0f, 0.0f, cT );
            pClr = ( D3DCOLOR* )( pTmp + 1 );
            *pClr = D3DCOLOR_ARGB( 0, 255, 0, 0 );
            rvi++;
        }
    }

    m_pSphereMesh->UnlockVertexBuffer();
    m_pSphereMesh->UnlockIndexBuffer();

    // Load effects 
    HRESULT hr;
    WCHAR str[MAX_PATH];
    V( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT( "SHFuncView.fx" ) ) );
    V( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL,
                                 D3DXFX_NOT_CLONEABLE, NULL, &m_pEffect, NULL ) );

    // Make sure the technique works
    hr = m_pEffect->ValidateTechnique( "Render" );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CSHFunctionViewer::OnResetDevice()
{
    HRESULT hr;
    if( m_pEffect )
    {
        V( m_pEffect->OnResetDevice() );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
void CSHFunctionViewer::OnLostDevice()
{
    HRESULT hr;
    if( m_pEffect )
    {
        V( m_pEffect->OnLostDevice() );
    }
}


//--------------------------------------------------------------------------------------
void CSHFunctionViewer::OnDestroyDevice()
{
    SAFE_RELEASE( m_pSphereMesh );
    SAFE_RELEASE( m_pEffect );
}


//--------------------------------------------------------------------------------------
void CSHFunctionViewer::SetCoeffs( float* pCoeffs, unsigned int uOrder, bool bStripe )
{
    char* pRawVB = NULL;
    D3DXVECTOR3* pTmp;
    D3DCOLOR* pClr;
    unsigned int szVB = sizeof( D3DXVECTOR3 ) * 2 + sizeof( D3DCOLOR );
    unsigned int vi, fi, rvi = 0, ci;
    float fCurVals[36];
    float fDerivs[2][36];

    m_pSphereMesh->LockVertexBuffer( 0, ( VOID** )&pRawVB );

    // then create all of the vertices...
    for( vi = 0; vi < m_uNumTheta; vi++ )
    {
        float fTheta = vi * D3DX_PI / ( m_uNumTheta - 1.0f );
        float cT = cosf( fTheta );
        float sT = sinf( fTheta );

        if( vi && ( vi < m_uNumTheta - 1 ) )
        {
            for( fi = 0; fi < m_uNumPhi; fi++ )
            {
                D3DCOLOR clr = D3DCOLOR_ARGB( 0, 255, 0, 0 );
                float fPhi = fi * 2.0f * D3DX_PI / m_uNumPhi;
                float cP = cosf( fPhi );
                float sP = sinf( fPhi );

                D3DXVECTOR3 vPos( cP* sT, sP* sT, cT ), vDT( 0.0f,0.0f,0.0f ), vDP( 0.0f,0.0f,0.0f ), vNrm;
                D3DXSHEvalDirection( fCurVals, 6, &vPos );

                CompDerivs( sT, cT, sP, cP, fDerivs );
                float fVal = 0.0f;

                for( ci = 0; ci < 36; ci++ )
                {
                    fVal += fCurVals[ci] * pCoeffs[ci];

                    vDT += D3DXVECTOR3( fCurVals[ci] * cT + sT * fDerivs[0][ci],
                                        fCurVals[ci] * cT + sT * fDerivs[0][ci],
                                        -fCurVals[ci] * sT + cT * fDerivs[0][ci] ) * pCoeffs[ci];

                    vDP += D3DXVECTOR3( fDerivs[1][ci] * cP - sP * fCurVals[ci],
                                        fDerivs[1][ci] * sP + cP * fCurVals[ci],
                                        fDerivs[1][ci] ) * pCoeffs[ci];
                }

                vDT = D3DXVECTOR3( vDT.x * cP, vDT.y * sP, vDT.z );
                vDP = D3DXVECTOR3( vDP.x * sT, vDP.y * sT, vDP.z * cT );

                D3DXVec3Cross( &vNrm, &vDT, &vDP );

                if( fVal < 0.0f )
                {
                    fVal *= -1.0f;
                    //vNrm *= -1.0f; // flip normal...
                    clr = D3DCOLOR_ARGB( 0, 0, 0, 255 );
                }

                if( bStripe && ( vi == ( m_uNumTheta / 2 ) ) ) clr = D3DCOLOR_ARGB( 0, 0, 255, 0 );

                D3DXVec3Normalize( &vNrm, &vNrm );
                const float fFudge = 1.0f;//D3DXVec3Dot(&vPos,&vNrm);

                vPos *= fVal * fFudge;

                pTmp = ( D3DXVECTOR3* )( pRawVB + rvi * szVB );
                *pTmp = vPos;
                ++pTmp; // bump to the normal
                *pTmp = vNrm;
                pClr = ( D3DCOLOR* )( pTmp + 1 );
                *pClr = clr;
                rvi++;
            }
        }
        else
        {
            // fudge cos/sin a bit...
            if( vi )
            {
                fTheta = vi * D3DX_PI / ( m_uNumTheta - 1.0f ) - 0.0001f;
            }
            else
            {
                fTheta = 0.0001f;
            }

            cT = cosf( fTheta );
            sT = sinf( fTheta );

            float sP = sqrtf( 0.5f ); // any phi is okay at the poles...
            float cP = sqrtf( 0.5f );

            D3DCOLOR clr = D3DCOLOR_ARGB( 0, 255, 0, 0 );
            D3DXVECTOR3 vPos( cP* sT, sP* sT, cT ), vDT( 0.0f,0.0f,0.0f ), vDP( 0.0f,0.0f,0.0f ), vNrm;

            D3DXSHEvalDirection( fCurVals, 6, &vPos );

            CompDerivs( sT, cT, sP, cP, fDerivs );
            float fVal = 0.0f;

            for( ci = 0; ci < 36; ci++ )
            {
                fVal += fCurVals[ci] * pCoeffs[ci];
                vDT += D3DXVECTOR3( fCurVals[ci] * cT + sT * fDerivs[0][ci],
                                    fCurVals[ci] * cT + sT * fDerivs[0][ci],
                                    -fCurVals[ci] * sT + cT * fDerivs[0][ci] ) * pCoeffs[ci];

                vDP += D3DXVECTOR3( fDerivs[1][ci] * cP - sP * fCurVals[ci],
                                    fDerivs[1][ci] * sP + cP * fCurVals[ci],
                                    fDerivs[1][ci] ) * pCoeffs[ci];
            }

            vDT = D3DXVECTOR3( vDT.x * cP, vDT.y * sP, vDT.z );
            vDP = D3DXVECTOR3( vDP.x * sT, vDP.y * sT, vDP.z * cT );

            D3DXVec3Cross( &vNrm, &vDT, &vDP );

            if( fVal < 0.0f )
            {
                fVal *= -1.0f;
                //vNrm *= -1.0f; // flip normal...
                clr = D3DCOLOR_ARGB( 0, 0, 0, 255 );
            }

            D3DXVec3Normalize( &vNrm, &vNrm );
            const float fFudge = 1.0f;//D3DXVec3Dot(&vPos,&vNrm);

            vPos *= fVal * fFudge;

            pTmp = ( D3DXVECTOR3* )( pRawVB + rvi * szVB );
            *pTmp = vPos;
            ++pTmp; // bump to the normal
            *pTmp = vNrm;
            pClr = ( D3DCOLOR* )( pTmp + 1 );
            *pClr = clr;
            rvi++;
        }
    }

    m_pSphereMesh->UnlockVertexBuffer();
}


//--------------------------------------------------------------------------------------
void CSHFunctionViewer::Render( IDirect3DDevice9* pd3dDevice, float fObjectRadius, D3DXMATRIX& mWorld,
                                D3DXMATRIX& mView, D3DXMATRIX& mProj )
{
    D3DXMATRIXA16 mScreenMat, mTransBack;
    D3DXMATRIXA16 mJustRot, mRot;
    mJustRot = mWorld * mView;
    mJustRot( 3, 0 ) = mJustRot( 3, 1 ) = mJustRot( 3, 2 ) = 0.0f;

    // Normalize this matrix
    const float fMatScale = 1.0f / sqrtf( mJustRot( 0, 0 ) * mJustRot( 0, 0 ) + mJustRot( 0, 1 ) * mJustRot( 0,
                                                                                                             1 ) +
                                          mJustRot( 0, 2 ) * mJustRot( 0, 2 ) );
    for( int i = 0; i < 3; i++ )
    {
        for( int j = 0; j < 3; j++ )
        {
            mJustRot( i, j ) *= fMatScale; // normalize rotation sub matrix
        }
    }

    D3DXMatrixOrthoLH( &mScreenMat, m_fAspectRatio, 1.0f, 0.1f, 100.0f );
    D3DXMatrixTranslation( &mTransBack, 0.0f, 0.0f, 1.0f );
    mScreenMat = mJustRot * mTransBack * mScreenMat;

    // Clear the zbuffer so that you can see the visualization even when its obsured
    pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    float fScale = 0.15f * fObjectRadius;

    D3DXMATRIXA16 mScale;
    D3DXMatrixTranslation( &mTransBack, m_vVisPos.x, m_vVisPos.y, m_vVisPos.z );
    D3DXMatrixScaling( &mScale, fScale, fScale, fScale );

    D3DXMATRIX mWorldViewProjection = mWorld * mView * mProj;
    mScreenMat = mScale * mTransBack * mWorldViewProjection;



    HRESULT hr;
    UINT iPass, cPasses;

    m_pEffect->SetMatrix( "g_mWorldViewProjection", &mScreenMat );
    m_pEffect->SetMatrix( "g_mNormalXform", &mJustRot );
    m_pEffect->SetTechnique( "Render" );
    ID3DXMesh* pMeshUse = m_pSphereMesh;

    V( m_pEffect->Begin( &cPasses, 0 ) );
    for( iPass = 0; iPass < cPasses; iPass++ )
    {
        V( m_pEffect->BeginPass( iPass ) );
        V( pMeshUse->DrawSubset( 0 ) );
        V( m_pEffect->EndPass() );
    }
    V( m_pEffect->End() );
}


//-----------------------------------------------------------------------------
// Updates the transfer visualization data under the currently selected vertex 
//-----------------------------------------------------------------------------
void CSHFunctionViewer::UpdateDataForActiveVertex( CPRTMesh* pPRTMesh, int nAppTechnique, bool bUpdateData )
{
    const DWORD dwActiveChannel = 0; // change this if you want to visualize the other channels (green or blue)
    const float fSHVisScale[3] = {4.0f,4.0f,4.0f};

    if( bUpdateData )
    {
        pPRTMesh->GetSHTransferFunctionAtVertex( m_dwVertexUnderMouse, nAppTechnique, dwActiveChannel, m_pVisCoeffs );
        pPRTMesh->GetVertexPosition( m_dwVertexUnderMouse, &m_vVisPos );
    }

    float fVals[36];
    for( int vi = 0; vi < 36; vi++ )
        fVals[vi] = m_pVisCoeffs[vi] * fSHVisScale[dwActiveChannel];

    SetCoeffs( fVals, 6, false );
}


//-----------------------------------------------------------------------------
// Updates the transfer visualization data under the currently selected vertex 
//-----------------------------------------------------------------------------
void CSHFunctionViewer::GetVertexUnderMouse( CPRTMesh* pPRTMesh, int nAppTechnique, const D3DXMATRIX* pmProj,
                                             const D3DXMATRIX* pmView, const D3DXMATRIX* pmWorld )
{
    pPRTMesh->GetVertexUnderMouse( pmProj, pmView, pmWorld, &m_dwVertexUnderMouse );
    UpdateDataForActiveVertex( pPRTMesh, nAppTechnique, true );
}


//-----------------------------------------------------------------------------
void CSHFunctionViewer::ClearActiveVertex( CPRTMesh* pPRTMesh, int nAppTechnique )
{
    m_dwVertexUnderMouse = 0;
    ZeroMemory( m_vVisPos, sizeof( D3DXVECTOR3 ) );
    ZeroMemory( m_pVisCoeffs, sizeof( float ) * 36 );
    m_pVisCoeffs[0] = 1.0f;
    UpdateDataForActiveVertex( pPRTMesh, nAppTechnique, true );
}


//--------------------------------------------------------------------------------------
// Computes derivatives for a given point on the sphere (theta/phi, sines/cosines are handed in)
// this can beused to create the surface normals of the spherical function...
//--------------------------------------------------------------------------------------
void CSHFunctionViewer::CompDerivs( float sT, float cT, float sP, float cP, float retval[2][36] )
{
    float t1 = sqrtf( 0.3e1f );
    float t2 = sqrtf( 0.3141592654e1f );
    float t3 = 0.1e1f / t2;
    float t4 = t1 * t3;
    float t5 = sP;
    float t6 = cT;
    float t7 = t5 * t6;
    float t10 = sT;
    float t13 = cP;
    float t14 = t6 * t13;
    float t17 = sqrtf( 0.15e2f );
    float t18 = t17 * t3;
    float t20 = t13 * t10;
    float t21 = t20 * t6;
    float t23 = t17 * t5;
    float t24 = t6 * t6;
    float t27 = ( 0.2e1f * t24 - 0.1e1f ) * t3;
    float t30 = sqrtf( 0.5e1f );
    float t32 = t6 * t10;
    float t39 = t13 * t13;
    float t40 = 0.2e1f * t39;
    float t41 = t40 - 0.1e1f;
    float t43 = t41 * t6 * t3;
    float t46 = sqrtf( 0.2e1f );
    float t47 = sqrtf( 0.35e2f );
    float t48 = t46 * t47;
    float t49 = t48 * t5;
    float t50 = 0.4e1f * t39;
    float t51 = t50 - 0.1e1f;
    float t52 = t24 - 0.1e1f;
    float t54 = t6 * t3;
    float t58 = sqrtf( 0.105e3f );
    float t60 = t58 * t5 * t13;
    float t61 = 0.3e1f * t24;
    float t62 = -0.1e1f + t61;
    float t67 = sqrtf( 0.21e2f );
    float t68 = t46 * t67;
    float t69 = t68 * t5;
    float t70 = 0.15e2f * t24;
    float t71 = -0.11e2f + t70;
    float t76 = sqrtf( 0.7e1f );
    float t78 = 0.5e1f * t24;
    float t79 = -0.1e1f + t78;
    float t89 = t24 * t39;
    float t91 = 0.6e1f * t89 - t40 - t61 + 0.1e1f;
    float t95 = t48 * t52;
    float t96 = t50 - 0.3e1f;
    float t97 = t13 * t96;
    float t102 = t13 * t41;
    float t104 = t52 * t10;
    float t105 = t104 * t54;
    float t108 = 0.16e2f * t89;
    float t116 = t30 * t5 * t13;
    float t117 = 0.7e1f * t24;
    float t123 = t46 * t30;
    float t124 = t24 * t24;
    float t126 = 0.27e2f * t24;
    float t127 = 0.28e2f * t124 - t126 + 0.3e1f;
    float t132 = t117 - 0.3e1f;
    float t134 = t32 * t132 * t3;
    float t141 = 0.8e1f * t39;
    float t142 = 0.14e2f * t89;
    float t148 = 0.12e2f * t24;
    float t156 = t39 * t39;
    float t157 = 0.8e1f * t156;
    float t158 = t157 - t141 + 0.1e1f;
    float t163 = sqrtf( 0.77e2f );
    float t164 = t46 * t163;
    float t166 = 0.16e2f * t156;
    float t168 = t166 - 0.12e2f * t39 + 0.1e1f;
    float t169 = t52 * t52;
    float t174 = sqrtf( 0.385e3f );
    float t183 = t46 * t174;
    float t185 = t52 * t6;
    float t186 = 0.60e2f * t89;
    float t187 = 0.28e2f * t39;
    float t193 = sqrtf( 0.1155e4f );
    float t195 = t193 * t5 * t13;
    float t196 = 0.15e2f * t124;
    float t202 = sqrtf( 0.165e3f );
    float t203 = t202 * t5;
    float t206 = 0.105e3f * t124 - 0.126e3f * t24 + 0.29e2f;
    float t211 = sqrtf( 0.11e2f );
    float t215 = 0.21e2f * t124 - 0.14e2f * t24 + 0.1e1f;
    float t232 = t183 * t52;
    float t248 = t164 * t169;
    float t250 = t166 - 0.20e2f * t39 + 0.5e1f;
    float t257 = t5 * t10;
    float t261 = t41 * t3;
    float t274 = t10 * t3;
    float t292 = t51 * t3;
    float t342 = 0.36e2f * t89;
    retval[0][0] = 0.0e0f;
    retval[0][1] = -t4 * t7 / 0.2e1f;
    retval[0][2] = -t4 * t10 / 0.2e1f;
    retval[0][3] = -t4 * t14 / 0.2e1f;
    retval[0][4] = t18 * t5 * t21;
    retval[0][5] = -t23 * t27 / 0.2e1f;
    retval[0][6] = -0.3e1f / 0.2e1f * t30 * t3 * t32;
    retval[0][7] = -t17 * t13 * t27 / 0.2e1f;
    retval[0][8] = t17 * t10 * t43 / 0.2e1f;
    retval[0][9] = 0.3e1f / 0.8e1f * t49 * t51 * t52 * t54;
    retval[0][10] = t60 * t10 * t62 * t3 / 0.2e1f;
    retval[0][11] = -t69 * t6 * t71 * t3 / 0.8e1f;
    retval[0][12] = -0.3e1f / 0.4e1f * t76 * t10 * t79 * t3;
    retval[0][13] = -t68 * t6 * t13 * t71 * t3 / 0.8e1f;
    retval[0][14] = t58 * t10 * t91 * t3 / 0.4e1f;
    retval[0][15] = 0.3e1f / 0.8e1f * t95 * t97 * t54;
    retval[0][16] = -0.3e1f * t47 * t5 * t102 * t105;
    retval[0][17] = 0.3e1f / 0.8e1f * t49 * t52 * ( t108 - t50 - 0.4e1f * t24 + 0.1e1f ) * t3;
    retval[0][18] = 0.3e1f * t116 * t32 * ( -0.4e1f + t117 ) * t3;
    retval[0][19] = -0.3e1f / 0.8e1f * t123 * t5 * t127 * t3;
    retval[0][20] = -0.15e2f / 0.4e1f * t134;
    retval[0][21] = -0.3e1f / 0.8e1f * t123 * t13 * t127 * t3;
    retval[0][22] = 0.3e1f / 0.2e1f * t30 * t10 * t6 * ( -t141 + t142 + 0.4e1f - t117 ) * t3;
    retval[0][23] = 0.3e1f / 0.8e1f * t95 * t13 * ( t108 - t50 - t148 + 0.3e1f ) * t3;
    retval[0][24] = -0.3e1f / 0.4e1f * t47 * t52 * t10 * t158 * t6 * t3;
    retval[0][25] = -0.15e2f / 0.32e2f * t164 * t5 * t168 * t169 * t54;
    retval[0][26] = -0.3e1f / 0.4e1f * t174 * t5 * t13 * t104 * ( 0.10e2f * t89 - t40 - t78 + 0.1e1f ) * t3;
    retval[0][27] = 0.3e1f / 0.32e2f * t183 * t5 * t185 * ( t186 - t187 - t70 + 0.7e1f ) * t3;
    retval[0][28] = t195 * t10 * ( t196 - t148 + 0.1e1f ) * t3 / 0.4e1f;
    retval[0][29] = -t203 * t6 * t206 * t3 / 0.16e2f;
    retval[0][30] = -0.15e2f / 0.16e2f * t211 * t10 * t215 * t3;
    retval[0][31] = -t202 * t6 * t13 * t206 * t3 / 0.16e2f;
    retval[0][32] = t193 * t10 * ( 0.30e2f * t39 * t124 - 0.24e2f * t89 + t40 - t196 + t148 - 0.1e1f ) * t3 / 0.8e1f;
    retval[0][33] = 0.3e1f / 0.32e2f * t232 * t14 * ( t186 - t187 - 0.45e2f * t24 + 0.21e2f ) * t3;
    retval[0][34] = -0.3e1f / 0.16e2f * t174 * t52 * t10 * ( 0.40e2f * t156 * t24 - t157 - 0.40e2f * t89 + t141 + t78 -
                                                             0.1e1f ) * t3;
    retval[0][35] = -0.15e2f / 0.32e2f * t248 * t13 * t250 * t54;
    retval[1][0] = 0.0e0f;
    retval[1][1] = -t4 * t20 / 0.2e1f;
    retval[1][2] = 0.0e0f;
    retval[1][3] = t4 * t257 / 0.2e1f;
    retval[1][4] = -t17 * t52 * t261 / 0.2e1f;
    retval[1][5] = -t18 * t21 / 0.2e1f;
    retval[1][6] = 0.0e0f;
    retval[1][7] = t18 * t7 * t10 / 0.2e1f;
    retval[1][8] = t23 * t13 * t52 * t3;
    retval[1][9] = 0.3e1f / 0.8e1f * t48 * t13 * t96 * t52 * t274;
    retval[1][10] = -t58 * t52 * t43 / 0.2e1f;
    retval[1][11] = -t68 * t10 * t79 * t13 * t3 / 0.8e1f;
    retval[1][12] = 0.0e0f;
    retval[1][13] = t69 * t10 * t79 * t3 / 0.8e1f;
    retval[1][14] = t60 * t185 * t3;
    retval[1][15] = -0.3e1f / 0.8e1f * t95 * t257 * t292;
    retval[1][16] = 0.3e1f / 0.4e1f * t47 * t158 * t169 * t3;
    retval[1][17] = 0.9e1f / 0.8e1f * t48 * t97 * t105;
    retval[1][18] = -0.3e1f / 0.4e1f * t30 * t52 * ( t142 - t117 - t40 + 0.1e1f ) * t3;
    retval[1][19] = -0.3e1f / 0.8e1f * t123 * t10 * t6 * t132 * t13 * t3;
    retval[1][20] = 0.0e0f;
    retval[1][21] = 0.3e1f / 0.8e1f * t123 * t5 * t134;
    retval[1][22] = 0.3e1f / 0.2e1f * t116 * t52 * ( t117 - 0.1e1f ) * t3;
    retval[1][23] = -0.9e1f / 0.8e1f * t48 * t104 * t7 * t292;
    retval[1][24] = -0.3e1f * t47 * t169 * t5 * t102 * t3;
    retval[1][25] = -0.15e2f / 0.32e2f * t164 * t13 * t250 * t169 * t274;
    retval[1][26] = 0.3e1f / 0.4e1f * t174 * t158 * t169 * t6 * t3;
    retval[1][27] = 0.3e1f / 0.32e2f * t183 * t13 * t104 * ( t342 - t50 - t126 + 0.3e1f ) * t3;
    retval[1][28] = -t193 * t52 * t6 * t91 * t3 / 0.4e1f;
    retval[1][29] = -t202 * t10 * t215 * t13 * t3 / 0.16e2f;
    retval[1][30] = 0.0e0f;
    retval[1][31] = t203 * t10 * t215 * t3 / 0.16e2f;
    retval[1][32] = t195 * t185 * t62 * t3 / 0.2e1f;
    retval[1][33] = -0.3e1f / 0.32e2f * t232 * t257 * ( t342 - 0.9e1f * t24 - t50 + 0.1e1f ) * t3;
    retval[1][34] = -0.3e1f * t174 * t169 * t6 * t5 * t13 * t261;
    retval[1][35] = 0.15e2f / 0.32e2f * t248 * t257 * t168 * t3;
}


