// /assets/js/modules/charts.js
import { state, domElements, CONSTANTS } from '../config.js';
import { appendLog } from '../ui.js';

/**
 * 创建一个图表实例
 * @param {HTMLCanvasElement} canvas 
 * @param {string[]} labels 
 * @param {object} yAxisConfig - Y轴配置
 * @returns {Chart|null}
 */
function createChart(canvas, labels, yAxisConfig) {
  if (!window.Chart) {
    canvas.parentElement.innerHTML = '<div class="readout">Chart.js 未加载，图表不可用</div>';
    return null;
  }
  
  // 颜色方案：区分不同数据
  const colors = [
    { border: 'rgba(255, 99, 132, 0.9)', bg: 'rgba(255, 99, 132, 0.1)' },   // 红色 - target
    { border: 'rgba(54, 162, 235, 1)', bg: 'rgba(54, 162, 235, 0.1)' },    // 蓝色 - now
    { border: 'rgba(255, 206, 86, 1)', bg: 'rgba(255, 206, 86, 0.1)' }     // 黄色 - err
  ];
  
  return new Chart(canvas, {
    type: 'line',
    data: {
      labels: Array.from({ length: CONSTANTS.MAX_CHART_POINTS }, (_, i) => i),
      datasets: labels.map((label, idx) => ({
        label,
        data: Array(CONSTANTS.MAX_CHART_POINTS).fill(0),
        borderColor: colors[idx].border,
        backgroundColor: colors[idx].bg,
        borderWidth: 2,
        borderDash: idx === 0 ? [5, 5] : [],  // 第一条线（target）使用虚线
        tension: 0.3,
        pointRadius: 0,
        fill: false
      }))
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      interaction: {
        intersect: false,
        mode: 'index'
      },
      scales: {
        x: { 
          display: false,
          grid: { display: false }
        },
        y: { 
          min: yAxisConfig.min,
          max: yAxisConfig.max,
          title: { 
            display: true, 
            text: yAxisConfig.title,
            font: { size: 11 }
          },
          ticks: { 
            maxTicksLimit: 6,
            callback: function(value) {
              return value.toFixed(1);
            }
          },
          grid: {
            color: 'rgba(255, 255, 255, 0.1)'
          }
        }
      },
      plugins: {
        legend: {
          display: true,
          position: 'bottom',
          labels: { 
            boxWidth: 12, 
            padding: 8,
            font: { size: 11 }
          },
        },
        tooltip: {
          enabled: true,
          mode: 'index',
          intersect: false
        }
      },
      elements: {
        line: {
          borderJoinStyle: 'round'
        }
      }
    }
  });
}

/**
 * 初始化所有图表
 */
export function initCharts() {
  // 图表1：角度跟踪（-15°到+15°）
  state.charts.chart1 = createChart(
    domElements.chart1.canvas, 
    ['target', 'now', 'err'],
    { min: -15, max: 15, title: '角度 (°)' }
  );
  
  // 图表2：力矩输出（-300到+300）
  state.charts.chart2 = createChart(
    domElements.chart2.canvas, 
    ['ang_duty', 'spd×10', 'pos×10'],
    { min: -300, max: 300, title: '力矩输出' }
  );
  
  // 图表3：速度跟踪（-20到+20）
  state.charts.chart3 = createChart(
    domElements.chart3.canvas, 
    ['spd.now', 'spd.tar', 'pos_err×10'],
    { min: -20, max: 20, title: '速度 (rad/s)' }
  );
  
  appendLog('[INIT] charts ready');
}

/**
 * 应用从后端接收的图表配置
 * @param {Array<object>} config 
 */
export function applyChartConfig(config) {
  if (!Array.isArray(config)) return;

  const chartRefs = [
    { chart: state.charts.chart1, titleEl: domElements.chart1.title },
    { chart: state.charts.chart2, titleEl: domElements.chart2.title },
    { chart: state.charts.chart3, titleEl: domElements.chart3.title },
  ];

  config.forEach((c, i) => {
    const { chart, titleEl } = chartRefs[i];
    if (titleEl && c.title) titleEl.textContent = c.title;

    if (chart && Array.isArray(c.legends)) {
      c.legends.forEach((name, j) => {
        if (chart.data.datasets[j]) {
          chart.data.datasets[j].label = name;
        }
      });
      chart.update('none');
    }
  });
}

/**
 * 向图表推送一组新数据
 * @param {Chart} chart 
 * @param {number[]} dataPoints 
 */
function pushDataToChart(chart, dataPoints) {
  if (!chart) return;
  const datasets = chart.data.datasets;
  dataPoints.forEach((value, i) => {
    if (datasets[i]) {
      datasets[i].data.push(value);
      if (datasets[i].data.length > CONSTANTS.MAX_CHART_POINTS) {
        datasets[i].data.shift();
      }
    }
  });
  chart.update('none');
}

/**
 * 将遥测数据馈送到所有图表
 * @param  {...number} data - 9个数据点
 */
export function feedChartsData(...data) {
  pushDataToChart(state.charts.chart1, data.slice(0, 3));
  pushDataToChart(state.charts.chart2, data.slice(3, 6));
  pushDataToChart(state.charts.chart3, data.slice(6, 9));
}