# NPR Local Lighting

[English](README.md) | [简体中文](README.zh-CN.md)

**Let Unlit materials receive artistic local lighting from ordinary Unreal Engine Point and Spot lights.**

Unlit materials are useful for anime/NPR characters and other art-directed objects because you control their final color. However, they do not receive Unreal's normal surface lighting. NPR Local Lighting adds a controllable local-light fill without replacing your existing shader with Default Lit.

A runtime component selects up to **two lights per mesh**, sends their data through Custom Primitive Data (CPD), and a material function produces the additional color. Your base color, hand-painted shading and existing highlights remain yours.

## What it does—and does not do

- Follows real Point/Spot light position, range, color, temperature and brightness.
- Offers scene-light color or a custom material-instance color.
- Separates fill strength, radius, edge softness and an optional brightness ceiling.
- Supports static and skeletal meshes; different meshes can share one material instance without sharing their light selection.
- Approximates whole-object occlusion with collision queries around a configured anchor.
- Requires no engine-source changes, additional render pass, SceneCapture, global MPC or MCP connection.

This is **artistic additive fill**, not a PBR shading model or a native shadow replacement. It does not add directional/sky lighting, hair shading, KK highlights, Matcap, outlines or transparency. Integrate it into an existing Unlit shader, or start with the included plain example.

## Requirements and repository contents

Tested with **Unreal Engine 5.8.3 on Windows / Win64**. Other engine versions and platforms are not yet verified. The plugin is a beta source distribution; matching precompiled DLLs are **not** included. Building requires a compatible Unreal C++ toolchain, including MSVC and the Windows SDK.

| Location | Purpose |
| --- | --- |
| `Plugins/NPRLocalLighting/` | Standalone plugin: C++ runtime module and three material assets |
| `Content/NPRLocalLightingDemo/` | Optional demonstration map, primitive-motion Blueprints and example instances |
| `NPRLocalLightingDemo.uproject` | Clean demonstration project; no character models or third-party textures |
| `Plugins/NPRLocalLighting/Docs/VALIDATION.md` | Recorded checks and remaining validation limits |

Only the plugin folder is needed for your own project. The demonstration project is not a runtime dependency.

## 1. Download and build

Download the repository ZIP from GitHub, or clone it:

```powershell
git clone https://github.com/Tofu-22/UE5-NPR-Local-Lighting.git
```

Close the target Unreal Editor before installing or replacing plugin binaries. Back up an existing installation; do not install two copies of the same plugin.

For a source-only download, Unreal's `BuildPlugin` command can create a local, engine-matched plugin package. Example in PowerShell—**replace all three paths** with your own:

```powershell
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$pluginFile = 'C:\Projects\UE5-NPR-Local-Lighting\Plugins\NPRLocalLighting\NPRLocalLighting.uplugin'
$packageDir = 'C:\BuildOutput\NPRLocalLighting'
& "$ueRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin "-Plugin=$pluginFile" "-Package=$packageDir" -TargetPlatforms=Win64
```

Use a dedicated output directory **outside the source plugin**. This command needs the Unreal C++ build tools even if your project is Blueprint-only.

- **To use the demo:** use the built package to supply the plugin in this repository's `Plugins/NPRLocalLighting/`, then open `NPRLocalLightingDemo.uproject`.
- **To use your own project:** install the built plugin folder as `YourProject/Plugins/NPRLocalLighting/`. An existing C++ project can instead build the source plugin with its normal project build.

Enable **NPRLocalLighting** in the Plugins window and restart if requested. In the Content Browser settings, enable **Show Plugin Content**.

Keep the materials inside the plugin mount `/NPRLocalLighting/Materials/`. Do not move only the three assets into `/Game`; their internal references depend on that mount.

## 2. Try the included demonstration

Open `/Game/NPRLocalLightingDemo/Maps/L_NPRLocalLighting_Demo` and select **Simulate** from the Play menu.

