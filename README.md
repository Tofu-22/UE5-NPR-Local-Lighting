# NPR Local Lighting — 独立发布与展示工程

本工程与原Shader学习工程完全分开。使用UE5.8.3和引擎自带Sphere/Cube/Cylinder，不含角色、第三方贴图或教程材质。

## 打开与观察

双击 `NPRLocalLightingDemo.uproject`，启动关卡是 `/Game/NPRLocalLightingDemo/Maps/L_NPRLocalLighting_Demo`。
点击 **Simulate（模拟）**，才会有运行时补光。

- 三列是Sphere/Cube/Cylinder，三排是关闭补光、硬边、柔和边缘。
- MotionDemo中的球体往返平移、立方体原地旋转，灯光固定。实例可改TravelAmplitude、PeriodSeconds和RotationSpeed。
- Radius x2、跨Actor附着与墙体遮挡站位保留。墙体整体遮挡近似不是原生逐像素阴影。
- `Content/NPRLocalLightingDemo/Materials` 的四个MI只改变对照参数，统一继承插件 `M_NPR_Unlit`。

## 发布目标区分

`Plugins/NPRLocalLighting/` 是独立插件：Runtime源码、一个MF、一个Unlit主材质和一个MI，以及插件使用文档。自身没有 `/Game` 资产依赖。

`Content/NPRLocalLightingDemo/` 是可选展示内容：一个地图、两个小型运动蓝图、四个对照MI。它依赖插件及UE引擎基础资产；不是插件必须安装的内容。

GitHub源码整理时保留uproject、Config、Content、Plugins、README和Docs即可；不包含Saved、Intermediate、DerivedDataCache或本机二进制。当前本地工程会安装针对本机UE5.8.3编译的插件DLL，方便直接查看；其他UE版本需要重新编译。

MCP、Python、Terminal和原项目Editor Lab都不是运行依赖。工程配置不主动启用维护Python工具，并明确关闭MCP/Terminal；引擎默认编辑器插件仍可能加载Python，这不代表插件依赖它。维护命令不写进发布依赖。

当前仓库用于公开源码与可选展示工程；**许可证尚未确定，当前没有授予他人复用、修改或再分发本项目的许可**。UE版本及平台编译验证不等于Fab审核通过或完整游戏打包验收。具体迁移与验证记录见 [迁移报告](Docs/MIGRATION_20261004.md)。
