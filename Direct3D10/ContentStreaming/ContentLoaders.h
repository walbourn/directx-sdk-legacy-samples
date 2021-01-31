//--------------------------------------------------------------------------------------
// File: ContentLoaders.h
//
// Illustrates streaming content using Direct3D 9/10
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "AsyncLoader.h"
#include "ResourceReuseCache.h"

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
struct SDKMESH_CALLBACKS10;
struct SDKMESH_CALLBACKS9;
class CDXUTSDKMesh;
class CPackedFile;

//--------------------------------------------------------------------------------------
// IDataLoader is an interface that the AsyncLoader class uses to load data from disk.
//
// Load is called from the IO thread to load data.
// Decompress is called by one of the processing threads to decompress the data.
// Destroy is called by the graphics thread when it has consumed the data.
//--------------------------------------------------------------------------------------
class IDataLoader
{
public:
    virtual                 ~IDataLoader()
    {
    };
    virtual HRESULT WINAPI  Decompress( void** ppData, SIZE_T* pcBytes ) = 0;
    virtual HRESULT WINAPI  Destroy() = 0;
    virtual HRESULT WINAPI  Load() = 0;
};

//--------------------------------------------------------------------------------------
// IDataProcessor is an interface that the AsyncLoader class uses to process and copy
// data into locked resource pointers.
//
// Process is called by one of the processing threads to process the data before it is
//   consumed.
// LockDeviceObject is called from the Graphics thread to lock the device object (D3D9).
// UnLockDeviceObject is called from the Graphics thread to unlock the device object, or
//   to call updatesubresource for D3D10.
// CopyToResource copies the data from memory to the locked device object (D3D9).
// SetResourceError is called to set the resource pointer to an error code in the event
//   that something went wrong.
// Destroy is called by the graphics thread when it has consumed the data.
//--------------------------------------------------------------------------------------
class IDataProcessor
{
public:
    virtual                 ~IDataProcessor()
    {
    };
    virtual HRESULT WINAPI  LockDeviceObject() = 0;
    virtual HRESULT WINAPI  UnLockDeviceObject() = 0;
    virtual HRESULT WINAPI  Destroy() = 0;
    virtual HRESULT WINAPI  Process( void* pData, SIZE_T cBytes ) = 0;
    virtual HRESULT WINAPI  CopyToResource() = 0;
    virtual void WINAPI     SetResourceError() = 0;
};

//--------------------------------------------------------------------------------------
// CTextureLoader implementation of IDataLoader
//--------------------------------------------------------------------------------------
class CTextureLoader : public IDataLoader
{
private:
    WCHAR           m_szFileName[MAX_PATH];
    BYTE* m_pData;
    UINT m_cBytes;
    CPackedFile* m_pPackedFile;

public:
                    CTextureLoader( WCHAR* szFileName, CPackedFile* pPackedFile );
                    ~CTextureLoader();

    // overrides
public:
    HRESULT WINAPI  Decompress( void** ppData, SIZE_T* pcBytes );
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Load();
};

//--------------------------------------------------------------------------------------
// CTextureProcessor implementation of IDataProcessor
//--------------------------------------------------------------------------------------
#define MAX_MIP_LEVELS 32
class CTextureProcessor : public IDataProcessor
{
private:
    LOADER_DEVICE m_Device;
    ID3D10ShaderResourceView** m_ppRV10;
    IDirect3DTexture9** m_ppTexture9;
    BYTE* m_pData;
    SIZE_T m_cBytes;
    CResourceReuseCache* m_pResourceReuseCache;
    ID3D10ShaderResourceView* m_pRealRV10;
    ID3D10Texture2D* m_pStaging10;
    IDirect3DTexture9* m_pRealTexture9;
    D3DLOCKED_RECT          m_pLockedRects[MAX_MIP_LEVELS];
    D3D10_MAPPED_TEXTURE2D  m_pLockedRects10[MAX_MIP_LEVELS];
    UINT m_iNumLockedPtrs;
    UINT m_SkipMips;

private:
    BOOL                    PopulateTexture();

public:
                            CTextureProcessor( ID3D10Device* pDevice, ID3D10ShaderResourceView** ppRV10,
                                               CResourceReuseCache* pResourceReuseCache, UINT SkipMips );
                            CTextureProcessor( IDirect3DDevice9* pDevice, IDirect3DTexture9** ppTexture9,
                                               CResourceReuseCache* pResourceReuseCache, UINT SkipMips );
                            ~CTextureProcessor();

    // overrides
public:
    HRESULT WINAPI          LockDeviceObject();
    HRESULT WINAPI          UnLockDeviceObject();
    HRESULT WINAPI          Destroy();
    HRESULT WINAPI          Process( void* pData, SIZE_T cBytes );
    HRESULT WINAPI          CopyToResource();
    void WINAPI             SetResourceError();
};

//--------------------------------------------------------------------------------------
// CVertexBufferLoader implementation of IDataLoader
//--------------------------------------------------------------------------------------
class CVertexBufferLoader : public IDataLoader
{
private:

public:
                    CVertexBufferLoader();
                    ~CVertexBufferLoader();

    // overrides
public:
    HRESULT WINAPI  Decompress( void** ppData, SIZE_T* pcBytes );
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Load();
};

