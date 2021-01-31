//--------------------------------------------------------------------------------------
// File: renderables.cpp
//
// Material Management classes for EffectStateManager 'StateManager' Sample
// These classes serve in support of the sample, but are not directly related to the
// intended topic of the ID3DXEffectStateManager Interface.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#pragma warning(disable: 4995)
#include "renderables.h"
#pragma warning(default: 4995)



//--------------------------------------------------------------------------------------
// Resource Maps for Texture, Effect, and Mesh resources
// These maps contain the set of loaded resources.
//--------------------------------------------------------------------------------------
CResource <LPDIRECT3DBASETEXTURE9>::resourceMap CResource <LPDIRECT3DBASETEXTURE9>::m_mapLoaded;
CResource <LPD3DXEFFECT>::resourceMap           CResource <LPD3DXEFFECT>          ::m_mapLoaded;
CResource <LPD3DXMESH>::resourceMap             CResource <LPD3DXMESH>            ::m_mapLoaded;


//--------------------------------------------------------------------------------------
// Creates and loads a texture from the specified filename
// If a texture by the same name has already been loaded, it will be returned and its
// reference count incremented.
// Otherwise, the texture will be created and loaded.
//--------------------------------------------------------------------------------------
HRESULT CTexture::Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CTexture** ppTexture )
{
    assert( ppTexture );
    *ppTexture = NULL;

    // getExisting() will return any previously-loaded instance of this resource,
    // with it's reference count incremented.
    if( CTexture* pTexture = static_cast<CTexture*>( getExisting( wszFilename ) ) )
    {
        *ppTexture = pTexture;
        return S_OK;
    }

    HRESULT hr = S_OK;
    D3DXIMAGE_INFO image_info;
    LPDIRECT3DBASETEXTURE9 pBaseTexture = NULL;
    memset( &image_info, 0, sizeof image_info );

    WCHAR wszPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( wszPath, MAX_PATH, wszFilename ) );

    // The texture-type must be determined prior to loading it
    V_RETURN( D3DXGetImageInfoFromFile( wszPath, &image_info ) );

    // 2D Textures
    if( D3DRTYPE_TEXTURE == image_info.ResourceType )
    {
        LPDIRECT3DTEXTURE9 pTexture = NULL;

        V_RETURN( D3DXCreateTextureFromFile( pDevice, wszPath, &pTexture ) );

        pBaseTexture = pTexture;
    }
        // Cube maps
    else if( D3DRTYPE_CUBETEXTURE == image_info.ResourceType )
    {
        LPDIRECT3DCUBETEXTURE9 pTexture = NULL;

        V_RETURN( D3DXCreateCubeTextureFromFile( pDevice, wszPath, &pTexture ) );

        pBaseTexture = pTexture;
    }

    // Create a CTexture resource type
    // The CTexture class will be responsible for tracking this texture,
    // such that it can be notified during lost device, etc events
    *ppTexture = new CTexture( wszFilename, pBaseTexture );

    return NULL != *ppTexture ? S_OK : E_OUTOFMEMORY;
}


