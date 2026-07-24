# RFID网关系统协议文档

## 版本信息
- 文档版本：1.0
- 发布日期：2026-07-22
- 适用项目：老人防走丢RFID网关系统

---

## 1. 系统架构

```
┌──────────────┐      MQTT      ┌──────────────┐
│  云端服务端   │◄──────────────►│   ESP32网关  │
└──────────────┘   (SMtest/cmd)  └──────┬───────┘
                                        │ RS485
                              ┌─────────▼─────────┐
                              │   RFID读写器模块   │
                              └─────────┬─────────┘
                                        │ RFID射频
                              ┌─────────▼─────────┐
                              │   老人RFID标签    │
                              └───────────────────┘
```

**硬件连接**：

| 模块 | ESP32引脚 | 说明 |
|------|-----------|------|
| RS485 TX | GPIO17 | 发送数据到RFID模块 |
| RS485 RX | GPIO16 | 接收RFID模块数据 |
| RS485 DE | GPIO21 | 方向控制（LOW=接收，HIGH=发送） |
| 喇叭控制 | GPIO4 | 报警喇叭 |
| 红色LED | GPIO5 | 报警指示灯 |
| 蓝色LED | GPIO6 | 报警指示灯 |

---

## 2. RS485通信协议

### 2.1 帧格式定义

| 字段 | 长度 | 说明 |
|------|------|------|
| Header | 2字节 | 帧起始标志 'R' 'F' (0x52 0x46) |
| Frame Type | 1字节 | 帧类型：0x00=命令帧，0x01=响应帧，0x02=通知帧 |
| Address | 2字节 | 设备地址（MSB在前，LSB在后） |
| Frame Code | 1字节 | 命令码/响应码 |
| Param Length | 2字节 | 参数长度（MSB在前，LSB在后） |
| Parameters | N字节 | 参数数据（TLV格式） |
| Checksum | 1字节 | 校验码 |

### 2.2 校验和计算

```
checksum = ~(sum(Header + FrameType + Address + FrameCode + ParamLength + Parameters)) + 1
```

### 2.3 帧类型说明

| 值 | 类型 | 说明 |
|----|------|------|
| 0x00 | 命令帧 | 主机→RFID模块，下发控制命令 |
| 0x01 | 响应帧 | RFID模块→主机，返回执行结果 |
| 0x02 | 通知帧 | RFID模块→主机，主动上报标签数据 |

---

## 3. 命令帧详解

### 3.1 查询设备版本 (0x40)

**命令帧**：
```
52 46 00 00 00 40 00 00 [checksum]
```

**响应帧**：
```
52 46 01 00 00 40 00 0B 07 01 00 20 03 04 00 01 21 01 05 [checksum]
                          ↑       ↑
                      Status TLV  Software Version TLV
```

### 3.2 开始盘存标签 (0x21)

**命令帧**：
```
52 46 00 00 00 21 00 00 [checksum]
```

**响应帧**：
```
52 46 01 00 00 21 00 03 07 01 00 [checksum]
```

### 3.3 停止盘存标签 (0x23)

**命令帧**：
```
52 46 00 00 00 23 00 00 [checksum]
```

**响应帧**：
```
52 46 01 00 00 23 00 03 07 01 00 [checksum]
```

### 3.4 单次盘存标签 (0x22)

**命令帧**：
```
52 46 00 00 00 22 00 00 [checksum]
```

**响应帧**：返回标签数据（参考3.11节）

### 3.5 设置工作参数 (0x41)

**命令帧**：
```
52 46 00 00 00 41 00 11 23 0F 05 [RF Power] [Interval] [Mode] [Membank] [StartAddr] [Length] [FilterTime] [DevAddr MSB] [DevAddr LSB] [Beep] [Record] [Trigger] [Antenna MSB] [Antenna LSB] [checksum]
```

**Working TLV字段说明**：

