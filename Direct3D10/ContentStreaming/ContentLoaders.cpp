//--------------------------------------------------------------------------------------
// File: ContentLoaders.cpp
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "ContentLoaders.h"
#include "AsyncLoader.h"
#include "PackedFile.h"

//--------------------------------------------------------------------------------------
CTextureLoader::CTextureLoader( WCHAR* szFileName, CPackedFile* pPackedFile ) : m_pData( NULL ),
                                                                                m_cBytes( 0 ),
                                                                                m_pPackedFile( pPackedFile )
{
    wcscpy_s( m_szFileName, MAX_PATH, szFileName );
}

//--------------------------------------------------------------------------------------
CTextureLoader::~CTextureLoader()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// The SDK uses only DXTn (BCn) textures with a few small non-compressed texture.  However,
// for a game that uses compressed textures or textures in a zip file, this is the place
// to decompress them.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureLoader::Decompress( void** ppData, SIZE_T* pcBytes )
{
    *ppData = ( void* )m_pData;
    *pcBytes = m_cBytes;
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureLoader::Destroy()
{
    if( !m_pPackedFile->UsingMemoryMappedIO() )
    {
        SAFE_DELETE_ARRAY( m_pData );
    }
    m_cBytes = 0;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Load the texture from the packed file.  If not-memory mapped, allocate enough memory
// to hold the data.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureLoader::Load()
{
    if( m_pPackedFile->UsingMemoryMappedIO() )
    {
        if( !m_pPackedFile->GetPackedFile( m_szFileName, &m_pData, &m_cBytes ) )
            return E_FAIL;
    }
    else
    {
        if( !m_pPackedFile->GetPackedFileInfo( m_szFileName, &m_cBytes ) )
            return E_FAIL;

        // create enough space for the file data
        m_pData = new BYTE[ m_cBytes ];
        if( !m_pData )
            return E_OUTOFMEMORY;

        if( !m_pPackedFile->GetPackedFile( m_szFileName, &m_pData, &m_cBytes ) )
            return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// This is a private function that either Locks and copies the data (D3D9) or calls
// UpdateSubresource (D3D10).
//--------------------------------------------------------------------------------------
BOOL CTextureProcessor::PopulateTexture()
{
    if( 0 == m_iNumLockedPtrs )
        return FALSE;

    DDS_HEADER* pSurfDesc9 = ( DDS_HEADER* )( ( BYTE* )m_pData + sizeof( DWORD ) );

    UINT Width = pSurfDesc9->dwWidth;
    UINT Height = pSurfDesc9->dwHeight;
    UINT MipLevels = pSurfDesc9->dwMipMapCount;
    if( MipLevels > m_SkipMips )
        MipLevels -= m_SkipMips;
    else
        m_SkipMips = 0;
    if( 0 == MipLevels )
        MipLevels = 1;
    D3DFORMAT Format = GetD3D9Format( pSurfDesc9->ddspf );

    // Skip X number of mip levels
    UINT BytesToSkip = 0;
    for( UINT i = 0; i < m_SkipMips; i++ )
    {
        UINT SurfaceBytes;
        GetSurfaceInfo( Width, Height, Format, &SurfaceBytes, NULL, NULL );

        BytesToSkip += SurfaceBytes;
        Width = Width >> 1;
        Height = Height >> 1;
    }

    void* pTexData = ( BYTE* )m_pData + sizeof( DWORD ) + sizeof( DDS_HEADER ) + BytesToSkip;

    // Lock, fill, unlock
    UINT NumBytes, RowBytes, NumRows;
    BYTE* pSrcBits = ( BYTE* )pTexData;

    ID3D10Resource* pTexture10 = NULL;
    if( LDT_D3D10 == m_Device.Type )
    {
        m_pRealRV10->GetResource( &pTexture10 );
    }

    for( UINT i = 0; i < m_iNumLockedPtrs; i++ )
    {
        GetSurfaceInfo( Width, Height, Format, &NumBytes, &RowBytes, &NumRows );

        if( LDT_D3D10 == m_Device.Type )
        {
#if defined(USE_D3D10_STAGING_RESOURCES)
            BYTE* pDestBits = ( BYTE* )m_pLockedRects10[i].pData;

            // Copy stride line by line
            for( UINT h = 0; h < NumRows; h++ )
            {
                CopyMemory( pDestBits, pSrcBits, RowBytes );
                pDestBits += m_pLockedRects10[i].RowPitch;
                pSrcBits += RowBytes;
            }
#endif

#if !defined(USE_D3D10_STAGING_RESOURCES)
            // Use UpdateSubresource in d3d10
            m_Device.pDev10->UpdateSubresource( pTexture10, D3D10CalcSubresource( i, 0, MipLevels ), NULL, pSrcBits, RowBytes, 0 );
            pSrcBits += NumBytes;
#endif
        }
        else if( LDT_D3D9 == m_Device.Type )
        {
            BYTE* pDestBits = ( BYTE* )m_pLockedRects[i].pBits;

            // Copy stride line by line
            for( UINT h = 0; h < NumRows; h++ )
            {
                CopyMemory( pDestBits, pSrcBits, RowBytes );
                pDestBits += m_pLockedRects[i].Pitch;
                pSrcBits += RowBytes;
            }
        }

        Width = Width >> 1;
        Height = Height >> 1;
        if( Width == 0 )
            Width = 1;
        if( Height == 0 )
            Height = 1;
    }

    if( LDT_D3D10 == m_Device.Type )
    {
        SAFE_RELEASE( pTexture10 );
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
CTextureProcessor::CTextureProcessor( ID3D10Device* pDevice,
                                      ID3D10ShaderResourceView** ppRV10,
                                      CResourceReuseCache* pResourceReuseCache,
                                      UINT SkipMips ) : m_Device( pDevice ),
                                                        m_ppRV10( ppRV10 ),
                                                        m_ppTexture9( NULL ),
                                                        m_pResourceReuseCache( pResourceReuseCache ),
                                                        m_SkipMips( SkipMips )
{
    *m_ppRV10 = NULL;
}

//--------------------------------------------------------------------------------------
CTextureProcessor::CTextureProcessor( IDirect3DDevice9* pDevice,
                                      IDirect3DTexture9** ppTexture9,
                                      CResourceReuseCache* pResourceReuseCache,
                                      UINT SkipMips ) : m_Device( pDevice ),
                                                        m_ppRV10( NULL ),
                                                        m_ppTexture9( ppTexture9 ),
                                                        m_pResourceReuseCache( pResourceReuseCache ),
                                                        m_SkipMips( SkipMips )
{
    *m_ppTexture9 = NULL;
}

//--------------------------------------------------------------------------------------
CTextureProcessor::~CTextureProcessor()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// LockDeviceObject is called by the graphics thread to find an appropriate resource from
// the resource reuse cache.  If no resource is found, the return code tells the calling
// thread to try again later.  For D3D9, this function also locks all mip-levels.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureProcessor::LockDeviceObject()
{
    HRESULT hr = S_OK;
    m_iNumLockedPtrs = 0;

    if( !m_pResourceReuseCache )
        return E_FAIL;

    // setup the pointers in the process request
    DDS_HEADER* pSurfDesc9 = ( DDS_HEADER* )( ( BYTE* )m_pData + sizeof( DWORD ) );

    UINT Width = pSurfDesc9->dwWidth;
    UINT Height = pSurfDesc9->dwHeight;
    UINT MipLevels = pSurfDesc9->dwMipMapCount;
    if( MipLevels > m_SkipMips )
        MipLevels -= m_SkipMips;
    else
        m_SkipMips = 0;
    if( 0 == MipLevels )
        MipLevels = 1;
    D3DFORMAT Format = GetD3D9Format( pSurfDesc9->ddspf );

    // Skip X number of mip levels
    for( UINT i = 0; i < m_SkipMips; i++ )
    {
        Width = Width >> 1;
        Height = Height >> 1;
    }

    // Find an appropriate resource
    if( LDT_D3D10 == m_Device.Type )
    {
        m_pRealRV10 = m_pResourceReuseCache->GetFreeTexture10( Width, Height,
                                                               MipLevels, ( UINT )Format, &m_pStaging10 );

        if( NULL == m_pRealRV10 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
#if defined(USE_D3D10_STAGING_RESOURCES)
            // Lock
            m_iNumLockedPtrs = MipLevels - m_SkipMips;
            for( UINT i = 0; i < m_iNumLockedPtrs; i++ )
            {
                hr = m_pStaging10->Map( i, D3D10_MAP_WRITE, 0, &m_pLockedRects10[i] );
                if( FAILED( hr ) )
                {
                    m_iNumLockedPtrs = 0;
                    *m_ppRV10 = ( ID3D10ShaderResourceView* )ERROR_RESOURCE_VALUE;
                    return hr;
                }
            }
#endif

#if !defined(USE_D3D10_STAGING_RESOURCES)
            m_iNumLockedPtrs = MipLevels;
            hr = S_OK;
#endif
        }
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        m_pRealTexture9 = m_pResourceReuseCache->GetFreeTexture9( Width, Height, MipLevels, ( UINT )Format );

        if( NULL == m_pRealTexture9 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
            // Lock
            m_iNumLockedPtrs = MipLevels - m_SkipMips;
            for( UINT i = 0; i < m_iNumLockedPtrs; i++ )
            {
                hr = m_pRealTexture9->LockRect( i, &m_pLockedRects[i], NULL, 0 );
                if( FAILED( hr ) )
                {
                    m_iNumLockedPtrs = 0;
                    *m_ppTexture9 = ( IDirect3DTexture9* )ERROR_RESOURCE_VALUE;
                    return hr;
                }
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// On D3D9, this unlocks the resource.  On D3D10, this actually populates the resource.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureProcessor::UnLockDeviceObject()
{
    HRESULT hr = S_OK;

    if( 0 == m_iNumLockedPtrs )
        return E_FAIL;

    // Find an appropriate resource
    if( LDT_D3D10 == m_Device.Type )
    {
#if defined(USE_D3D10_STAGING_RESOURCES)
        // Unlock
        for( UINT i = 0; i < m_iNumLockedPtrs; i++ )
        {
            m_pStaging10->Unmap( i );
        }
        ID3D10Resource* pDest;
        m_pRealRV10->GetResource( &pDest );
        m_Device.pDev10->CopyResource( pDest, m_pStaging10 );
        SAFE_RELEASE( pDest );
#endif

#if !defined(USE_D3D10_STAGING_RESOURCES)
        if( !PopulateTexture() )
            hr = E_FAIL;
#endif
        *m_ppRV10 = m_pRealRV10;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        // Unlock
        for( UINT i = 0; i < m_iNumLockedPtrs; i++ )
        {
            hr = m_pRealTexture9->UnlockRect( i );
            if( FAILED( hr ) )
            {
                return hr;
            }
        }

        *m_ppTexture9 = m_pRealTexture9;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureProcessor::Destroy()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Any texture processing would go here.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureProcessor::Process( void* pData, SIZE_T cBytes )
{
    HRESULT hr = S_OK;

    if( m_pResourceReuseCache )
    {	
        DWORD dwMagicNumber = *( DWORD* )pData;
        if( dwMagicNumber != 0x20534444 )
            hr = E_FAIL;
    }

    m_pData = ( BYTE* )pData;
    m_cBytes = cBytes;

    return hr;
}

//--------------------------------------------------------------------------------------
// Copies the data to the locked pointer on D3D9
//--------------------------------------------------------------------------------------
HRESULT WINAPI CTextureProcessor::CopyToResource()
{
    HRESULT hr = S_OK;

#if 0
    if( LDT_D3D9 == m_Device.Type )
#endif
    {
        if( !PopulateTexture() )
        {
            hr = E_FAIL;
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
void WINAPI CTextureProcessor::SetResourceError()
{
    if( LDT_D3D10 == m_Device.Type )
    {
        *m_ppRV10 = ( ID3D10ShaderResourceView* )ERROR_RESOURCE_VALUE;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        *m_ppTexture9 = ( IDirect3DTexture9* )ERROR_RESOURCE_VALUE;
    }
}

//--------------------------------------------------------------------------------------
CVertexBufferLoader::CVertexBufferLoader()
{
}
CVertexBufferLoader::~CVertexBufferLoader()
{
    Destroy();
}
HRESULT WINAPI CVertexBufferLoader::Decompress( void** ppData, SIZE_T* pcBytes )
{
    *ppData = NULL; *pcBytes = 0; return S_OK;
}
HRESULT WINAPI CVertexBufferLoader::Destroy()
{
    return S_OK;
}
HRESULT WINAPI CVertexBufferLoader::Load()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
CVertexBufferProcessor::CVertexBufferProcessor( ID3D10Device* pDevice,
                                                ID3D10Buffer** ppBuffer10,
                                                D3D10_BUFFER_DESC* pBufferDesc,
                                                void* pData,
                                                CResourceReuseCache* pResourceReuseCache ) : m_Device( pDevice ),
                                                                                             m_ppBuffer10(
                                                                                             ppBuffer10 ),
                                                                                             m_ppBuffer9( NULL ),
                                                                                             m_iSizeBytes( 0 ),
                                                                                             m_Usage( 0 ),
                                                                                             m_FVF( 0 ),
                                                                                             m_Pool( D3DPOOL_DEFAULT ),
                                                                                             m_pData( pData ),
                                                                                             m_pResourceReuseCache(
                                                                                             pResourceReuseCache )
{
    *m_ppBuffer10 = NULL;
    CopyMemory( &m_BufferDesc, pBufferDesc, sizeof( D3D10_BUFFER_DESC ) );
}

//--------------------------------------------------------------------------------------
CVertexBufferProcessor::CVertexBufferProcessor( IDirect3DDevice9* pDevice,
                                                IDirect3DVertexBuffer9** ppBuffer9,
                                                UINT iSizeBytes,
                                                DWORD Usage,
                                                DWORD FVF,
                                                D3DPOOL Pool,
                                                void* pData,
                                                CResourceReuseCache* pResourceReuseCache ) : m_Device( pDevice ),
                                                                                             m_ppBuffer10( NULL ),
                                                                                             m_ppBuffer9( ppBuffer9 ),
                                                                                             m_iSizeBytes(
                                                                                             iSizeBytes ),
                                                                                             m_Usage( Usage ),
                                                                                             m_FVF( FVF ),
                                                                                             m_Pool( Pool ),
                                                                                             m_pData( pData ),
                                                                                             m_pResourceReuseCache(
                                                                                             pResourceReuseCache )
{
    *m_ppBuffer9 = NULL;
}

//--------------------------------------------------------------------------------------
CVertexBufferProcessor::~CVertexBufferProcessor()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// LockDeviceObject is called by the graphics thread to find an appropriate resource from
// the resource reuse cache.  If no resource is found, the return code tells the calling
// thread to try again later.  For D3D9, this function also locks the resource.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CVertexBufferProcessor::LockDeviceObject()
{
    HRESULT hr = S_OK;
    if( !m_pResourceReuseCache )
        return E_FAIL;

    if( LDT_D3D10 == m_Device.Type )
    {
        m_pRealBuffer10 = m_pResourceReuseCache->GetFreeVB10( m_BufferDesc.ByteWidth );
        if( NULL == m_pRealBuffer10 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
            hr = S_OK;
        }
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        m_pRealBuffer9 = m_pResourceReuseCache->GetFreeVB9( m_iSizeBytes );
        if( NULL == m_pRealBuffer9 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
            // Lock
            hr = m_pRealBuffer9->Lock( 0, 0, ( void** )&m_pLockedData, 0 );
            if( FAILED( hr ) )
            {
                m_pLockedData = NULL;
                return hr;
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// On D3D9, this unlocks the resource.  On D3D10, this actually populates the resource.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CVertexBufferProcessor::UnLockDeviceObject()
{
    HRESULT hr = S_OK;

    if( LDT_D3D10 == m_Device.Type )
    {
        m_Device.pDev10->UpdateSubresource( m_pRealBuffer10, 0, NULL, m_pData, 0, 0 );
        *m_ppBuffer10 = m_pRealBuffer10;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        if( !m_pLockedData )
            return E_FAIL;

        // Unlock
        hr = m_pRealBuffer9->Unlock();
        *m_ppBuffer9 = m_pRealBuffer9;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CVertexBufferProcessor::Destroy()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CVertexBufferProcessor::Process( void* pData, SIZE_T cBytes )
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Copies the data to the locked pointer on D3D9
//--------------------------------------------------------------------------------------
HRESULT WINAPI CVertexBufferProcessor::CopyToResource()
{
    if( LDT_D3D9 == m_Device.Type )
    {
        if( !m_pLockedData )
        {
            return E_FAIL;
        }

        CopyMemory( m_pLockedData, m_pData, m_iSizeBytes );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
void WINAPI CVertexBufferProcessor::SetResourceError()
{
    if( LDT_D3D10 == m_Device.Type )
    {
        *m_ppBuffer10 = ( ID3D10Buffer* )ERROR_RESOURCE_VALUE;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        *m_ppBuffer9 = ( IDirect3DVertexBuffer9* )ERROR_RESOURCE_VALUE;
    }
}

//--------------------------------------------------------------------------------------
CIndexBufferLoader::CIndexBufferLoader()
{
}
CIndexBufferLoader::~CIndexBufferLoader()
{
    Destroy();
}
HRESULT WINAPI CIndexBufferLoader::Decompress( void** ppData, SIZE_T* pcBytes )
{
    *ppData = NULL; *pcBytes = 0; return S_OK;
}
HRESULT WINAPI CIndexBufferLoader::Destroy()
{
    return S_OK;
}
HRESULT WINAPI CIndexBufferLoader::Load()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
CIndexBufferProcessor::CIndexBufferProcessor( ID3D10Device* pDevice,
                                              ID3D10Buffer** ppBuffer10,
                                              D3D10_BUFFER_DESC* pBufferDesc,
                                              void* pData,
                                              CResourceReuseCache* pResourceReuseCache ) : m_Device( pDevice ),
                                                                                           m_ppBuffer10( ppBuffer10 ),
                                                                                           m_ppBuffer9( NULL ),
                                                                                           m_iSizeBytes( 0 ),
                                                                                           m_Usage( 0 ),
                                                                                           m_ibFormat(
                                                                                           D3DFMT_INDEX32 ),
                                                                                           m_Pool( D3DPOOL_DEFAULT ),
                                                                                           m_pData( pData ),
                                                                                           m_pResourceReuseCache(
                                                                                           pResourceReuseCache )
{
    *m_ppBuffer10 = NULL;
    CopyMemory( &m_BufferDesc, pBufferDesc, sizeof( D3D10_BUFFER_DESC ) );
}

//--------------------------------------------------------------------------------------
CIndexBufferProcessor::CIndexBufferProcessor( IDirect3DDevice9* pDevice,
                                              IDirect3DIndexBuffer9** ppBuffer9,
                                              UINT iSizeBytes,
                                              DWORD Usage,
                                              D3DFORMAT ibFormat,
                                              D3DPOOL Pool,
                                              void* pData,
                                              CResourceReuseCache* pResourceReuseCache ) : m_Device( pDevice ),
                                                                                           m_ppBuffer10( NULL ),
                                                                                           m_ppBuffer9( ppBuffer9 ),
                                                                                           m_iSizeBytes( iSizeBytes ),
                                                                                           m_Usage( Usage ),
                                                                                           m_ibFormat( ibFormat ),
                                                                                           m_Pool( Pool ),
                                                                                           m_pData( pData ),
                                                                                           m_pResourceReuseCache(
                                                                                           pResourceReuseCache )
{
    *m_ppBuffer9 = NULL;
}

//--------------------------------------------------------------------------------------
CIndexBufferProcessor::~CIndexBufferProcessor()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// LockDeviceObject is called by the graphics thread to find an appropriate resource from
// the resource reuse cache.  If no resource is found, the return code tells the calling
// thread to try again later.  For D3D9, this function also locks the resource.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CIndexBufferProcessor::LockDeviceObject()
{
    HRESULT hr = S_OK;
    if( !m_pResourceReuseCache )
        return E_FAIL;

    if( LDT_D3D10 == m_Device.Type )
    {
        m_pRealBuffer10 = m_pResourceReuseCache->GetFreeIB10( m_BufferDesc.ByteWidth, 0 );
        if( NULL == m_pRealBuffer10 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
            hr = S_OK;
        }
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        m_pRealBuffer9 = m_pResourceReuseCache->GetFreeIB9( m_iSizeBytes, m_ibFormat );
        if( NULL == m_pRealBuffer9 )
        {
            hr = E_TRYAGAIN;
        }
        else
        {
            // Lock
            hr = m_pRealBuffer9->Lock( 0, 0, ( void** )&m_pLockedData, 0 );
            if( FAILED( hr ) )
            {
                m_pLockedData = NULL;
                return hr;
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// On D3D9, this unlocks the resource.  On D3D10, this actually populates the resource.
//--------------------------------------------------------------------------------------
HRESULT WINAPI CIndexBufferProcessor::UnLockDeviceObject()
{
    HRESULT hr = S_OK;

    if( LDT_D3D10 == m_Device.Type )
    {
        m_Device.pDev10->UpdateSubresource( m_pRealBuffer10, 0, NULL, m_pData, 0, 0 );
        *m_ppBuffer10 = m_pRealBuffer10;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        if( !m_pLockedData )
            return E_FAIL;

        // Unlock
        hr = m_pRealBuffer9->Unlock();
        *m_ppBuffer9 = m_pRealBuffer9;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CIndexBufferProcessor::Destroy()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CIndexBufferProcessor::Process( void* pData, SIZE_T cBytes )
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Copies the data to the locked pointer on D3D9
//--------------------------------------------------------------------------------------
HRESULT WINAPI CIndexBufferProcessor::CopyToResource()
{
    if( LDT_D3D10 == m_Device.Type )
    {

    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        if( !m_pLockedData )
        {
            return E_FAIL;
        }

        CopyMemory( m_pLockedData, m_pData, m_iSizeBytes );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
void WINAPI CIndexBufferProcessor::SetResourceError()
{
    if( LDT_D3D10 == m_Device.Type )
    {
        *m_ppBuffer10 = ( ID3D10Buffer* )ERROR_RESOURCE_VALUE;
    }
    else if( LDT_D3D9 == m_Device.Type )
    {
        *m_ppBuffer9 = ( IDirect3DIndexBuffer9* )ERROR_RESOURCE_VALUE;
    }
}

//--------------------------------------------------------------------------------------
// SDKMesh
//--------------------------------------------------------------------------------------
CSDKMeshLoader::CSDKMeshLoader( CDXUTSDKMesh* pMesh, ID3D10Device* pDev10, LPCTSTR szFileName,
                                bool bCreateAdjacencyIndices, SDKMESH_CALLBACKS10* pLoaderCallbacks ) : m_pMesh(
                                                                                                        pMesh ),
                                                                                                        m_Device(
                                                                                                        pDev10 ),
                                                                                                        m_bCreateAdjacencyIndices( bCreateAdjacencyIndices ),
                                                                                                        m_pLoaderCallbacks10( pLoaderCallbacks ),
                                                                                                        m_pLoaderCallbacks9( NULL )
{
    wcscpy_s( m_szFileName, MAX_PATH, szFileName );
}

//--------------------------------------------------------------------------------------
CSDKMeshLoader::CSDKMeshLoader( CDXUTSDKMesh* pMesh, IDirect3DDevice9* pDev9, LPCTSTR szFileName,
                                bool bCreateAdjacencyIndices, SDKMESH_CALLBACKS9* pLoaderCallbacks ) : m_pMesh(
                                                                                                       pMesh ),
                                                                                                       m_Device(
                                                                                                       pDev9 ),
                                                                                                       m_bCreateAdjacencyIndices( bCreateAdjacencyIndices ),
                                                                                                       m_pLoaderCallbacks10( NULL ),
                                                                                                       m_pLoaderCallbacks9( pLoaderCallbacks )
{
    wcscpy_s( m_szFileName, MAX_PATH, szFileName );
}

//--------------------------------------------------------------------------------------
CSDKMeshLoader::~CSDKMeshLoader()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CSDKMeshLoader::Decompress( void** ppData, SIZE_T* pcBytes )
{
    *ppData = NULL;
    *pcBytes = 0;
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CSDKMeshLoader::Destroy()
{
    return S_OK;
}

//--------------------------------------------------------------------------------------
HRESULT WINAPI CSDKMeshLoader::Load()
{
    HRESULT hr = E_FAIL;

    if( m_pMesh )
    {
        if( LDT_D3D10 == m_Device.Type )
        {
            hr = m_pMesh->Create( m_Device.pDev10, m_szFileName, m_bCreateAdjacencyIndices, m_pLoaderCallbacks10 );
        }
        else if( LDT_D3D9 == m_Device.Type )
        {
            hr = m_pMesh->Create( m_Device.pDev9, m_szFileName, m_bCreateAdjacencyIndices, m_pLoaderCallbacks9 );
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
CSDKMeshProcessor::CSDKMeshProcessor()
{
}
CSDKMeshProcessor::~CSDKMeshProcessor()
{
    Destroy();
}
HRESULT WINAPI CSDKMeshProcessor::LockDeviceObject()
{
    return S_OK;
}
HRESULT WINAPI CSDKMeshProcessor::UnLockDeviceObject()
{
    return S_OK;
}
HRESULT WINAPI CSDKMeshProcessor::Destroy()
{
    return S_OK;
}
HRESULT WINAPI CSDKMeshProcessor::Process( void* pData, SIZE_T cBytes )
{
    return S_OK;
}
HRESULT WINAPI CSDKMeshProcessor::CopyToResource()
{
    return S_OK;
}
void    WINAPI CSDKMeshProcessor::SetResourceError()
{
}
