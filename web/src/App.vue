<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import mqtt, { type MqttClient } from 'mqtt'
import { init, use, type ECharts } from 'echarts/core'
import { LineChart } from 'echarts/charts'
import { GridComponent, LegendComponent, MarkLineComponent, TooltipComponent } from 'echarts/components'
import { CanvasRenderer } from 'echarts/renderers'

use([LineChart, GridComponent, LegendComponent, MarkLineComponent, TooltipComponent, CanvasRenderer])

type Telemetry = {
  deviceId: string
  timestamp: number
  temperature: number
  humidity: number
}

type ConnectionState = 'connecting' | 'connected' | 'reconnecting' | 'offline'

const rootTopic = 'home/livingroom/sensor-01'
const mqttUrl = import.meta.env.VITE_MQTT_URL || 'ws://localhost:9001'
const connection = ref<ConnectionState>('connecting')
const deviceStatus = ref<'online' | 'offline' | 'unknown'>('unknown')
const samples = ref<Telemetry[]>([])
const temperatureLimit = ref(30)
const humidityLimit = ref(70)
const chartHost = ref<HTMLDivElement | null>(null)
const latest = computed(() => samples.value.at(-1) ?? null)
const isAlert = computed(() => Boolean(latest.value && (
  latest.value.temperature >= temperatureLimit.value || latest.value.humidity >= humidityLimit.value
)))
const updatedLabel = computed(() => latest.value
  ? new Date(latest.value.timestamp * 1000).toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
  : '等待传感器数据')

let client: MqttClient | null = null
let chart: ECharts | null = null

function renderChart() {
  if (!chart) return
  const rows = samples.value
  chart.setOption({
    animationDuration: 350,
    color: ['#e0a86c', '#72b7a2'],
    grid: { top: 28, right: 44, bottom: 32, left: 42 },
    legend: {
      top: 0,
      right: 0,
      textStyle: { color: '#9ca6ae', fontSize: 11 },
      itemWidth: 12,
      itemHeight: 7,
    },
    tooltip: {
      trigger: 'axis',
      backgroundColor: '#182128',
      borderColor: '#2b3941',
      textStyle: { color: '#edf0ed' },
    },
    xAxis: {
      type: 'category',
      boundaryGap: false,
      data: rows.map((row) => new Date(row.timestamp * 1000).toLocaleTimeString('zh-CN', { minute: '2-digit', second: '2-digit' })),
      axisLabel: { color: '#73808a', fontSize: 10, hideOverlap: true },
      axisLine: { lineStyle: { color: '#28343b' } },
      axisTick: { show: false },
    },
    yAxis: [
      {
        type: 'value', name: '°C', min: 10, max: 40,
        nameTextStyle: { color: '#82909a' },
        axisLabel: { color: '#73808a', fontSize: 10 },
        splitLine: { lineStyle: { color: '#202b32' } },
      },
      {
        type: 'value', name: '%', min: 0, max: 100,
        nameTextStyle: { color: '#82909a' },
        axisLabel: { color: '#73808a', fontSize: 10 },
        splitLine: { show: false },
      },
    ],
    series: [
      {
        name: '温度', type: 'line', smooth: true, showSymbol: false, yAxisIndex: 0,
        data: rows.map((row) => row.temperature),
        lineStyle: { width: 2.5 },
        areaStyle: { opacity: 0.07 },
        markLine: { silent: true, symbol: 'none', lineStyle: { color: '#d87865', type: 'dashed', opacity: 0.7 }, data: [{ yAxis: temperatureLimit.value }] },
      },
      {
        name: '湿度', type: 'line', smooth: true, showSymbol: false, yAxisIndex: 1,
        data: rows.map((row) => row.humidity),
        lineStyle: { width: 2.5 },
        areaStyle: { opacity: 0.04 },
        markLine: { silent: true, symbol: 'none', lineStyle: { color: '#d87865', type: 'dashed', opacity: 0.7 }, data: [{ yAxis: humidityLimit.value }] },
      },
    ],
  }, true)
}

function handleMessage(topic: string, payload: Buffer) {
  const text = payload.toString()
  if (topic.endsWith('/status')) {
    deviceStatus.value = text === 'online' ? 'online' : text === 'offline' ? 'offline' : 'unknown'
    return
  }

  if (!topic.endsWith('/telemetry')) return
  try {
    const data = JSON.parse(text) as Partial<Telemetry>
    if (typeof data.timestamp !== 'number' || typeof data.temperature !== 'number' || typeof data.humidity !== 'number') return
    if (![data.timestamp, data.temperature, data.humidity].every(Number.isFinite)) return
    samples.value.push({
      deviceId: typeof data.deviceId === 'string' ? data.deviceId : 'sensor-01',
      timestamp: data.timestamp,
      temperature: data.temperature,
      humidity: data.humidity,
    })
    if (samples.value.length > 60) samples.value.splice(0, samples.value.length - 60)
  } catch {
    // Ignore malformed or unrelated broker payloads.
  }
}

function connect() {
  client = mqtt.connect(mqttUrl, {
    clientId: `dashboard-${Math.random().toString(16).slice(2, 10)}`,
    clean: true,
    connectTimeout: 5000,
    reconnectPeriod: 1500,
  })
  client.on('connect', () => {
    connection.value = 'connected'
    client?.subscribe(`${rootTopic}/#`, { qos: 1 })
  })
  client.on('reconnect', () => { connection.value = 'reconnecting' })
  client.on('offline', () => { connection.value = 'offline' })
  client.on('close', () => {
    if (connection.value !== 'reconnecting') connection.value = 'offline'
  })
  client.on('error', (error) => { console.warn('MQTT connection error:', error.message) })
  client.on('message', handleMessage)
}