| 字段 | 长度 | 说明 |
|------|------|------|
| TLV Type | 1字节 | 0x23 |
| TLV Length | 1字节 | 0x0F |
| Version | 1字节 | 固定为0x05 |
| RF Power | 1字节 | 0~30 dBm |
| Interval | 1字节 | 盘存间隔(10ms单位) |
| Mode | 1字节 | 工作模式(0=主动,1=被动,2=触发) |
| Membank | 1字节 | 盘存区域(0=Reserve,1=EPC,2=TID,3=User) |
| StartAddr | 1字节 | 起始地址 |
| Length | 1字节 | 长度 |
| FilterTime | 1字节 | 过滤时间(秒) |
| DeviceAddr | 2字节 | 设备地址 |
| Beep | 1字节 | 蜂鸣器开关(0=关,非0=开) |
| Record | 1字节 | 记录标志 |
| Trigger | 1字节 | 触发时间 |
| Antenna | 2字节 | 天线标志 |  

**响应帧**：
```
52 46 01 00 00 41 00 03 07 01 00 [checksum]
```

### 3.6 设置单个参数 (0x48)

**命令帧**：
```
52 46 00 00 00 48 [len] 26 [param len] [param type] [param value] [checksum]看
```

**参数类型**：

| 参数类型 | 说明 | 值范围 |
|----------|------|--------|
| 0x01 | 功率设置 | 0~30 (dBm) |
| 0x02 | 蜂鸣器开关 | 0=关,1=开 |
| 0x03 | 标签过滤时间 | 1~255 (秒) |

**响应帧**：
```
52 46 01 00 00 48 00 03 07 01 00 [checksum]
```

### 3.7 查询单个参数 (0x49)

**命令帧**：
```
52 46 00 00 00 49 00 03 26 01 [param type] [checksum]
```

**响应帧**：
```
52 46 01 00 00 49 00 08 07 01 00 26 03 [param type] [param value] [checksum]
```

### 3.8 写标签 (0x30)

**命令帧**：
```
52 46 00 00 00 30 [len] 08 [op len] [Password MSB...LSB] 01 [membank] [address] [length] [data...] [checksum]
```

**Operation TLV字段说明**：

| 字段 | 长度 | 说明 |
|------|------|------|
| TLV Type | 1字节 | 0x08 |
| TLV Length | 1字节 | 0x08 + data_len |
| Password | 4字节 | 访问密码 |
| Type | 1字节 | 1=写标签 |
| Membank | 1字节 | 存储区(0=Reserve,1=EPC,2=TID,3=User) |
| Address | 1字节 | 起始地址 |
| Length | 1字节 | 长度(字单位) |
| Data | Length×2字节 | 写入数据 |

**响应帧**：
```
52 46 01 00 00 30 00 03 07 01 00 [checksum]
```

### 3.9 读标签 (0x31)

**命令帧**：
```
52 46 00 00 00 31 00 08 08 08 [Password MSB...LSB] 00 [membank] [address] [length] [checksum]
```

**响应帧**：
```
52 46 01 00 00 31 [len] 07 01 00 08 [op len] [Password] 00 [membank] [address] [length] [data...] [checksum]
```

### 3.10 锁定标签 (0x30)

**命令帧**：
```
52 46 00 00 00 30 00 08 08 08 [Password MSB...LSB] 02 [membank] [address] [length] [checksum]
```

**响应帧**：
```
52 46 01 00 00 30 00 03 07 01 00 [checksum]
```

### 3.11 销毁标签 (0x30)

**命令帧**：
```
52 46 00 00 00 30 00 08 08 08 [Password MSB...LSB] 03 00 00 00 [checksum]
```

**响应帧**：
```
52 46 01 00 00 30 00 03 07 01 00 [checksum]
```

### 3.12 重启设备 (0x10)

**命令帧**：
```
52 46 00 00 00 10 00 00 [checksum]
```

**响应帧**：
```
52 46 01 00 00 10 00 03 07 01 00 [checksum]
```

### 3.13 继电器控制 (0x4C)

**命令帧**：
```
52 46 00 00 00 4C [len] 27 03 [RelayNo] [Operation] [Time] [checksum]
```

**Relay TLV字段说明**：

| 字段 | 长度 | 说明 |
|------|------|------|
| TLV Type | 1字节 | 0x27 |
| TLV Length | 1字节 | 0x03 |
| Relay No | 1字节 | 继电器号(1或2) |
| Operation | 1字节 | 0=关闭,非0=打开 |
| Time | 1字节 | 开启时间(秒),0=长期开启 |

