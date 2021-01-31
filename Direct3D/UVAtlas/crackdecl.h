//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2000 Microsoft Corporation.  All Rights Reserved.
//
//  File:       crackdecl.h
//  Content:    Used to access vertex data using Declarations or FVFs
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __CRACKDECL_H__
#define __CRACKDECL_H__

#include <d3dx9.h>

//----------------------------------------------------------------------------
// CD3DXCrackDecl
//----------------------------------------------------------------------------

class CD3DXCrackDecl
{
protected:

    //just a pointer to the Decl! No data stored!
    CONST
    D3DVERTEXELEMENT9* pElements;
    DWORD dwNumElements;

    //still need the stream pointer though
    //and the strides
    LPBYTE          pStream[16];
    DWORD           dwStride[16];

public:

                    CD3DXCrackDecl();


    HRESULT SetDeclaration( CONST D3DVERTEXELEMENT9 *pDecl );
    HRESULT         SetStreamSource( UINT Stream, LPVOID pData, UINT Stride );

    // Get
    inline UINT     GetVertexStride( UINT Stream );


    inline UINT GetFields( CONST D3DVERTEXELEMENT9* );

    inline LPBYTE   GetVertex( UINT Stream, UINT Index );
    inline LPBYTE GetElementPointer( CONST D3DVERTEXELEMENT9 *Element,UINT Index );


    inline CONST
    D3DVERTEXELEMENT9* GetSemanticElement( UINT Usage, UINT UsageIndex );
    //simple function that gives part of the decl back 
    inline CONST
    D3DVERTEXELEMENT9* GetIndexElement( UINT Index );

    // Encode/Decode
    VOID Decode( CONST D3DVERTEXELEMENT9 *, UINT Index, FLOAT* pData, UINT cData );
    VOID Encode( CONST D3DVERTEXELEMENT9 *, UINT Index, CONST FLOAT* pData, UINT cData );

    inline VOID     DecodeSemantic( UINT Usage, UINT UsageIndex, UINT VertexIndex, FLOAT* pData, UINT cData );
    inline VOID     EncodeSemantic( UINT Usage, UINT UsageIndex, UINT VertexIndex, FLOAT* pData, UINT cData );

    inline CONST
    D3DVERTEXELEMENT9* GetElements()
    {
        return pElements;
    };
    inline DWORD    GetNumElements()
    {
        return dwNumElements;
    };

    CONST
    WCHAR* DeclUsageToString( D3DDECLUSAGE usage );
};


//----------------------------------------------------------------------------
// CD3DXCrackDecl inline methods
//----------------------------------------------------------------------------
inline CONST
D3DVERTEXELEMENT9* CD3DXCrackDecl::GetIndexElement( UINT Index )
{
    if( Index < dwNumElements )
        return &pElements[Index];
    else
        return NULL;
}

inline CONST
D3DVERTEXELEMENT9* CD3DXCrackDecl::GetSemanticElement( UINT Usage, UINT UsageIndex )
{
    CONST D3DVERTEXELEMENT9* pPlace = pElements;
    while( pPlace->Stream != 0xFF )
    {
        if( pPlace->Usage == Usage &&
            pPlace->UsageIndex == UsageIndex )
            return pPlace;
        pPlace++;
    }
    return NULL;
}

inline VOID CD3DXCrackDecl::DecodeSemantic( UINT Usage, UINT UsageIndex, UINT VertexIndex, FLOAT* pData, UINT cData )
{
    Decode( GetSemanticElement( Usage, UsageIndex ), VertexIndex, pData, cData );
}

inline VOID CD3DXCrackDecl::EncodeSemantic( UINT Usage, UINT UsageIndex, UINT VertexIndex, FLOAT* pData, UINT cData )
{
    Encode( GetSemanticElement( Usage, UsageIndex ), VertexIndex, pData, cData );
}


inline UINT CD3DXCrackDecl::GetVertexStride( UINT Stream )
{
    return dwStride[Stream];
}

const BYTE x_rgcbFields[] =
{
    1, // D3DDECLTYPE_FLOAT1,        // 1D float expanded to (value, 0., 0., 1.)
    2, // D3DDECLTYPE_FLOAT2,        // 2D float expanded to (value, value, 0., 1.)
    3, // D3DDECLTYPE_FLOAT3,       / 3D float expanded to (value, value, value, 1.)
    4, // D3DDECLTYPE_FLOAT4,       / 4D float
    4, // D3DDECLTYPE_D3DCOLOR,      // 4D packed unsigned bytes mapped to 0. to 1. range
    //                      // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
    4, // D3DDECLTYPE_UBYTE4,        // 4D unsigned byte
    2, // D3DDECLTYPE_SHORT2,        // 2D signed short expanded to (value, value, 0., 1.)
    4, // D3DDECLTYPE_SHORT4         // 4D signed short

    4, // D3DDECLTYPE_UBYTE4N,       // Each of 4 bytes is normalized by dividing to 255.0
    2, // D3DDECLTYPE_SHORT2N,       // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
    4, // D3DDECLTYPE_SHORT4N,       // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
    2, // D3DDECLTYPE_USHORT2N,      // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
    4, // D3DDECLTYPE_USHORT4N,      // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
    3, // D3DDECLTYPE_UDEC3,         // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
    3, // D3DDECLTYPE_DEC3N,         // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
    2, // D3DDECLTYPE_FLOAT16_2,     // 2D 16 bit float expanded to (value, value, 0, 1 )
    4, // D3DDECLTYPE_FLOAT16_4,     // 4D 16 bit float 
    0, // D3DDECLTYPE_UNKNOWN,       // Unknown
};


