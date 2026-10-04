# NPR Local Lighting — release candidate

Artistic local-light fill for Unreal Engine **Unlit** materials. Ordinary Point and Spot lights supply position, range, scene color and brightness; a receiver component selects at most two lights and writes per-mesh Custom Primitive Data. The bundled material function turns this data into an additive RGB fill.

This is an NPR control layer, **not Default Lit/PBR**, a native shadow replacement or a renderer modification. No extra rendering pass, SceneCapture, global MPC or modified engine is required.

## Contents

Only these three content assets are distributed, under the plugin mount `/NPRLocalLighting/Materials/`:

- `MF_NPR_LocalFill`: complete CPD input, relative pixel position, Point/Spot response, optional AO and hue-preserving additive cap.
- `M_NPR_Unlit`: a plain opaque Surface / Unlit example.
- `MI_NPR_Unlit`: its material instance; no external textures or tutorial overrides.

The C++ Runtime module supplies the subsystem and receiver. The original project-only Editor lab module, tutorial materials, character assets, MPC, control Blueprints and experimental maps are excluded.

## Installation

1. Copy this entire folder into **your project's `Plugins/NPRLocalLighting/`**. Do not put the `.uasset` files under `/Game`: their internal references use the plugin mount.
2. Build against your exact installed Unreal Engine version. This GitHub-style folder deliberately contains source, not machine-specific DLLs. A C++ toolchain is required when no matching precompiled distribution is provided.
3. Enable **NPR Local Lighting**, then restart the editor. In the Content Browser, enable **Show Plugin Content**.
4. Open `NPRLocalLighting/Materials/MI_NPR_Unlit`, then assign it to a test mesh.

Do not install a second copy of the same plugin name alongside an existing installation. Back up the existing plugin before upgrading.

## Minimal material connection

Set your own material to **Surface / Unlit**. Add `MF_NPR_LocalFill`, then connect:

```text
Your original unlit color -> Add.A
MF_NPR_LocalFill.Fill     -> Add.B
Add                      -> Emissive Color
```

That's the entire material-side integration. Do not connect it to Base Color expecting Lit lighting. The example has only BaseColor, one function call, Add and an output Named Reroute. The function's optional `AO` input defaults to **1**; supply a scalar occlusion mask if desired. It does not implement the material's base shading/AO by itself.

The eight runtime CPD vector parameters and `AbsoluteWorldPosition - ActorPositionWS` are **inside the function**. Do not recreate them in the parent material or override `PLR_L0_*` / `PLR_L1_*` in the instance.

## Mesh/actor setup — no character Blueprint required

1. Select the placed mesh actor and **Add Component → NPR Local Light Receiver**.
2. With one compatible mesh, leave **Auto Find Target Mesh** enabled. With multiple meshes, choose **Mesh Component** explicitly. Target and receiver must belong to the same actor/world.
3. For a static mesh, click **Create Head Anchor**, select the new anchor and move it to the region receiving fill. The initial 150 cm is only a starting offset, not automatic detection. For a skeletal mesh, choose a valid **Head Socket** instead.
4. Click **Validate Setup**. `Ready` means setup passed, not that editor-viewport lighting is running.
5. Place a real **Movable Point Light** or **Spot Light** whose range covers the anchor. Start with a broad spot cone or a point light.
6. **Play or Simulate**. The runtime subsystem does not update the static editor viewport. `Receiving` and an empty `LastError` indicate runtime registration.

For a prop/sphere rather than a head, move the anchor to the desired sampling center. Candidate selection and whole-object occlusion still use that anchor, not every pixel. ConfigureReceiver is available to Blueprint/C++; native component pickers and direct references also support integrations such as MCP. No MCP plugin is a product dependency.

## Material instance tuning

| Parameter | Default | Meaning |
| --- | --- | --- |
| BaseColor | dark blue-grey | Original flat Unlit color; replace with your own base color chain in custom materials. |
| PLR_UseCustomColor | false | Static switch: false uses scene-light color; true replaces it with PLR_FillTint. |
| PLR_FillTint | white | Custom fill color, used only when PLR_UseCustomColor is true. Not multiplied by scene-light hue. |
| PLR_FillStrength | 1 | Material-side artistic multiplier before the additive cap; 0 disables this fill. |
| PLR_RadiusScale | 1 | Artistic scale applied to real attenuation radius. |
| PLR_EdgeSoftness | 0.25 | Fraction of the effective radius occupied by the inner transition: 0 intentionally hard, 1 smooth across the full radius. Does not move the outer boundary. |
| PLR_UseMaxAdd | false | Static switch enabling the optional fill ceiling. MaxAdd is visible only when enabled. |
| PLR_MaxAdd | 1 | Maximum RGB component of the **added fill**, not the final Emissive. Only with UseMaxAdd; scales RGB uniformly to preserve hue. Lower it BELOW the current peak to see a change. |

Version 0.4 replaces the old Hardness/Smooth/Offset controls with separate edge-shape, gain and optional ceiling controls. There is no one-to-one mapping of arbitrary old combinations. Original tutorial assets and historical packages are preserved. `EdgeSoftness=0` deliberately selects a hard boundary without division by zero; positive values use a smoothstep fade inside the effective outer radius. Custom FillTint is visible only when UseCustomColor is enabled.

