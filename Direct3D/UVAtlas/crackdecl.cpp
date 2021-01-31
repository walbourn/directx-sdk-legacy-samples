//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2000 Microsoft Corporation.  All Rights Reserved.
//
//  File:       crackdecl.cpp
//  Content:    Used to access vertex data using Declarations 
//  Updated Oct 16, 2001 to decode DX9 decls
//
//////////////////////////////////////////////////////////////////////////////

#include "DXUT.h"
#include "crackdecl.h"

//----------------------------------------------------------------------------
// CD3DXCrackDecl methods
//----------------------------------------------------------------------------

CD3DXCrackDecl::CD3DXCrackDecl()
{
    for( UINT i = 0; i < 16; i++ )
    {
        dwStride[i] = 0;
    }
}

const WCHAR* x_szDeclStrings[MAXD3DDECLUSAGE+2] =
{
    L"D3DDECLUSAGE_POSITION",
    L"D3DDECLUSAGE_BLENDWEIGHT",
    L"D3DDECLUSAGE_BLENDINDICES",
    L"D3DDECLUSAGE_NORMAL",
    L"D3DDECLUSAGE_PSIZE",
    L"D3DDECLUSAGE_TEXCOORD",
    L"D3DDECLUSAGE_TANGENT",
    L"D3DDECLUSAGE_BINORMAL",
    L"D3DDECLUSAGE_TESSFACTOR",
    L"D3DDECLUSAGE_POSITIONT",
    L"D3DDECLUSAGE_COLOR",
    L"D3DDECLUSAGE_FOG",
    L"D3DDECLUSAGE_DEPTH",
    L"D3DDECLUSAGE_SAMPLE",
    L"UNKNOWN",
};

const WCHAR* CD3DXCrackDecl::DeclUsageToString( D3DDECLUSAGE usage )
{
    if( usage >= D3DDECLUSAGE_POSITION && usage <= MAXD3DDECLUSAGE )
    {
        return x_szDeclStrings[usage];
    }
    else
    {
        return x_szDeclStrings[MAXD3DDECLUSAGE + 1];
    }
}


HRESULT
CD3DXCrackDecl::SetDeclaration( CONST D3DVERTEXELEMENT9 *pDecl )
 {
    pElements = pDecl;
    dwNumElements = D3DXGetDeclLength( pDecl );
     return S_OK;
}


HRESULT CD3DXCrackDecl::SetStreamSource( UINT Stream, LPVOID pData, UINT Stride )
{

    pStream[Stream] = ( LPBYTE )pData;
    if( Stride == 0 )
        dwStride[Stream] = D3DXGetDeclVertexSize( pElements, Stream );
    else
        dwStride[Stream] = Stride;

    return S_OK;
}