inline UINT
CD3DXCrackDecl::GetFields( CONST D3DVERTEXELEMENT9 *pElement )
 {
    if( pElement->Type <= D3DDECLTYPE_FLOAT16_4 )
        return x_rgcbFields[pElement->Type];

    return 0;
}



inline LPBYTE CD3DXCrackDecl::GetVertex( UINT Stream, UINT Index )
{
    return pStream[Stream] + dwStride[Stream] * Index;
}


inline LPBYTE
CD3DXCrackDecl::GetElementPointer( CONST D3DVERTEXELEMENT9 *pElement, UINT Index )
 {
    return GetVertex( pElement->Stream, Index ) + pElement->Offset;
}


inline BOOL BIdenticalDecls( CONST D3DVERTEXELEMENT9 *pDecl1, CONST D3DVERTEXELEMENT9 *pDecl2 )
 {
    CONST D3DVERTEXELEMENT9 *pCurSrc = pDecl1;
    CONST D3DVERTEXELEMENT9 *pCurDest = pDecl2;
    while( ( pCurSrc->Stream != 0xff ) && ( pCurDest->Stream != 0xff ) )
 {
        if( ( pCurDest->Stream != pCurSrc->Stream )
            && ( pCurDest->Offset != pCurSrc->Offset )
            || ( pCurDest->Type != pCurSrc->Type )
            || ( pCurDest->Method != pCurSrc->Method )
            || ( pCurDest->Usage != pCurSrc->Usage )
            || ( pCurDest->UsageIndex != pCurSrc->UsageIndex ) )
            break;

        pCurSrc++;
        pCurDest++;
    }

    // it is the same decl if reached the end at the same time on both decls
    return ( ( pCurSrc->Stream == 0xff ) && ( pCurDest->Stream == 0xff ) );
}

inline void CopyDecls( D3DVERTEXELEMENT9 *pDest, CONST D3DVERTEXELEMENT9 *pSrc )
 {
    while( pSrc->Stream != 0xff )
 {
        memcpy( pDest, pSrc, sizeof( D3DVERTEXELEMENT9 ) );

        pSrc++;
        pDest++;
    }
    memcpy( pDest, pSrc, sizeof( D3DVERTEXELEMENT9 ) );
}

const UINT x_cTypeSizes = D3DDECLTYPE_FLOAT16_4;

const BYTE x_rgcbTypeSizes[] =
{
    4, // D3DDECLTYPE_FLOAT1,        // 1D float expanded to (value, 0., 0., 1.)
    8, // D3DDECLTYPE_FLOAT2,        // 2D float expanded to (value, value, 0., 1.)
    12, // D3DDECLTYPE_FLOAT3,       / 3D float expanded to (value, value, value, 1.)
    16, // D3DDECLTYPE_FLOAT4,       / 4D float
    4, // D3DDECLTYPE_D3DCOLOR,      // 4D packed unsigned bytes mapped to 0. to 1. range
    //                      // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
    4, // D3DDECLTYPE_UBYTE4,        // 4D unsigned byte
    4, // D3DDECLTYPE_SHORT2,        // 2D signed short expanded to (value, value, 0., 1.)
    8, // D3DDECLTYPE_SHORT4         // 4D signed short

    4, // D3DDECLTYPE_UBYTE4N,       // Each of 4 bytes is normalized by dividing to 255.0
    4, // D3DDECLTYPE_SHORT2N,       // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
    8, // D3DDECLTYPE_SHORT4N,       // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
    4, // D3DDECLTYPE_USHORT2N,      // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
    8, // D3DDECLTYPE_USHORT4N,      // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
    4, // D3DDECLTYPE_UDEC3,         // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
    4, // D3DDECLTYPE_DEC3N,         // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
    4, // D3DDECLTYPE_FLOAT16_2,     // 2D 16 bit float expanded to (value, value, 0, 1 )
    8, // D3DDECLTYPE_FLOAT16_4,     // 4D 16 bit float 
    0, // D3DDECLTYPE_UNUSED,        // Unused
};

inline CONST D3DVERTEXELEMENT9* GetDeclElement( CONST D3DVERTEXELEMENT9 *pDecl, BYTE Usage, BYTE UsageIndex )
 {
    while( pDecl->Stream != 0xff )
 {
        if( ( pDecl->Usage == Usage ) && ( pDecl->UsageIndex == UsageIndex ) )
 {
            return pDecl;
        }

        pDecl++;
    }

    return NULL;
}