**响应帧**：
```
52 46 01 00 00 4C 00 03 07 01 00 [checksum]
```

### 3.14 语音播放 (0x4D)

**命令帧**：
```
52 46 00 00 00 4D [len] 28 [len] [Operation] [Text...] [checksum]
```

**Audio TLV字段说明**：

| 字段 | 长度 | 说明 |
|------|------|------|
| TLV Type | 1字节 | 0x28 |
| TLV Length | 1字节 | 1 + text_len |
| Operation | 1字节 | 0x01=播放语音,0x02=设置离线语音 |
| Text | N字节 | GBK编码的文本内容 |

**响应帧**：
```
52 46 01 00 00 4D 00 03 07 01 00 [checksum]
```

---

## 4. 通知帧详解

### 4.1 标签上传通知 (0x80)

**帧格式**：
```
52 46 02 00 00 80 [len] 50 [tag len] 01 [epc len] [EPC data...] 05 01 [RSSI] 06 07 [Year MSB...LSB] [Month] [Day] [Hour] [Minute] [Second] [checksum]
```

**Tag TLV字段说明**：

| 子TLV类型 | 长度 | 说明 |
|-----------|------|------|
| 0x01 | 可变 | EPC数据 |
| 0x05 | 1字节 | RSSI信号强度 |
| 0x06 | 7字节 | 时间戳(年2字节+月+日+时+分+秒) |

**示例**：
```
52 46 02 00 00 80 00 19 50 17 01 0C E2 00 00 17 02 17 01 99 23 90 21 7D 05 01 C3 06 04 3D 00 00 00 4C
                      ↑       ↑        ↑
                  Tag TLV   EPC TLV   RSSI TLV
```

### 4.2 离线标签上传 (0x81)

**帧格式**：同4.1，Frame Code为0x81

---

## 5. TLV格式汇总

### 5.1 Status TLV (0x07)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x07 |
| Length | 1字节 | 0x01 |
| Status Code | 1字节 | 状态码 |

**状态码说明**：

| 状态码 | 名称 | 说明 |
|--------|------|------|
| 0x00 | SUCCESS | 命令成功完成 |
| 0x14 | Parameter unsupport | 不支持的参数 |
| 0x15 | Parameter len error | 参数长度错误 |
| 0x16 | Parameter context error | 参数内容错误 |
| 0x17 | Unsupport command | 不支持的命令 |
| 0x18 | Device Address error | 设备地址错误 |
| 0x20 | Check Sum error | 校验码错误 |
| 0x21 | Unsupport TLV Type | 不支持的TLV类型 |
| 0x22 | Flash Error | Flash写入错误 |
| 0xFF | Internal Error | 内部错误 |

### 5.2 Software Version TLV (0x20)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x20 |
| Length | 1字节 | 0x03 |
| Main Version | 1字节 | 主版本号 |
| Sub Version | 1字节 | 子版本号 |
| Modify Version | 1字节 | 修改版本号 |

### 5.3 Device Type TLV (0x21)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x21 |
| Length | 1字节 | 0x01 |
| Device Type | 1字节 | 设备类型 |

### 5.4 Single Parameter TLV (0x26)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x26 |
| Length | 1字节 | 参数长度 |
| Parameter Type | 1字节 | 参数类型(0x01=功率,0x02=蜂鸣器,0x03=过滤时间) |
| Parameter Value | N字节 | 参数值 |

### 5.5 Operation TLV (0x08)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x08 |
| Length | 1字节 | 可变 |
| Password | 4字节 | 访问密码 |
| Operation Type | 1字节 | 0=读,1=写,2=锁定,3=销毁 |
| Membank | 1字节 | 存储区 |
| Address | 1字节 | 地址 |
| Length | 1字节 | 长度 |
| Data | 可变 | 数据(写操作时) |

### 5.6 EPC TLV (0x01)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x01 |
| Length | 1字节 | EPC长度 |
| EPC Data | N字节 | EPC数据 |

### 5.7 RSSI TLV (0x05)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x05 |
| Length | 1字节 | 0x01 |
| RSSI | 1字节 | 信号强度(有符号) |

