# 智能家居温湿度监控看板

一个可在本机复现的 MQTT 端到端示例：C 传感器通过 MQTT/TCP 上报数据，Mosquitto 负责转发，Vue 网页通过 MQTT over WebSocket 实时展示温湿度、趋势、告警和在线状态。

## 技术栈

- C11、libmosquitto：传感器模拟器，周期发布 QoS 1 JSON 数据，并用遗嘱消息处理异常离线。
- Eclipse Mosquitto：MQTT Broker，TCP 端口 `1883`，WebSocket 端口 `9001`。
- Vue 3、TypeScript、Vite、MQTT.js、ECharts：浏览器仪表盘。
- Docker Compose：启动 Broker 和 C 传感器；Node.js：运行网页开发服务器。

## 启动

需要 Docker Desktop（含 Compose）和 Node.js 20 或更新版本。

1. 从项目根目录启动 Broker 和 C 传感器：

   ```bash
   docker compose up --build -d
   docker compose logs -f broker sensor
   ```

2. 新开终端，启动前端：

   ```bash
   cd web
   npm ci
   npm run dev -- --host 0.0.0.0
   ```

3. 浏览器打开 <http://localhost:5173>。默认 MQTT WebSocket 地址为 `ws://localhost:9001`。

Windows + WSL 用户也可以按上面的步骤在 WSL 中执行；WSL 2 会将本地服务端口转发给 Windows 浏览器的 `localhost`。

## 演示告警

根目录的 `.env.example` 将传感器设为 `normal`。复制为 `.env`，把 `SENSOR_MODE` 改为 `alert`，再重建传感器：

```text
SENSOR_MODE=alert
```

```bash
docker compose up --build -d --force-recreate sensor
```

`alert` 模式固定发布高温高湿值，页面应出现告警。改回 `normal` 并再次重建即可恢复周期变化的模拟数据。

## 主题与消息

| Topic | 内容 | 保留 |
| --- | --- | --- |
| `home/livingroom/sensor-01/telemetry` | 温度、湿度、Unix 时间戳 | 是，页面刷新后可立即读到最新值 |
| `home/livingroom/sensor-01/status` | `online` / `offline` | 是；异常断开时由 Broker 发布遗嘱 |

遥测 JSON 示例：

```json
{"deviceId":"sensor-01","timestamp":1794787200,"temperature":24.6,"humidity":51.2}
```

## 结构

```text
sensor/       C 传感器和容器构建文件
mosquitto/    Broker 本地配置
web/          Vue 看板
.github/      前端与 C 编译构建检查
```

## 停止

在项目根目录运行 `docker compose down` 停止并移除容器；数据卷保留。删除卷可运行 `docker compose down -v`。

开发 Broker 只映射到本机回环地址，并启用匿名访问以便快速演示；不要将该开发配置直接用于公网部署。
