//--------------------------------------------------------------------------------------
// File: renderables.h
//
// Material Management classes for EffectStateManager 'StateManager' Sample
// These classes serve in support of the sample, but are not directly related to the
// intended topic of the ID3DXEffectStateManager Interface.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

//--------------------------------------------------------------------------------------
// STL dependancies
//--------------------------------------------------------------------------------------
#pragma warning ( push )
#pragma warning ( disable : 4512 ) // 'class' : assignment operator could not be generated
#pragma warning ( disable : 4702 ) // unreachable code
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <exception>
#pragma warning ( pop )
using std::map;
using std::vector;
using std::pair;
using std::string;
using std::wstring;
using std::remove;
using std::sort;
using std::find;
using std::lexicographical_compare;
using std::exception;
using std::bad_alloc;


//--------------------------------------------------------------------------------------
// Allows STL Map containers to use case-insensitive string comparisons
//--------------------------------------------------------------------------------------
template <typename T> class compare_case_insensitive
{
protected:
    static bool compare( typename T::value_type c1, typename T::value_type c2 )
    {
        return tolower( c1 ) < tolower( c2 );
    }
public:
    bool        operator()( const T& s1, const T& s2 ) const
    {
        return lexicographical_compare( s1.begin(), s1.end(),
                                        s2.begin(), s2.end(),
                                        compare );
    }
};

typedef compare_case_insensitive <wstring>  wcompare;
typedef compare_case_insensitive <string>   scompare;

//--------------------------------------------------------------------------------------
// Provides tracking of DX device-dependant Resources, which may require attention
// following device lost, device reset, etc events.
// This templated-class also provides a means to 'singularize' resources.  Eg, if
// multiple instances of the same resource are requested, only one instance of the
// resource will be loaded.  This is accomplished via an STL map -- see implementation
// comments for further details.
//--------------------------------------------------------------------------------------
template <typename _dxty> class CResource
{
public:
    typedef map <wstring, CResource*, wcompare> resourceMap;
protected:
    _dxty m_pResource;                  // Pointer to the underlying device-dependant resource
    ULONG m_ulRef;                      // A reference count of the number of requested instances
    wstring m_wsName;                   // The filename of the resource
    static resourceMap m_mapLoaded;     // An STL map used to track all loaded instances of this
    // resource type
public:
    // Constructor is called when a new instance of a resource is loaded
                    CResource( const wstring tsName, const _dxty pResource ) : m_wsName( tsName ),
                                                                               m_pResource( pResource ),
                                                                               m_ulRef( 1UL )
                    {
                        // Add the loaded resource into the STL map
                        // The map allows us to detect duplicate requests for the same resource
                        // (textures, meshes, effects, etc).
                        // The map also allows tracking of resources that are required to take
                        // action following device lost, etc events
                        m_mapLoaded.insert( resourceMap::value_type( m_wsName, this ) );
                    }

    // Destructor is called when all instances of the resource have been released
    virtual         ~CResource()
    {
        // erase the instance from the map of loaded resources
        resourceMap::iterator it = m_mapLoaded.find( m_wsName );
        if( m_mapLoaded.end() != it )
            m_mapLoaded.erase( it );

        // release (unload) the underlying resource
        m_pResource->Release();
    }

    // Entry point for resource to take action following Lost Device Event
    virtual HRESULT OnLostDevice() = 0;

    // Entry point for resource to take action prior to Reset Device Event
    virtual HRESULT OnResetDevice() = 0;

    // Must be called when the resource is re-instanced (eg, if you make a new copy
    // of the resource pointer and keep it active, you must call AddRef)
    ULONG           AddRef()
    {
        return ++m_ulRef;
    }

    // Called to de-instance the resource (eg, retire a pointer that was previously
    // obtained, or AddRef'd)
    ULONG           Release()
    {
        ULONG ulRef = --m_ulRef;

        // when no further instances are outstanding, delete the resource
        if( 0UL == m_ulRef )
            delete this;

        return ulRef;
    }

    // Static Entry-Point to the resource list, for LostDevice event
    // Each active resource of this type will be iterated, and it's OnDeviceLost
    // entry point will be called.
    static HRESULT  LostDevice()
    {
        bool bResult = true;

        // Iterate through the list of loaded resources
        for( resourceMap::iterator it = m_mapLoaded.begin();
             it != m_mapLoaded.end();
             it++ )
            // Notify the instance of this resource that the device is lost
            // by calling it's OnLostDevice entry point
            bResult &= SUCCEEDED( ( *it ).second->OnLostDevice() );

        return bResult ? S_OK : E_FAIL;
    }

    // Static Entry-Point to the resource list, for ResetDevice event
    // Each active resource of this type will be iterated, and it's OnResetDevice
    // entry point will be called.
    static HRESULT  ResetDevice()
    {
        bool bResult = true;

        // Iterate through the list of loaded resources
        for( resourceMap::iterator it = m_mapLoaded.begin();
             it != m_mapLoaded.end();
             it++ )
            // Notify the instance of this resource that the device is lost
            // by calling it's OnLostDevice entry point
            bResult &= SUCCEEDED( ( *it ).second->OnResetDevice() );

        return bResult ? S_OK : E_FAIL;
    }

    // Allows the resource list to determine whether or not a resource
    // by this (file)name already exists
    static CResource* getExisting( wstring tsName )
    {
        // Locate any existing resource of this name
        resourceMap::iterator it = m_mapLoaded.find( tsName );

        // No resource was located, return NULL
        if( m_mapLoaded.end() == it )
            return NULL;

        // A matching resource was found, increment it's ref count and return the
        // pointer to the resource
        ( *it ).second->AddRef();
        return ( *it ).second;
    }

    const wstring& getName() const
    {
        return m_wsName;
    }

    // Obtain the current pointer to the underlying resource
    inline _dxty    getPointer()
    {
        return m_pResource;
    }
};


