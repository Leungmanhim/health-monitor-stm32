# 基于 STM32 的多功能体表参数监测预警装置

基于 STM32F103C8T6 的可穿戴体表参数监测装置，实时采集**心率、血氧、体温、步数、体位姿态与 GPS 位置**，本地 OLED 显示 + 蜂鸣器/报警灯提示，并通过 ESP8266 上云，配套 Android APP 远程查看与控制。

> 本科毕业设计项目。硬件基于通用模块，软件在开源工程基础上做了大量二次开发。

---

## 功能

| 功能 | 说明 |
|---|---|
| 心率 / 血氧 | MAX30102，连续采集，超阈值报警 |
| 体温 | DS18B20，接触式测量，上限阈值可调（APP/按键） |
| 跌倒检测 | MPU6050，倾斜锁存 + 快速翻转判据（0.7s 内翻转 >44°） |
| 久坐提醒 | 阈值 10s~1800s 可调，可短时演示 |
| 计步 | MPU6050 加速度峰值检测 |
| GPS 定位 | ATGM336H，经纬度上报 APP，支持室内演示坐标 |
| 声光报警 | 有源蜂鸣器（PA6）+ LED（PB0），随警报节奏闪烁 |
| 按键交互 | 4 键消音 / 切页 / 阈值加减 |
| 云端 | 巴法云 TCP，14 字段上报，支持远程指令 |
| APP | 实时看板、高德静态地图、阈值下发、闹钟对时、屏幕切换 |

## 硬件清单

| 模块 | 型号 | 接口 |
|---|---|---|
| 主控 | STM32F103C8T6 核心板（64KB Flash） | — |
| 心率血氧 | MAX30102 | I2C：PB12/PB13，**5V 供电** |
| 姿态/计步 | MPU6050 | I2C：PB6/PB7，**5V 供电**，AD0 接 GND |
| 体温 | DS18B20 | 单总线：PA11 |
| 定位 | ATGM336H | USART3：PB11（只接 TXD） |
| WiFi | ESP8266 | USART2：PA2/PA3，**3.3V 供电** |
| 显示 | 0.96" OLED | 软件 I2C：PB8/PB9 |
| 声光 | 有源蜂鸣器 + 3mm LED | PA6 / PB0 |
| 按键 | 轻触 ×4 | PA12 / PA15 / PB3 / PB4 |

完整引脚与打板连线见 [docs/PCB打板连线表.xlsx](docs/PCB打板连线表.xlsx)，KiCad 原理图见 [docs/kicad/](docs/kicad/)。

## 目录结构

```
.
├─ Project/        Keil MDK 工程（BH-F103.uvprojx）
├─ User/           主逻辑 main.c、私有配置 secrets(.example).h
├─ Hardware/       各模块驱动（OLED / MPU6050 / MAX30102 / GPS / ESP8266 / DS18B20 / LED / KEY）
├─ Libraries/      STM32 标准外设库 + CMSIS
├─ SYSTEM/         delay / sys / usart 等底层
├─ APP/            Android 源码（WebView 壳 + assets/index.html）
└─ docs/           连线表、KiCad 原理图
```

## 编译与烧录

1. 打开 `Project/BH-F103.uvprojx`（Keil MDK 5，需 ARMCC）
2. 首次编译前，把 `User/secrets.example.h` 复制为 `User/secrets.h`，填入自己的 WiFi 与巴法云信息
3. `Rebuild` → 生成 `Objects/BH-F103.hex`
4. ST-Link / DAP-Link 烧录（SWD：PA13/PA14）

当前固件尺寸：Code 32028 B + RO 14336 B，占用 64KB Flash 的约 72%。

## 配置说明

`User/secrets.h`（**已被 .gitignore 忽略，不会上传**）：

```c
#define WIFI_SSID    "your_wifi"      /* 必须 2.4GHz */
#define WIFI_PASS    "your_password"
#define BEMFA_UID    "your_uid"
#define BEMFA_TOPIC  "your_topic"
```

APP 端 `APP/assets/index.html` 内需替换：
- `YOUR_BEMFA_UID` / `YOUR_TOPIC`：巴法云账号
- `YOUR_AMAP_KEY`：高德「Web 服务」Key（也可在 APP 界面里手动设置）

## 云协议

上报（`#` 分隔，14 字段）：

```
#步数#心率#血氧#体温整数.小数#跌倒标志#体温阈值#久坐秒数#
心率上限#心率下限#血氧上限#血氧下限#经度.%06ld#纬度.%06ld#久坐标志#
```

下行指令：`TEMP_ADD/SUB`、`SED_ADD/SUB`、`SED n`、`HR_UP/LOW_ADD/SUB`、
`SPO2_UP/LOW_ADD/SUB`、`REST_ALL`、`ALARM HH:MM`、`TS <unix秒>`、`PAGE 0~4`、`POS 经度,纬度`。

## APP

- 源码为纯 Java + WebView 壳，界面在 `assets/index.html`
- 已发布 APK 见 Releases
- 权限：INTERNET、VIBRATE

## 已知问题

- GPS 室内无信号，APP 提供"演示定位"手动坐标
- MPU6050 在高频振动（跑步）时计步正常，但剧烈甩动可能触发跌倒判据
- 巴法云免费主题有频率限制，上报间隔做了 10s 节流

## 版权与声明

本项目底层框架基于 **凉介单片机设计**（B 站）发布的 STM32 人体健康监测系统开源工程进行二次开发，
原始工程代码著作权归原作者所有，本项目仅用于学习交流与毕业设计，不作商业用途。

- **新增代码**（APP、云端协议、跌倒算法、声光报警、PCB 设计等）以 [MIT License](LICENSE) 发布
- **原始框架代码**（`Libraries/`、`SYSTEM/` 及未改动的 `Hardware/` 驱动）不适用本项目 MIT 许可

完整出处、二次开发范围与许可边界见 [NOTICE](NOTICE)。
