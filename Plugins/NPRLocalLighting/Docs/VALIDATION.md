# Release candidate validation — 2026-10-02

## Review fixes and current content — 2026-10-04

This section supersedes the old Hardness/Smooth/Offset public interface below; historical results are retained, not represented as current performance measurements.

- Runtime R01–R04 fixes: renderer attachment-root origin, retained Current/Pending visibility inside the four-candidate budget, exclusive mesh ownership before the first CPD write, and material/MID RadiusScale-aware spatial bounds and selection.
- Independently rebuilt Runtime for Editor, Game Development and Shipping. Subsequent edits only fix development-test world-context registration; non-test Runtime implementation remains identical. Compilation is not a packaged-game cook/playthrough.
- The three bundled final-mount assets now use EdgeSoftness, conditional custom FillTint and optional UseMaxAdd. An isolated project without any learning Content verified the exact MI→M→MF dependency chain, no texture/character/project references, four-node Unlit example and four SM6 permutations. PS instruction counts 174/179/174/178 are compiler statistics, not GPU timings. Content assembly and validation commandlets exited 0 with zero errors/warnings.
- Actual editor automation with NullRHI, no project Content and only the Runtime plugin: MaterialABI, MathContract, OriginOwnershipRadius and RetainedVisibilityBudget all succeeded, 4 tests, 0 failures, exit0. This includes attached Engine spheres, duplicate-zero-data ownership, live MID radius expansion across cells, six-light hysteresis and 12-receiver trace budgeting. NullRHI proves data/assertions, not images.
- A first complete-editor automation run exposed missing World Context in the new test fixture; Commandlet fallback had hidden that test-harness omission. Fixed by registering and releasing isolated test-world contexts, then reran the complete editor automation successfully. The failing process was isolated; the user's editor was not affected. The final test fixture is distributed.
- Basic-shape comparison map in the learning project was simulated separately: hard/soft range edges, outside-physical-radius fill, attached cylinder and moving-wall visibility comparison were observed. No basic-shape map or character resource is included in this package.
- No new CPU/GPU millisecond benchmark, packaged streaming, animated production character or Fab certification claim. License/publisher are still undecided; no upload occurred.

Environment: Unreal Engine 5.8.3, CL 58210709, compatible CL 55116800, Windows x64, MSVC 14.44 / Windows SDK 10.0.22621.0. This is a local candidate validation, not Fab approval or a general performance certification.

## Passed

- Standalone `BuildPlugin`: UnrealEditor Development, UnrealGame Development and UnrealGame Shipping all succeeded. Only the Runtime module is distributed; no original Editor lab module is required. A successful module build is not a packaged-game playthrough/cook.
- Three assets were remapped and saved at their final plugin paths; the resulting dependency graph is exactly `MI_NPR_Unlit -> M_NPR_Unlit -> MF_NPR_LocalFill`. The function has no external package dependency. No `/Game`, tutorial, texture, MPC or character reference is required.
- Fresh private host project, with no learning-project Content, loaded all three assets and compiled the material using D3D12 / **SM5**. Compiler error list empty; commandlet completed with 0 errors / 0 warnings.
- Before the color-mode update, that SM5 resource reported 205 pixel instructions, 136 vertex instructions, 0 samplers and 3 pixel texture samples in UE statistics. These are historical engine statistics, **not current GPU timings**, and texture sample counts are not an inventory of bundled textures. No texture asset is bundled.
- `NPRLocalLighting.Release.MaterialABI` passed: CPD metadata from the nested MF passed the actual receiver's audit, both Engine-sphere primitives could use one MI independently, a manual sample wrote the intended 32-float layout, and clearing the owned range preserved float 35.
- `NPRLocalLighting.Release.MathContract` passed: brightness mapping, 32-float ABI and hue-preserving cap. Automation report: 2 successes, 0 failures, 0 test warnings.
- In the learning project's live UE editor, the separate preview MF/M/MI were built, recompiled and explicitly saved; only the new three assets were saved. The example is opaque Surface / Unlit, with four expressions and only Emissive connected.

## Color-mode update — 2026-10-02

- Added `PLR_UseCustomColor`, a Static Switch Parameter defaulting to False. False selects scene chroma; True replaces it with `PLR_FillTint`. Neither mode alters CPD ABI, runtime lamp selection, visibility, cones or energy mapping. Old light-times-tint semantics are intentionally superseded.
- Saved only the isolated host's three bundled assets, then loaded them in separate read-only validation processes. Both mode permutations compiled for D3D12 / SM6: scene PS180, custom PS179. The default resource also reported VS148, samplers0 and estimated pixel texture samples3. These are resource statistics, not a performance benchmark.
- Audited shader structure plus CPU color-stage reference assertions covered red lamp/green custom color, identical custom hue with red/blue CPD at equal weights, two-lamp sum, no lamps and zero weight/strength/cap. This is **not** a GPU pixel-image A/B.
- Independent package dependency check still yields exactly `MI -> M -> MF`, no `/Game` dependencies. `NPRLocalLighting.Release.MaterialABI` and `MathContract` rerun with the updated content: 2 Success, 0 failures.
- Runtime C++ and installed binaries were not edited or rebuilt. Previous source-build claims above remain historical. No new Game/Shipping cook or visual/performance certification is implied.
- The local validation harness initially asserted the return of UE5.8's static-switch setter; local source shows the function never sets its bool result to True. Corrected tests verify actual readback and compiled shader resources instead. A separate CustomInput constructor incompatibility was corrected to use `set_editor_property`; failed runs did not save assets.

## Original test-tool correction

The first clean-content validation used a material-statistics method that is not exposed by UE 5.8. It failed with an AttributeError, not a shader error. Reading the local engine header identified `GetStatistics` / `get_statistics`; the corrected run explicitly enabled commandlet rendering and checked the compiler's returned error array. Only the corrected successful run supports the shader claims above.

## Deliberately not claimed

- No new whole-system CPU/GPU benchmark, close-up cost or cross-platform/older-engine compatibility claim.
- No new packaged game cook/playthrough, production character animation or packaged streaming coverage.
- No automated visual pixel equivalence to the private tutorial hair. This product bundles a new plain Unlit example, not that hair shader. The public Hardness parameter is normalized; mapping from the old private interface is documented.
- No native per-strand shadows or VSM sampling. Receiver occlusion remains collision-driven, whole-head/object approximation.
- No license selection, public upload, listing-media rights approval or Fab review has happened.

## Proven isolation

The live installed plugin descriptor/source/DLLs were not replaced. The learning project's original level, tutorial M/MI and old MF were preserved; their monitored file hashes matched the start of preparation. The plugin candidate is a **separate folder**. Test host projects, compiler binaries, build logs, Python assembly utilities and raw automation reports stay outside it.

Release preview assets use `/Game/NPRLocalLightingRelease/Materials/` in the learning project. They are **not** the files to manually redistribute; use the plugin's `Content/Materials/` files, whose internal references target `/NPRLocalLighting/Materials/`.
