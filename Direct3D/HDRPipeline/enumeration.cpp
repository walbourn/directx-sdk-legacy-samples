//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, November 2005
//
//======================================================================

#include "DXUT.h"
#include "Enumeration.h"

namespace HDREnumeration
{

//--------------------------------------------------------------------------------------
//  FindBestHDRFormat( )
//
//      DESC:
//          Due to High Dynamic Range rendering still being a relatively high-end
//          feature in consumer hardware, several basic properties are still not
//          uniformly available. Two particular examples are texture filtering and
//          post pixel-shader blending.
//
//          Substantially better results can be gained from this sample if texture
//          filtering is available. Post pixel-shader blending is not required. The
//          following function will enumerate supported formats in the following order:
//
//              1. Single Precision (128 bit) with support for linear texture filtering
//              2. Half Precision (64 bit) with support for linear texture filtering
//              3. Single Precision (128 bit) with NO texture filtering support
//              4. Half Precision (64 bit) with NO texture filtering support
//
//          If none of the these can be satisfied, the device should be considered
//          incompatable with this sample code.
//
//      PARAMS:
//          pBestFormat  : A container for the detected best format. Can be NULL.
//
//      NOTES:
//          The pBestFormat parameter can be set to NULL if the specific format is
//          not actually required. Because of this, the function can be used as a
//          simple predicate to determine if the device can handle HDR rendering:
//
//              if( FAILED( FindBestHDRFormat( NULL ) ) )
//              {
//                  // This device is not compatable.
//              }
//
//--------------------------------------------------------------------------------------
HRESULT FindBestHDRFormat( D3DFORMAT* pBestFormat )
{

    // Retrieve important information about the current configuration
    DXUTDeviceSettings info = DXUTGetDeviceSettings();

    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );    

    if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                        info.d3d9.AdapterFormat,
                                                        D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET,
                                                        D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) ) )
    {
        // We don't support 128 bit render targets with filtering. Check the next format.
        OutputDebugString(
            L"Enumeration::FindBestHDRFormat() - Current device does *not* support single-precision floating point textures with filtering.\n" );

        if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                            info.d3d9.AdapterFormat,
                                                            D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET,
                                                            D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
        {
            // We don't support 64 bit render targets with filtering. Check the next format.
            OutputDebugString(
                L"Enumeration::FindBestHDRFormat() - Current device does *not* support half-precision floating point textures with filtering.\n" );

            if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                                info.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                                                D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) ) )
            {
                // We don't support 128 bit render targets. Check the next format.
                OutputDebugString(
                    L"Enumeration::FindBestHDRFormat() - Current device does *not* support single-precision floating point textures.\n" );

                if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                                    info.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                                                    D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) ) )
                {
                    // We don't support 64 bit render targets. This device is not compatable.
                    OutputDebugString(
                        L"Enumeration::FindBestHDRFormat() - Current device does *not* support half-precision floating point textures.\n" );
                    OutputDebugString(
                        L"Enumeration::FindBestHDRFormat() - THE CURRENT HARDWARE DOES NOT SUPPORT HDR RENDERING!\n" );
                    return E_FAIL;
                }
                else
                {
                    // We have support for 64 bit render targets without filtering
                    OutputDebugString(
                        L"Enumeration::FindBestHDRFormat() - Best available format is 'half-precision without filtering'.\n" );
                    if( NULL != pBestFormat ) *pBestFormat = D3DFMT_A16B16G16R16F;
                }
            }
            else
            {
                // We have support for 128 bit render targets without filtering
                OutputDebugString(
                    L"Enumeration::FindBestHDRFormat() - Best available format is 'single-precision without filtering'.\n" );
                if( NULL != pBestFormat ) *pBestFormat = D3DFMT_A32B32G32R32F;
            }

        }
        else
        {
            // We have support for 64 bit render targets with filtering
            OutputDebugString(
                L"Enumeration::FindBestHDRFormat() - Best available format is 'half-precision with filtering'.\n" );
            if( NULL != pBestFormat ) *pBestFormat = D3DFMT_A16B16G16R16F;
        }
    }
    else
    {
        // We have support for 128 bit render targets with filtering
        OutputDebugString(
            L"Enumeration::FindBestHDRFormat() - Best available format is 'single-precision with filtering'.\n" );
        if( NULL != pBestFormat ) *pBestFormat = D3DFMT_A32B32G32R32F;
    }

    return S_OK;

}



