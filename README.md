# DWinDirect
It is not a standalone project but rather addition to [DGLE](https://github.com/DGLE-HQ/DGLE).
DGLE headers have to be copied from [DGLE/include/cpp/](https://github.com/DGLE-HQ/DGLE/tree/master/include/cpp) to [DWinDirect/dgle/](https://github.com/DGLE-HQ/DWinDirect/tree/master/dgle) prior to attempt to build the project and DGLE.dll from [DGLE/bin/windows/](https://github.com/DGLE-HQ/DGLE/tree/master/bin/windows) should be placed to [DWinDirect/bin/](https://github.com/DGLE-HQ/DWinDirect/tree/master/bin) in order to be able to run Test project.
* __CoreRendererDX9__ depends on legacy DXSDK.
* __CoreRendererDX11__ is not yet complete and is not able to compile.
* __General__ contains general stuff such as splines, collisions, etc.

# render prototype
This is experimental high level DX11 renderer billet. Also contains vector math library which have syntax similar to HLSL/GLSL shading languages including swizzling.