VOID
CD3DXCrackDecl::Decode( CONST D3DVERTEXELEMENT9 *pElem, UINT index,FLOAT* pData, UINT cData )
 {
    FLOAT Data[4];

    if( pElem )
 {
        LPVOID pElement = GetElementPointer( pElem,index );

        switch( pElem->Type )
 {
        case D3DDECLTYPE_FLOAT1:
            Data[0] = ( ( FLOAT * ) pElement )[0];
            Data[1] = 0.0f;
            Data[2] = 0.0f;
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_FLOAT2:
            Data[0] = ( ( FLOAT * ) pElement )[0];
            Data[1] = ( ( FLOAT * ) pElement )[1];
            Data[2] = 0.0f;
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_FLOAT3:
            Data[0] = ( ( FLOAT * ) pElement )[0];
            Data[1] = ( ( FLOAT * ) pElement )[1];
            Data[2] = ( ( FLOAT * ) pElement )[2];
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_FLOAT4:
            Data[0] = ( ( FLOAT * ) pElement )[0];
            Data[1] = ( ( FLOAT * ) pElement )[1];
            Data[2] = ( ( FLOAT * ) pElement )[2];
            Data[3] = ( ( FLOAT * ) pElement )[3];
            break;

        case D3DDECLTYPE_D3DCOLOR:
            Data[0] = ( 1.0f / 255.0f ) * ( FLOAT ) ( UINT8 ) ( *( ( D3DCOLOR * ) pElement ) >> 16 );
            Data[1] = ( 1.0f / 255.0f ) * ( FLOAT ) ( UINT8 ) ( *( ( D3DCOLOR * ) pElement ) >>  8 );
            Data[2] = ( 1.0f / 255.0f ) * ( FLOAT ) ( UINT8 ) ( *( ( D3DCOLOR * ) pElement ) >>  0 );
            Data[3] = ( 1.0f / 255.0f ) * ( FLOAT ) ( UINT8 ) ( *( ( D3DCOLOR * ) pElement ) >> 24 );
            break;

        case D3DDECLTYPE_UBYTE4:
            Data[0] = ( FLOAT ) ( ( UINT8 * ) pElement )[0];
            Data[1] = ( FLOAT ) ( ( UINT8 * ) pElement )[1];
            Data[2] = ( FLOAT ) ( ( UINT8 * ) pElement )[2];
            Data[3] = ( FLOAT ) ( ( UINT8 * ) pElement )[3];
            break;

        case D3DDECLTYPE_SHORT2:
            Data[0] = ( FLOAT ) ( ( INT16 * ) pElement )[0];
            Data[1] = ( FLOAT ) ( ( INT16 * ) pElement )[1];
            Data[2] = 0.0f;
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_SHORT4:
            Data[0] = ( FLOAT ) ( ( INT16 * ) pElement )[0];
            Data[1] = ( FLOAT ) ( ( INT16 * ) pElement )[1];
            Data[2] = ( FLOAT ) ( ( INT16 * ) pElement )[2];
            Data[3] = ( FLOAT ) ( ( INT16 * ) pElement )[3];
            break;

        case D3DDECLTYPE_UBYTE4N:
            Data[0] = ( 1.0f / 255.0f ) * ( FLOAT ) ( ( ( UINT8 * ) pElement )[0] );
            Data[1] = ( 1.0f / 255.0f ) * ( FLOAT ) ( ( ( UINT8 * ) pElement )[1] );
            Data[2] = ( 1.0f / 255.0f ) * ( FLOAT ) ( ( ( UINT8 * ) pElement )[2] );
            Data[3] = ( 1.0f / 255.0f ) * ( FLOAT ) ( ( ( UINT8 * ) pElement )[3] );
            break;

        case D3DDECLTYPE_SHORT2N:
 {
                INT16 nX = ( ( ( INT16 * ) pElement )[0] );
                INT16 nY = ( ( ( INT16 * ) pElement )[1] );

                nX += ( -32768 == nX );
                nY += ( -32768 == nY );

                Data[0] = ( 1.0f / 32767.0f ) * ( FLOAT ) nX;
                Data[1] = ( 1.0f / 32767.0f ) * ( FLOAT ) nY;
                Data[2] = 0.0f;
                Data[3] = 1.0f;
            }
            break;

        case D3DDECLTYPE_SHORT4N:
 {
                INT16 nX = ( ( ( INT16 * ) pElement )[0] );
                INT16 nY = ( ( ( INT16 * ) pElement )[1] );
                INT16 nZ = ( ( ( INT16 * ) pElement )[2] );
                INT16 nW = ( ( ( INT16 * ) pElement )[3] );

                nX += ( -32768 == nX );
                nY += ( -32768 == nY );
                nZ += ( -32768 == nZ );
                nW += ( -32768 == nW );

                Data[0] = ( 1.0f / 32767.0f ) * ( FLOAT ) nX;
                Data[1] = ( 1.0f / 32767.0f ) * ( FLOAT ) nY;
                Data[2] = ( 1.0f / 32767.0f ) * ( FLOAT ) nZ;
                Data[3] = ( 1.0f / 32767.0f ) * ( FLOAT ) nW;
            }
            break;

        case D3DDECLTYPE_USHORT2N:
            Data[0] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[0] );
            Data[1] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[1] );
            Data[2] = 0.0f;
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_USHORT4N:
            Data[0] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[0] );
            Data[1] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[1] );
            Data[2] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[2] );
            Data[3] = ( 1.0f / 65535.0f ) * ( FLOAT ) ( ( ( UINT16 * ) pElement )[3] );
            break;

        case D3DDECLTYPE_UDEC3:
            Data[0] = ( FLOAT ) ( ( *( ( UINT32 * ) pElement ) >>  0 ) & 0x3ff );
            Data[1] = ( FLOAT ) ( ( *( ( UINT32 * ) pElement ) >> 10 ) & 0x3ff );
            Data[2] = ( FLOAT ) ( ( *( ( UINT32 * ) pElement ) >> 20 ) & 0x3ff );
            Data[3] = 1.0f;
            break;

        case D3DDECLTYPE_DEC3N:
 {
                INT32 nX = ( ( *( ( INT32 * ) pElement ) << 22 ) ) >> 22;
                INT32 nY = ( ( *( ( INT32 * ) pElement ) << 12 ) ) >> 22;
                INT32 nZ = ( ( *( ( INT32 * ) pElement ) <<  2 ) ) >> 22;

                nX += ( -512 == nX );
                nY += ( -512 == nY );
                nZ += ( -512 == nZ );

                Data[0] = ( 1.0f / 511.0f ) * ( FLOAT ) nX;
                Data[1] = ( 1.0f / 511.0f ) * ( FLOAT ) nY;
                Data[2] = ( 1.0f / 511.0f ) * ( FLOAT ) nZ;
                Data[3] = 1.0f;
            }
            break;
        case D3DDECLTYPE_FLOAT16_2:
            D3DXFloat16To32Array( Data,( D3DXFLOAT16* )pElement,2 );
            Data[2] = 0.0f;
            Data[3] = 1.0f;
            break;
        case D3DDECLTYPE_FLOAT16_4:
            D3DXFloat16To32Array( Data,( D3DXFLOAT16* )pElement,4 );
            break;
        }
    }
    else
 {
        Data[0] = 0.0f;
        Data[1] = 0.0f;
        Data[2] = 0.0f;
        Data[3] = 1.0f;
    }

    if( cData > 4 )
        cData = 4;

    for( UINT i = 0; i < cData; i++ )
        pData[i] = Data[i];
}


