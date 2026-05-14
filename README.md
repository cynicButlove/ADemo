# ADemo

ADemo 是一个基于 Unreal Engine 5.5 的战术 FPS / 撤离模式 Demo，重点验证服务器权威战斗、武器手感、战术 AI、拾取与撤离流程，以及运行时调试工具。

## 项目亮点

- 基于 UE Dedicated Server、RPC、Actor Replication、CharacterMovement 和 GAS AttributeSet 复制构建服务器权威战斗链路。
- Hitscan 武器逻辑运行在真实 UE 服务端世界中，墙体、掩体、角色和地图碰撞都会参与权威命中校验。
- 数据驱动武器系统，支持配置伤害、射速、弹匣容量、备弹、换弹时间、开镜散布、后坐力、散布恢复和身体部位伤害倍率。
- 使用确定性的持续射击弹道模式替代临时随机散布，并提供运行时武器工作台，用于对比基础武器与配件改装后的弹着分布。
- 简化版运行时配件系统，支持枪口、握把、弹匣和瞄具参数修正，同时保持武器外观不变。
- 战术 AI 基于 Behavior Tree、Blackboard、AI Perception、队伍警戒共享、压制/侧翼角色、掩体评分、搜索状态和调试可视化构建。
- PvPvE 撤离循环包含背包、战利品容器、拾取/丢弃流程、撤离区域、对局计时、HUD 反馈和撤离结果汇总。

## 技术栈

- Unreal Engine 5.5
- C++ 与 Blueprint
- Gameplay Ability System
- UE Dedicated Server
- Behavior Tree / Blackboard / AI Perception
- Git LFS，用于管理 Unreal 二进制资源

## 仓库结构

- `DemoClient/` - UE 项目、玩法代码、内容资源、打包脚本和本地运行脚本。
- `DemoServer/` - 外部服务器原型，用于早期自定义 TCP / 快照同步实验。

## 主要代码目录

- `DemoClient/Source/DemoClient/Character/` - 玩家角色、输入、战斗 RPC、武器切换、死亡与重生。
- `DemoClient/Source/DemoClient/Weapon/` - 武器基础逻辑、Hitscan、后坐力、弹道模式、配件、弹孔贴花和伤害应用。
- `DemoClient/Source/DemoClient/Combat/` - GAS 属性集与伤害执行。
- `DemoClient/Source/DemoClient/AI/` - AI 角色、控制器、战术状态和行为树任务。
- `DemoClient/Source/DemoClient/Inventory/` - 背包、战利品物品和战利品容器。
- `DemoClient/Source/DemoClient/Online/` - 大厅流程、直连 UE Dedicated Server 旅行和外部服务器子系统。
- `DemoClient/Source/DemoClient/UI/` - HUD、命中反馈、撤离 UI、背包面板和武器工作台。

## 本地运行

1. 使用 Unreal Engine 5.5 打开 `DemoClient/DemoClient.uproject`。
2. 构建 `DemoClientEditor` 目标。
3. 可以在编辑器中使用 PIE 本地测试，也可以使用 `DemoClient/` 目录下的脚本启动本地 Dedicated Server 测试。

常用脚本包括：

- `DemoClient/Run_UEDedicatedServer_Local.bat`
- `DemoClient/Run_DemoClient_UEDirect_Local.bat`
- `DemoClient/Run_DemoClient_UEDirect_Player1.bat`
- `DemoClient/Run_DemoClient_UEDirect_Player2.bat`
- `DemoClient/Package_UEDedicatedServer_Win64.bat`

## 说明

`Binaries`、`Intermediate`、`Saved`、打包产物、调试符号和本地项目笔记等 Unreal 生成内容已被有意排除在版本控制之外。
