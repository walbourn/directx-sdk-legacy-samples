//--------------------------------------------------------------------------------------
// File: LoadSceneFromX.cpp
//
// Enables the sample to build a scene from an x-file ('scene.x').
// The x-file has been extended to include custom templates for specifying mesh filenames
// and camera objects within frames.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#pragma warning(disable: 4995)
#include "LoadSceneFromX.h"
#include <d3dx9xof.h>
#pragma warning(default: 4995)


//--------------------------------------------------------------------------------------
// Forward declaration 
//--------------------------------------------------------------------------------------
HRESULT ProcessFrame( ID3DXFileData* pFrame,
                      D3DXMATRIX* pParentMatrix,
                      vector <FRAMENODE>& vecFrameNodes );


//--------------------------------------------------------------------------------------
// The scene is generated from an xfile ('scene.x') that contains a hierarchy of
// transformations and mesh file refernces.
// Two private xfile templates are used, extending the xfile format to provide support
// for such features.  These templates are found within a 'frame' datatype, which
// are organized into a hierarchy.  For the purposes of this sample, the hierarchy
// is immediately collapsed, as there is need to animate objects within the scene.
// Template FrameMeshName includes:
//      RenderPass: Default Render Pass, in which the mesh is to be rendered in
//      FileName: Filename of a mesh that exists at this frame location
// Template FrameCamera includes:
//      RotationScaler:  Sets the speed by which the camera rotates
//      MoveScaler:      Sets the speed at which the camera moves
//--------------------------------------------------------------------------------------
static const CHAR szTemplates[] = "xof 0303txt 0032\
    template FrameMeshName { \
    <c2a50aed-0ee9-4d97-8732-c14a2d8a7825> \
        DWORD RenderPass;STRING FileName;} \
    template FrameCamera { \
    <f96e7de6-40ce-4847-b7e9-5875232e5201> \
        FLOAT RotationScaler;FLOAT MoveScaler;} \
    template Frame { \
    <3d82ab46-62da-11cf-ab39-0020af71e433> \
    [...] } \
    template Matrix4x4 { \
    <f6f23f45-7686-11cf-8f52-0040333594a3> \
    array FLOAT matrix[16]; } \
    template FrameTransformMatrix { \
    <f6f23f41-7686-11cf-8f52-0040333594a3> \
    Matrix4x4 frameMatrix; \
    }";


//--------------------------------------------------------------------------------------
// GUIDS, corresponding to the above templates
//--------------------------------------------------------------------------------------
static const GUID   gFrameMeshName =
{
    0xc2a50aed, 0xee9, 0x4d97, { 0x87, 0x32, 0xc1, 0x4a, 0x2d, 0x8a, 0x78, 0x25 }
};
static const GUID   gCamera =
{
    0xf96e7de6, 0x40ce, 0x4847, { 0xb7, 0xe9, 0x58, 0x75, 0x23, 0x2e, 0x52, 0x1 }
};
static const GUID   gFrameTransformMatrix =
{
    0xf6f23f41, 0x7686, 0x11cf, { 0x8f, 0x52, 0x0, 0x40, 0x33, 0x35, 0x94, 0xa3 }
};
static const GUID   gFrame =
{
    0x3d82ab46, 0x62da, 0x11cf, { 0xab, 0x39, 0x0, 0x20, 0xaf, 0x71, 0xe4, 0x33 }
};