//--------------------------------------------------------------------------------------
// Texture Lost-Device Entry Point
// This entry point allows textures to take actions necessary prior to a Device-Reset,
// such as releasing any D3DPOOL_DEFAULT resources.
// Following a lost-device event, each texture within the map of loaded textures
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::LostDevice() is responsible for enumerating each texture and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CTexture::OnLostDevice()
{
    // D3DPOOL_MANAGED textures need not implement special lost device handling
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Texture Reset-Device Entry Point
// This entry point allows textures to take actions necessary following a Device-Reset,
// such as re-creating any D3DPOOL_DEFAULT resources.
// Following a reset-device event, each texture within the map of loaded textures
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::ResetDevice() is responsible for enumerating each texture and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CTexture::OnResetDevice()
{
    // D3DPOOL_MANAGED textures need not implement special reset handling
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Each effect may specify whether or not it wants to use the texture as a normal map.
// In this case, the original texture is considered to be a height map and must be
// converted prior to use.
// In this sample, all normal maps are generated in a consistent manner.  For this
// reason, the original texture caches any children that have been created as normal maps.
// This ensures that multiple copies of the same converted texture are not allowed
// to proliferate.
//--------------------------------------------------------------------------------------
HRESULT CTexture::GetNormalMapPtr( LPDIRECT3DBASETEXTURE9* ppTexture )
{
    HRESULT hr;
    assert( ppTexture );
    *ppTexture = NULL;

    // 
    if( D3DRTYPE_TEXTURE != m_pResource->GetType() )
        return E_FAIL;

    // if the normal map has already been generated, return it.
    if( m_pNormalMap )
    {
        *ppTexture = m_pNormalMap;
        return S_OK;
    }

    // A normal map must be created
    LPDIRECT3DDEVICE9 pDevice = NULL;
    D3DSURFACE_DESC desc;
    memset( &desc, 0, sizeof( desc ) );

    // To create the normal map, a clone of the original texture is created (of format D3DFMT_A8R8G8B8)
    // The texture dimensions are needed to clone the texture
    V_RETURN( static_cast<LPDIRECT3DTEXTURE9>( m_pResource )->GetLevelDesc( 0, &desc ) );

    V_RETURN( m_pResource->GetDevice( &pDevice ) );
    V( D3DXCreateTexture( pDevice,
                          desc.Width,
                          desc.Height,
                          D3DX_DEFAULT,
                          0,
                          D3DFMT_A8R8G8B8,
                          D3DPOOL_MANAGED,
                          &m_pNormalMap ) );

    SAFE_RELEASE( pDevice );

    if( FAILED( hr ) )
        return hr;

    // The texture clone can now be used to hold the normal map
    V_RETURN( D3DXComputeNormalMap( m_pNormalMap,
                                    static_cast<LPDIRECT3DTEXTURE9>( m_pResource ),
                                    NULL,
                                    0,
                                    D3DX_CHANNEL_RED,
                                    10.f ) );

    *ppTexture = m_pNormalMap;
    return hr;
}


//--------------------------------------------------------------------------------------
// CEffect constructor
// When each effect is loaded, several D3DXHANDLEs to common scene-level parameters are
// cached.  This allows a general scene interface (such as CEffect::SetMatrices) to be
// used across all D3DX Effects in the application.
//--------------------------------------------------------------------------------------
CEffect::CEffect( const wstring tsName, const LPD3DXEFFECT pEffect ) : CResource <LPD3DXEFFECT>( tsName, pEffect )
{
    // Effects may not require all of the following matrices.
    // Prior to updating the transformation matrices, the D3DXHANDLE value will
    // be checked to determine whether or not it was declared by the effect.
    m_hMatWorld = m_pResource->GetParameterBySemantic( NULL, "WORLD" );
    m_hMatView = m_pResource->GetParameterBySemantic( NULL, "VIEW" );
    m_hMatViewInv = m_pResource->GetParameterBySemantic( NULL, "VIEWINV" );
    m_hMatViewProj = m_pResource->GetParameterBySemantic( NULL, "VIEWPROJECTION" );
    m_hMatProjection = m_pResource->GetParameterBySemantic( NULL, "PROJECTION" );

    // Used to avoid redundantly setting view/projection matrices -- ensure that it does not start at 0
    m_nFrameTimestamp = ( UINT )-1;

    // The first technique that is valid is used.
    // This assumes that multiple materials are not packed into the same effect resource.
    // The intention to represent each unique material type with one effect resource.
    // Within the effect resource, techniques (including fallback methods) are ordered
    // in decreasing order of hardware requirements.  Thus, selecting the first valid
    // technique within the effect will chose the most advanced technique that is
    // supported by the underlying hardware.
    D3DXHANDLE hTechnique;
    if( FAILED( m_pResource->FindNextValidTechnique( NULL, &hTechnique ) ) )
        throw exception( "ID3DXEffect::FindNextValidTechnique failed" );

    // It is possible for FindNextValidTechnique to succeed yet return NULL
    // This would indicate no valid techniques were found.
    if( NULL == hTechnique )
        throw exception( "No Valid Techniques in Effect" );

    // Prior to rendering from the effect, the desired technique must be set
    if( FAILED( m_pResource->SetTechnique( hTechnique ) ) )
        throw exception( "ID3DXEffect::SetTechnique failed" );
}


//--------------------------------------------------------------------------------------
// Creates and loads a D3DX Effect from the specified filename
// If an effect by the same name has already been loaded, it will be returned and its
// reference count incremented.
// Otherwise, the effect will be created and loaded.
//--------------------------------------------------------------------------------------
HRESULT CEffect::Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CEffect** ppEffect )
{
    HRESULT hr;

    assert( ppEffect );
    *ppEffect = NULL;

    // getExisting() will return any previously-loaded instance of this resource,
    // with it's reference count incremented.
    if( CEffect* pEffect = static_cast<CEffect*>( getExisting( wszFilename ) ) )
    {
        *ppEffect = pEffect;
        return S_OK;
    }

    // The effect has not yet been loaded -- locate the file
    WCHAR wszPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( wszPath, MAX_PATH, wszFilename ) );

    LPD3DXEFFECT pd3dxEffect = NULL;
    LPD3DXBUFFER lpd3dxbufferErrors = NULL;

    // Instantiate the effect
    V( D3DXCreateEffectFromFile( pDevice,
                                 wszPath,
                                 NULL,
                                 NULL,
                                 D3DXSHADER_NO_PRESHADER | D3DXFX_NOT_CLONEABLE,
                                 NULL,
                                 &pd3dxEffect,
                                 &lpd3dxbufferErrors ) );

    // If there are errors, notify the users
    if( FAILED( hr ) && lpd3dxbufferErrors )
    {
        WCHAR wsz[256];
        MultiByteToWideChar( CP_ACP, 0, ( LPSTR )lpd3dxbufferErrors->GetBufferPointer(), -1, wsz, 256 );
        wsz[ 255 ] = 0;
        DXUTTrace( __FILE__, ( DWORD )__LINE__, E_FAIL, wsz, true );
    }

    // Release and error buffer that was returned
    SAFE_RELEASE( lpd3dxbufferErrors );

    if( FAILED( hr ) )
        return E_FAIL;

    // Create a CEffect resource type
    // The CEffect class will be responsible for tracking this effect,
    // such that it can be notified during lost device, etc events
    *ppEffect = new CEffect( wszFilename, pd3dxEffect );

    return NULL != *ppEffect ? S_OK : E_OUTOFMEMORY;
}