### 5.8 Time TLV (0x06)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x06 |
| Length | 1字节 | 0x07 |
| Year | 2字节 | 年份 |
| Month | 1字节 | 月份 |
| Day | 1字节 | 日期 |
| Hour | 1字节 | 小时 |
| Minute | 1字节 | 分钟 |
| Second | 1字节 | 秒 |

### 5.9 Tag TLV (0x50)

| 字段 | 长度 | 值 |
|------|------|-----|
| Type | 1字节 | 0x50 |
| Length | 1字节 | 子TLV总长度 |
| Sub TLVs | N字节 | EPC TLV + RSSI TLV + Time TLV... |

---

## 6. MQTT接口协议

### 6.1 MQTT配置

| 参数 | 值 |
|------|-----|
| Broker | broker.emqx.io |
| Port | 1883 |
| Client ID | rfid_gateway_0001 |
| 发布主题 | SMtest |
| 订阅主题 | SMtest/cmd |

### 6.2 命令下发（SMtest/cmd）

**命令格式**：JSON格式

```json
{"cmd":"命令名称","参数1":"值1","参数2":"值2"}
```

**支持的命令列表**：

| 命令名称 | 功能 | 参数 |
|----------|------|------|
| query_version | 查询版本 | 无 |
| start_inventory | 开始盘存 | 无 |
| stop_inventory | 停止盘存 | 无 |
| single_inventory | 单次盘存 | 无 |
| reboot | 重启设备 | 无 |
| set_power | 设置功率 | power(0~30) |
| set_beep | 设置蜂鸣器 | enable(0或1) |
| set_filter_time | 设置过滤时间 | seconds(1~255) |
| query_param | 查询参数 | param_type(0x01/0x02/0x03) |
| set_work_params | 设置工作参数 | power,interval,mode,membank,start_addr,length,filter_time,device_addr,beep,antenna |
| read_tag | 读标签 | password,membank,address,length |
| write_tag | 写标签 | password,membank,address,length,data(十六进制字符串) |
| lock_tag | 锁定标签 | password,membank,address,length |
| destroy_tag | 销毁标签 | password |
| control_relay | 继电器控制 | relay_no(1或2),open(0或1),duration(秒) |
| play_audio | 语音播放 | text(GBK编码文本) |
| add_whitelist | 添加白名单 | epc(标签EPC) |
| remove_whitelist | 移除白名单 | epc(标签EPC) |
| query_whitelist | 查询白名单 | 无 |
| clear_whitelist | 清空白名单 | 无 |
| save_config | 保存配置 | 无 |
| reset_config | 重置配置 | 无 |
| stop_alarm | 停止报警 | 无 |
| ota_update | OTA升级 | url(固件URL) |
| ota_status | 查询OTA状态 | 无 |
| ota_check | 检查GitHub更新 | repo(用户名/仓库名) |
| ota_github | 从GitHub升级 | repo(用户名/仓库名), asset(可选，资产文件名) |

**命令示例**：

```json
// 查询版本
{"cmd":"query_version"}

// 开始盘存
{"cmd":"start_inventory"}

// 设置功率为30dBm
{"cmd":"set_power","power":30}

// 写标签
{"cmd":"write_tag","password":"00000000","membank":1,"address":2,"length":4,"data":"E200001702170199"}

// 读标签
{"cmd":"read_tag","password":"00000000","membank":1,"address":0,"length":4}
```

### 6.3 数据上报（SMtest）

**上报消息格式**：JSON格式

```json
{"type":"消息类型","字段1":"值1","字段2":"值2"}
```

**消息类型**：

| 类型 | 说明 | 字段 |
|------|------|------|
| heartbeat | 心跳消息 | type,time |
| tag_report | 标签上报 | type,epc,rssi,alarm,in_whitelist |
| response | 命令响应 | type,frame_code,status,status_msg |
| whitelist_query | 白名单查询结果 | type,count,items |
| ota_status | OTA状态 | type,status,progress,current_version,message |
| ota_check | 更新检查结果 | type,current_version,latest_version,has_update |

**消息示例**：