function resizeChart() {
  chart?.resize()
}

watch([samples, temperatureLimit, humidityLimit], renderChart, { deep: true })

onMounted(() => {
  if (chartHost.value) chart = init(chartHost.value)
  renderChart()
  window.addEventListener('resize', resizeChart)
  connect()
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', resizeChart)
  client?.end(true)
  chart?.dispose()
})

const connectionText = computed(() => ({
  connecting: '正在连接', connected: '实时连接中', reconnecting: '正在重连', offline: '连接中断',
}[connection.value]))
</script>

<template>
  <main class="app-shell">
    <header class="topbar">
      <a class="brand" href="#top" aria-label="居家环境监控首页">
        <span class="brand-mark"><span></span><span></span><span></span></span>
        <span>FIELDNOTE <b>/ HOME</b></span>
      </a>
      <div class="topbar-right">
        <span class="live-dot" :class="connection"></span>
        <span class="connection-label">{{ connectionText }}</span>
        <span class="topbar-divider"></span>
        <span class="local-tag">LOCAL SYSTEM</span>
      </div>
    </header>

    <section class="intro" id="top">
      <div>
        <p class="eyebrow">HOME ENVIRONMENT <span>·</span> ROOM 01</p>
        <h1>让舒适，保持在<span>刚刚好</span>。</h1>
        <p class="intro-copy">客厅环境实时监测 · 最近 60 次采样</p>
      </div>
      <div class="device-chip">
        <span class="device-icon">⌂</span>
        <div><small>设备状态</small><strong :class="`status-${deviceStatus}`">{{ deviceStatus === 'online' ? '在线' : deviceStatus === 'offline' ? '离线' : '等待数据' }}</strong></div>
        <span class="status-ring" :class="`ring-${deviceStatus}`"></span>
      </div>
    </section>

    <section class="metric-grid" aria-label="最新环境数据">
      <article class="metric-card temperature-card">
        <div class="metric-top"><span>室内温度</span><span class="metric-icon">°</span></div>
        <div class="metric-value">{{ latest ? latest.temperature.toFixed(1) : '--' }}<small>°C</small></div>
        <div class="metric-foot"><span class="metric-dot warm"></span><span>舒适范围 20–28°C</span><span class="metric-foot-end">实时</span></div>
      </article>
      <article class="metric-card humidity-card">
        <div class="metric-top"><span>相对湿度</span><span class="metric-icon">◌</span></div>
        <div class="metric-value">{{ latest ? latest.humidity.toFixed(1) : '--' }}<small>%</small></div>
        <div class="metric-foot"><span class="metric-dot cool"></span><span>舒适范围 40–65%</span><span class="metric-foot-end">实时</span></div>
      </article>
      <article class="metric-card quality-card" :class="{ 'quality-alert': isAlert }">
        <div class="metric-top"><span>环境提醒</span><span class="metric-icon">✳</span></div>
        <div class="quality-title">{{ isAlert ? '需要留意' : latest ? '状态平稳' : '等待数据' }}</div>
        <div class="metric-foot"><span class="metric-dot" :class="isAlert ? 'alert' : 'good'"></span><span>{{ isAlert ? (latest && latest.temperature >= temperatureLimit ? '温度超过设定阈值' : '湿度超过设定阈值') : '当前读数在阈值内' }}</span></div>
      </article>
    </section>

    <section class="main-grid">
      <article class="panel chart-panel">
        <div class="panel-heading">
          <div><p class="eyebrow">LIVE TELEMETRY</p><h2>环境趋势</h2></div>
          <div class="chart-window"><span class="chart-window-dot"></span>最近 60 点</div>
        </div>
        <div ref="chartHost" class="chart-host"></div>
        <div v-if="samples.length === 0" class="chart-empty">等待传感器发布第一条数据…</div>
        <div class="chart-footer"><span>更新时间 <b>{{ updatedLabel }}</b></span><span>数据源 <b>sensor-01</b></span></div>
      </article>

      <aside class="panel settings-panel">
        <div class="panel-heading compact-heading">
          <div><p class="eyebrow">ALERT RULES</p><h2>提醒阈值</h2></div>
          <span class="settings-mark">⌁</span>
        </div>
        <p class="settings-copy">读数达到阈值时，环境提醒会即时更新。</p>
        <label class="threshold-row">
          <span><i class="threshold-swatch temp-swatch"></i>温度上限</span>
          <span class="threshold-input"><input v-model.number="temperatureLimit" type="number" min="10" max="50" step="1" aria-label="温度告警阈值"><b>°C</b></span>
        </label>
        <label class="threshold-row">
          <span><i class="threshold-swatch humidity-swatch"></i>湿度上限</span>
          <span class="threshold-input"><input v-model.number="humidityLimit" type="number" min="10" max="100" step="1" aria-label="湿度告警阈值"><b>%</b></span>
        </label>
        <div class="rule-divider"></div>
        <div class="topic-block"><span>MQTT TOPIC</span><code>…/sensor-01/telemetry</code></div>
        <div class="topic-block"><span>TRANSPORT</span><code>MQTT over WebSocket · 9001</code></div>
      </aside>
    </section>

    <footer class="page-footer">
      <span><b>FIELDNOTE</b> · SMART HOME MONITOR</span>
      <span>{{ mqttUrl }} <i>•</i> <span :class="`status-${deviceStatus}`">{{ deviceStatus.toUpperCase() }}</span></span>
    </footer>
  </main>
</template>

