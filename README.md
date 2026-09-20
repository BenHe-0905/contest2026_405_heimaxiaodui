# 智能电子工卡

> 队伍：闪灵（Shining）

## 一、作品简介

**智能电子工卡** 是一款基于 **ESP32-P4** 与 **4 色墨水屏** 的边缘智能工卡设备，在
**ESP-Claw（Espressif「聊天造物」框架）** 上开发。它把传统纸质工卡升级为一块会「看天气、
记日程、报时间」的 AI 桌面信息屏：Agent Loop 直接在端侧芯片上运行，以对话定义设备行为，
用墨水屏把工卡信息与天气、日程、待办等卡片直观呈现，配合音频完成晨间播报与整点报时。

核心亮点：

- **工卡信息屏**：工卡 + 天气 + 日程 + 待办多屏卡片，自动刷新（`ui_screens.c` + `gen_ui_screens.py` 生成）
- **6 个设备技能（skill）**：天气、日程管理、晨间播报、整点报时、墨水屏显图、SD 卡存储
- **DeepSeek 大模型接入**：Agent Loop 经 openai_compatible 后端对接 DeepSeek（可配置）
- **外设点亮**：摄像头（SC2336）、音频（ES8311）、墨水屏、SD 卡均可被 AI 当作工具调用

## 二、选题方向

**AI 硬件产品创新**

理由：作品是一台跑在端侧芯片上的 AI 智能体设备，以「智能工卡」为产品形态，Agent Loop 直接在
ESP32-P4 上运行，用墨水屏呈现工卡与信息卡片，属典型 AI 硬件产品。

## 三、目录结构

- `application/edge_agent/` — 主固件应用（本作品核心代码）
  - `main/` — 入口 `main.c`、墨水屏 UI `ui_screens.c`、屏幕切换 `screen_switcher.cpp`
  - `main/skills/` — 6 个技能：`weather` / `schedule_manager` / `announce_morning` / `announce_time` / `epaper_show_image` / `sd_storage`
  - `boards/espressif/esp32_p4_function_ev/` — 目标板级配置（引脚 / 外设 YAML）
  - `tools/` — `gen_ui_screens.py`（UI 生成）、`img2epd.py`（图片转墨水屏）、`gen_time_clips_wav.py`
  - `fatfs_image/` — SD 卡镜像
- `components/` — ESP-Claw 框架的 Lua 模块（camera / audio / epaper / storage …）
- `docs/`、`.agents/` — 框架文档与开发规范
- `logs/` — AI Coding 日志
- `application/edge_agent/README.md` — 详细使用与移植指南

## 四、运行方式

> 说明：本作品基于 **ESP-IDF**（非 openvela 的 NuttX 体系），**无法**用 openvela 工作区的
> `./build.sh` 编译，需用 ESP-IDF 环境构建。完整步骤见 `application/edge_agent/README.md`，摘要：

1. 环境：ESP-IDF v6.1-beta1（Windows PowerShell）
2. 选板：`idf.py bmgr -c ./boards -b esp32_p4_function_ev`
3. 配置：`idf.py menuconfig` 填入 DeepSeek API Key / 模型 / IM / 时区
4. 编译：`idf.py build`
5. 烧录：`idf.py -p COM3 flash`
6. 使用：浏览器打开 `http://<板子IP>`（Web IM），对 AI 说「显示今天天气」「记录一个日程」等

## 五、AI Coding 使用说明

本作品全程借助 **Claude Code** 辅助开发：

- **方案设计**：外设移植（墨水屏 bit-bang SPI、Lua 模块 + skill 五处登记范式）由 AI 梳理
- **编码**：`ui_screens.c` 墨水屏 UI、各 skill 的 Lua 脚本由 AI 生成与联调
- **调试**：Kconfig `default n` 陷阱、组件管理器 rules 缓存、C++ Lua 头缺 `extern "C"` 等由 AI 定位修复
- **文档**：`application/edge_agent/README.md` 使用指南由 AI 整理

AI 显著缩短了从「点亮外设」到「智能工卡」的迭代周期。完整对话日志见 `logs/`。