//--------------------------------------------------------------------------------------
// Effect Lost-Device Entry Point
// This entry point allows D3DX Effects to take actions necessary prior to a Device-Reset,
// such as calling the ID3DXEffect::OnLostDevice() member.
// Following a lost-device event, each effect within the map of loaded effects
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::LostDevice() is responsible for enumerating each effect and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CEffect::OnLostDevice()
{
    return m_pResource->OnLostDevice();
}


//--------------------------------------------------------------------------------------
// Effect Reset-Device Entry Point
// This entry point allows D3DX Effects to take actions necessary following a Device-Reset,
// such as calling the ID3DXEffect::OnResetDevice() member.
// Following a reset-device event, each effect within the map of loaded effects
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::ResetDevice() is responsible for enumerating each effect and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CEffect::OnResetDevice()
{
    return m_pResource->OnResetDevice();
}


//--------------------------------------------------------------------------------------
// Sets the state manager across all effects
//--------------------------------------------------------------------------------------
HRESULT CEffect::SetStateManager( LPD3DXEFFECTSTATEMANAGER pManager )
{
    bool bResult = true;

    // Iterate through the list of loaded effects
    CResource <LPD3DXEFFECT>::resourceMap::iterator it;
    for( it = CResource <LPD3DXEFFECT>::m_mapLoaded.begin();
         it != CResource <LPD3DXEFFECT>::m_mapLoaded.end();
         it++ )

        // Set the state manager for this effect
        bResult &= SUCCEEDED( ( *it ).second->getPointer()->SetStateManager( pManager ) );

    return bResult ? S_OK : E_FAIL;
}


