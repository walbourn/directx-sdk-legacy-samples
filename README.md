# DirectX SDK Legacy Samples

This repo contains Direct3D 9, Direct3D 10, a few Direct3D 11, and DirectSound samples that originally shipped in the legacy DirectX SDK. These are all **Windows desktop** applications for Windows 7 Service Pack 1 or later.

They have been cleaned up to build using the Windows 10 SDK, and _DO NOT_ require the DirectX SDK to build. They make use of the [Microsoft.DXSDK.D3DX](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX) NuGet package for the legacy D3DX9/D3DX10/D3DX11 libraries. Projects for Visual Studio 2019 are provided, and can be upgraded to VS 2022.

The DXUT here supports Direct3D 9 and Direct3D 10. The DXUT11 here supports Direct3D 9 and Direct3D 11. Both have been modified to use a locally built dxerr instead of dxerr.lib. For more information, see [this blog post](https://walbourn.github.io/wheres-dxerr-lib/).

For fully cleaned up and modernized versions of DXUT11, Effects11, Direct3D 11 samples/tutorials, and other legacy DirectX SDK samples see [DXUT](https://github.com/microsoft/DXUT/wiki), [FX11](https://github.com/microsoft/FX11/wiki), and [directx-sdk-samples](https://github.com/walbourn/directx-sdk-samples). Those samples do not make use of the legacy D3DX libraries.

* [Microsoft Docs](https://docs.microsoft.com/en-us/windows/desktop/directx-sdk--august-2009-)* [Where is the DirectX SDK (2021 Edition)?](https://aka.ms/dxsdk)
* [DirectX SDK Samples Catalog](https://walbourn.github.io/directx-sdk-samples-catalog/)
* [The Zombie DirectX SDK](https://aka.ms/AA4gfea)

## Notices

All content and source code for this package are subject to the terms of the [MIT License](https://github.com/walbourn/directx-sdk-legacy-samples/blob/main/LICENSE). Use of the NuGet is subject to it's own license terms.

The ATI Research 3D Application Research Group contributed IrradianceVolume and ParallaxOcclusionMapping.

The AMD Developer Relations Team contributed DepthOfField10.1, HDAO10.1, TransparencyAA10.1, ContactHardeningShadows11, DecalTessellation11, DetailTessellation11, and PNTriangles11.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Release notes

* The DirectSound samples use MFC, so with Visual Studio you need to install the *C++ MFC for latest v142 build tools (x86 & x64)* (``Microsoft.VisualStudio.Component.VC.ATLMFC``) optional component.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.