//--------------------------------------------------------------------------------------
// CVertexBufferProcessor implementation of IDataProcessor
//--------------------------------------------------------------------------------------
class CVertexBufferProcessor : public IDataProcessor
{
private:
    LOADER_DEVICE m_Device;
    ID3D10Buffer** m_ppBuffer10;
    D3D10_BUFFER_DESC m_BufferDesc;
    IDirect3DVertexBuffer9** m_ppBuffer9;
    UINT m_iSizeBytes;
    DWORD m_Usage;
    DWORD m_FVF;
    D3DPOOL m_Pool;
    void* m_pData;
    CResourceReuseCache* m_pResourceReuseCache;
    ID3D10Buffer* m_pRealBuffer10;
    IDirect3DVertexBuffer9* m_pRealBuffer9;
    void* m_pLockedData;

public:
                    CVertexBufferProcessor( ID3D10Device* pDevice,
                                            ID3D10Buffer** ppBuffer10,
                                            D3D10_BUFFER_DESC* pBufferDesc,
                                            void* pData,
                                            CResourceReuseCache* pResourceReuseCache );
                    CVertexBufferProcessor( IDirect3DDevice9* pDevice,
                                            IDirect3DVertexBuffer9** ppBuffer9,
                                            UINT iSizeBytes,
                                            DWORD Usage,
                                            DWORD FVF,
                                            D3DPOOL Pool,
                                            void* pData,
                                            CResourceReuseCache* pResourceReuseCache );
                    ~CVertexBufferProcessor();

    // overrides
public:
    HRESULT WINAPI  LockDeviceObject();
    HRESULT WINAPI  UnLockDeviceObject();
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Process( void* pData, SIZE_T cBytes );
    HRESULT WINAPI  CopyToResource();
    void WINAPI     SetResourceError();
};

//--------------------------------------------------------------------------------------
// CIndexBufferLoader implementation of IDataLoader
//--------------------------------------------------------------------------------------
class CIndexBufferLoader : public IDataLoader
{
private:

public:
                    CIndexBufferLoader();
                    ~CIndexBufferLoader();

    // overrides
public:
    HRESULT WINAPI  Decompress( void** ppData, SIZE_T* pcBytes );
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Load();
};

//--------------------------------------------------------------------------------------
// CIndexBufferProcessor implementation of IDataProcessor
//--------------------------------------------------------------------------------------
class CIndexBufferProcessor : public IDataProcessor
{
private:
    LOADER_DEVICE m_Device;
    ID3D10Buffer** m_ppBuffer10;
    D3D10_BUFFER_DESC m_BufferDesc;
    IDirect3DIndexBuffer9** m_ppBuffer9;
    UINT m_iSizeBytes;
    DWORD m_Usage;
    D3DFORMAT m_ibFormat;
    D3DPOOL m_Pool;
    void* m_pData;
    CResourceReuseCache* m_pResourceReuseCache;
    ID3D10Buffer* m_pRealBuffer10;
    IDirect3DIndexBuffer9* m_pRealBuffer9;
    void* m_pLockedData;

public:
                    CIndexBufferProcessor( ID3D10Device* pDevice,
                                           ID3D10Buffer** ppBuffer10,
                                           D3D10_BUFFER_DESC* pBufferDesc,
                                           void* pData,
                                           CResourceReuseCache* pResourceReuseCache );
                    CIndexBufferProcessor( IDirect3DDevice9* pDevice,
                                           IDirect3DIndexBuffer9** ppBuffer9,
                                           UINT iSizeBytes,
                                           DWORD Usage,
                                           D3DFORMAT ibFormat,
                                           D3DPOOL Pool,
                                           void* pData,
                                           CResourceReuseCache* pResourceReuseCache );
                    ~CIndexBufferProcessor();

    // overrides
public:
    HRESULT WINAPI  LockDeviceObject();
    HRESULT WINAPI  UnLockDeviceObject();
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Process( void* pData, SIZE_T cBytes );
    HRESULT WINAPI  CopyToResource();
    void WINAPI     SetResourceError();
};

//--------------------------------------------------------------------------------------
// CSDKMeshLoader implementation of IDataLoader
//--------------------------------------------------------------------------------------
class CSDKMeshLoader : public IDataLoader
{
private:
    LOADER_DEVICE m_Device;
    CDXUTSDKMesh* m_pMesh;
    WCHAR           m_szFileName[MAX_PATH];
    bool m_bCreateAdjacencyIndices;
    SDKMESH_CALLBACKS10* m_pLoaderCallbacks10;
    SDKMESH_CALLBACKS9* m_pLoaderCallbacks9;

public:
                    CSDKMeshLoader( CDXUTSDKMesh* pMesh,
                                    ID3D10Device* pDev10,
                                    LPCTSTR szFileName,
                                    bool bCreateAdjacencyIndices=false,
                                    SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
                    CSDKMeshLoader( CDXUTSDKMesh* pMesh,
                                    IDirect3DDevice9* pDev9,
                                    LPCTSTR szFileName,
                                    bool bCreateAdjacencyIndices=false,
                                    SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );
                    ~CSDKMeshLoader();

    // overrides
public:
    HRESULT WINAPI  Decompress( void** ppData, SIZE_T* pcBytes );
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Load();
};

//--------------------------------------------------------------------------------------
// CSDKMeshProcessor implementation of IDataProcessor
//--------------------------------------------------------------------------------------
class CSDKMeshProcessor : public IDataProcessor
{
public:
                    CSDKMeshProcessor();
                    ~CSDKMeshProcessor();

    // overrides
public:
    HRESULT WINAPI  LockDeviceObject();
    HRESULT WINAPI  UnLockDeviceObject();
    HRESULT WINAPI  Destroy();
    HRESULT WINAPI  Process( void* pData, SIZE_T cBytes );
    HRESULT WINAPI  CopyToResource();
    void WINAPI     SetResourceError();
};