inline void RemoveDeclElement( BYTE Usage, BYTE UsageIndex, D3DVERTEXELEMENT9 pDecl[MAX_FVF_DECL_SIZE] )
{
    LPD3DVERTEXELEMENT9 pCur;
    LPD3DVERTEXELEMENT9 pPrev;
    BYTE cbElementSize;

    pCur = pDecl;
    while( pCur->Stream != 0xff )
    {
        if( ( pCur->Usage == Usage ) && ( pCur->UsageIndex == UsageIndex ) )
        {
            break;
        }
        pCur++;
    }

    //. if we found one to remove, then remove it
    if( pCur->Stream != 0xff )
    {
        cbElementSize = x_rgcbTypeSizes[pCur->Type];;

        pPrev = pCur;
        pCur++;
        while( pCur->Stream != 0xff )
        {
            memcpy( pPrev, pCur, sizeof( D3DVERTEXELEMENT9 ) );

            pPrev->Offset -= cbElementSize;

            pPrev++;
            pCur++;
        }

        // copy the end of stream down one
        memcpy( pPrev, pCur, sizeof( D3DVERTEXELEMENT9 ) );
    }

}

// NOTE: size checking of array should happen OUTSIDE this function!
inline void AppendDeclElement( CONST D3DVERTEXELEMENT9 *pNew, D3DVERTEXELEMENT9 pDecl[MAX_FVF_DECL_SIZE] )
 {
    LPD3DVERTEXELEMENT9 pCur;
    BYTE cbOffset;

    pCur = pDecl;
    cbOffset = 0;
    while( ( pCur->Stream != 0xff ) )
 {
        cbOffset += x_rgcbTypeSizes[pCur->Type];

        pCur++;
    }

    // NOTE: size checking of array should happen OUTSIDE this function!
    assert( pCur - pDecl + 1 < MAX_FVF_DECL_SIZE );

    // move the end of the stream down one
    memcpy( pCur+1, pCur, sizeof( D3DVERTEXELEMENT9 ) );

    // copy the new element in and update the offset
    memcpy( pCur, pNew, sizeof( D3DVERTEXELEMENT9 ) );
    pCur->Offset = cbOffset;
}

// NOTE: size checking of array should happen OUTSIDE this function!
inline void InsertDeclElement( UINT iInsertBefore, CONST D3DVERTEXELEMENT9 *pNew, D3DVERTEXELEMENT9 pDecl[MAX_FVF_DECL_SIZE] )
 {
    LPD3DVERTEXELEMENT9 pCur;
    BYTE cbOffset;
    BYTE cbNewOffset;
    D3DVERTEXELEMENT9   TempElement1;
    D3DVERTEXELEMENT9   TempElement2;
    UINT iCur;

    pCur = pDecl;
    cbOffset = 0;
    iCur = 0;
    while( ( pCur->Stream != 0xff ) && ( iCur < iInsertBefore ) )
 {
        cbOffset += x_rgcbTypeSizes[pCur->Type];

        pCur++;
        iCur++;
    }

    // NOTE: size checking of array should happen OUTSIDE this function!
    assert( pCur - pDecl + 1 < MAX_FVF_DECL_SIZE );

    // if we hit the end, just append
    if( pCur->Stream == 0xff )
 {
    // move the end of the stream down one
        memcpy( pCur+1, pCur, sizeof( D3DVERTEXELEMENT9 ) );

    // copy the new element in and update the offset
        memcpy( pCur, pNew, sizeof( D3DVERTEXELEMENT9 ) );
        pCur->Offset = cbOffset;

    }
    else  // insert in the middle
 {
    // save off the offset for the new decl
        cbNewOffset = cbOffset;

    // calculate the offset for the first element shifted up
        cbOffset += x_rgcbTypeSizes[pNew->Type];

    // save off the first item to move, data so that we can copy the new element in
        memcpy( &TempElement1, pCur, sizeof( D3DVERTEXELEMENT9 ) );

    // copy the new element in
        memcpy( pCur, pNew, sizeof( D3DVERTEXELEMENT9 ) );
        pCur->Offset = cbNewOffset;

    // advance pCur one because we effectively did an iteration of the loop adding the new element
        pCur++;

        while( pCur->Stream != 0xff )
 {
    // save off the current element
            memcpy( &TempElement2, pCur, sizeof( D3DVERTEXELEMENT9 ) );

    // update the current element with the previous's value which was stored in TempElement1
            memcpy( pCur, &TempElement1, sizeof( D3DVERTEXELEMENT9 ) );

    // move the current element's value into TempElement1 for the next iteration
            memcpy( &TempElement1, &TempElement2, sizeof( D3DVERTEXELEMENT9 ) );

            pCur->Offset = cbOffset;
            cbOffset += x_rgcbTypeSizes[pCur->Type];

            pCur++;
        }

    // now we exited one element, need to move the end back one and copy in the last element
    // move the end element back one
        memcpy( pCur + 1, pCur, sizeof( D3DVERTEXELEMENT9 ) );

    // copy the prev element's data out of the temp and into the last element
        memcpy( pCur, &TempElement1, sizeof( D3DVERTEXELEMENT9 ) );
        pCur->Offset = cbOffset;
    }
}
#endif // __CRACKDECL_H__