The map contains sphere/cube/cylinder comparisons using fill-off, hard-edge and soft-edge instances, a radius-scale example, occlusion/attachment checks, and a moving sphere plus a rotating cube. Compare the models while the simulation is running, and move or recolor the ordinary scene lights to see the response.

**The effect runs during Play / Simulate, not in the idle editor viewport.** Stop simulation before making permanent setup changes.

## 3. Set up a mesh manually

You do **not** need to create an Actor Blueprint for an already placed Static Mesh Actor.

1. Assign `MI_NPR_Unlit` from the plugin's Materials folder to a test mesh.
2. Select its actor in the level. Use **Add Component → NPR Local Light Receiver**.
3. Select the receiver in the actor's component list:
   - With one compatible mesh, leave **Auto Find Target Mesh** enabled.
   - With several meshes, explicitly choose **Mesh Component**. The receiver and mesh must belong to the same actor.
4. Configure where the object samples lights:
   - **Static mesh / prop:** click **Create Head Anchor**. Select the new `NPR_HeadAnchor` component and move it to the head or desired sampling region. For a sphere/cube, use its center.
   - **Skeletal mesh:** set **Head Socket** to a valid socket or bone on the selected mesh. A configured socket takes priority over anchor mode; an invalid name is an error.
5. Click **Validate Setup**. `Setup Status: Ready` confirms valid configuration only.
6. Add an ordinary **Movable Point Light** or **Spot Light**. Its attenuation range must cover the anchor; for a Spot Light, point the cone at it as well.
7. **Play / Simulate**. Check the receiver's diagnostics: **Receiving** should be true and **Last Error** empty.

The created anchor starts at an offset of **150 cm**. That is not automatic head detection; move it to suit your model. The anchor determines light selection and whole-object occlusion, while the material computes the fill shape at each pixel.

For a first setup check, you can temporarily disable the lamp's **Cast Shadows** to bypass the plugin's collision-based occlusion. Restore it when testing occlusion. If a physical light produces weak fill, check intensity and the receiver's **Physical Reference** rather than assuming the material is disconnected.

Blueprint/C++ users can call `ConfigureReceiver(Mesh, Anchor, Socket)`; newly added runtime light components on an existing actor must be registered through the subsystem's `RegisterLight`. Spawned light actors and streamed levels are discovered automatically.

## 4. Add it to your own Unlit material

Set the material to **Surface / Unlit**, add a `MF_NPR_LocalFill` function call, and connect:

```text
Your original final Unlit color → Add.A
MF_NPR_LocalFill.Fill           → Add.B
Add                            → Emissive Color
```

Here “original final color” means your existing color/shading/highlight result, not necessarily just a texture.

The function's optional scalar **AO** input defaults to **1**. Connect an occlusion mask if you want it to attenuate the new fill; this does not replace your shader's original AO treatment.

The function already contains CPD inputs and the matching relative-position calculation. Do not duplicate that wiring or manually override its internal `PLR_L0_*` / `PLR_L1_*` data.

After connecting the function, create a material instance, assign it to the mesh, and follow the receiver setup above. **The function alone is not enough; the receiver supplies its runtime light data.**

## 5. Adjust the effect in a material instance

In a material instance, tick the parameter's **override checkbox** before changing its value. A static switch has a separate value checkbox; overriding the parameter does not automatically set its value to true.

| Parameter | Default | What to change |
| --- | --- | --- |
| `BaseColor` | Example base color | Flat base color in the included example; your custom shader can use its own color chain |
| `PLR_FillStrength` | 1 | Overall fill gain; 0 turns this material's fill off |
| `PLR_RadiusScale` | 1 | Multiplier on the real lamp's attenuation radius |
| `PLR_EdgeSoftness` | 0.25 | Fraction of the effective radius used for the inside fade; 0 = hard boundary, 1 = fade across the full radius |
| `PLR_UseCustomColor` | false | False: use scene-light color. True: use `PLR_FillTint` instead |
| `PLR_FillTint` | White | Custom fill color; shown only when custom-color mode is enabled |
| `PLR_UseMaxAdd` | false | Enable an optional ceiling on the added fill |
| `PLR_MaxAdd` | 1 | Shown only when the ceiling is enabled; limits the fill's largest RGB component, preserving hue |

