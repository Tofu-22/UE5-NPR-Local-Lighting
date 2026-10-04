# 独立发布工程迁移与验证 — 2026-10-04

## 交付范围

工程：`NPRLocalLightingDemo.uproject`，使用本机 UE 5.8.3 / Win64。原 Shader 学习工程保留，不删除、移动或覆盖其资产；38 份此前登记的受保护资产 SHA256 均保持一致。

- `Plugins/NPRLocalLighting/`：版本 `0.4.0-review-fix`，仅一个 Runtime 模块。Runtime 的九份源码/构建文件与原独立发布候选逐文件一致；未带入项目 Editor Lab。
- 插件 Content 只有 `Materials/MF_NPR_LocalFill`、`M_NPR_Unlit`、`MI_NPR_Unlit`。依赖链是 MI → M → MF，MF 无外部资产依赖；插件没有 `/Game` 依赖。
- 工程 Content 只有七份展示资产：四个对照 MI、两个运动 Blueprint、一个地图，统一放在 `/Game/NPRLocalLightingDemo/`。
- 所有展示 MI 重指插件 `M_NPR_Unlit`，保留有效 BaseColor、硬/软边、关闭补光及 RadiusScale=2 的对照值；Blueprint 和地图引用已重新序列化为新路径，没有 HairLab 引用或重定向器。
- 模型、地面和其他基础资源只用引擎自带资产；没有角色、贴图、教程 Shader、MPC、旧实验控制器或第三方内容。

默认打开 `/Game/NPRLocalLightingDemo/Maps/L_NPRLocalLighting_Demo`，点击 **Simulate（模拟）**观察受光。静态编辑视口不是 Receiver 的运行时测试。展示内容是可选例子，不是插件的最小安装依赖。

## 验证结果

| 检查 | 结果与证据 |
| --- | --- |
| Runtime 编译 | `BuildPlugin`：Win64 Editor、Game Development、Shipping 全部成功。日志/构建输出保存在本地 Saved；不等于整工程 Cook/打包验收。 |
| 本机插件安装 | 只安装新工程重新构建的 Editor DLL/PDB/modules，BuildId 与本机 UE 一致；没有改动引擎安装或原工程 DLL。 |
| 资产隔离和材质 | 独立渲染 Commandlet 检查七份工程资产与三份插件资产；依赖白名单为新展示目录、插件、Engine、Script。最终 `StandaloneContent_R2_20261004.log` PASS，退出 0，0 errors / 0 warnings。四 MI 的 PS 指令统计均 174，不是 GPU 毫秒数。 |
| 原生自动化 | `MaterialABI`、`MathContract`、`OriginOwnershipRadius`、`RetainedVisibilityBudget`：4/4 Success，失败/未运行/处理中均 0。`Saved/Automation_20261004/index.json`。 |
| 实际运行数据 | 完整编辑器 Simulate：12 个 Receiver 均 Receiving、LastError 空、CPD 为 32 floats。两次采样确认球体 X 平移、立方体旋转，两者的补光权重非零；遮挡站位权重为零。`StandalonePreview_20261004.json` PASS。 |
| 测试恢复 | PIE 已停止，临时后台 CPU 节流改动恢复；正常展示编辑器另行启动，不携带临时 Python 命令，地图检查 0 errors / 0 warnings。 |

本轮验证运行数据与资产完整性，不代替用户对运动画面的审阅；未新增 GPU 性能测量、跨引擎版本或其他平台验收。

## 使用与分发

1. 双击工程文件；本机已装对应版本 DLL，可直接打开。
2. 演示关卡自动加载。用 Simulate 看 Off / Hard / Soft、扩大半径、附着、整体遮挡与 MotionDemo。
3. 项目 Content 的四个 MI 用来调整对照；运动 Blueprint 的实例参数为 `TravelAmplitude`、`PeriodSeconds`、`RotationSpeed`。
4. 插件单独分发仅需要 `Plugins/NPRLocalLighting/` 的源码/配置/三资产/文档；展示工程分发再增加工程配置及七份展示资产。

本机 `Dist/NPRLocalLighting_Source_20261004.zip` 和 `Dist/NPRLocalLightingDemo_Source_20261004.zip` 是待发布源码整理包：不含本机 Binaries/PDB、Intermediate、Saved、DerivedDataCache、迁移暂存及测试日志；它们刻意不放入 Git 仓库。其他使用者需要针对其 UE 编译插件。没有添加未经用户选择的 LICENSE。Fab 发布尚未进行。

## 维护过程中的失败与修正

- 第一次迁移已保存目标资产，但同一 Python 脚本持有复制 World 的引用，再次加载地图触发 World Memory Leaks；属于迁移工具生命周期问题，不是 Runtime 的加载失败。已解除 Python 引用，并由新进程独立重载验证，不再次覆盖已成功迁移的资产。[Epic UE-177376](https://issues.unrealengine.com/issue/UE-177376)及本机 EditorServer.cpp 的引用链诊断支持这一处理。
- 首次 Commandlet 未允许渲染，材质统计未生成；补上 `-AllowCommandletRendering` 后取得真实 shader 统计，不以 0 指令误判成功。
- 编辑器性能设置没有 Python 导出的同名类；读取 `/Script/UnrealEd.Default__EditorPerformanceSettings` 实际对象设置/恢复后台节流。异步 Slate 测试通过 `EditorPythonScripting.set_keep_python_script_alive` 延长一次性脚本生命周期，结束时正常退出。
- Preview R2 的运行采样通过，结束时额外保存地图受版本控制检出失败影响，保存返回 false，报告正确记为 FAIL。取消测试中非必要的保存，并在新工程测试会话指定 `-SCCProvider=None`；R3 完整 PASS，失败日志/报告仍保留在 Saved。只读冒烟测试不再把保存失败当作补光失败。

临时旧路径的迁移文件已可恢复地移到 `Saved/MigrationSources/HairLab`，没有递归删除。旧工程的学习记录/维护脚本不包含在分发包。产品不依赖 MCP、Python 或 Terminal；电脑操作技能所需执行器本轮不可用，实际通过文件操作与 UE 原生接口完成，不宣称使用了鼠标自动化。

正常启动日志仍显示引擎默认 ControlRig/IKRig 的 Python 初始化；这是引擎默认编辑器插件行为，不是本插件的依赖。工程配置没有主动启用维护 Python 工具，MCP 已显式关闭；没有为了隐藏该日志修改全局引擎设置。