```json
// 心跳消息
{"type":"heartbeat","time":123456}

// 标签上报（检测到老人标签）
{"type":"tag_report","epc":"E2000017021701992390217D","rssi":-61,"alarm":true,"in_whitelist":true}

// 命令响应
{"type":"response","frame_code":0x21,"status":0,"status_msg":"SUCCESS"}

// 白名单查询结果
{"type":"whitelist_query","count":2,"items":["E200001702170199","E200001702170200"]}

// OTA升级状态
{"type":"ota_status","status":"downloading","progress":50,"current_version":"1.0.0","message":"正在下载固件"}
```

---

## 7. 报警功能

### 7.1 报警触发条件

当RFID模块检测到标签并上报通知帧时，自动触发报警。

### 7.2 报警行为

| 设备 | 行为 |
|------|------|
| 喇叭 | 发出报警声 |
| 红色LED | 与蓝色LED交替闪烁 |
| 蓝色LED | 与红色LED交替闪烁 |
| MQTT | 上报报警信息 |

### 7.3 报警参数

| 参数 | 值 |
|------|-----|
| 报警持续时间 | 10秒 |
| LED闪烁间隔 | 200ms |
| 报警期间重复检测 | 忽略（防止重复触发） |

---

## 8. 系统流程

### 8.1 启动流程

```
1. 初始化RS485模块（波特率115200）
2. 初始化设备控制模块（喇叭、LED）
3. 初始化网络模块（蓝牙+WiFi）
4. 连接网络（优先WiFi，蓝牙始终开启）
5. 进入主循环
```

### 8.2 主循环流程

```
循环：
1. 更新网络状态
2. 更新设备控制状态（处理报警计时）
3. 发送心跳（每5分钟）
4. 检查RS485接收数据
5. 解析RFID帧
   - 如果是通知帧(0x02)：
     - 解析EPC数据
     - 触发报警（喇叭+LED）
     - MQTT上报标签信息
   - 如果是响应帧(0x01)：
     - 解析状态码
     - MQTT上报响应信息
```

### 8.3 报警流程

```
检测到标签 → 解析EPC → 触发报警(喇叭响+LED闪烁) → MQTT上报 → 10秒后自动停止
```

---

## 9. 代码文件结构

```
src/
├── main.cpp          # 主程序入口，系统初始化和主循环
├── rs485.cpp         # RS485通信底层实现
├── CommandHandler.cpp # RFID命令处理（命令构造、解析）
├── NetworkManager.cpp # 网络管理（WiFi、蓝牙、MQTT）
├── WiFiNetwork.cpp   # WiFi模块实现
├── BluetoothNetwork.cpp # 蓝牙模块实现
└── DeviceControl.cpp # 设备控制（喇叭、LED）

include/
├── rs485.h           # RS485通信接口
├── CommandHandler.h  # 命令处理接口
├── NetworkManager.h  # 网络管理接口
├── WiFiNetwork.h     # WiFi接口
├── BluetoothNetwork.h # 蓝牙接口
└── DeviceControl.h   # 设备控制接口
```

---

## 10. 附录

### 10.1 常用命令速查表

| 功能 | Frame Code | 命令示例 |
|------|-----------|----------|
| 查询版本 | 0x40 | 52 46 00 00 00 40 00 00 28 |
| 开始盘存 | 0x21 | 52 46 00 00 00 21 00 00 47 |
| 停止盘存 | 0x23 | 52 46 00 00 00 23 00 00 45 |
| 单次盘存 | 0x22 | 52 46 00 00 00 22 00 00 46 |
| 设置功率(30dBm) | 0x48 | 52 46 00 00 00 48 00 05 26 03 01 1E C4 24 |
| 重启设备 | 0x10 | 52 46 00 00 00 10 00 00 58 |

### 10.2 响应状态码速查表

| 状态码 | 含义 |
|--------|------|
| 0x00 | 成功 |
| 0x14 | 参数不支持 |
| 0x15 | 参数长度错误 |
| 0x16 | 参数内容错误 |
| 0x17 | 命令不支持 |
| 0x18 | 设备地址错误 |
| 0x20 | 校验码错误 |
| 0x22 | Flash错误 |
| 0xFF | 内部错误 |