//--------------------------------------------------------------------------------------
// Texture Resource Management
//--------------------------------------------------------------------------------------
class CTexture : public CResource <LPDIRECT3DBASETEXTURE9>
{
protected:
    LPDIRECT3DTEXTURE9 m_pNormalMap;            // A normal map equivalent
    // If an effect specifies that the texture should
    // be converted, the original texture owns the converted child
    // This avoids maintaining multiple converted normal maps
    // on a per-effect basis.
public:
    // Called following the loading of a new texture
    inline          CTexture( const wstring tsName,
                              const LPDIRECT3DBASETEXTURE9 pTexture ) : CResource <LPDIRECT3DBASETEXTURE9>( tsName,
                                                                                                            pTexture ),
                                                                        m_pNormalMap( NULL )
    {
    }
                    ~CTexture()
                    {
                        SAFE_RELEASE( m_pNormalMap );
                    }

    // Entry point to lost device handling
    virtual HRESULT OnLostDevice();

    // Entry point to device reset handling
    virtual HRESULT OnResetDevice();

    // Retrieves a normal map for this texture
    HRESULT         GetNormalMapPtr( LPDIRECT3DBASETEXTURE9* ppTexture );

    // Called to create a new texture resource.  If the texture has been previously
    // loaded, the existing instance of the texture will be returned.
    static HRESULT  Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CTexture** ppTexture );
};


//--------------------------------------------------------------------------------------
// D3DX Effects Resource Management
//--------------------------------------------------------------------------------------
class CEffect : public CResource <LPD3DXEFFECT>
{
protected:
    // Cached Handles to common effect parameters
    D3DXHANDLE m_hMatViewProj;
    D3DXHANDLE m_hMatProjection;
    D3DXHANDLE m_hMatWorld;
    D3DXHANDLE m_hMatView;
    D3DXHANDLE m_hMatViewInv;
    UINT m_nFrameTimestamp;    // Used to avoid redundantly setting view & projection matrices
public:
    // Called following the loading of a new effect
                    CEffect( const wstring tsName, const LPD3DXEFFECT pEffect );

    // Entry point to lost device handling
    virtual HRESULT OnLostDevice();

    // Entry point to device reset handling
    virtual HRESULT OnResetDevice();

    // Setup the transformation matrices within the effect
    virtual HRESULT SetMatrices( const D3DXMATRIX* pWorld,
                                 const D3DXMATRIX* pView,
                                 const D3DXMATRIX* pProjection,
                                 UINT nFrameTimeStamp );

    // Called to change the state manager interface for all loaded effects
    static HRESULT  SetStateManager( LPD3DXEFFECTSTATEMANAGER pStateManager );

    // Called to create a new d3dx effect resource.  If the effect has been
    // previously loaded, the existing instance of the effect will be returned.
    static HRESULT  Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CEffect** ppEffect );
};