VOID
CD3DXCrackDecl::Encode( CONST D3DVERTEXELEMENT9 *pElem, UINT index,CONST FLOAT* pData, UINT cData )
 {
    UINT i = 0;
    FLOAT Data[4];

    if( cData > 4 )
        cData = 4;

    switch( pElem->Type )
 {
    case D3DDECLTYPE_D3DCOLOR:
    case D3DDECLTYPE_UBYTE4N:
    case D3DDECLTYPE_USHORT2N:
    case D3DDECLTYPE_USHORT4N:
        for(; i < cData; i++ )
            Data[i] = ( 0.0f > pData[i] ) ? 0.0f : ( ( 1.0f < pData[i] ) ? 1.0f : pData[i] );
        break;

    case D3DDECLTYPE_SHORT2N:
    case D3DDECLTYPE_SHORT4N:
    case D3DDECLTYPE_DEC3N:
        for(; i < cData; i++ )
            Data[i] = ( -1.0f > pData[i] ) ? -1.0f : ( ( 1.0f < pData[i] ) ? 1.0f : pData[i] );
        break;

    case D3DDECLTYPE_UBYTE4:
    case D3DDECLTYPE_UDEC3:
        for(; i < cData; i++ )
            Data[i] = ( 0.0f > pData[i] ) ? 0.0f : pData[i];
        break;

    default:
        for(; i < cData; i++ )
            Data[i] = pData[i];
        break;
    }

    for(; i < 3; i++ )
        Data[i] = 0.0f;

    for(; i < 4; i++ )
        Data[i] = 1.0f;

    if( pElem )
 {
        LPVOID pElement = GetElementPointer( pElem,index );

        switch( pElem->Type )
 {
        case D3DDECLTYPE_FLOAT1:
            ( ( FLOAT * ) pElement )[0] = Data[0];
            break;

        case D3DDECLTYPE_FLOAT2:
            ( ( FLOAT * ) pElement )[0] = Data[0];
            ( ( FLOAT * ) pElement )[1] = Data[1];
            break;

        case D3DDECLTYPE_FLOAT3:
            ( ( FLOAT * ) pElement )[0] = Data[0];
            ( ( FLOAT * ) pElement )[1] = Data[1];
            ( ( FLOAT * ) pElement )[2] = Data[2];
            break;

        case D3DDECLTYPE_FLOAT4:
            ( ( FLOAT * ) pElement )[0] = Data[0];
            ( ( FLOAT * ) pElement )[1] = Data[1];
            ( ( FLOAT * ) pElement )[2] = Data[2];
            ( ( FLOAT * ) pElement )[3] = Data[3];
            break;

        case D3DDECLTYPE_D3DCOLOR:
            ( ( D3DCOLOR * ) pElement )[0] =
                ( ( ( D3DCOLOR ) ( Data[0] * 255.0f + 0.5f ) & 0xff ) << 16 ) |
                ( ( ( D3DCOLOR ) ( Data[1] * 255.0f + 0.5f ) & 0xff ) <<  8 ) |
                ( ( ( D3DCOLOR ) ( Data[2] * 255.0f + 0.5f ) & 0xff ) <<  0 ) |
                ( ( ( D3DCOLOR ) ( Data[3] * 255.0f + 0.5f ) & 0xff ) << 24 );
            break;

        case D3DDECLTYPE_UBYTE4:
            ( ( UINT8 * ) pElement )[0] = ( UINT8 ) ( Data[0] + 0.5f );
            ( ( UINT8 * ) pElement )[1] = ( UINT8 ) ( Data[1] + 0.5f );
            ( ( UINT8 * ) pElement )[2] = ( UINT8 ) ( Data[2] + 0.5f );
            ( ( UINT8 * ) pElement )[3] = ( UINT8 ) ( Data[3] + 0.5f );
            break;

        case D3DDECLTYPE_SHORT2:
            ( ( INT16 * ) pElement )[0] = ( INT16 ) ( Data[0] + 0.5f );
            ( ( INT16 * ) pElement )[1] = ( INT16 ) ( Data[1] + 0.5f );
            break;

        case D3DDECLTYPE_SHORT4:
            ( ( INT16 * ) pElement )[0] = ( INT16 ) ( Data[0] + 0.5f );
            ( ( INT16 * ) pElement )[1] = ( INT16 ) ( Data[1] + 0.5f );
            ( ( INT16 * ) pElement )[2] = ( INT16 ) ( Data[2] + 0.5f );
            ( ( INT16 * ) pElement )[3] = ( INT16 ) ( Data[3] + 0.5f );
            break;

        case D3DDECLTYPE_UBYTE4N:
            ( ( UINT8 * ) pElement )[0] = ( UINT8 ) ( Data[0] * 255.0f + 0.5f );
            ( ( UINT8 * ) pElement )[1] = ( UINT8 ) ( Data[1] * 255.0f + 0.5f );
            ( ( UINT8 * ) pElement )[2] = ( UINT8 ) ( Data[2] * 255.0f + 0.5f );
            ( ( UINT8 * ) pElement )[3] = ( UINT8 ) ( Data[3] * 255.0f + 0.5f );
            break;

        case D3DDECLTYPE_SHORT2N:
            ( ( INT16 * ) pElement )[0] = ( INT16 ) ( Data[0] * 32767.0f + 0.5f );
            ( ( INT16 * ) pElement )[1] = ( INT16 ) ( Data[1] * 32767.0f + 0.5f );
            break;

        case D3DDECLTYPE_SHORT4N:
            ( ( INT16 * ) pElement )[0] = ( INT16 ) ( Data[0] * 32767.0f + 0.5f );
            ( ( INT16 * ) pElement )[1] = ( INT16 ) ( Data[1] * 32767.0f + 0.5f );
            ( ( INT16 * ) pElement )[2] = ( INT16 ) ( Data[2] * 32767.0f + 0.5f );
            ( ( INT16 * ) pElement )[3] = ( INT16 ) ( Data[3] * 32767.0f + 0.5f );
            break;

        case D3DDECLTYPE_USHORT2N:
            ( ( UINT16 * ) pElement )[0] = ( UINT16 ) ( Data[0] * 65535.0f + 0.5f );
            ( ( UINT16 * ) pElement )[1] = ( UINT16 ) ( Data[1] * 65535.0f + 0.5f );
            break;

        case D3DDECLTYPE_USHORT4N:
            ( ( UINT16 * ) pElement )[0] = ( UINT16 ) ( Data[0] * 65535.0f + 0.5f );
            ( ( UINT16 * ) pElement )[1] = ( UINT16 ) ( Data[1] * 65535.0f + 0.5f );
            ( ( UINT16 * ) pElement )[2] = ( UINT16 ) ( Data[2] * 65535.0f + 0.5f );
            ( ( UINT16 * ) pElement )[3] = ( UINT16 ) ( Data[3] * 65535.0f + 0.5f );
            break;

        case D3DDECLTYPE_UDEC3:
            ( ( UINT32 * ) pElement )[0] =
                ( ( ( UINT32 ) ( Data[0] + 0.5f ) & 0x3ff ) <<  0 ) |
                ( ( ( UINT32 ) ( Data[1] + 0.5f ) & 0x3ff ) << 10 ) |
                ( ( ( UINT32 ) ( Data[2] + 0.5f ) & 0x3ff ) << 20 );
            break;

        case D3DDECLTYPE_DEC3N:
            ( ( UINT32 * ) pElement )[0] =
                ( ( ( UINT32 ) ( Data[0] * 511.0f + 0.5f ) & 0x3ff ) <<  0 ) |
                ( ( ( UINT32 ) ( Data[1] * 511.0f + 0.5f ) & 0x3ff ) << 10 ) |
                ( ( ( UINT32 ) ( Data[2] * 511.0f + 0.5f ) & 0x3ff ) << 20 );
            break;
        case D3DDECLTYPE_FLOAT16_2:
            D3DXFloat32To16Array( ( D3DXFLOAT16* )pElement,Data,2 );
            break;
        case D3DDECLTYPE_FLOAT16_4:
            D3DXFloat32To16Array( ( D3DXFLOAT16* )pElement,Data,4 );
            break;
        }
    }
}