//--------------------------------------------------------------------------------------
//  FindBestLuminanceFormat( )
//
//      DESC:
//          << See notes for 'FindBestHDRFormat' >>
//          The luminance calculations store a single intensity and maximum brightness, and
//          as such don't need to use textures with the full 128 or 64 bit sizes. D3D
//          offers two formats, G32R32F and G16R16F for this sort of purpose - and this
//          function will return the best supported format. The following function will 
//          enumerate supported formats in the following order:
//
//              1. Single Precision (32 bit) with support for linear texture filtering
//              2. Half Precision (16 bit) with support for linear texture filtering
//              3. Single Precision (32 bit) with NO texture filtering support
//              4. Half Precision (16 bit) with NO texture filtering support
//
//          If none of the these can be satisfied, the device should be considered
//          incompatable with this sample code.
//
//      PARAMS:
//          pBestLuminanceFormat    : A container for the detected best format. Can be NULL.
//
//      NOTES:
//          The pBestLuminanceFormatparameter can be set to NULL if the specific format is
//          not actually required. Because of this, the function can be used as a
//          simple predicate to determine if the device can handle HDR rendering:
//
//              if( FAILED( FindBestLuminanceFormat( NULL ) ) )
//              {
//                  // This device is not compatable.
//              }
//
//--------------------------------------------------------------------------------------
HRESULT FindBestLuminanceFormat( D3DFORMAT* pBestLuminanceFormat )
{

    // Retrieve important information about the current configuration
    DXUTDeviceSettings info = DXUTGetDeviceSettings();

    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );

    if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                        info.d3d9.AdapterFormat,
                                                        D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET,
                                                        D3DRTYPE_TEXTURE, D3DFMT_G32R32F ) ) )
    {
        // We don't support 32 bit render targets with filtering. Check the next format.
        OutputDebugString(
            L"Enumeration::FindBestLuminanceFormat() - Current device does *not* support single-precision floating point textures with filtering.\n" );

        if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                            info.d3d9.AdapterFormat,
                                                            D3DUSAGE_QUERY_FILTER | D3DUSAGE_RENDERTARGET,
                                                            D3DRTYPE_TEXTURE, D3DFMT_G16R16F ) ) )
        {
            // We don't support 16 bit render targets with filtering. Check the next format.
            OutputDebugString(
                L"Enumeration::FindBestLuminanceFormat() - Current device does *not* support half-precision floating point textures with filtering.\n" );

            if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                                info.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                                                D3DRTYPE_TEXTURE, D3DFMT_G32R32F ) ) )
            {
                // We don't support 32 bit render targets. Check the next format.
                OutputDebugString(
                    L"Enumeration::FindBestLuminanceFormat() - Current device does *not* support single-precision floating point textures.\n" );

                if( FAILED( pD3D->CheckDeviceFormat( info.d3d9.AdapterOrdinal, info.d3d9.DeviceType,
                                                                    info.d3d9.AdapterFormat, D3DUSAGE_RENDERTARGET,
                                                                    D3DRTYPE_TEXTURE, D3DFMT_G16R16F ) ) )
                {
                    // We don't support 16 bit render targets. This device is not compatable.
                    OutputDebugString(
                        L"Enumeration::FindBestLuminanceFormat() - Current device does *not* support half-precision floating point textures.\n" );
                    OutputDebugString(
                        L"Enumeration::FindBestLuminanceFormat() - THE CURRENT HARDWARE DOES NOT SUPPORT HDR RENDERING!\n" );
                    return E_FAIL;
                }
                else
                {
                    // We have support for 16 bit render targets without filtering
                    OutputDebugString(
                        L"Enumeration::FindBestLuminanceFormat() - Best available format is 'half-precision without filtering'.\n" );
                    if( NULL != pBestLuminanceFormat ) *pBestLuminanceFormat = D3DFMT_G16R16F;
                }
            }
            else
            {
                // We have support for 32 bit render targets without filtering
                OutputDebugString(
                    L"Enumeration::FindBestLuminanceFormat() - Best available format is 'single-precision without filtering'.\n" );
                if( NULL != pBestLuminanceFormat ) *pBestLuminanceFormat = D3DFMT_G32R32F;
            }

        }
        else
        {
            // We have support for 16 bit render targets with filtering
            OutputDebugString(
                L"Enumeration::FindBestLuminanceFormat() - Best available format is 'half-precision with filtering'.\n"
                );
            if( NULL != pBestLuminanceFormat ) *pBestLuminanceFormat = D3DFMT_G16R16F;
        }
    }
    else
    {
        // We have support for 32 bit render targets with filtering
        OutputDebugString(
            L"Enumeration::FindBestLuminanceFormat() - Best available format is 'single-precision with filtering'.\n"
            );
        if( NULL != pBestLuminanceFormat ) *pBestLuminanceFormat = D3DFMT_G32R32F;
    }

    return S_OK;

}

}
;
