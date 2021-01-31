# DirectX SDK Legacy Samples

This repo contains Direct3D 9 and Direct3D 10 samples that originally shipped in the legacy DirectX SDK. These are all **Windows desktop** applications for Windows 7 Service Pack 1 or later.

They have been cleaned up to build using the Windows 10 SDK, and _DO NOT_ require the DirectX SDK to build. They make use of the ``Microsoft.DXSDK.D3DX`` NuGet package for the legacy D3DX9/D3DX10/D3DX11 libraries. Projects for Visual Studio 2019 are provided.

The DXUT here supports Direct3D 9 and Direct3D 10. The DXUT11 here supports Direct3D 9 and Direct3D 11. Both have been modified to use a locally built dxerr instead of dxerr.lib. For more information, see [this blog post](https://walbourn.github.io/wheres-dxerr-lib/).

For fully cleaned up versions of DXUT11, Direct3D 11 samples/tutorials, and other legacy DirectX SDK samples see [directx-sdk-samples](https://github.com/walbourn/directx-sdk-samples). Those samples do not make use of the legacy D3DX libraries.

* [Microsoft Docs](https://docs.microsoft.com/en-us/windows/desktop/directx-sdk--august-2009-)* [Where is the DirectX SDK (2021 Edition)?](https://aka.ms/dxsdk)
* [DirectX SDK Samples Catalog](https://walbourn.github.io/directx-sdk-samples-catalog/)
* [The Zombie DirectX SDK](https://aka.ms/AA4gfea)

All content and source code for this package are subject to the terms of the [MIT License](http://opensource.org/licenses/MIT). Use of the NuGet is subject to it's own license terms.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