RadiusScale above 1 now expands CPU candidate selection as well as the material's range. The receiver reads effective PLR_RadiusScale values (including live MID overrides) from compatible slots at 10 Hz. Its conservative selection scale is the maximum of 1 and those values; CPD retains the physical radius and the shader scales it once. Multiple compatible slots share one two-light shortlist. A large-scale slot can therefore influence which two lamps another slot receives. Non-finite or negative scales refuse setup.

Receiver **Artistic Strength** is per object. **Physical Reference** (default 100) and **Unitless Reference** (default 8) control brightness mapping. Engine-unit/color-temperature conversion is read first; mapped brightness is artistic `E/(E+Reference)`, not a photometrically exact Lit exposure match. Increasing raw light intensity is not the only way to adjust fill.

### Choosing the color mode

In the instance's Static Switch Parameters, enable the override checkbox for `PLR_UseCustomColor`, then set its value. **False (default)** follows the real lamp color, including color temperature. **True** uses `PLR_FillTint` instead; enable that vector's override and pick the desired color. Red scene light plus green custom color produces green fill, not a red-times-green blackout. Both selected lamps use the same custom color.

This is a material-instance configuration switch, not a per-frame Blueprint toggle; changing it may compile a shader permutation. It changes only fill color. Lamp selection, energy, range, spot cones, occlusion and switching weights still come from the scene; no valid light or zero strength still means zero fill. Energy mapping includes scene-color luminance, so equal raw intensity with different lamp colors need not produce equal brightness. The old light-color-times-tint behavior is intentionally replaced by these two explicit modes.

## Data contract and limitations

- Reserve CPD **float indices 0–31 on the entire mesh primitive**, including its other material slots. This is not a per-material allocation. Foreign parameters/nonzero reserved data/external writers stop the receiver rather than silently overwrite them. Indices 32+ are not owned.
- Each light uses four Float4 parameters: PositionRadius, ColorWeight, DirectionOuter, Control. Names `PLR_L0_*`/`PLR_L1_*` and their indices are ABI v1; defaults are all zero.
- Pixel/light positions are relative to **shader ActorPositionWS**, supplied by `TargetMesh->GetActorPositionForRenderer()`. With cross-actor attachment this may be the render attachment root's actor origin, not the child actor's own origin. The CPU and shader use the same origin; keep the built-in coordinate chain. Manual integrations must follow this contract too.
- Up to two selected lights, four shortlisted candidates, 10 Hz candidate/occlusion updates, smooth light-identity switching. Viable current/pending identities are included in the four-candidate budget so hysteresis cannot leave their visibility unrefreshed. Current selected lamp data is updated every frame. One receiver exclusively owns a mesh; a duplicate is rejected before writing even when CPD is still zero.
- Three asynchronous Visibility traces from the anchor and left/right offsets approximate **whole-head/object occlusion**. This does not sample VSM or individual strands. No blocking Visibility collision means no occlusion. Invisible auxiliary proxies should not block Visibility. A lamp with Cast Shadows disabled bypasses this occlusion approximation.
- Global trace scheduling budget 32/frame; stale results conservatively switch fill off. Lighting Channels, visibility, AffectsWorld and invalid/destroyed lamps are respected.
- Spawned lamp actors/streamed levels are discovered. Newly added light components on an already existing actor must call `RegisterLight`.
- No automatic eyebrow-through-bangs/transparency, directional-light NPR shading, hair BRDF, KK, Matcap or Rim is bundled. This release only adds local fill to the Unlit material you already have.
- No claim of zero GPU cost, all-platform performance, production skeletal-animation coverage or packaged streaming coverage. Original lab timings are not a new release benchmark.

## Validation and release status

See `Docs/VALIDATION.md` for evidence and pending checks. Local tests do not imply Fab acceptance. This plugin is licensed under MIT; see the accompanying `LICENSE` file. Publisher identity and Fab listing details still need to be completed. The MIT license does not grant rights to third-party material.

## 中文快速使用

把整个插件文件夹放进自己项目的 `Plugins`，编译、启用并重启；Content Browser 打开「显示插件内容」。材质端只需 `原颜色 + MF_NPR_LocalFill.Fill → Emissive`，保持 Unlit。MF 的 AO 可不接，默认 1；艺术参数在 MI 的 NPR Local Fill 分组中。

普通 Mesh Actor 添加 NPR Local Light Receiver，创建并移动锚点、Validate Setup，再放普通 Point/Spot Light，进入 Play/Simulate 看效果。锚点用于选灯/整体遮挡，不是逐像素阴影。运行时 CPD 参数不要手动修改，CPD 0–31 不要被其他系统占用。

颜色模式：MI 中 `PLR_UseCustomColor` 默认关闭，跟随真实灯色；勾选参数左侧覆盖框并把开关值打开后，用 `PLR_FillTint` 自定义补光颜色，不再与灯色相乘。两盏灯共用这个自定义色；没有灯仍不补光。开关是静态材质配置，首次切换可能编译。

本文件夹只用于独立发布准备，不包括原 Shader 学习项目、角色、教程资源或测试关卡。代码与随附插件内容按 MIT 许可；Fab 上架仍需填写发布者信息并通过平台审核。