//--------------------------------------------------------------------------------------
// Used to bind a mesh material EffectInstance to a D3DX Effect
// A list of all parameter default values is mantained, along with a cached handle to
// the parameter within the effect.
// Groups of effect parameters (which represent material settings) are sent into the
// D3DX Effect via the apply() method.
//--------------------------------------------------------------------------------------
class CEffectInstance
{
public:
    // parameter names are maintained in ANSI-format.  This is because D3DXHANDLE values are
    // obtained from GetParameterByName -- which has ANSI-only string inputs
    typedef vector <BYTE> instance_value;       // Holds the Value of one parameter
    typedef map <D3DXHANDLE, instance_value> instance_params;      // Maps Parameter Handles to Values
    typedef pair <string, wstring> texture_annotation;   // Holds a matching texture annotation name/filename
    typedef map <string, wstring> texture_annotations;  // Maps texture annotation names to filenames
protected:
    LPD3DXEFFECT m_pEffect;                         // A pointer to the effect (for which D3DXHANDLE values are cached)
    instance_params m_instanceParams;           // List of Effect Default Values
    texture_annotations m_textureAnnotations;       // List of Texture Filenames

    // set an effect instance parameter value (this value will be loaded into the effet
    // when apply() is called)
    void    setValue( string name, BYTE* pValue, UINT nSizeInBytes );

public:
    //
            CEffectInstance( const D3DXEFFECTINSTANCE* pEffectInstance, LPD3DXEFFECT pEffect );

    //
            ~CEffectInstance();

    // Called to bind a direct3d texture to the effect instance
    HRESULT addTexture( string annotation_name, CTexture* pTexture );

    // Called to apply all Effect Instance Values to the D3DX Effect
    HRESULT apply() const;

    // Returns a list of texture annotation values obtained from the effect instance.
    // The client uses this to load all required textures, calling addTexture() for each one.
    inline const texture_annotations& getTextureAnnotations()
    {
        return m_textureAnnotations;
    };
};


//--------------------------------------------------------------------------------------
// Used to bind an effect instance and texture list to an effect file.  Together, these
// uniquely specify a material.
//--------------------------------------------------------------------------------------
class CMaterial
{
protected:
    CEffect* m_pEffect;            // A pointer to the effect this material uses
    CEffectInstance* m_pEffectInstance;    // A pointer to the effect instance values
    vector <CTexture*> m_vecTextures;        // A list of textures for this material
public:
    //
    CMaterial( LPDIRECT3DDEVICE9 pDevice, D3DXEFFECTINSTANCE* pEffectInstance );

    //
    ~CMaterial();

    //
    inline CEffect* getEffect()
    {
        return m_pEffect;
    }
    inline CEffectInstance* getEffectInstance()
    {
        return m_pEffectInstance;
    }
};


//--------------------------------------------------------------------------------------
// D3DX Mesh Resource Management
//--------------------------------------------------------------------------------------
class CMeshObject : public CResource <LPD3DXMESH>
{
protected:
    vector <CMaterial*> m_vecMaterialList;       // list of materials used within the mesh
public:
    //
                    CMeshObject( LPDIRECT3DDEVICE9 pDevice, wstring tsName, const LPD3DXMESH pMesh,
                                 D3DXEFFECTINSTANCE* pEffectInstance, DWORD dwMaterials );

    //
    virtual         ~CMeshObject();

    // Entry point to lost device handling
    virtual HRESULT OnLostDevice();

    // Entry point to device reset handling
    virtual HRESULT OnResetDevice();

    // Called to create a new D3DX Mesh resource.  If the mesh has been
    // previously loaded, the existing instance of the mesh will be returned.
    static HRESULT  Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CMeshObject** ppMesh );

    // Called to retrieve the number of mesh subsets
    inline DWORD    getSubsetCount()
    {
        return ( DWORD )m_vecMaterialList.size();
    }

    // Retrieves the material used by the given subset
    inline CMaterial* getMaterial( DWORD dwSubset )
    {
        assert( dwSubset < m_vecMaterialList.size() );
        return m_vecMaterialList[dwSubset];
    }
};