//--------------------------------------------------------------------------------------
// Reads the scene x-file, and adds the collapsed frame hierarchy to the vecFrameNodes Hierarchy.
//--------------------------------------------------------------------------------------
HRESULT LoadSceneFromX( vector <FRAMENODE>& vecFrameNodes, LPWSTR wszFileName )
{
    HRESULT hr;

    // vecNodes will contain frames found within the x-file frame hierarchy.
    vector <FRAMENODE> vecNodes;
    ID3DXFile* pXFile = NULL;
    ID3DXFileEnumObject* pEnum = NULL;

    // To begin reading the x-file, a ID3DXFile interface must be created
    V_RETURN( D3DXFileCreate( &pXFile ) );

    // To 'understand' the x-file, templates that are used within it must be registered
    V_RETURN( pXFile->RegisterTemplates( szTemplates, sizeof( szTemplates ) - 1 ) );

    // Creating an ID3DXFileEnumObject allows the app to enumerate top-level data objects
    V_RETURN( pXFile->CreateEnumObject( wszFileName, D3DXF_FILELOAD_FROMWFILE, &pEnum ) );

    // Because the enum object was successfully created, the ID3DXFile interface pointer can be released
    SAFE_RELEASE( pXFile );

    SIZE_T toplevel_children = 0;

    // Retrieving the number of children allows the app to iterate across each child in a loop.
    V_RETURN( pEnum->GetChildren( &toplevel_children ) );

    for( SIZE_T i = 0; i < toplevel_children; i++ )
    {
        GUID guid;
        ID3DXFileData* pChild = NULL;

        // To read the data object, a pointer to its ID3DXFileData interface is obtained
        V_RETURN( pEnum->GetChild( i, &pChild ) );

        // The guid corresponding to the type of the data object can be obtained via GetType.
        // For the purposes of this sample, if the top-level data object is not a frame, it is ignored.
        // Any frames containing mesh filename references will be added to vecFrameNodes.
        V_RETURN( pChild->GetType( &guid ) );

        // Add any frames containing meshes to the vector of frames, vecFrameNodes
        if( guid == gFrame )
            ProcessFrame( pChild, NULL, vecFrameNodes );

        // 
        SAFE_RELEASE( pChild );
    }

    //
    SAFE_RELEASE( pEnum );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Invoked by LoadSceneFromX - Process one frame located within the scene xfile
// Reads any meshes, or cameras, that may exist within this frame.
// Additionally, the frame's transform is collapsed (its matrix is concatenated with that
// of it's parent).
// Note: Assumes the parent node has been collapsed
//--------------------------------------------------------------------------------------
HRESULT ProcessFrame( ID3DXFileData* pFrame, D3DXMATRIX* pParentMatrix, vector <FRAMENODE>& vecFrameNodes )
{
    HRESULT hr = S_OK;

    SIZE_T children = 0;
    FRAMENODE node;

    // In the event no corresponding frame transform matrix is located within the frame,
    // consider it to be identity.
    // Use the collapsed value of the parent matrix if it exists.
    // If no parent matrix exists, this is a top-level data frame.
    if( pParentMatrix )
        node.mat = *pParentMatrix;
    else
        D3DXMatrixIdentity( &node.mat );

    // For the purposes of this sample, the frame hierarchy is collapsed in-place as each frame is encountered.
    // A typical application may have a 'scene graph' arrangement of transformations, which are collapsed
    // as-needed at *runtime*.  
    // However, since the hierarchy *is* collaped in-place, it must be ensured that the frame's transform matrix
    // has been updated and collapsed BEFORE processing child frames.
    // To defer processing of child frames, they are placed into the vecChildFrames container.
    vector <ID3DXFileData*> vecChildFrames;

    // Retrieving the number of children allows the app to iterate across each child in a loop.
    V_RETURN( pFrame->GetChildren( &children ) );

    for( SIZE_T i = 0; i < children; i++ )
    {
        ID3DXFileData* pChild = NULL;
        GUID guid;

        SIZE_T data_size = 0;
        LPCVOID pData = NULL;

        // To read the data object, a pointer to its ID3DXFileData interface is obtained
        V_RETURN( pFrame->GetChild( i, &pChild ) );

        // The guid corresponding to the type of the data object can be obtained via GetType.
        V_RETURN( pChild->GetType( &guid ) );

        // The child data object is a transformation matrix -- collapse it in place, and store
        // the collapsed matrix in node.mat
        if( guid == gFrameTransformMatrix )
        {
            // ID3DXFileData::Lock allows the app to read the actual data
            // If the data size of the object does not match the expectation, it is discarded
            if( SUCCEEDED( hr ) && SUCCEEDED( hr = pChild->Lock( &data_size, &pData ) ) )
            {
                if( sizeof( D3DXMATRIX ) == data_size )
                {
                    // Collapse the matrix
                    // If the frame has a parent, the collapsed matrix is the product of this frame and the collapsed matrix
                    // of the parent frame.
                    // Otherwise, the collapsed value is the matrix itself
                    if( pParentMatrix )
                        D3DXMatrixMultiply( &node.mat, ( D3DXMATRIX* )pData, pParentMatrix );
                    else
                        node.mat = *( D3DXMATRIX* )pData;
                }

                // Having read the required data, it can now be unlocked with ID3DXFileData::Unlock
                hr = pChild->Unlock();
            }
        }

            // If the child data is a mesh file name, the mesh is added to the frame's MESH_REFERENCE container
        else if( guid == gFrameMeshName )
        {
            if( SUCCEEDED( hr ) && SUCCEEDED( hr = pChild->Lock( &data_size, &pData ) ) )
            {
                if( sizeof( DWORD ) < data_size )
                    node.meshes.push_back( MESH_REFERENCE( *( DWORD* )pData, ( LPSTR )pData + 4 ) );

                hr = pChild->Unlock();
            }
        }

            // Processing the children must be delayed until it can be guaranteed that any
            // transform matrices have been applied. (eg, until the matrix stack has been collapsed).
        else if( guid == gFrame )
        {
            // A child frame has been found
            // Keep the data object around by adding a reference to it.
            // The child will eventually be released when it is processed by iterating the container
            // of child frames (vecChildFrames).
            pChild->AddRef();
            vecChildFrames.push_back( pChild );
        }

            // A camera object was found within the frame
        else if( guid == gCamera )
        {
            if( SUCCEEDED( hr ) && SUCCEEDED( hr = pChild->Lock( &data_size, &pData ) ) )
            {
                if( 2 * sizeof( FLOAT ) == data_size )
                    node.cameras.push_back( CAMERA_REFERENCE( *( FLOAT* )pData, *( ( FLOAT* )pData + 1 ) ) );

                hr = pChild->Unlock();
            }
        }

        // Now that the Child Data Object has been read, it can be released.
        // Exception: child 'Frame' objects have been AddRef'd, to defer processing
        SAFE_RELEASE( pChild );
    }

    // Add the Frame to the collapsed node container
    vecFrameNodes.push_back( node );

    // Each child frame that was deferred can now be processed
    // This occurs by recursively invoking ProcessFrame, once for each child frame
    for( vector <ID3DXFileData*>::iterator it = vecChildFrames.begin();
         it != vecChildFrames.end();
         it++
         )
    {
        if( SUCCEEDED( hr ) )
        {
            // Recurse into ProcessFrame for this child
            hr = ProcessFrame( *it, &node.mat, vecFrameNodes );
        }

        // Done processing the child 'Frame' data object, it can be released
        SAFE_RELEASE( *it );
    }

    return hr;
}