//--------------------------------------------------------------------------------------
// Allows the effect to be updated with scene-level information (the active
// transformation matrices).
//--------------------------------------------------------------------------------------
HRESULT CEffect::SetMatrices( const D3DXMATRIX* pWorld,
                              const D3DXMATRIX* pView,
                              const D3DXMATRIX* pProjection,
                              UINT nFrameTimestamp )
{
    bool bResult = true;

    // Avoid updating the view unless needed
    // The view and projection matrices in this sample do not vary between objects
    if( m_nFrameTimestamp != nFrameTimestamp )
    {
        // Update the effect timestamp to avoid setting view/projection matrices until next frame
        m_nFrameTimestamp = nFrameTimestamp;

        // If the effect required the projection matrix, update it
        if( m_hMatProjection )
            bResult &= SUCCEEDED( m_pResource->SetMatrix( m_hMatProjection, pProjection ) );

        // If the effect required the view matrix, update it
        if( m_hMatView )
            bResult &= SUCCEEDED( m_pResource->SetMatrix( m_hMatView, pView ) );

        // If this effect required the contatenated view-proj matrix, it must be computed
        // and updated
        if( m_hMatViewProj )
        {
            D3DXMATRIXA16 matViewProj;
            D3DXMatrixMultiply( &matViewProj, pView, pProjection );
            bResult &= SUCCEEDED( m_pResource->SetMatrix( m_hMatViewProj, &matViewProj ) );
        }

        // If this effect requires the true inverse of the view matrix, compute and update it
        if( m_hMatViewInv )
        {
            D3DXMATRIXA16 matViewInv;
            D3DXMatrixInverse( &matViewInv, NULL, pView );
            bResult &= SUCCEEDED( m_pResource->SetMatrix( m_hMatViewInv, &matViewInv ) );
        }
    }

    // If the effect required the world matrix, update it
    if( m_hMatWorld )
        bResult &= SUCCEEDED( m_pResource->SetMatrix( m_hMatWorld, pWorld ) );

    return bResult ? S_OK : E_FAIL;
}


//--------------------------------------------------------------------------------------
// Creates and loads a D3DX Mesh from the specified filename
// If a mesh by the same name has already been loaded, it will be returned and its
// reference count incremented.
// Otherwise, the mesh will be created and loaded.
//--------------------------------------------------------------------------------------
HRESULT CMeshObject::Create( LPDIRECT3DDEVICE9 pDevice, LPCWSTR wszFilename, CMeshObject** ppMeshObject )
{
    HRESULT hr;
    assert( ppMeshObject );
    *ppMeshObject = NULL;

    // getExisting() will return any previously-loaded instance of this resource,
    // with it's reference count incremented.
    if( CMeshObject* pMeshObject = static_cast<CMeshObject*>( getExisting( wszFilename ) ) )
    {
        *ppMeshObject = pMeshObject;
        return S_OK;
    }

    // Locate the media file
    WCHAR wszPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( wszPath, MAX_PATH, wszFilename ) );

    // Now that the media file has been found, it can be loaded.
    // Start with a system memory mesh, which will be cloned into a managed mesh.
    // Cloning will add tangent space to the mesh for effects that require it.
    LPD3DXMESH pMesh = NULL;
    LPD3DXBUFFER bufEffectInstances = NULL;
    LPD3DXBUFFER bufAdjacency = NULL;
    DWORD dwMaterials = 0;

    V_RETURN( D3DXLoadMeshFromX( wszPath,
                                 D3DXMESH_SYSTEMMEM,
                                 pDevice,
                                 &bufAdjacency,
                                 NULL,
                                 &bufEffectInstances,
                                 &dwMaterials,
                                 &pMesh
                                 ) );

    // Optimize the mesh for performance
    V( pMesh->OptimizeInplace(
       D3DXMESHOPT_COMPACT | D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_VERTEXCACHE,
       ( DWORD* )bufAdjacency->GetBufferPointer(), NULL, NULL, NULL ) );

    // Size of D3DDECLTYPEs -- see d3d9types.h
    static const WORD DeclTypeSizes[] =
    {
        4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8,
    };

    D3DVERTEXELEMENT9 DeclTangent =
    {
        0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0
    };
    D3DVERTEXELEMENT9 DeclEnd = D3DDECL_END();
    vector <D3DVERTEXELEMENT9> vecDecl( MAX_FVF_DECL_SIZE );
    vector <D3DVERTEXELEMENT9>::iterator it_end;

    if( SUCCEEDED( hr ) )
    {
        // Obtain the original declaration of the mesh
        V( pMesh->GetDeclaration( &*vecDecl.begin() ) );
    }

    LPD3DXMESH pMesh2 = NULL;
    if( SUCCEEDED( hr ) )
    {
        // Locate the last element in the declaration
        if( vecDecl.end() != ( it_end = find( vecDecl.begin(), vecDecl.end(), DeclEnd ) )
            && vecDecl.begin() != it_end )
        {
            // Back up to the previous element
            --it_end;

            // Position the new element -- determine the offset of the previous element
            WORD wOffset = ( *it_end ).Offset;

            // add the size of the previous element to it's offset
            wOffset += ( *it_end ).Type < ( sizeof DeclTypeSizes / sizeof DeclTypeSizes[0] )
                ?  DeclTypeSizes[ ( *it_end ).Type ] : 0;

            // Tangent can go here
            DeclTangent.Offset = wOffset;

            // Move forward to the insertion point
            it_end++;

            // insert the tangent declaration element
            vecDecl.insert( it_end, DeclTangent );
        }

        // Now, clone the entire mesh with tangent
        V( pMesh->CloneMesh( D3DXMESH_MANAGED, &*vecDecl.begin(), pDevice, &pMesh2 ) );
        SAFE_RELEASE( pMesh );
    }

    if( SUCCEEDED( hr ) )
    {
        // Create a CMeshObject resource type
        // The CMeshObject class will be responsible for tracking this mesh,
        // such that it can be notified during lost device, etc events
        *ppMeshObject = new CMeshObject( pDevice,
                                         wszFilename,
                                         pMesh2,
                                         ( D3DXEFFECTINSTANCE* )bufEffectInstances->GetBufferPointer(),
                                         dwMaterials );

        if( NULL == *ppMeshObject )
            hr = E_OUTOFMEMORY;
    }

    SAFE_RELEASE( bufEffectInstances );
    SAFE_RELEASE( bufAdjacency );

    return hr;
}


