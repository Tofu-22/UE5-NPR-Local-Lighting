# NPR Local Lighting

[English](README.md) | [简体中文](README.zh-CN.md)

**让 Unlit 材质接收 UE 普通点光源和聚光灯，获得可控的 NPR 局部补光。**

二次元角色或其他需要艺术控制的物体经常使用 Unlit 材质，以便自行处理最终颜色。但 Unlit 不会接收引擎通常的表面光照。本插件在保留原有着色的基础上，增加受真实场景灯驱动的局部补光，不需要把材质改成 Default Lit。

运行时组件为**每个 Mesh 最多选择两盏灯**，通过 Custom Primitive Data（CPD）传入灯光数据，再由一个材质函数生成补光颜色。原有的基础颜色、绘制阴影和高光仍由你的材质负责。

## 作用与边界

- 跟随真实 Point／Spot Light 的位置、范围、颜色、色温与亮度。
- 可选择使用场景灯颜色，或在材质实例中指定补光颜色。
- 补光强度、作用范围、边缘柔和度和可选亮度上限分别控制。
- 支持静态与骨骼 Mesh；不同 Mesh 共用同一个材质实例，也能分别接收不同灯光。
- 在指定锚点附近进行碰撞查询，近似实现整个头部／物体的遮挡。
- 不修改引擎源码，不增加渲染 Pass、SceneCapture 或全局 MPC，也不依赖 MCP 连接。

这是**艺术化的加算补光**，不是 PBR 着色模型，也不能替代引擎原生阴影。插件不负责方向光／天光、头发着色、KK 高光、Matcap、描边或半透明。你可以接入已有 Unlit 材质，也可以直接从附带的简单材质开始。

## 内容与环境要求

已在 **UE 5.8.3、Windows／Win64** 验证；其他版本／平台尚未验证。当前为 Beta 源码发布，需要匹配的 UE C++ 工具链，不包含预编译 DLL。

一个 Runtime 模块，以及 `/NPRLocalLighting/Materials/` 下的三个资产：

- `MF_NPR_LocalFill`：完整运行时补光函数。
- `M_NPR_Unlit`：简单的 Surface／Opaque／Unlit 示例。
- `MI_NPR_Unlit`：对应材质实例，无贴图依赖。

