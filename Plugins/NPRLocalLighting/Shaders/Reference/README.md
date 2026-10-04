# HLSL reference

`LocalFill.hlsl` mirrors the code embedded in the bundled MaterialFunction Custom node. It is a readable reference, not a shader include: the material has no file-path dependency on this folder.

Inputs are wired inside the function. Eight Float4 CPD vector parameters use ABI v1 and default to zero. The only public function pin is optional AO (default 1). Artistic tuning parameters are named material parameters inside the function.

`UseCustomColor` is wired from the `PLR_UseCustomColor` Static Switch Parameter (True=1, False=0, default False). False selects CPD scene chroma; True selects `PLTint` / `PLR_FillTint`. Runtime energy/visibility/fade remains in ColorWeight.w in both modes. The custom color replaces, rather than multiplies, scene hue.

`PLR_Hardness` is normalized 0–1 in this public interface. The old private lab multiplied its input by 100; map old values accordingly if matching that experiment. No old asset is changed by this release.