//--------------------------------------------------------------------------------------
// A CInstance object is used to specify an instance of an object to be drawn.
// Multiple instances may use the same underlying object (m_pMesh) while each may have
// its own unique world-space position (m_matWorld).
//--------------------------------------------------------------------------------------
class CInstance
{
protected:
    D3DXMATRIXA16 m_matWorld;        // World Matrix for the instance of this object
    CMeshObject* m_pMesh;           // Mesh representation of this object
    DWORD m_dwRenderPass;    // Default render pass in which the object should be rendered
public:
    //
    inline          CInstance( CMeshObject* pMesh, D3DXMATRIX* pWorld, DWORD dwDefaultPass ) : m_pMesh( pMesh ),
                                                                                               m_matWorld( *pWorld ),
                                                                                               m_dwRenderPass(
                                                                                               dwDefaultPass )
    {
    }

    //
    inline          ~CInstance()
    {
        SAFE_RELEASE( m_pMesh );
    }

    //
    inline CMeshObject* getMeshObject()
    {
        return m_pMesh;
    }
    inline D3DXMATRIX* getMatrix()
    {
        return &m_matWorld;
    }
    inline DWORD    getRenderPass()
    {
        return m_dwRenderPass;
    }
};


//--------------------------------------------------------------------------------------
// A renderable object is the smallest unit that can be drawn by the sample.  It
// corresponds to a binding between a Mesh Subset (plus it's associanted material)
// and an instance of the mesh that provides the world position (m_pInstance).
// For the purposes of this sample, sorting the render queue does is not required to
// be particularly fast, as sorting occurs as a pre-render process.
//--------------------------------------------------------------------------------------
class CRenderable
{
protected:
    // Defining values that uniquely specify the renderable item
    CInstance* m_pInstance;
    DWORD m_dwSubset;

    // Derived/cached values
    // For the purposes of this sample, the material pointer may safely be cached.
    // (Any action that invalidates the material pointer also results in rebuilding the
    // list of renderables)
    CMaterial* m_pMaterial;

public:
    //
    inline          CRenderable( CInstance* pInstance, DWORD dwSubset ) : m_pInstance( pInstance ),
                                                                          m_dwSubset( dwSubset )
    {
        m_pMaterial = m_pInstance->getMeshObject()->getMaterial( m_dwSubset );
    }

    //
    inline CInstance* getInstance()
    {
        return m_pInstance;
    }
    inline DWORD    getSubset()
    {
        return m_dwSubset;
    }
    inline CEffect* getEffect()
    {
        return m_pMaterial->getEffect();
    }
    inline CEffectInstance* getEffectInstance()
    {
        return m_pMaterial->getEffectInstance();
    }
};



//--------------------------------------------------------------------------------------
// Defines the sort order when CRenderable objects are sorted by Material.
// The ordering in which renderables are sorted is:
//      effect
//      effect instance
// Using this sort order causes all renderables of like effects to be grouped together
//--------------------------------------------------------------------------------------
inline bool greaterMaterial( CRenderable& lhs, CRenderable& rhs )
{
    // sort by effect, which can be costly to switch
    if( rhs.getEffect() > lhs.getEffect() )
        return true;
    else if( rhs.getEffect() < lhs.getEffect() )
        return false;

    // enforce the sorting by effect instance, lastly
    // changing parameter values within an effect is likely to be less costly than switching effects.
    return rhs.getEffectInstance() > lhs.getEffectInstance();
}


//--------------------------------------------------------------------------------------
// Defines the sort order when CRenderable objects are sorted by Instance
// The ordering in which renderables are sorted is:
//      instance of mesh
//      mesh subset
// Using this sort order causes all object instances to be drawn in their entirety
// before moving on to the next object instance to be drawn.
//--------------------------------------------------------------------------------------
inline bool greaterInstance( CRenderable& lhs, CRenderable& rhs )
{
    // sort by instance
    if( rhs.getInstance() > lhs.getInstance() )
        return true;
    else if( rhs.getInstance() < lhs.getInstance() )
        return false;

    // sort by mesh material subset
    return rhs.getSubset() > lhs.getSubset();
}

//--------------------------------------------------------------------------------------
// Required for std::find to operate on D3DVERTEXELEMENT9
//--------------------------------------------------------------------------------------
inline bool operator==( const D3DVERTEXELEMENT9& lhs, const D3DVERTEXELEMENT9& rhs )
{
    return 0 == memcmp( &lhs, &rhs, sizeof( lhs ) );
}
