//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, November 2005
//
//======================================================================

#include <d3dx9.h>

#ifndef INC_ENUMERATION_H
#define INC_ENUMERATION_H

// Several modules making up this sample code need to make
// decisions based on what features the underlying hardware
// supports. These queries are contained in the following
// namespace:
namespace HDREnumeration
{

// The following function will examine the capabilities of the device
// and store the best format in 'pBestFormat'.
HRESULT FindBestHDRFormat( D3DFORMAT* pBestFormat );

// A similar function to above, but this one examines the single-channel
// G32R32F and G16R16F support - used by the luminance measurement calculations.
HRESULT FindBestLuminanceFormat( D3DFORMAT* pBestLuminanceFormat );

}
;

#endif