Suggested tuning order: **FillStrength → RadiusScale → EdgeSoftness → color mode → optional MaxAdd**.

- A hard cutoff: increase **EdgeSoftness**, not FillStrength. The fade stays inside the scaled outer boundary.
- Excessive brightness: lower **FillStrength**, or enable **UseMaxAdd** and reduce **MaxAdd**.
- MaxAdd seems ineffective: it only acts when the fill exceeds the ceiling. It does not clamp the final material color.
- Custom color replaces scene hue; it is not multiplied by it. Both selected lamps use the same custom color, but still supply their own range, energy, cone and occlusion.
- The two color/cap switches are **static material configuration**, not per-frame animation controls; changing them may compile a shader variant.

On the receiver, **Artistic Strength** (default 1) adjusts the entire object. **Physical Reference** (100) and **Unitless Reference** (8) adjust the brightness mapping: increasing the relevant reference weakens fill at a fixed light energy. The mapping is artistic `E / (E + Reference)`, not an exact match to Lit exposure. Scene-color luminance affects energy even in custom-color mode.

## 6. Troubleshooting

| Symptom | Check |
| --- | --- |
| No change in the editor viewport | Start Play / Simulate; Ready is not runtime preview |
| No fill during Play | Correct material on the correct slot, Receiving=true, Last Error empty, nonzero strength, lamp range/cone covers the anchor |
| Receiver reports missing PL Runtime ABI | The assigned material must actually contain `MF_NPR_LocalFill`; reconnect, compile and validate |
| Cannot choose a unique mesh | Set Mesh Component explicitly; do not rely on auto-find for a multi-mesh actor |
| Head position / socket error | Move or bind the anchor correctly, or supply a valid skeletal socket/bone |
| Occlusion seems wrong | Check Visibility collision, including invisible proxy meshes; no blocking collision means no occlusion |
| CPD conflict / external writer error | Reserve indices 0–31 on the whole mesh; do not clear another system's data just to suppress the error |
| Larger RadiusScale still looks wrong | Confirm the instance override and anchor; selection updates at 10 Hz and is limited to two lights |
| Several colored lights turn one color | Custom-color mode is enabled; disable it to retain each lamp's scene color |

Use **Restart Receiver** in runtime after resolving setup issues, or stop and restart Play. **Validate Setup** reports configuration errors without turning on editor-preview lighting.

## Design limits and performance

- **Two selected Point/Spot lights per mesh**, with at most four candidates checked per selection update. No Directional Light or Sky Light response is provided by this plugin.
- Candidate selection and occlusion update at **10 Hz**; selected light data updates each frame. Switching identities uses fading rather than interpolating unrelated lamp positions.
- Three asynchronous **Visibility** traces around the anchor approximate whole-head/object occlusion. They do not read VSM or provide per-strand/self shadows. Lights with Cast Shadows disabled bypass this approximation.
- A shared budget of **32 traces per frame** limits occlusion scheduling; old results conservatively suppress fill when stale.
- CPD **float indices 0–31 belong to this receiver on the whole mesh**, across all material slots. One receiver owns one mesh. Other systems must not write these indices.
- Multiple compatible slots on one mesh share the same two-light selection. The largest effective RadiusScale influences selection; larger ranges do not increase the light-count limit.
- Ordinary visibility, AffectsWorld and Lighting Channels are respected.
- No extra pass does **not** mean zero cost: the material adds shader work, and CPU selection/queries depend on scene density and receiver count. No universal frame-time guarantee is claimed.

See [validation evidence](Plugins/NPRLocalLighting/Docs/VALIDATION.md) for recorded tests and remaining coverage. Other platforms, full packaged-game/streaming coverage and production skeletal-animation coverage remain to be validated. Fab publication/approval is not claimed.

## License

[MIT](LICENSE), copyright © 2026 Tofu. Applies to the code and included original plugin/demo assets. It does not grant rights to third-party resources you use with the plugin.