//--------------------------------------------------------------------------------------
// Following the sucessful load of a new mesh, a material is created for each mesh
// material subset.
//--------------------------------------------------------------------------------------
CMeshObject::CMeshObject( LPDIRECT3DDEVICE9 pDevice,
                          wstring tsName,
                          const LPD3DXMESH pMesh,
                          D3DXEFFECTINSTANCE* pEffectInstance,
                          DWORD dwMaterials ) : CResource <LPD3DXMESH>( tsName, pMesh )
{
    while( dwMaterials-- )
    {
        CMaterial* pMaterial;

        if( NULL == ( pMaterial = new CMaterial( pDevice, pEffectInstance ) ) )
            throw bad_alloc();

        m_vecMaterialList.push_back( pMaterial );
        pEffectInstance++;
    }
}


//--------------------------------------------------------------------------------------
// Following the release of a mesh, its associated materials (allocated in the
// CMeshObject constructor) must be destroyed.
//--------------------------------------------------------------------------------------
CMeshObject::~CMeshObject()
{
    while( !m_vecMaterialList.empty() )
    {
        CMaterial* pMaterial = m_vecMaterialList.back();
        m_vecMaterialList.pop_back();
        delete pMaterial;
    }
}


//--------------------------------------------------------------------------------------
// Mesh Lost-Device Entry Point
// This entry point allows D3DX Meshes to take actions necessary prior to a Device-Reset,
// such releasing any D3DPOOL_DEFAULT resources.
// Following a lost-device event, each mesh within the map of loaded meshes
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::LostDevice() is responsible for enumerating each mesh and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CMeshObject::OnLostDevice()
{
    // D3DPOOL_MANAGED meshes need not implement special lost device handling
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Mesh Reset-Device Entry Point
// This entry point allows meshes to take actions necessary following a Device-Reset,
// such as re-creating any D3DPOOL_DEFAULT resources.
// Following a reset-device event, each mesh within the map of loaded meshes
// (CResource::m_mapLoaded) will have this entry point called.
// CResource::ResetDevice() is responsible for enumerating each mesh and calling this
// entry point.
//--------------------------------------------------------------------------------------
HRESULT CMeshObject::OnResetDevice()
{
    // D3DPOOL_MANAGED meshes need not implement special reset handling
    return S_OK;
}


//--------------------------------------------------------------------------------------
// On creation of a new material, the required effect is loaded, and an effect
// instance is created to hold parameter values for the effect.
//--------------------------------------------------------------------------------------
CMaterial::CMaterial( LPDIRECT3DDEVICE9 pDevice, D3DXEFFECTINSTANCE* pEffectInstance ) : m_pEffect( NULL ),
                                                                                         m_pEffectInstance( NULL )
{
    // The D3DXEFFECTINSTANCE structure contains pointers to ANSI strings.
    // The Effect filename is converted prior to usage.
    WCHAR wszEffectFile[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, pEffectInstance->pEffectFilename, -1, wszEffectFile, MAX_PATH );
    wszEffectFile[ MAX_PATH - 1 ] = 0;

    // If the effect file has been previously loaded, CEffect::Create will return the previously loaded
    // effect (after incrementing it's ref count).
    if( FAILED( CEffect::Create( pDevice, wszEffectFile, &m_pEffect ) ) )
        throw exception( "CEffect::Create failed" );

    // Even if re-using a previously loaded effect, an effect instance is required to hold the unique
    // parameters for this material
    if( NULL == ( m_pEffectInstance = new CEffectInstance( pEffectInstance, m_pEffect->getPointer() ) ) )
        throw bad_alloc();

    // The effect instance tells is which texture filenames are required, but does not load them.
    // The material must obtain the list of materials from the effect instance class and load them
    // itself.
    const CEffectInstance::texture_annotations& annotations = m_pEffectInstance->getTextureAnnotations();

    for( CEffectInstance::texture_annotations::const_iterator it = annotations.begin();
         it != annotations.end();
         it++ )
    {
        // Create the texture (if previously loaded, CTexture::Create will return the
        // previously loaded instance and increment its ref count).
        CTexture* pTexture = NULL;
        if( FAILED( CTexture::Create( pDevice, ( *it ).second.c_str(), &pTexture ) ) )
            throw exception( "CTexture::Create Failed" );

        // The effect instance must be notified with the underlying D3D texture resource
        // pointer.  This will allow CEffectInstance::apply to update the effect with the
        // correct texture prior to rendering.
        if( FAILED( m_pEffectInstance->addTexture( ( *it ).first, pTexture ) ) )
            throw exception( "CEffectInstance::addTexture failed" );

        // The texture will need to be released when no longer needed by the material (eg,
        // when the CMaterial destructor executes).  This requires tracking the texture
        // until release time.
        m_vecTextures.push_back( pTexture );
    }
}


//--------------------------------------------------------------------------------------
// When the material is destroyed (due to the destruction of the mesh that created it)
// all resources that it created (see the CMaterial constructor) must be released
//--------------------------------------------------------------------------------------
CMaterial::~CMaterial()
{
    SAFE_RELEASE( m_pEffect );

    // Each texture in the m_vecTextures container is released, and removed from the
    // container, until the container is empty.
    while( !m_vecTextures.empty() )
    {
        CTexture* pTexture = m_vecTextures.back();
        m_vecTextures.pop_back();
        SAFE_RELEASE( pTexture );
    }

    SAFE_DELETE( m_pEffectInstance );
}


//--------------------------------------------------------------------------------------
// The D3DXEFFECTINSTANCE structure (obtained from the mesh) must be parsed, and
// individual parameter values for this material are stored in the CEffectInstance
// class.
// Parsing the D3DXEFFECTINSTANCE structure occurs here, in the constructor.  Additionally,
// the CEffectInstance class is responsible for determining the desired technique from the
// effect file, which also is done here.
//--------------------------------------------------------------------------------------
CEffectInstance::CEffectInstance( const D3DXEFFECTINSTANCE* pEffectInstance,
                                  LPD3DXEFFECT pEffect ) : m_pEffect( pEffect )
{
    // The CEffectInstance class caches D3DXHANDLE values within the effect, such that
    // parameter values can be easily loaded into the effect at render time.
    // Should the D3DX Effect resource be unloaded and recreated, the handles would need
    // to be re-cached from the new instance of the effect resource.  To enforce this, 
    // the Effect Pointer is also stored by the Effect Instance and it's reference count
    // is incremented.  To unload the effect, all associated instances of 
    // CEffectInstance must be destroyed.
    m_pEffect->AddRef();

    // This loop is where parameter values are parsed out of the D3DXEFFECTDEFAULT structure,
    // which was obtained when the mesh was loaded.
    LPD3DXEFFECTDEFAULT pDefaults = pEffectInstance->pDefaults;
    for( DWORD dw = 0; dw < pEffectInstance->NumDefaults; dw++ )
    {
        // The only string parameters that are supported are texture filenames, which
        // (for this sample) are loaded in the form of Texture#@Name, where # indicates the
        // intended texture stage.
        if( D3DXEDT_STRING == pDefaults->Type )
        {
            string value( ( LPSTR )pDefaults->pValue, ( LPSTR )pDefaults->pValue + pDefaults->NumBytes );
            string tsName( pDefaults->pParamName );
            if( string::npos != tsName.find( "Texture" ) && string::npos != tsName.find( "@Name" ) )
            {
                // A texture annotaion was found
                // convert the texture name to WCHARs from ANSI
                // The texture loader (CTexture::Create) does not accept ANSI strings.
                WCHAR wszTexture[MAX_PATH];
                MultiByteToWideChar( CP_ACP, 0, value.c_str(), -1, wszTexture, MAX_PATH );
                wszTexture[ MAX_PATH - 1 ] = 0;

                // Add texture filename to the texture annotaions container
                m_textureAnnotations.insert( texture_annotations::value_type( tsName, wszTexture ) );
            }
            pDefaults++;
            continue;
        }

        // Add a new value parameter
        setValue( string( pDefaults->pParamName ), ( BYTE* )pDefaults->pValue, pDefaults->NumBytes );

        pDefaults++;
    }
}


//--------------------------------------------------------------------------------------
// Release the reference count (held on the D3DX Effect) that was AddRef'd in the
// CEffectInstance constructor
//--------------------------------------------------------------------------------------
CEffectInstance::~CEffectInstance()
{
    m_pEffect->Release();
}


//--------------------------------------------------------------------------------------
// This entry point is called to map an Effect Texture parameter to a D3D Texture
// that was loaded from the filename specified in the texture annotation.
//--------------------------------------------------------------------------------------
HRESULT CEffectInstance::addTexture( string annotation_name, CTexture* pTexture )
{
    string param_name = annotation_name;

    // Strip the annotation off the annotaion string -- this determines the name
    // of the effect texture parameter ('Texture#@Name' becomes 'Texture#'.)
    string::size_type annotation_delimeter = param_name.find_last_of( "@" );
    if( string::npos != annotation_delimeter )
        param_name.erase( param_name.begin() + annotation_delimeter, param_name.end() );

    // Heightmaps must be converted into normal maps before use
    // Determine if the effect requires a normal map
    // If the effect texture parameter contains an annotation "NormalMap", convert to a normal map
    LPDIRECT3DBASETEXTURE9 pTexturePtr = NULL;
    D3DXHANDLE hP;
    if( NULL != ( hP = m_pEffect->GetParameterByName( NULL, param_name.c_str() ) )
        && NULL != m_pEffect->GetAnnotationByName( hP, "NormalMap" ) )
    {
        HRESULT hr;
        V_RETURN( pTexture->GetNormalMapPtr( &pTexturePtr ) );
    }
    else
    {
        pTexturePtr = pTexture->getPointer();
    }

    // Add a new "value" to be loaded into the effect when apply() is called.
    // Here, the value is a texture pointer.
    setValue( param_name, ( BYTE* )&pTexturePtr, sizeof pTexturePtr );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// The CEffectInstance class is used to set Effect Parameter values that may be unique
// to each material instance.  This action is accomplished via a call to apply() prior
// to rendering.
//--------------------------------------------------------------------------------------
HRESULT CEffectInstance::apply() const
{
    bool bResult = true;

    instance_params::const_iterator it;

    // All material parameters (originating from the D3DXEFFECTINSTANCE structure obtained
    // during mesh loading) are contained by the m_instanceParams container.  Parameters
    // that did not exist within the effect are not added to this container (see
    // CEffectInstance::setValue).
    for( it = m_instanceParams.begin();
         it != m_instanceParams.end();
         it++ )
    {
        bResult &= SUCCEEDED(
            m_pEffect->SetValue( ( *it ).first, &*( *it ).second.begin(), ( UINT )( *it ).second.size() )
            );
    }

    return bResult ? S_OK : E_FAIL;
}


//--------------------------------------------------------------------------------------
// Protected member setValue is used to set parameter values within the effect instance.
//--------------------------------------------------------------------------------------
void CEffectInstance::setValue( string name, BYTE* pValue, UINT nSizeInBytes )
{
    instance_value value( pValue, pValue + nSizeInBytes );

    // If the parameter is not exposed by the effect, it is ignored.
    D3DXHANDLE h = m_pEffect->GetParameterByName( NULL, name.c_str() );

    if( NULL != h )
    {
        // The underlying container type of m_instanceParams is a map.
        // The map container was selected because a map ensures that duplicate parameter
        // entries do not exist.  Furthermore, a map efficiently implements a find()
        // operation, allowing existing items to be located quickly.
        // Because an attempt to insert duplicate items of the same key value (D3DXHANDLE)
        // will fail, supporting the update of an existing key value requires locating
        // the item and manually updating it, as is done below.
        instance_params::iterator it = m_instanceParams.find( h );

        if( m_instanceParams.end() != it )
            ( *it ).second = value;
        else
            m_instanceParams.insert( instance_params::value_type( h, value ) );
    }
}