可选展示工程在 [GitHub 仓库](https://github.com/Tofu-22/UE5-NPR-Local-Lighting)中，使用插件不需要展示工程。

## 1. 下载与编译安装

在 GitHub 下载仓库 ZIP，或者克隆：

```powershell
git clone https://github.com/Tofu-22/UE5-NPR-Local-Lighting.git
```

安装或替换插件二进制文件前，先关闭目标 UE 编辑器。已有安装请先备份，不要同时安装两份同名插件。

源码下载可以用 UE 的 `BuildPlugin` 命令生成与本机引擎匹配的插件包。下面是 PowerShell 示例，**三个路径都需要换成自己的路径**：

```powershell
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$pluginFile = 'C:\Projects\UE5-NPR-Local-Lighting\Plugins\NPRLocalLighting\NPRLocalLighting.uplugin'
$packageDir = 'C:\BuildOutput\NPRLocalLighting'
& "$ueRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin "-Plugin=$pluginFile" "-Package=$packageDir" -TargetPlatforms=Win64
```

输出目录使用**源码插件之外的专用目录**。即使目标项目只有蓝图，执行这个编译命令也需要 UE C++ 编译工具。

- **体验展示工程：**用编译生成的插件包为本仓库的 `Plugins/NPRLocalLighting/` 提供插件，然后打开 `NPRLocalLightingDemo.uproject`。
- **接入自己的项目：**把编译好的插件文件夹放在 `你的项目/Plugins/NPRLocalLighting/`。已有 C++ 项目也可以通过正常的项目编译来编译源码插件。

在 Plugins 窗口启用 **NPRLocalLighting**，按提示重启。在 Content Browser 设置中打开 **Show Plugin Content（显示插件内容）**。

材质需要留在插件挂载路径 `/NPRLocalLighting/Materials/` 内。不要只把三个资产搬到 `/Game`，内部引用依赖插件路径。

## 2. 手动给一个 Mesh 配置接收组件

已经放在关卡里的普通 Static Mesh Actor **不必先转换成 Actor 蓝图**。

1. 给测试 Mesh 赋予插件 Materials 文件夹中的 `MI_NPR_Unlit`。
2. 在关卡中选中这个 Actor，点击 **Add Component → NPR Local Light Receiver**。
3. 在 Actor 组件列表中选中接收组件：
   - 只有一个兼容 Mesh 时，保持 **Auto Find Target Mesh** 开启。
   - 有多个 Mesh 时，手动选择 **Mesh Component**。接收组件和目标 Mesh 必须属于同一个 Actor。
4. 指定选灯与遮挡采样位置：
   - **静态 Mesh／道具：**点击 **Create Head Anchor**，选中新出现的 `NPR_HeadAnchor`，移动到头部或需要采样的位置。球体／立方体可以放在模型中心。
   - **骨骼 Mesh：**把 **Head Socket** 设为该 Mesh 上真实存在的 Socket 或骨骼名。填写后优先使用 Socket，不再使用锚点模式；无效名称会报错。
5. 点击 **Validate Setup**。`Setup Status: Ready` 只表示配置检查通过。
6. 放一个普通的 **Movable Point Light／Spot Light**。光源范围需要覆盖锚点；聚光灯的锥体也要朝向锚点。
7. 进入 **Play／Simulate**，检查接收组件诊断：**Receiving** 应为 true，**Last Error** 应为空。

新建锚点初始偏移是 **150 cm**，不是自动识别头部。请根据模型手动移动。锚点负责选灯和整个物体的遮挡判断，材质则在每个像素上计算补光形状。

初次排查可以暂时关闭灯的 **Cast Shadows**，绕过插件的碰撞遮挡近似；验证遮挡时再恢复。物理灯补光太弱时，要同时检查灯的强度和组件的 **Physical Reference**，不能仅凭亮度判断材质未连接。

蓝图／C++ 可以调用 `ConfigureReceiver(Mesh, Anchor, Socket)`。运行时向一个已存在 Actor 新增灯组件时，需要通过子系统 `RegisterLight` 注册；新生成的灯 Actor 和流送关卡中的灯会自动发现。

## 3. 接入自己的 Unlit 材质

材质设置为 **Surface／Unlit**，添加 `MF_NPR_LocalFill` 函数调用节点，连接：

```text
原材质最终 Unlit 颜色       → Add.A
MF_NPR_LocalFill 的 Fill   → Add.B
Add                       → Emissive Color
```

这里的「原材质最终颜色」可以是已有基础颜色、阴影、高光合成后的结果，不一定只是贴图。

函数可选的标量 **AO** 输入默认是 **1**。需要让遮蔽贴图减弱新增补光时，可接入 AO Mask；它不会替代原材质自身的 AO 处理。

CPD 输入和匹配的相对位置计算已封装在函数中。不要在主材质重复搭建，也不要手动覆盖内部 `PLR_L0_*`／`PLR_L1_*` 数据。

连接后创建材质实例、赋予 Mesh，再按上一节添加和配置接收组件。**只连接 MF 不够，接收组件负责提供运行时灯光数据。**

## 4. 在材质实例里调效果

在材质实例中，先勾选参数左侧的**覆盖复选框**，再修改数值。Static Switch 还有单独的开关值复选框；勾选覆盖不等于把值设成 true。

| 参数 | 默认值 | 用途 |
| --- | --- | --- |
| `BaseColor` | 示例基础色 | 附带示例的纯色；自己的材质可使用原有颜色链 |
| `PLR_FillStrength` | 1 | 整体补光增益；0 关闭这个材质的补光 |
| `PLR_RadiusScale` | 1 | 真实灯光范围的艺术缩放倍数 |
| `PLR_EdgeSoftness` | 0.25 | 有效半径内用于渐变的比例；0 为硬边，1 为整个半径内渐变 |
| `PLR_UseCustomColor` | false | false 跟随场景灯色；true 改用 `PLR_FillTint` |
| `PLR_FillTint` | 白色 | 自定义补光色，只在自定义颜色模式开启时显示 |
| `PLR_UseMaxAdd` | false | 开启新增补光的亮度上限 |
| `PLR_MaxAdd` | 1 | 开启上限后才显示；限制补光 RGB 最大分量，保持色相 |

建议按 **FillStrength → RadiusScale → EdgeSoftness → 颜色模式 → 可选 MaxAdd** 的顺序调整。

- 受光边界太硬：增加 **EdgeSoftness**，不是 FillStrength。柔化发生在缩放后的外边界以内。
- 补光太亮：降低 **FillStrength**，或开启 **UseMaxAdd** 并降低 **MaxAdd**。
- MaxAdd 调了看不出变化：只有补光超过上限才生效；它不限制材质最终颜色。
- 自定义色是替换灯色色相，不是与灯色相乘。两盏灯使用同一个自定义色，但各自的范围、能量、锥体和遮挡依然来自场景。
- 颜色／上限两个开关是**静态材质配置**，不是逐帧动画控制；切换可能触发 Shader 编译。

接收组件的 **Artistic Strength**（默认 1）控制整个物体的补光。**Physical Reference**（100）与 **Unitless Reference**（8）控制亮度映射：固定灯光能量下，增加对应 Reference 会减弱补光。映射为艺术化的 `E / (E + Reference)`，不保证与 Lit 的曝光一致。即使启用自定义色，场景灯颜色的明度仍影响能量。

## 5. 常见问题

| 现象 | 检查方式 |
| --- | --- |
| 普通编辑器视口没有变化 | 进入 Play／Simulate；Ready 不代表运行时预览已开始 |
| Play 后仍无补光 | 材质及槽位是否正确、Receiving=true、Last Error 为空、强度不为 0、灯范围／锥体覆盖锚点 |
| 提示缺少 PL Runtime ABI | 实际赋予的材质必须包含 `MF_NPR_LocalFill`；连接、编译后重新验证 |
| 无法唯一选择 Mesh | 手动指定 Mesh Component；多 Mesh Actor 不要依赖自动选择 |
| 头部位置／Socket 报错 | 正确移动或指定锚点，或填写有效的骨骼 Socket／骨骼名 |
| 遮挡结果不对 | 检查 Visibility 碰撞，包括不可见代理 Mesh；不阻挡查询的模型不会遮挡 |
| CPD 冲突／外部写入报错 | 整个 Mesh 的索引 0–31 要留给插件；不要为消除报错直接清除其他系统的数据 |
| RadiusScale 放大后仍不对 | 检查实例覆盖值与锚点；选灯每秒更新 10 次，最多仍为两盏 |
| 不同颜色的灯变成同一种颜色 | 自定义颜色模式已启用；关闭后才能保留每盏灯的场景颜色 |

运行时修正配置问题后可以点击 **Restart Receiver**，也可以停止并重新 Play。**Validate Setup** 只检查配置，不会启用普通编辑器视口受光。

## 设计限制与开销

- **每个 Mesh 最多接收两盏 Point／Spot Light**，每轮最多检查四个候选。本插件不提供 Directional Light 或 Sky Light 响应。
- 选灯和遮挡更新频率为 **10 Hz**，已选中灯的数据逐帧更新。换灯采用淡出／淡入，不在不同灯的位置之间插值。
- 锚点附近的三条异步 **Visibility** 射线近似整个头部／物体遮挡，不读取 VSM，也不是逐发束或自阴影。关闭 Cast Shadows 的灯不进行这项遮挡近似。
- 所有接收者共享**每帧最多 32 条查询**的调度预算；结果过期时保守关闭补光。
- CPD **float 索引 0–31 在整个 Mesh 上归接收组件使用**，包括所有材质槽。一个 Mesh 只能由一个接收组件持有，其他系统不能写入这些索引。
- 同一 Mesh 的多个兼容材质槽共用两盏灯的选择结果；最大的有效 RadiusScale 会影响选灯。扩大范围不会增加灯数上限。
- 尊重灯的可见性、AffectsWorld 和 Lighting Channels。
- 不新增 Pass **不等于零开销**：材质增加 Shader 计算，CPU 选灯与碰撞查询受灯密度和接收者数量影响。不承诺通用毫秒数。

详见[验证记录](Docs/VALIDATION.md)。其他平台、完整打包游戏／流送覆盖，以及生产环境骨骼动画覆盖仍待验证。当前不代表已在 Fab 发布或通过审核。

## 许可证

[MIT](LICENSE)，版权 © 2026 Tofu。覆盖代码及随附原创插件／展示资产，不授予你所使用的第三方资源的权利。
