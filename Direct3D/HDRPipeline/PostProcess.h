//======================================================================
//
//      HIGH DYNAMIC RANGE RENDERING DEMONSTRATION
//      Written by Jack Hoxley, November 2005
//
//======================================================================

// System includes
#include <d3dx9.h>

#ifndef INC_POSTPROCESS_H
#define INC_POSTPROCESS_H

// All post processing functionality is wrapped up, and consequently
// accessed via, the following namespace:
namespace PostProcess
{

// This function creates the necessary resources for the
// functionality of this module.
HRESULT CreateResources( IDirect3DDevice9* pDevice, const D3DSURFACE_DESC* pDisplayDesc );

// This provides the opposite functionality to the above
// 'CreateResources' - making sure that any allocated resources
// are correctly released.
HRESULT DestroyResources();

// The core function for this method - takes the HDR data from
// 'HDRScene' and applies the necessary steps to produce the
// post-processed input into the final image composition.
HRESULT PerformPostProcessing( IDirect3DDevice9* pDevice );

// The final image composition requires the results of this modules
// work as an input texture. This function allows other modules to
// access the texture and use it as appropriate.
HRESULT GetTexture( IDirect3DTexture9** pTexture );

// The following 4 pairs of functions allow the respective parameters
// to be exposed via the GUI. Because the actual GUI events are contained
// entirely in 'HDRDemo.cpp' these accessors are required:
float GetBrightPassThreshold();
void SetBrightPassThreshold( const float& threshold );

float GetGaussianMultiplier();
void SetGaussianMultiplier( const float& multiplier );

float GetGaussianMean();
void SetGaussianMean( const float& mean );

float GetGaussianStandardDeviation();
void SetGaussianStandardDeviation( const float& sd );

// This function takes all the intermediary steps created/used by
// the 'PerformPostProcessing' function (above) and displays them
// to the GUI. Also annotates the steps.
HRESULT DisplaySteps( IDirect3DDevice9* pDevice, ID3DXFont* pFont, ID3DXSprite* pTextSprite,
                      IDirect3DTexture9* pArrowTex );

// A simple utility function that makes a guesstimate as to
// how much VRAM is being used by this modules key resources.
DWORD CalculateResourceUsage();

}
;

#endif // INC_POSTPROCESS_H
