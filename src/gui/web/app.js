// Global state variables
let nodes = [];
let links = [];
let selectedElement = null; // { type: 'node'|'link', id: number }
let activePreset = 'vehicle';
let isRunning = false;
let simTime = 0.0;
let timeStep = 0.05;
let solverType = 'rk4';
let simulationSpeed = 1;
let chart = null;
let animationFrameId = null;

// History for CSV export
let timeHistory = [];
let tempHistory = {}; // { nodeId: [temp1, temp2, ...] }
let activePlotNodes = new Set(); // Set of node IDs currently plotted

// Canvas configurations
const canvas = document.getElementById('network-canvas');
const ctx = canvas.getContext('2d');
let isDragging = false;
let dragNode = null;
let dragOffset = { x: 0, y: 0 };
let linkingStartNode = null;
let tempMousePos = { x: 0, y: 0 };
let currentTool = 'select'; // 'select', 'link'
let flowOffset = 0;
let gridSnap = true;
const GRID_SIZE = 20;

// Conversions
const K_ZERO = 273.15;
function cToK(c) { return c + K_ZERO; }
function kToC(k) { return k - K_ZERO; }

// --- PRESET SCENARIO DATA (GT-Suite Schematic Positions) ---
const PRESETS = {
    vehicle: {
        name: "Vehicle Cooling Loop",
        nodes: [
            { id: 1, name: "Engine Block", x: 180, y: 100, temp: 40.0, capacity: 50000.0, q_gen: 15000.0, is_fixed: false },
            { id: 2, name: "Engine Jacket", x: 180, y: 260, temp: 30.0, capacity: 4000.0, q_gen: 0.0, is_fixed: false },
            { id: 3, name: "Radiator Coolant", x: 500, y: 260, temp: 25.0, capacity: 3000.0, q_gen: 0.0, is_fixed: false },
            { id: 4, name: "Radiator Core", x: 500, y: 100, temp: 25.0, capacity: 8000.0, q_gen: 0.0, is_fixed: false },
            { id: 5, name: "Ambient Air", x: 680, y: 100, temp: 30.0, capacity: 1.0, q_gen: 0.0, is_fixed: true }
        ],
        links: [
            { id: 101, node_a: 1, node_b: 2, type: 0, p1: 1200.0, p2: 0.0 }, // Cond
            { id: 102, node_a: 3, node_b: 4, type: 1, p1: 800.0, p2: 0.0 }, // Conv
            { id: 103, node_a: 4, node_b: 5, type: 1, p1: 500.0, p2: 0.0 }, // Conv
            { id: 104, node_a: 2, node_b: 3, type: 3, p1: 400.0, p2: 1.0 }, // Flow E->R
            { id: 105, node_a: 3, node_b: 2, type: 3, p1: 400.0, p2: 1.0 }  // Flow R->E
        ],
        sliders: [
            { id: "engine_power", label: "Engine Heat Output (kW)", min: 0, max: 80, value: 15, step: 2, target: "node_kw", targetId: 1, field: "q_gen" },
            { id: "pump_speed", label: "Water Pump Rate (W/K)", min: 0, max: 2000, value: 400, step: 100, target: "links_all_flow", targetIds: [104, 105], field: "p1" },
            { id: "radiator_fan", label: "Radiator Cooling Fan (W/K)", min: 50, max: 3000, value: 500, step: 50, target: "link", targetId: 103, field: "p1" },
            { id: "ambient_temp", label: "Ambient Environment Temp (°C)", min: 5, max: 48, value: 30, step: 1, target: "node", targetId: 5, field: "temp" }
        ]
    },
    cpu: {
        name: "CPU Cooler Assembly",
        nodes: [
            { id: 1, name: "CPU Die", x: 140, y: 180, temp: 40.0, capacity: 150.0, q_gen: 85.0, is_fixed: false },
            { id: 2, name: "TIM Thermal Paste", x: 300, y: 180, temp: 32.0, capacity: 15.0, q_gen: 0.0, is_fixed: false },
            { id: 3, name: "Copper Heat Sink", x: 460, y: 180, temp: 28.0, capacity: 350.0, q_gen: 0.0, is_fixed: false },
            { id: 4, name: "Chassis Air", x: 620, y: 180, temp: 25.0, capacity: 1.0, q_gen: 0.0, is_fixed: true }
        ],
        links: [
            { id: 101, node_a: 1, node_b: 2, type: 0, p1: 80.0, p2: 0.0 },
            { id: 102, node_a: 2, node_b: 3, type: 0, p1: 50.0, p2: 0.0 },
            { id: 103, node_a: 3, node_b: 4, type: 1, p1: 4.0, p2: 0.0 }
        ],
        sliders: [
            { id: "cpu_power", label: "CPU TDP Heat (W)", min: 0, max: 200, value: 85, step: 5, target: "node", targetId: 1, field: "q_gen" },
            { id: "fan_speed", label: "Heat Sink Convection (W/K)", min: 0.1, max: 20.0, value: 4.0, step: 0.5, target: "link", targetId: 103, field: "p1" }
        ]
    },
    battery: {
        name: "Li-Ion Battery Liquid Cooling",
        nodes: [
            { id: 1, name: "Battery Cell 1", x: 180, y: 100, temp: 25.0, capacity: 600.0, q_gen: 20.0, is_fixed: false },
            { id: 2, name: "Battery Cell 2", x: 300, y: 100, temp: 25.0, capacity: 600.0, q_gen: 20.0, is_fixed: false },
            { id: 3, name: "Battery Cell 3", x: 420, y: 100, temp: 25.0, capacity: 600.0, q_gen: 20.0, is_fixed: false },
            { id: 4, name: "Battery Cell 4", x: 540, y: 100, temp: 25.0, capacity: 600.0, q_gen: 20.0, is_fixed: false },
            { id: 5, name: "Coolant Channel 1", x: 180, y: 240, temp: 20.0, capacity: 100.0, q_gen: 0.0, is_fixed: false },
            { id: 6, name: "Coolant Channel 2", x: 300, y: 240, temp: 20.0, capacity: 100.0, q_gen: 0.0, is_fixed: false },
            { id: 7, name: "Coolant Channel 3", x: 420, y: 240, temp: 20.0, capacity: 100.0, q_gen: 0.0, is_fixed: false },
            { id: 8, name: "Coolant Channel 4", x: 540, y: 240, temp: 20.0, capacity: 100.0, q_gen: 0.0, is_fixed: false },
            { id: 9, name: "Coolant Inlet", x: 60, y: 240, temp: 20.0, capacity: 1.0, q_gen: 0.0, is_fixed: true },
            { id: 10, name: "Coolant Outlet", x: 660, y: 240, temp: 20.0, capacity: 1.0, q_gen: 0.0, is_fixed: true }
        ],
        links: [
            { id: 101, node_a: 1, node_b: 2, type: 0, p1: 0.8, p2: 0.0 },
            { id: 102, node_a: 2, node_b: 3, type: 0, p1: 0.8, p2: 0.0 },
            { id: 103, node_a: 3, node_b: 4, type: 0, p1: 0.8, p2: 0.0 },
            { id: 104, node_a: 1, node_b: 5, type: 1, p1: 12.0, p2: 0.0 },
            { id: 105, node_a: 2, node_b: 6, type: 1, p1: 12.0, p2: 0.0 },
            { id: 106, node_a: 3, node_b: 7, type: 1, p1: 12.0, p2: 0.0 },
            { id: 107, node_a: 4, node_b: 8, type: 1, p1: 12.0, p2: 0.0 },
            { id: 108, node_a: 9, node_b: 5, type: 3, p1: 8.0, p2: 1.0 },
            { id: 109, node_a: 5, node_b: 6, type: 3, p1: 8.0, p2: 1.0 },
            { id: 110, node_a: 6, node_b: 7, type: 3, p1: 8.0, p2: 1.0 },
            { id: 111, node_a: 7, node_b: 8, type: 3, p1: 8.0, p2: 1.0 },
            { id: 112, node_a: 8, node_b: 10, type: 3, p1: 8.0, p2: 1.0 }
        ],
        sliders: [
            { id: "battery_heat", label: "Cell Dissipation (W/cell)", min: 0, max: 50, value: 20, step: 2, target: "nodes_all_q", targetIds: [1, 2, 3, 4], field: "q_gen" },
            { id: "coolant_flow", label: "Fluid Heat Capacity Flow (W/K)", min: 0.5, max: 25, value: 8.0, step: 0.5, target: "links_all_flow", targetIds: [108, 109, 110, 111, 112], field: "p1" },
            { id: "inlet_temp", label: "Coolant Feed Temp (°C)", min: 5, max: 35, value: 20, step: 1, target: "node", targetId: 9, field: "temp" }
        ]
    },
    window: {
        name: "Double-Pane Window Insulation",
        nodes: [
            { id: 1, name: "Indoor Room Air", x: 100, y: 180, temp: 22.0, capacity: 1.0, q_gen: 0.0, is_fixed: true },
            { id: 2, name: "Inner Pane Glass", x: 260, y: 180, temp: 16.0, capacity: 500.0, q_gen: 0.0, is_fixed: false },
            { id: 3, name: "Gas Gap (Argon)", x: 420, y: 180, temp: 8.0, capacity: 20.0, q_gen: 0.0, is_fixed: false },
            { id: 4, name: "Outer Pane Glass", x: 580, y: 180, temp: 1.0, capacity: 500.0, q_gen: 0.0, is_fixed: false },
            { id: 5, name: "Outdoor Atmosphere", x: 740, y: 180, temp: -8.0, capacity: 1.0, q_gen: 0.0, is_fixed: true }
        ],
        links: [
            { id: 101, node_a: 1, node_b: 2, type: 1, p1: 8.0, p2: 0.0 },
            { id: 102, node_a: 2, node_b: 3, type: 0, p1: 3.5, p2: 0.0 },
            { id: 103, node_a: 3, node_b: 4, type: 0, p1: 3.5, p2: 0.0 },
            { id: 104, node_a: 4, node_b: 5, type: 1, p1: 30.0, p2: 0.0 }
        ],
        sliders: [
            { id: "indoor_temp", label: "Indoor Thermostat (°C)", min: 16, max: 28, value: 22, step: 0.5, target: "node", targetId: 1, field: "temp" },
            { id: "outdoor_temp", label: "Outdoor Temperature (°C)", min: -35, max: 15, value: -8, step: 1.0, target: "node", targetId: 5, field: "temp" },
            { id: "wind_convection", label: "Wind Heat Transfer (W/K)", min: 5, max: 100, value: 30.0, step: 2.0, target: "link", targetId: 104, field: "p1" }
        ]
    }
};

// --- INITIALIZATION ---
window.addEventListener('DOMContentLoaded', () => {
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    
    initChart();
    setupEventHandlers();
    setupTabs();
    
    loadPreset(activePreset);
    logInfo("Application GUI initialized. Dotted grid snapping activated.");
});

function resizeCanvas() {
    const parent = canvas.parentElement;
    canvas.width = parent.clientWidth;
    canvas.height = parent.clientHeight;
    draw();
}

// --- LOGGING ---
function logConsole(message, type = 'info') {
    const consoleBox = document.getElementById('console-output');
    if (!consoleBox) return;
    
    const timestamp = new Date().toTimeString().split(' ')[0];
    const line = document.createElement('div');
    line.className = `console-line ${type}`;
    line.innerText = `[${timestamp}] [${type.toUpperCase()}] ${message}`;
    
    consoleBox.appendChild(line);
    consoleBox.scrollTop = consoleBox.scrollHeight;
}

function logInfo(msg) { logConsole(msg, 'info'); }
function logSuccess(msg) { logConsole(msg, 'success'); }
function logWarning(msg) { logConsole(msg, 'warning'); }
function logError(msg) { logConsole(msg, 'error'); }

// --- TABS ---
function setupTabs() {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            
            btn.classList.add('active');
            const tabId = btn.getAttribute('data-tab');
            document.getElementById(tabId).classList.add('active');
            
            if (tabId === 'tab-charts' && chart) {
                chart.resize();
            } else if (tabId === 'tab-fan') {
                updateFanMatching();
            }
        });
    });
}

// --- SIMULATION RUNS ---

async function initSystemOnServer() {
    const payload = {
        nodes: nodes.map(n => ({
            id: n.id,
            name: n.name,
            temp_k: cToK(n.temp),
            capacity: n.capacity,
            q_gen: n.q_gen,
            is_fixed: n.is_fixed,
            c_a1: n.c_a1 || 0.0,
            c_a2: n.c_a2 || 0.0,
            domain_type: n.domain || 0,
            fluid_medium: n.fluid_medium || "",
            fluid_volume: n.fluid_volume || 0.0,
            fluid_mix_ratio: n.fluid_mix_ratio !== undefined ? n.fluid_mix_ratio : 0.5,
            fluid_rho_a0: n.fluid_rho_a0 !== undefined ? n.fluid_rho_a0 : 1000.0,
            fluid_rho_a1: n.fluid_rho_a1 !== undefined ? n.fluid_rho_a1 : 0.0,
            fluid_rho_a2: n.fluid_rho_a2 !== undefined ? n.fluid_rho_a2 : 0.0,
            fluid_cp_a0: n.fluid_cp_a0 !== undefined ? n.fluid_cp_a0 : 4184.0,
            fluid_cp_a1: n.fluid_cp_a1 !== undefined ? n.fluid_cp_a1 : 0.0,
            fluid_cp_a2: n.fluid_cp_a2 !== undefined ? n.fluid_cp_a2 : 0.0
        })),
        links: links.map(l => ({
            id: l.id,
            node_a: l.node_a,
            node_b: l.node_b,
            type: l.type,
            p1: l.p1,
            p2: l.p2,
            g_a1: l.g_a1 || 0.0,
            g_a2: l.g_a2 || 0.0,
            fan_area: l.fan_area !== undefined ? l.fan_area : 0.005
        }))
    };
    
    try {
        const response = await fetch('/api/init', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const result = await response.json();
        if (result.status === 'success') {
            logInfo(`Thermal model compiled successfully on C++ core. (${nodes.length} nodes, ${links.length} links)`);
        } else {
            logError("Solver compilation failed: " + result.message);
        }
    } catch (e) {
        logError("Failed to connect to the C++ solver server: " + e.message);
    }
}

async function runSimulationStep() {
    if (!isRunning) return;
    
    const payload = {
        dt: timeStep,
        solver: solverType,
        nodes: nodes.map(n => ({
            id: n.id,
            temp_k: cToK(n.temp),
            capacity: n.capacity,
            q_gen: n.q_gen,
            is_fixed: n.is_fixed,
            c_a1: n.c_a1 || 0.0,
            c_a2: n.c_a2 || 0.0,
            domain_type: n.domain || 0,
            fluid_medium: n.fluid_medium || "",
            fluid_volume: n.fluid_volume || 0.0,
            fluid_mix_ratio: n.fluid_mix_ratio !== undefined ? n.fluid_mix_ratio : 0.5,
            fluid_rho_a0: n.fluid_rho_a0 !== undefined ? n.fluid_rho_a0 : 1000.0,
            fluid_rho_a1: n.fluid_rho_a1 !== undefined ? n.fluid_rho_a1 : 0.0,
            fluid_rho_a2: n.fluid_rho_a2 !== undefined ? n.fluid_rho_a2 : 0.0,
            fluid_cp_a0: n.fluid_cp_a0 !== undefined ? n.fluid_cp_a0 : 4184.0,
            fluid_cp_a1: n.fluid_cp_a1 !== undefined ? n.fluid_cp_a1 : 0.0,
            fluid_cp_a2: n.fluid_cp_a2 !== undefined ? n.fluid_cp_a2 : 0.0
        })),
        links: links.map(l => ({
            id: l.id,
            p1: l.p1,
            p2: l.p2,
            g_a1: l.g_a1 || 0.0,
            g_a2: l.g_a2 || 0.0,
            fan_area: l.fan_area !== undefined ? l.fan_area : 0.005
        }))
    };
    
    try {
        const response = await fetch('/api/step', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const result = await response.json();
        if (result.status === 'success') {
            // Update temperatures
            for (const [idStr, tempK] of Object.entries(result.temperatures)) {
                const node = nodes.find(n => n.id === parseInt(idStr));
                if (node) {
                    node.temp = kToC(tempK);
                    syncNodeProperties(node);
                }
            }
            
            simTime += timeStep;
            document.getElementById('time-display').innerText = simTime.toFixed(2) + 's';
            
            recordHistory();
            updateChart();
            updateDiagnostics();
            updateModelTree();
            updateSpreadsheet();
            draw();
        }
    } catch (e) {
        logError("Simulation step communication failed: " + e.message);
        pauseSimulation();
    }
}

async function runSteadyStateSolve() {
    logInfo("Starting steady-state iterative Gauss-Seidel balance (Newton-Raphson radiation solver)...");
    const payload = {
        tolerance: 1e-6,
        max_iterations: 1500,
        nodes: nodes.map(n => ({
            id: n.id,
            temp_k: cToK(n.temp),
            capacity: n.capacity,
            q_gen: n.q_gen,
            is_fixed: n.is_fixed,
            c_a1: n.c_a1 || 0.0,
            c_a2: n.c_a2 || 0.0,
            domain_type: n.domain || 0,
            fluid_medium: n.fluid_medium || "",
            fluid_volume: n.fluid_volume || 0.0,
            fluid_mix_ratio: n.fluid_mix_ratio !== undefined ? n.fluid_mix_ratio : 0.5,
            fluid_rho_a0: n.fluid_rho_a0 !== undefined ? n.fluid_rho_a0 : 1000.0,
            fluid_rho_a1: n.fluid_rho_a1 !== undefined ? n.fluid_rho_a1 : 0.0,
            fluid_rho_a2: n.fluid_rho_a2 !== undefined ? n.fluid_rho_a2 : 0.0,
            fluid_cp_a0: n.fluid_cp_a0 !== undefined ? n.fluid_cp_a0 : 4184.0,
            fluid_cp_a1: n.fluid_cp_a1 !== undefined ? n.fluid_cp_a1 : 0.0,
            fluid_cp_a2: n.fluid_cp_a2 !== undefined ? n.fluid_cp_a2 : 0.0
        })),
        links: links.map(l => ({
            id: l.id,
            p1: l.p1,
            p2: l.p2,
            g_a1: l.g_a1 || 0.0,
            g_a2: l.g_a2 || 0.0,
            fan_area: l.fan_area !== undefined ? l.fan_area : 0.005
        }))
    };
    
    try {
        const response = await fetch('/api/solve_steady', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const result = await response.json();
        if (result.status === 'success') {
            for (const [idStr, tempK] of Object.entries(result.temperatures)) {
                const node = nodes.find(n => n.id === parseInt(idStr));
                if (node) {
                    node.temp = kToC(tempK);
                    syncNodeProperties(node);
                }
            }
            
            logSuccess(`Steady-state converged in ${result.iterations} solver sweeps. Max residual error < 1e-6 K.`);
            
            recordHistory();
            updateChart();
            updateDiagnostics();
            updateModelTree();
            updateSpreadsheet();
            draw();
        }
    } catch (e) {
        logError("Steady-state solver execution failed: " + e.message);
    }
}

function recordHistory() {
    timeHistory.push(simTime);
    nodes.forEach(node => {
        if (!tempHistory[node.id]) tempHistory[node.id] = [];
        tempHistory[node.id].push(node.temp);
    });
}

function loop() {
    if (!isRunning) return;
    
    const steps = simulationSpeed;
    const runSteps = async () => {
        for (let i = 0; i < steps; i++) {
            await runSimulationStep();
        }
        flowOffset = (flowOffset + 2) % 100;
        animationFrameId = requestAnimationFrame(loop);
    };
    runSteps();
}

function startSimulation() {
    if (isRunning) return;
    isRunning = true;
    logInfo(`Simulation started. Solver: ${solverType.toUpperCase()}, Step: ${timeStep}s.`);
    document.getElementById('btn-play').disabled = true;
    document.getElementById('btn-pause').disabled = false;
    loop();
}

function pauseSimulation() {
    if (!isRunning) return;
    isRunning = false;
    logInfo("Simulation paused.");
    document.getElementById('btn-play').disabled = false;
    document.getElementById('btn-pause').disabled = true;
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
    }
}

function resetSimulation() {
    pauseSimulation();
    simTime = 0.0;
    document.getElementById('time-display').innerText = '0.00s';
    
    const preset = PRESETS[activePreset];
    if (preset) {
        nodes.forEach(node => {
            const initialNode = preset.nodes.find(n => n.id === node.id);
            if (initialNode) {
                node.temp = initialNode.temp;
            }
        });
    }
    
    timeHistory = [];
    tempHistory = {};
    
    chart.data.labels = [];
    chart.data.datasets.forEach(ds => ds.data = []);
    chart.update();
    
    initSystemOnServer();
    updateDiagnostics();
    updateSpreadsheet();
    updateModelTree();
    draw();
    logInfo("Model simulation state reset to boundary initializations.");
}

function clearWorkspace() {
    pauseSimulation();
    nodes = [];
    links = [];
    selectedElement = null;
    simTime = 0.0;
    document.getElementById('time-display').innerText = '0.00s';
    
    timeHistory = [];
    tempHistory = {};
    
    initSystemOnServer();
    resetChartData();
    updateDiagnostics();
    updateModelTree();
    updateSpreadsheet();
    draw();
    logWarning("Schematic workspace cleared.");
}

// --- DATA EXPORT TO CSV ---
function exportCSV() {
    if (timeHistory.length === 0) {
        logWarning("No simulation history available to export. Run the simulation first.");
        return;
    }
    
    let csvContent = "Time (s)";
    nodes.forEach(node => {
        csvContent += `,${node.name.replace(/,/g, '')} (°C)`;
    });
    csvContent += "\n";
    
    for (let i = 0; i < timeHistory.length; i++) {
        let row = `${timeHistory[i].toFixed(3)}`;
        nodes.forEach(node => {
            const temps = tempHistory[node.id] || [];
            const tempVal = temps[i] !== undefined ? temps[i].toFixed(4) : "";
            row += `,${tempVal}`;
        });
        csvContent += row + "\n";
    }
    
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `thermal_sim_export_${activePreset}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    
    logSuccess(`Model time history exported successfully (${timeHistory.length} timesteps logged).`);
}

// --- CHART MANAGEMENT ---
function initChart() {
    const ctxChart = document.getElementById('temp-chart').getContext('2d');
    chart = new Chart(ctxChart, {
        type: 'line',
        data: {
            labels: [],
            datasets: []
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            scales: {
                x: {
                    type: 'linear',
                    title: { display: true, text: 'Time (s)', color: '#4b5563', font: { size: 10 } },
                    grid: { color: 'rgba(0, 0, 0, 0.05)' },
                    ticks: { color: '#4b5563', font: { size: 9 } }
                },
                y: {
                    title: { display: true, text: 'Temperature (°C)', color: '#4b5563', font: { size: 10 } },
                    grid: { color: 'rgba(0, 0, 0, 0.05)' },
                    ticks: { color: '#4b5563', font: { size: 9 } }
                }
            },
            plugins: {
                legend: {
                    display: false // We use our own checklist legend
                }
            }
        }
    });
}

const colors = ['#025ca2', '#e07a00', '#15803d', '#b91c1c', '#6b21a8', '#db2777', '#ea580c'];

function resetChartData() {
    chart.data.labels = [];
    chart.data.datasets = [];
    
    const selectorContainer = document.getElementById('series-selector-list');
    selectorContainer.innerHTML = '';
    
    nodes.forEach((node, i) => {
        const color = colors[i % colors.length];
        
        // Add all node IDs to active list by default
        activePlotNodes.add(node.id);
        
        // Add Checkbox in list
        const div = document.createElement('div');
        div.className = 'series-check-item';
        div.innerHTML = `
            <input type="checkbox" id="chk-series-${node.id}" checked>
            <span class="series-color-legend" style="background-color: ${color};"></span>
            <label for="chk-series-${node.id}">${node.name}</label>
        `;
        selectorContainer.appendChild(div);
        
        // Bind toggle listener
        div.querySelector('input').addEventListener('change', (e) => {
            if (e.target.checked) {
                activePlotNodes.add(node.id);
            } else {
                activePlotNodes.delete(node.id);
            }
            rebuildDatasets();
        });
    });
    
    rebuildDatasets();
}

function rebuildDatasets() {
    if (!chart) return;
    
    const datasets = [];
    nodes.forEach((node, i) => {
        if (!activePlotNodes.has(node.id)) return;
        
        const color = colors[i % colors.length];
        const dataPoints = [];
        
        for (let k = 0; k < timeHistory.length; k++) {
            const temps = tempHistory[node.id] || [];
            if (temps[k] !== undefined) {
                dataPoints.push({ x: timeHistory[k], y: temps[k] });
            }
        }
        
        datasets.push({
            label: node.name,
            data: dataPoints,
            borderColor: color,
            borderWidth: 1.5,
            pointRadius: 0,
            fill: false,
            tension: 0.1,
            nodeId: node.id
        });
    });
    
    chart.data.datasets = datasets;
    chart.update('none');
}

function updateChart() {
    if (!chart) return;
    
    chart.data.labels.push(simTime);
    
    chart.data.datasets.forEach(dataset => {
        const node = nodes.find(n => n.id === dataset.nodeId);
        if (node) {
            dataset.data.push({ x: simTime, y: node.temp });
        }
    });
    
    // Slide window limit (500 pts)
    const maxPts = 500;
    if (chart.data.labels.length > maxPts) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(ds => ds.data.shift());
    }
    
    chart.update('none');
}

// --- DIAGNOSTICS ---
function updateDiagnostics() {
    document.getElementById('diag-nodes').innerText = `${nodes.filter(n=>!n.is_fixed).length} / ${nodes.filter(n=>n.is_fixed).length} fixed`;
    document.getElementById('diag-links').innerText = links.length;
    
    if (nodes.length > 0) {
        const temps = nodes.map(n => n.temp);
        const maxTemp = Math.max(...temps);
        const minTemp = Math.min(...temps);
        document.getElementById('diag-max-temp').innerText = `${maxTemp.toFixed(1)} °C (${cToK(maxTemp).toFixed(1)} K)`;
        document.getElementById('diag-min-temp').innerText = `${minTemp.toFixed(1)} °C (${cToK(minTemp).toFixed(1)} K)`;
        
        let totalQGen = nodes.reduce((sum, n) => sum + (n.is_fixed ? 0 : n.q_gen), 0);
        document.getElementById('diag-energy-bal').innerText = (totalQGen/1000.0).toFixed(2) + ' kW';
    } else {
        document.getElementById('diag-max-temp').innerText = '--';
        document.getElementById('diag-min-temp').innerText = '--';
        document.getElementById('diag-energy-bal').innerText = '--';
    }
}

// --- MODEL DIRECTORY TREE ---
function updateModelTree() {
    const nodesList = document.getElementById('tree-nodes-list');
    const linksList = document.getElementById('tree-links-list');
    
    nodesList.innerHTML = '';
    linksList.innerHTML = '';
    
    nodes.forEach(node => {
        const div = document.createElement('div');
        const isSelected = selectedElement && selectedElement.type === 'node' && selectedElement.id === node.id;
        div.className = `tree-item ${isSelected ? 'selected' : ''}`;
        div.innerHTML = `⚫ [N:${node.id}] ${node.name}`;
        div.addEventListener('click', (e) => {
            e.stopPropagation();
            selectElement('node', node.id);
        });
        nodesList.appendChild(div);
    });
    
    links.forEach(link => {
        const div = document.createElement('div');
        const isSelected = selectedElement && selectedElement.type === 'link' && selectedElement.id === link.id;
        div.className = `tree-item ${isSelected ? 'selected' : ''}`;
        
        let typeStr = "Cond";
        if (link.type === 1) typeStr = "Conv";
        else if (link.type === 2) typeStr = "Rad";
        else if (link.type === 3) typeStr = "Flow";
        else if (link.type === 4) typeStr = "Fan";
        
        div.innerHTML = `🔗 [L:${link.id}] ${typeStr} (${link.node_a} ➔ ${link.node_b})`;
        div.addEventListener('click', (e) => {
            e.stopPropagation();
            selectElement('link', link.id);
        });
        linksList.appendChild(div);
    });
}

// --- PRESET SCENARIO LOADING ---
function loadPreset(key) {
    activePreset = key;
    const preset = PRESETS[key];
    if (!preset) return;
    
    nodes = JSON.parse(JSON.stringify(preset.nodes));
    links = JSON.parse(JSON.stringify(preset.links));
    sanitizeModel();
    
    selectedElement = null;
    simTime = 0.0;
    document.getElementById('time-display').innerText = '0.00s';
    
    // Sliders / Scenario parameters
    const sliderContainer = document.getElementById('dynamic-sliders');
    sliderContainer.innerHTML = '';
    
    preset.sliders.forEach(slide => {
        const div = document.createElement('div');
        div.className = 'slider-group';
        
        let initialVal = slide.value;
        let suffix = "";
        if (slide.id.includes("power")) suffix = " kW";
        else if (slide.id.includes("speed") || slide.id.includes("fan") || slide.id.includes("flow")) suffix = " W/K";
        else if (slide.id.includes("temp")) suffix = " °C";
        
        div.innerHTML = `
            <div class="slider-label-val">
                <span class="slider-name">${slide.label}</span>
                <span class="slider-val" id="val-${slide.id}">${initialVal}${suffix}</span>
            </div>
            <input type="range" id="${slide.id}" min="${slide.min}" max="${slide.max}" value="${slide.value}" step="${slide.step}">
        `;
        sliderContainer.appendChild(div);
        
        div.querySelector('input').addEventListener('input', (e) => {
            const val = parseFloat(e.target.value);
            document.getElementById(`val-${slide.id}`).innerText = val + suffix;
            
            if (slide.target === "node") {
                const node = nodes.find(n => n.id === slide.targetId);
                if (node) node.temp = val;
            } else if (slide.target === "node_kw") {
                const node = nodes.find(n => n.id === slide.targetId);
                if (node) node.q_gen = val * 1000.0;
            } else if (slide.target === "nodes_all_q") {
                slide.targetIds.forEach(id => {
                    const node = nodes.find(n => n.id === id);
                    if (node) node.q_gen = val;
                });
            } else if (slide.target === "link") {
                const link = links.find(l => l.id === slide.targetId);
                if (link) link.p1 = val;
            } else if (slide.target === "links_all_flow") {
                slide.targetIds.forEach(id => {
                    const link = links.find(l => l.id === id);
                    if (link) link.p1 = val;
                });
            }
            updateSpreadsheet();
            draw();
        });
    });
    
    // Server Init
    initSystemOnServer();
    
    // History
    timeHistory = [];
    tempHistory = {};
    recordHistory();
    
    // Chart
    resetChartData();
    updateChart();
    
    // UI elements
    updateDiagnostics();
    updateModelTree();
    updateSpreadsheet();
    
    logSuccess(`Preset "${preset.name}" loaded successfully.`);
    draw();
}

// --- CANVAS CAD RENDERING ---

function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Grid Lines (CAD style, solid light grey lines)
    ctx.strokeStyle = '#e9ecf2';
    ctx.lineWidth = 0.5;
    
    // Vertical grid
    for (let x = 0; x < canvas.width; x += GRID_SIZE) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
        ctx.stroke();
    }
    // Horizontal grid
    for (let y = 0; y < canvas.height; y += GRID_SIZE) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvas.width, y);
        ctx.stroke();
    }
    
    // Draw Connection Link Lines
    links.forEach(link => {
        const nodeA = nodes.find(n => n.id === link.node_a);
        const nodeB = nodes.find(n => n.id === link.node_b);
        if (!nodeA || !nodeB) return;
        
        const isSelected = selectedElement && selectedElement.type === 'link' && selectedElement.id === link.id;
        
        // Find best connection points (ports) and compute route
        const portA = getClosestPort(nodeA, nodeB);
        const portB = getClosestPort(nodeB, nodeA);
        const route = computeOrthogonalRoute(portA, portA.name, portB, portB.name);
        
        ctx.beginPath();
        ctx.moveTo(route[0].x, route[0].y);
        for (let i = 1; i < route.length; ++i) {
            ctx.lineTo(route[i].x, route[i].y);
        }
        
        let color = '#475569'; // Default conduction (dark gray)
        let dash = [];
        
        if (link.type === 0) { // Conduction
            color = '#475569';
        } else if (link.type === 1) { // Convection
            color = '#15803d'; // Green
            dash = [4, 3];
        } else if (link.type === 2) { // Radiation
            color = '#ea580c'; // Orange
            dash = [6, 4];
        } else if (link.type === 3) { // Flow
            color = '#0284c7'; // Blue
        } else if (link.type === 4) { // Fan / Pump
            color = '#7c3aed'; // Violet
        }
        
        ctx.strokeStyle = color;
        ctx.lineWidth = isSelected ? 3 : 1.5;
        ctx.setLineDash(dash);
        
        // Animated flow dots
        if ((link.type === 3 || link.type === 4) && isRunning) {
            const dirFactor = link.type === 4 ? (link.p2 >= 0 ? 1.0 : -1.0) : (link.p2 || 1.0);
            ctx.lineDashOffset = -flowOffset * dirFactor * 0.5;
            ctx.setLineDash([8, 8]);
        }
        
        ctx.stroke();
        ctx.setLineDash([]); // Reset
        
        if (isSelected) {
            ctx.strokeStyle = 'rgba(2, 92, 162, 0.2)';
            ctx.lineWidth = 6;
            ctx.beginPath();
            ctx.moveTo(route[0].x, route[0].y);
            for (let i = 1; i < route.length; ++i) {
                ctx.lineTo(route[i].x, route[i].y);
            }
            ctx.stroke();
        }
        
        // Draw Parameter Value Box on the longest line segment
        let longestIdx = 0;
        let maxLen = -1;
        for (let i = 0; i < route.length - 1; ++i) {
            const len = Math.hypot(route[i+1].x - route[i].x, route[i+1].y - route[i].y);
            if (len > maxLen) {
                maxLen = len;
                longestIdx = i;
            }
        }
        const midX = (route[longestIdx].x + route[longestIdx+1].x) / 2;
        const midY = (route[longestIdx].y + route[longestIdx+1].y) / 2;
        
        ctx.fillStyle = '#ffffff';
        ctx.strokeStyle = color;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.rect(midX - 16, midY - 8, 32, 16);
        ctx.fill();
        ctx.stroke();
        
        ctx.fillStyle = color;
        ctx.font = '8px Consolas';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        
        let valStr = link.p1.toFixed(0);
        if (link.p1 >= 1000) valStr = (link.p1/1000).toFixed(1) + 'k';
        ctx.fillText(valStr, midX, midY);
    });
    
    // Draw Nodes (CAD Blocks)
    nodes.forEach(node => {
        const isSelected = selectedElement && selectedElement.type === 'node' && selectedElement.id === node.id;
        
        // Soft color representing temperature
        // 10°C (light blue) -> 100°C (light red)
        const tMin = 10, tMax = 100;
        let factor = (node.temp - tMin) / (tMax - tMin);
        factor = Math.max(0.0, Math.min(1.0, factor));
        
        // Soft blue to soft red interpolation
        const r = Math.round(220 + factor * 35);
        const g = Math.round(230 - factor * 40);
        const b = Math.round(250 - factor * 60);
        const fillStyle = node.is_fixed ? '#ffeec0' : `rgb(${r}, ${g}, ${b})`;
        const strokeStyle = isSelected ? '#025ca2' : (node.is_fixed ? '#d97706' : '#64748b');
        
        // Draw CAD Rectangle Block (60px width, 40px height)
        const w = 70, h = 40;
        const x = node.x - w / 2;
        const y = node.y - h / 2;
        
        ctx.fillStyle = fillStyle;
        ctx.strokeStyle = strokeStyle;
        ctx.lineWidth = isSelected ? 2.5 : 1.5;
        
        // Draw block
        ctx.beginPath();
        ctx.roundRect(x, y, w, h, 3);
        ctx.fill();
        ctx.stroke();
        
        // Double border for boundary fixed nodes
        if (node.is_fixed) {
            ctx.strokeStyle = '#b45309';
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.roundRect(x + 2, y + 2, w - 4, h - 4, 2);
            ctx.stroke();
        }
        
        // Draw connection port points (Blue squares on four sides)
        const ports = getPortPositions(node);
        ctx.fillStyle = '#025ca2';
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 1;
        
        ports.forEach(p => {
            ctx.beginPath();
            ctx.rect(p.x - 3, p.y - 3, 6, 6);
            ctx.fill();
            ctx.stroke();
        });
        
        // Node Text Labels
        // 1. Temperature
        ctx.fillStyle = '#111827';
        ctx.font = 'bold 11px "Segoe UI"';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(node.temp.toFixed(1) + ' °C', node.x, node.y - 2);
        
        // 2. Name
        ctx.fillStyle = '#374151';
        ctx.font = '9px "Segoe UI"';
        ctx.fillText(node.name, node.x, node.y + 12);
        
        // Heat Source Generation overlay tag
        if (node.q_gen > 0) {
            ctx.fillStyle = '#b91c1c';
            ctx.font = '8px Consolas';
            let qStr = (node.q_gen >= 1000 ? (node.q_gen/1000).toFixed(1) + ' kW' : node.q_gen.toFixed(0) + ' W');
            ctx.fillText("Q: " + qStr, node.x, node.y - h/2 - 6);
        }
    });
    
    // Draw temporary connection line
    if (linkingStartNode && tempMousePos) {
        const tempTargetNode = { id: 0, name: "", x: tempMousePos.x, y: tempMousePos.y, temp: 0.0, capacity: 0.0, q_gen: 0.0, is_fixed: false };
        const portA = getClosestPort(linkingStartNode, tempTargetNode);
        
        const previewRoute = [];
        previewRoute.push(portA);
        
        if (portA.name === 'R' || portA.name === 'L') {
            const midX = 0.5 * (portA.x + tempMousePos.x);
            previewRoute.push({ x: midX, y: portA.y });
            previewRoute.push({ x: midX, y: tempMousePos.y });
        } else {
            const midY = 0.5 * (portA.y + tempMousePos.y);
            previewRoute.push({ x: portA.x, y: midY });
            previewRoute.push({ x: tempMousePos.x, y: midY });
        }
        previewRoute.push(tempMousePos);
        
        ctx.beginPath();
        ctx.moveTo(previewRoute[0].x, previewRoute[0].y);
        for (let i = 1; i < previewRoute.length; ++i) {
            ctx.lineTo(previewRoute[i].x, previewRoute[i].y);
        }
        ctx.strokeStyle = '#025ca2';
        ctx.lineWidth = 1.5;
        ctx.setLineDash([4, 4]);
        ctx.stroke();
        ctx.setLineDash([]);
    }
}

// Port layout calculations
function getPortPositions(node) {
    const w = 70, h = 40;
    return [
        { name: 'T', x: node.x, y: node.y - h/2 }, // Top
        { name: 'B', x: node.x, y: node.y + h/2 }, // Bottom
        { name: 'L', x: node.x - w/2, y: node.y }, // Left
        { name: 'R', x: node.x + w/2, y: node.y }  // Right
    ];
}

function getClosestPort(nodeThis, nodeOther) {
    const ports = getPortPositions(nodeThis);
    let bestPort = ports[0];
    let minDist = Infinity;
    
    ports.forEach(p => {
        const dist = Math.hypot(p.x - nodeOther.x, p.y - nodeOther.y);
        if (dist < minDist) {
            minDist = dist;
            bestPort = p;
        }
    });
    return bestPort;
}

function computeOrthogonalRoute(portA, faceA, portB, faceB) {
    const route = [];
    route.push({ x: portA.x, y: portA.y });
    
    // Normal vectors
    let dirA = { x: 0, y: 0 };
    if (faceA === 'T') dirA = { x: 0, y: -1 };
    else if (faceA === 'B') dirA = { x: 0, y: 1 };
    else if (faceA === 'L') dirA = { x: -1, y: 0 };
    else if (faceA === 'R') dirA = { x: 1, y: 0 };
    
    let dirB = { x: 0, y: 0 };
    if (faceB === 'T') dirB = { x: 0, y: -1 };
    else if (faceB === 'B') dirB = { x: 0, y: 1 };
    else if (faceB === 'L') dirB = { x: -1, y: 0 };
    else if (faceB === 'R') dirB = { x: 1, y: 0 };
    
    const clearance = 15.0;
    const s1 = { x: portA.x + dirA.x * clearance, y: portA.y + dirA.y * clearance };
    const s2 = { x: portB.x + dirB.x * clearance, y: portB.y + dirB.y * clearance };
    
    route.push(s1);
    
    // Check orientation cases
    const isParallel = (dirA.x * dirB.x + dirA.y * dirB.y) !== 0.0;
    const isOpposing = (dirA.x * dirB.x + dirA.y * dirB.y) < -0.9;
    
    if (isParallel) {
        if (dirA.x !== 0.0) { // Horizontal exits (Left/Right)
            if (isOpposing) {
                if ((dirA.x > 0.0 && s1.x <= s2.x) || (dirA.x < 0.0 && s1.x >= s2.x)) {
                    const midX = 0.5 * (s1.x + s2.x);
                    route.push({ x: midX, y: s1.y });
                    route.push({ x: midX, y: s2.y });
                } else {
                    let midY = 0.5 * (s1.y + s2.y);
                    if (Math.abs(s1.y - s2.y) < 40.0) {
                        midY = s1.y + (s1.y > s2.y ? 40.0 : -40.0);
                    }
                    route.push({ x: s1.x, y: midY });
                    route.push({ x: s2.x, y: midY });
                }
            } else { // Same direction
                const xOut = (dirA.x > 0.0) ? Math.max(s1.x, s2.x) : Math.min(s1.x, s2.x);
                route.push({ x: xOut, y: s1.y });
                route.push({ x: xOut, y: s2.y });
            }
        } else { // Vertical exits (Top/Bottom)
            if (isOpposing) {
                if ((dirA.y > 0.0 && s1.y <= s2.y) || (dirA.y < 0.0 && s1.y >= s2.y)) {
                    const midY = 0.5 * (s1.y + s2.y);
                    route.push({ x: s1.x, y: midY });
                    route.push({ x: s2.x, y: midY });
                } else {
                    let midX = 0.5 * (s1.x + s2.x);
                    if (Math.abs(s1.x - s2.x) < 70.0) {
                        midX = s1.x + (s1.x > s2.x ? 70.0 : -70.0);
                    }
                    route.push({ x: midX, y: s1.y });
                    route.push({ x: midX, y: s2.y });
                }
            } else { // Same direction
                const yOut = (dirA.y > 0.0) ? Math.max(s1.y, s2.y) : Math.min(s1.y, s2.y);
                route.push({ x: s1.x, y: yOut });
                route.push({ x: s2.x, y: yOut });
            }
        }
    } else { // Perpendicular exits
        if (dirA.x !== 0.0) {
            route.push({ x: s2.x, y: s1.y });
        } else {
            route.push({ x: s1.x, y: s2.y });
        }
    }
    
    route.push(s2);
    route.push({ x: portB.x, y: portB.y });
    
    // Clean collinear and duplicate points
    const cleanRoute = [];
    for (const pt of route) {
        if (cleanRoute.length === 0) {
            cleanRoute.push(pt);
        } else {
            const last = cleanRoute[cleanRoute.length - 1];
            if (Math.abs(last.x - pt.x) < 0.1 && Math.abs(last.y - pt.y) < 0.1) {
                continue;
            }
            if (cleanRoute.length >= 2) {
                const prev = cleanRoute[cleanRoute.length - 2];
                const is_h = (Math.abs(prev.y - last.y) < 0.1 && Math.abs(last.y - pt.y) < 0.1);
                const is_v = (Math.abs(prev.x - last.x) < 0.1 && Math.abs(last.x - pt.x) < 0.1);
                if (is_h || is_v) {
                    cleanRoute.pop();
                }
            }
            cleanRoute.push(pt);
        }
    }
    return cleanRoute;
}

// --- MATERIAL DATABASE ---
const materials = [
    { name: "Custom", density: 0.0, cp0: 0.0, cp1: 0.0, cp2: 0.0 },
    { name: "Copper", density: 8960.0, cp0: 385.0, cp1: 0.1, cp2: 0.0 },
    { name: "Aluminium", density: 2700.0, cp0: 900.0, cp1: 0.5, cp2: 0.0 },
    { name: "Steel", density: 7850.0, cp0: 450.0, cp1: 0.2, cp2: 0.0 },
    { name: "Cast Iron", density: 7200.0, cp0: 460.0, cp1: 0.25, cp2: 0.0 },
    { name: "Silicon", density: 2330.0, cp0: 700.0, cp1: 0.3, cp2: 0.0 }
];

function syncNodeProperties(node) {
    if (node.domain === undefined) node.domain = 0;
    if (node.domain === 1) { // Fluid Domain
        if (node.fluid_medium === undefined) node.fluid_medium = "Water";
        if (node.fluid_volume === undefined) node.fluid_volume = 1.0;
        
        const T_c = node.temp;
        const medium = node.fluid_medium;
        const V_L = node.fluid_volume;
        const V_m3 = V_L * 1e-3;
        
        let rho = 0.0, drho = 0.0, cp = 0.0, dcp = 0.0;
        if (medium === "Water") {
            rho = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
            drho = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
            cp = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
            dcp = -0.09 + 0.012 * T_c;
        } else if (medium === "Glycol") {
            rho = 1060.0 - 0.65 * T_c;
            drho = -0.65;
            cp = 3300.0 + 3.5 * T_c;
            dcp = 3.5;
        } else if (medium === "Oil") {
            rho = 890.0 - 0.60 * T_c;
            drho = -0.60;
            cp = 1800.0 + 3.0 * T_c;
            dcp = 3.0;
        } else if (medium === "Air") {
            let T_k = T_c + 273.15;
            let denom = (T_k < 10.0) ? 10.0 : T_k;
            rho = 353.18295 / denom;
            drho = -353.18295 / (denom * denom);
            cp = 1005.0 + 0.05 * T_c;
            dcp = 0.05;
        } else if (medium === "Mixture") {
            const r = node.fluid_mix_ratio !== undefined ? node.fluid_mix_ratio : 0.5;
            const ir = 1.0 - r;
            
            // Water properties
            const rho_w = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
            const drho_w = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
            const cp_w = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
            const dcp_w = -0.09 + 0.012 * T_c;
            
            // Pure Glycol properties
            const rho_g = 1060.0 - 0.65 * T_c;
            const drho_g = -0.65;
            const cp_g = 3300.0 + 3.5 * T_c;
            const dcp_g = 3.5;
            
            rho = ir * rho_w + r * rho_g;
            drho = ir * drho_w + r * drho_g;
            cp = ir * cp_w + r * cp_g;
            dcp = ir * dcp_w + r * dcp_g;
        } else if (medium === "Custom") {
            const rho_a0 = node.fluid_rho_a0 !== undefined ? node.fluid_rho_a0 : 1000.0;
            const rho_a1 = node.fluid_rho_a1 !== undefined ? node.fluid_rho_a1 : 0.0;
            const rho_a2 = node.fluid_rho_a2 !== undefined ? node.fluid_rho_a2 : 0.0;
            
            const cp_a0 = node.fluid_cp_a0 !== undefined ? node.fluid_cp_a0 : 4184.0;
            const cp_a1 = node.fluid_cp_a1 !== undefined ? node.fluid_cp_a1 : 0.0;
            const cp_a2 = node.fluid_cp_a2 !== undefined ? node.fluid_cp_a2 : 0.0;
            
            rho = rho_a0 + rho_a1 * T_c + rho_a2 * T_c * T_c;
            drho = rho_a1 + 2.0 * rho_a2 * T_c;
            cp = cp_a0 + cp_a1 * T_c + cp_a2 * T_c * T_c;
            dcp = cp_a1 + 2.0 * cp_a2 * T_c;
        } else {
            rho = 1000.0;
            drho = 0.0;
            cp = 4184.0;
            dcp = 0.0;
        }
        
        node.capacity = rho * V_m3 * cp;
        if (node.capacity < 0.1) node.capacity = 0.1;
        node.c_a1 = V_m3 * (rho * dcp + cp * drho);
        node.c_a2 = 0.0;
    } else { // Solid Domain
        updateNodeCapacityFromMaterial(node);
    }
}

function updateNodeCapacityFromMaterial(node) {
    if (!node.material) node.material = "Custom";
    if (node.mass === undefined) node.mass = 1.0;
    if (node.material === "Custom") return;
    const mat = materials.find(m => m.name === node.material);
    if (mat) {
        node.capacity = node.mass * mat.cp0;
        node.c_a1 = node.mass * mat.cp1;
        node.c_a2 = node.mass * mat.cp2;
    }
}

function sanitizeModel() {
    nodes.forEach(n => {
        if (n.domain === undefined) n.domain = 0;
        if (n.fluid_medium === undefined) n.fluid_medium = "Water";
        if (n.fluid_volume === undefined) n.fluid_volume = 1.0;
        if (n.fluid_mix_ratio === undefined) n.fluid_mix_ratio = 0.5;
        if (n.fluid_rho_a0 === undefined) n.fluid_rho_a0 = 1000.0;
        if (n.fluid_rho_a1 === undefined) n.fluid_rho_a1 = 0.0;
        if (n.fluid_rho_a2 === undefined) n.fluid_rho_a2 = 0.0;
        if (n.fluid_cp_a0 === undefined) n.fluid_cp_a0 = 4184.0;
        if (n.fluid_cp_a1 === undefined) n.fluid_cp_a1 = 0.0;
        if (n.fluid_cp_a2 === undefined) n.fluid_cp_a2 = 0.0;
        if (!n.material) n.material = "Custom";
        if (n.mass === undefined) n.mass = 1.0;
        if (n.c_a1 === undefined) n.c_a1 = 0.0;
        if (n.c_a2 === undefined) n.c_a2 = 0.0;
        syncNodeProperties(n);
    });
    links.forEach(l => {
        if (l.g_a1 === undefined) l.g_a1 = 0.0;
        if (l.g_a2 === undefined) l.g_a2 = 0.0;
        if (l.fan_area === undefined) l.fan_area = 0.005;
    });
}

// --- INTERACTIVE ACTIONS & MOUSE EVENTS ---

function setupEventHandlers() {
    // Toolbar buttons
    document.getElementById('btn-tb-new').addEventListener('click', () => {
        if(confirm("Create new blank model? Any unsaved layout changes will be lost.")) {
            clearWorkspace();
            logInfo("New model project created.");
        }
    });
    document.getElementById('btn-tb-clear').addEventListener('click', clearWorkspace);
    
    document.getElementById('preset-select').addEventListener('change', (e) => {
        loadPreset(e.target.value);
    });
    
    document.getElementById('solver-select').addEventListener('change', (e) => {
        solverType = e.target.value;
    });
    
    document.getElementById('step-size').addEventListener('change', (e) => {
        timeStep = parseFloat(e.target.value) || 0.05;
    });
    
    document.getElementById('btn-snap').addEventListener('click', (e) => {
        gridSnap = !gridSnap;
        e.target.classList.toggle('active', gridSnap);
        e.target.innerText = gridSnap ? "🎯 Grid Snap: ON" : "🎯 Grid Snap: OFF";
        logInfo(`Grid snapping toggled ${gridSnap ? 'ON' : 'OFF'}.`);
    });
    
    document.getElementById('btn-export').addEventListener('click', exportCSV);
    document.getElementById('btn-clear-console').addEventListener('click', () => {
        document.getElementById('console-output').innerHTML = '';
    });
    document.getElementById('btn-clear-chart').addEventListener('click', () => {
        timeHistory = [];
        tempHistory = {};
        rebuildDatasets();
    });

    // Playback control
    document.getElementById('btn-play').addEventListener('click', startSimulation);
    document.getElementById('btn-pause').addEventListener('click', pauseSimulation);
    document.getElementById('btn-step').addEventListener('click', async () => {
        isRunning = true;
        await runSimulationStep();
        isRunning = false;
    });
    document.getElementById('btn-steady').addEventListener('click', runSteadyStateSolve);
    document.getElementById('btn-reset').addEventListener('click', resetSimulation);

    // Tools
    document.getElementById('tool-select').addEventListener('click', () => setTool('select'));
    document.getElementById('tool-link').addEventListener('click', () => setTool('link'));

    // Canvas Mouse listeners
    canvas.addEventListener('mousedown', onMouseDown);
    canvas.addEventListener('mousemove', onMouseMove);
    canvas.addEventListener('mouseup', onMouseUp);
    canvas.addEventListener('dblclick', onDoubleClick);
    
    // Delete key
    window.addEventListener('keydown', (e) => {
        if (e.key === 'Delete' && selectedElement) {
            deleteSelected();
        }
    });

    // Modal
    document.getElementById('btn-modal-cancel').addEventListener('click', () => {
        document.getElementById('add-link-overlay').classList.add('hidden');
        linkingStartNode = null;
        draw();
    });
    document.getElementById('btn-modal-create').addEventListener('click', createLinkFromModal);
    
    document.getElementById('modal-link-type').addEventListener('change', (e) => {
        const type = parseInt(e.target.value);
        const labelP1 = document.getElementById('label-p1');
        const grpP2 = document.getElementById('group-p2');
        if (type === 0) {
            labelP1.innerText = "Thermal Conductance (W/K)";
            grpP2.classList.add('hidden');
        } else if (type === 1) {
            labelP1.innerText = "Convection hA (W/K)";
            grpP2.classList.add('hidden');
        } else if (type === 2) {
            labelP1.innerText = "Radiation G_rad (sigma*epsilon*A)";
            grpP2.classList.add('hidden');
        } else if (type === 3) {
            labelP1.innerText = "Coolant Flow m_dot*cp (W/K)";
            grpP2.classList.remove('hidden');
        } else if (type === 4) {
            labelP1.innerText = "Shut-off Pressure P_max (Pa)";
            grpP2.classList.remove('hidden');
        }
    });
}

function setTool(toolName) {
    currentTool = toolName;
    document.getElementById('tool-select').classList.toggle('active', toolName === 'select');
    document.getElementById('tool-link').classList.toggle('active', toolName === 'link');
}

function getMousePos(evt) {
    const rect = canvas.getBoundingClientRect();
    return {
        x: evt.clientX - rect.left,
        y: evt.clientY - rect.top
    };
}

function getNodeAt(pos) {
    // Check rectangle boundary (70x40)
    const w = 70, h = 40;
    return nodes.find(n => {
        return pos.x >= n.x - w/2 && pos.x <= n.x + w/2 &&
               pos.y >= n.y - h/2 && pos.y <= n.y + h/2;
    });
}

function getLinkAt(pos) {
    return links.find(l => {
        const nodeA = nodes.find(n => n.id === l.node_a);
        const nodeB = nodes.find(n => n.id === l.node_b);
        if (!nodeA || !nodeB) return false;
        
        const portA = getClosestPort(nodeA, nodeB);
        const portB = getClosestPort(nodeB, nodeA);
        
        const route = computeOrthogonalRoute(portA, portA.name, portB, portB.name);
        let minDist = Infinity;
        for (let i = 0; i < route.length - 1; ++i) {
            const dist = getDistanceToSegment(pos, route[i], route[i+1]);
            if (dist < minDist) {
                minDist = dist;
            }
        }
        return minDist <= 10;
    });
}

function getDistanceToSegment(p, a, b) {
    const l2 = Math.hypot(a.x - b.x, a.y - b.y) ** 2;
    if (l2 === 0) return Math.hypot(p.x - a.x, p.y - a.y);
    let t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
    t = Math.max(0, Math.min(1, t));
    return Math.hypot(p.x - (a.x + t * (b.x - a.x)), p.y - (a.y + t * (b.y - a.y)));
}

function onMouseDown(e) {
    const mousePos = getMousePos(e);
    const node = getNodeAt(mousePos);
    
    if (currentTool === 'select') {
        if (node) {
            isDragging = true;
            dragNode = node;
            dragOffset = { x: mousePos.x - node.x, y: mousePos.y - node.y };
            selectElement('node', node.id);
        } else {
            const link = getLinkAt(mousePos);
            if (link) {
                selectElement('link', link.id);
            } else {
                deselect();
            }
        }
    } else if (currentTool === 'link') {
        if (node) {
            linkingStartNode = node;
            tempMousePos = mousePos;
        }
    }
}

function onMouseMove(e) {
    const mousePos = getMousePos(e);
    
    if (isDragging && dragNode) {
        let targetX = mousePos.x - dragOffset.x;
        let targetY = mousePos.y - dragOffset.y;
        
        if (gridSnap) {
            targetX = Math.round(targetX / GRID_SIZE) * GRID_SIZE;
            targetY = Math.round(targetY / GRID_SIZE) * GRID_SIZE;
        }
        
        dragNode.x = targetX;
        dragNode.y = targetY;
        draw();
    } else if (linkingStartNode) {
        tempMousePos = mousePos;
        draw();
    }
}

function onMouseUp(e) {
    if (isDragging) {
        isDragging = false;
        dragNode = null;
    }
    
    if (linkingStartNode) {
        const mousePos = getMousePos(e);
        const endNode = getNodeAt(mousePos);
        
        if (endNode && endNode.id !== linkingStartNode.id) {
            // Check if link already exists
            const duplicate = links.find(l => 
                (l.node_a === linkingStartNode.id && l.node_b === endNode.id) ||
                (l.node_b === linkingStartNode.id && l.node_a === endNode.id)
            );
            
            if (duplicate) {
                logWarning("A connection link already exists between these components.");
                linkingStartNode = null;
                draw();
                return;
            }
            
            // Show config modal
            const modalOverlay = document.getElementById('add-link-overlay');
            modalOverlay.classList.remove('hidden');
            document.getElementById('modal-link-p1').value = 10;
            document.getElementById('modal-link-type').value = 0;
            document.getElementById('label-p1').innerText = "Thermal Conductance (W/K)";
            document.getElementById('group-p2').classList.add('hidden');
        } else {
            linkingStartNode = null;
        }
        draw();
    }
}

function onDoubleClick(e) {
    const mousePos = getMousePos(e);
    const node = getNodeAt(mousePos);
    
    if (node) {
        selectElement('node', node.id);
    } else {
        // Create new Node block
        let x = mousePos.x;
        let y = mousePos.y;
        if (gridSnap) {
            x = Math.round(x / GRID_SIZE) * GRID_SIZE;
            y = Math.round(y / GRID_SIZE) * GRID_SIZE;
        }
        
        const nextId = nodes.length > 0 ? Math.max(...nodes.map(n => n.id)) + 1 : 1;
        const newNode = {
            id: nextId,
            name: `Mass ${nextId}`,
            x: x,
            y: y,
            temp: 25.0,
            capacity: 500.0,
            q_gen: 0.0,
            is_fixed: false
        };
        
        nodes.push(newNode);
        initSystemOnServer();
        resetChartData();
        selectElement('node', nextId);
        logInfo(`Created component [Mass ${nextId}] at coordinate (${x}, ${y}).`);
        draw();
    }
}

function createLinkFromModal() {
    if (!linkingStartNode) return;
    
    const mousePos = tempMousePos;
    const endNode = getNodeAt(mousePos);
    if (!endNode) return;
    
    const type = parseInt(document.getElementById('modal-link-type').value);
    const p1 = parseFloat(document.getElementById('modal-link-p1').value) || (type === 4 ? 1000.0 : 1.0);
    const p2 = type === 3 ? parseFloat(document.getElementById('modal-link-p2').value) : (type === 4 ? 10.0 * parseFloat(document.getElementById('modal-link-p2').value) : 0.0);
    
    const nextId = links.length > 0 ? Math.max(...links.map(l => l.id)) + 1 : 101;
    const newLink = {
        id: nextId,
        node_a: linkingStartNode.id,
        node_b: endNode.id,
        type: type,
        p1: p1,
        p2: p2,
        fan_area: type === 4 ? 0.005 : undefined
    };
    
    links.push(newLink);
    document.getElementById('add-link-overlay').classList.add('hidden');
    linkingStartNode = null;
    
    initSystemOnServer();
    selectElement('link', nextId);
    logInfo(`Added link connection [ID: ${nextId}] connecting Node ${newLink.node_a} to Node ${newLink.node_b}.`);
    draw();
}

function selectElement(type, id) {
    if (type === 'node') {
        const node = nodes.find(n => n.id === id);
        if (node) {
            selectedElement = { type: 'node', id: id, data: node };
        }
    } else if (type === 'link') {
        const link = links.find(l => l.id === id);
        if (link) {
            selectedElement = { type: 'link', id: id, data: link };
        }
    }
    
    updateSpreadsheet();
    updateModelTree();
    draw();
    updateFanTabVisibility();
}

function deselect() {
    selectedElement = null;
    updateSpreadsheet();
    updateModelTree();
    draw();
    updateFanTabVisibility();
}

function deleteSelected() {
    if (!selectedElement) return;
    
    if (selectedElement.type === 'node') {
        const nodeId = selectedElement.id;
        const node = nodes.find(n => n.id === nodeId);
        nodes = nodes.filter(n => n.id !== nodeId);
        links = links.filter(l => l.node_a !== nodeId && l.node_b !== nodeId);
        logWarning(`Deleted component [${node ? node.name : nodeId}] and all its link connections.`);
    } else if (selectedElement.type === 'link') {
        const linkId = selectedElement.id;
        links = links.filter(l => l.id !== linkId);
        logWarning(`Deleted connection link [ID: ${linkId}].`);
    }
    
    selectedElement = null;
    initSystemOnServer();
    resetChartData();
    updateSpreadsheet();
    updateModelTree();
    draw();
}

// --- PROPERTIES SPREADSHEET EDITOR ---
function updateSpreadsheet() {
    const sBody = document.getElementById('spreadsheet-body');
    if (!selectedElement) {
        sBody.innerHTML = `
            <tr>
                <td colspan="4" class="empty-spreadsheet">No component selected. Click an element on the canvas to inspect its properties.</td>
            </tr>
        `;
        return;
    }
    
    sBody.innerHTML = '';
    
    if (selectedElement.type === 'node') {
        const node = nodes.find(n => n.id === selectedElement.id);
        if (!node) {
            deselect();
            return;
        }
        
        if (node.domain === undefined) node.domain = 0;
        if (node.fluid_medium === undefined) node.fluid_medium = "Water";
        if (node.fluid_volume === undefined) node.fluid_volume = 1.0;
        if (!node.material) node.material = "Custom";
        if (node.mass === undefined) node.mass = 1.0;
        if (node.c_a1 === undefined) node.c_a1 = 0.0;
        if (node.c_a2 === undefined) node.c_a2 = 0.0;

        syncNodeProperties(node);

        const isReadOnly = (node.domain === 1 || node.material !== 'Custom');
        const isCustomSolid = (node.domain === 0 && node.material === 'Custom');
        
        const attributes = [
            { key: 'id', label: 'Component ID', val: node.id, unit: '-', desc: 'Unique identifier', type: 'readonly' },
            { key: 'name', label: 'Label Name', val: node.name, unit: '-', desc: 'Display label', type: 'text' },
            { key: 'temp', label: 'Temperature', val: node.temp, unit: '°C', desc: 'Current temperature', type: 'number', step: 0.1 },
            { 
                key: 'domain', 
                label: 'Domain', 
                val: node.domain === 1 ? 'Fluid' : 'Solid', 
                unit: '-', 
                desc: 'Node physics domain', 
                type: 'select', 
                options: ['Solid', 'Fluid'] 
            }
        ];
        
        if (node.domain === 0) {
            attributes.push({ 
                key: 'material', 
                label: 'Material', 
                val: node.material, 
                unit: '-', 
                desc: 'Material from database', 
                type: 'select', 
                options: materials.map(m => m.name) 
            });
            if (node.material !== 'Custom') {
                attributes.push({ 
                    key: 'mass', 
                    label: 'Mass', 
                    val: node.mass, 
                    unit: 'kg', 
                    desc: 'Mass of component', 
                    type: 'number', 
                    step: 0.1 
                });
            }
        } else {
            attributes.push({ 
                key: 'fluid_medium', 
                label: 'Fluid Medium', 
                val: node.fluid_medium, 
                unit: '-', 
                desc: 'Coolant fluid media', 
                type: 'select', 
                options: ['Water', 'Glycol', 'Oil', 'Air', 'Mixture', 'Custom'] 
            });
            attributes.push({ 
                key: 'fluid_volume', 
                label: 'Fluid Volume', 
                val: node.fluid_volume, 
                unit: 'L', 
                desc: 'Volume of fluid in liters', 
                type: 'number', 
                step: 0.1 
            });
            if (node.fluid_medium === "Mixture") {
                attributes.push({
                    key: 'fluid_mix_ratio',
                    label: 'Glycol Concentration',
                    val: Math.round((node.fluid_mix_ratio !== undefined ? node.fluid_mix_ratio : 0.5) * 100.0),
                    unit: '%',
                    desc: 'Water-Glycol ratio (0% = Water, 100% = Glycol)',
                    type: 'slider',
                    min: 0,
                    max: 100,
                    step: 1
                });
            } else if (node.fluid_medium === "Custom") {
                attributes.push({ key: 'fluid_rho_a0', label: 'Density Coeff a0', val: node.fluid_rho_a0 !== undefined ? node.fluid_rho_a0 : 1000.0, unit: 'kg/m³', desc: 'Constant density term', type: 'number', step: 1.0 });
                attributes.push({ key: 'fluid_rho_a1', label: 'Density Coeff a1', val: node.fluid_rho_a1 !== undefined ? node.fluid_rho_a1 : 0.0, unit: 'kg/m³/K', desc: 'Linear density term', type: 'number', step: 0.01 });
                attributes.push({ key: 'fluid_rho_a2', label: 'Density Coeff a2', val: node.fluid_rho_a2 !== undefined ? node.fluid_rho_a2 : 0.0, unit: 'kg/m³/K²', desc: 'Quadratic density term', type: 'number', step: 0.0001 });
                attributes.push({ key: 'fluid_cp_a0', label: 'Cp Coeff a0', val: node.fluid_cp_a0 !== undefined ? node.fluid_cp_a0 : 4184.0, unit: 'J/kgK', desc: 'Constant cp term', type: 'number', step: 1.0 });
                attributes.push({ key: 'fluid_cp_a1', label: 'Cp Coeff a1', val: node.fluid_cp_a1 !== undefined ? node.fluid_cp_a1 : 0.0, unit: 'J/kgK²', desc: 'Linear cp term', type: 'number', step: 0.01 });
                attributes.push({ key: 'fluid_cp_a2', label: 'Cp Coeff a2', val: node.fluid_cp_a2 !== undefined ? node.fluid_cp_a2 : 0.0, unit: 'J/kgK³', desc: 'Quadratic cp term', type: 'number', step: 0.0001 });
            }
        }
        
        attributes.push({
            key: 'capacity',
            label: 'Heat Capacity',
            val: isReadOnly ? parseFloat(node.capacity).toFixed(1) : node.capacity,
            unit: 'J/K',
            desc: node.domain === 1 ? 'Calculated capacity (rho*V*cp)' : (!isCustomSolid ? 'Calculated capacity (m*cp0)' : 'Constant capacity term a0'),
            type: isReadOnly ? 'readonly' : 'number',
            step: 1.0
        });

        attributes.push({
            key: 'c_a1',
            label: 'Cap Temp Coeff a1',
            val: isReadOnly ? parseFloat(node.c_a1).toFixed(4) : node.c_a1,
            unit: 'J/K²',
            desc: node.domain === 1 ? 'Calculated coefficient (dC/dT)' : (!isCustomSolid ? 'Calculated coefficient (m*cp1)' : 'Linear temp coefficient a1'),
            type: isReadOnly ? 'readonly' : 'number',
            step: 0.0001
        });

        attributes.push({
            key: 'c_a2',
            label: 'Cap Temp Coeff a2',
            val: isReadOnly ? parseFloat(node.c_a2).toFixed(6) : node.c_a2,
            unit: 'J/K³',
            desc: node.domain === 1 ? 'Zero (Quadratic term)' : (!isCustomSolid ? 'Calculated coefficient (m*cp2)' : 'Quadratic temp coefficient a2'),
            type: isReadOnly ? 'readonly' : 'number',
            step: 0.000001
        });

        attributes.push({ key: 'q_gen', label: 'Heat Source Generation', val: node.q_gen, unit: 'W', desc: 'Internal heat power generation (+Q)', type: 'number', step: 1.0 });
        attributes.push({ key: 'is_fixed', label: 'Fixed Temperature', val: node.is_fixed, unit: 'bool', desc: 'Boundary condition constraint', type: 'checkbox' });
        
        attributes.forEach(attr => {
            const tr = document.createElement('tr');
            
            let valHtml = '';
            if (attr.type === 'readonly') {
                valHtml = `<span id="ss-val-${attr.key}">${attr.val}</span>`;
            } else if (attr.type === 'text') {
                valHtml = `<input type="text" value="${attr.val}" id="ss-val-${attr.key}">`;
            } else if (attr.type === 'number') {
                valHtml = `<input type="number" value="${attr.val}" step="${attr.step}" id="ss-val-${attr.key}">`;
            } else if (attr.type === 'checkbox') {
                valHtml = `<input type="checkbox" ${attr.val ? 'checked' : ''} id="ss-val-${attr.key}">`;
            } else if (attr.type === 'select') {
                valHtml = `<select id="ss-val-${attr.key}">`;
                attr.options.forEach(opt => {
                    valHtml += `<option value="${opt}" ${attr.val === opt ? 'selected' : ''}>${opt}</option>`;
                });
                valHtml += `</select>`;
            } else if (attr.type === 'slider') {
                valHtml = `<div style="display: flex; align-items: center; gap: 8px; width: 100%;">
                             <input type="range" min="${attr.min}" max="${attr.max}" step="${attr.step}" value="${attr.val}" id="ss-val-${attr.key}" style="flex: 1; accent-color: #025ca2;">
                             <span id="ss-val-${attr.key}-display" style="min-width: 38px; text-align: right; font-family: var(--font-mono); font-size: 11px;">${attr.val}%</span>
                           </div>`;
            }
            
            tr.innerHTML = `
                <td style="font-weight: 600;">${attr.label}</td>
                <td>${valHtml}</td>
                <td style="color: #64748b; font-family: var(--font-mono);">${attr.unit}</td>
                <td style="color: #64748b;">${attr.desc}</td>
            `;
            sBody.appendChild(tr);
            
            // Event bindings
            const inputEl = document.getElementById(`ss-val-${attr.key}`);
            if (inputEl) {
                const updateEvent = (attr.type === 'checkbox' || attr.type === 'select') ? 'change' : 
                                    (attr.type === 'slider' ? 'input' : 'input');
                inputEl.addEventListener(updateEvent, (e) => {
                    let newVal = attr.type === 'checkbox' ? e.target.checked : e.target.value;
                    if (attr.type === 'number') newVal = parseFloat(newVal) || 0.0;
                    if (attr.type === 'slider') {
                        newVal = parseFloat(newVal) || 0.0;
                        const disp = document.getElementById(`ss-val-${attr.key}-display`);
                        if (disp) disp.innerText = newVal + '%';
                        newVal = newVal / 100.0;
                    }
                    
                    // Update state
                    if (attr.key === 'domain') {
                        node.domain = (newVal === 'Fluid') ? 1 : 0;
                        syncNodeProperties(node);
                        initSystemOnServer();
                        updateSpreadsheet(); // Rebuild spreadsheet
                    } else if (attr.key === 'material') {
                        node.material = newVal;
                        syncNodeProperties(node);
                        initSystemOnServer();
                        updateSpreadsheet(); // Rebuild spreadsheet
                    } else if (attr.key === 'fluid_medium') {
                        node.fluid_medium = newVal;
                        syncNodeProperties(node);
                        initSystemOnServer();
                        updateSpreadsheet(); // Rebuild spreadsheet
                    } else {
                        node[attr.key] = newVal;
                        const isPhysParam = attr.key === 'fluid_volume' || attr.key === 'mass' || attr.key === 'temp' || 
                                            attr.key === 'fluid_mix_ratio' || attr.key.startsWith('fluid_rho_') || attr.key.startsWith('fluid_cp_');
                        if (isPhysParam) {
                            syncNodeProperties(node);
                            // Update readonly values in DOM directly to prevent focus loss
                            const capEl = document.getElementById('ss-val-capacity');
                            const ca1El = document.getElementById('ss-val-c_a1');
                            const ca2El = document.getElementById('ss-val-c_a2');
                            if (capEl) capEl.innerText = node.capacity.toFixed(1);
                            if (ca1El) ca1El.innerText = node.c_a1.toFixed(4);
                            if (ca2El) ca2El.innerText = node.c_a2.toFixed(6);
                        } else if (attr.key === 'capacity' || attr.key === 'c_a1' || attr.key === 'c_a2') {
                            // User modified custom values
                            initSystemOnServer();
                        } else if (attr.key === 'name') {
                            // Refresh tree and chart legends
                            updateModelTree();
                            resetChartData();
                        }
                    }
                    draw();
                });
                
                // Add a change event listener specifically for inputs to sync with solver when editing finishes
                const isSolverTrigger = attr.key === 'mass' || attr.key === 'fluid_volume' || attr.key === 'temp' || 
                                        attr.key === 'fluid_mix_ratio' || attr.key.startsWith('fluid_rho_') || attr.key.startsWith('fluid_cp_');
                if (isSolverTrigger) {
                    inputEl.addEventListener('change', () => {
                        initSystemOnServer();
                    });
                }
            }
        });
        
    } else if (selectedElement.type === 'link') {
        const link = links.find(l => l.id === selectedElement.id);
        if (!link) {
            deselect();
            return;
        }
        
        let p1Label = "Conductance";
        let p1Unit = "W/K";
        let p1Desc = "Thermal transmission conductance";
        
        if (link.type === 1) {
            p1Label = "Convection Coefficient (hA)";
            p1Unit = "W/K";
            p1Desc = "Convection heat transfer coefficient";
        } else if (link.type === 2) {
            p1Label = "Radiation Parameter (sigma*eps*A)";
            p1Unit = "W/K^4";
            p1Desc = "Stefan-Boltzmann radiation exchange multiplier";
        } else if (link.type === 3) {
            const upNodeId = (link.p2 >= 0.0) ? link.node_a : link.node_b;
            const upNode = nodes.find(n => n.id === upNodeId);
            if (upNode && upNode.domain === 1) {
                p1Label = "Volumetric Flow Rate";
                p1Unit = "L/min";
                p1Desc = "Volumetric coolant flow rate";
            } else {
                p1Label = "Fluid Capacity Flow Rate (m_dot*cp)";
                p1Unit = "W/K";
                p1Desc = "Flow mass rate * specific heat";
            }
        }
        
        const attributes = [
            { key: 'id', label: 'Connection ID', val: link.id, unit: '-', desc: 'Unique connection identifier', type: 'readonly' },
            { key: 'nodes', label: 'Coupled Nodes', val: `Node ${link.node_a} ➔ Node ${link.node_b}`, unit: '-', desc: 'Connects ports', type: 'readonly' },
            { key: 'type', label: 'Physics Type', val: link.type, unit: '-', desc: '0:Cond, 1:Conv, 2:Rad, 3:Flow', type: 'select' },
            { key: 'p1', label: p1Label, val: link.p1, unit: p1Unit, desc: p1Desc, type: 'number', step: 1.0 },
            { key: 'g_a1', label: `${p1Label} Coeff a1`, val: link.g_a1 || 0.0, unit: `${p1Unit}/K`, desc: 'Linear temp coefficient a1', type: 'number', step: 0.0001 },
            { key: 'g_a2', label: `${p1Label} Coeff a2`, val: link.g_a2 || 0.0, unit: `${p1Unit}/K²`, desc: 'Quadratic temp coefficient a2', type: 'number', step: 0.000001 }
        ];
        
        if (link.type === 3) {
            attributes.push({ key: 'p2', label: 'Flow Direction', val: link.p2, unit: 'direction', desc: '1.0: A->B, -1.0: B->A', type: 'direction' });
        } else if (link.type === 4) {
            attributes.push({ key: 'p2', label: 'Free Velocity (v_max)', val: link.p2, unit: 'm/s', desc: 'Maximum fluid velocity (sign sets flow direction)', type: 'number', step: 0.1 });
            attributes.push({ key: 'fan_area', label: 'Flow Area (A)', val: link.fan_area !== undefined ? link.fan_area : 0.005, unit: 'm²', desc: 'Cross-sectional flow area', type: 'number', step: 0.0001 });
        }
        
        attributes.forEach(attr => {
            const tr = document.createElement('tr');
            let valHtml = '';
            
            if (attr.type === 'readonly') {
                valHtml = `<span id="ss-val-${attr.key}">${attr.val}</span>`;
            } else if (attr.type === 'number') {
                valHtml = `<input type="number" value="${attr.val}" step="${attr.step}" id="ss-val-${attr.key}">`;
            } else if (attr.type === 'select') {
                valHtml = `
                    <select id="ss-val-${attr.key}">
                        <option value="0" ${attr.val === 0 ? 'selected' : ''}>0 - Conduction</option>
                        <option value="1" ${attr.val === 1 ? 'selected' : ''}>1 - Convection</option>
                        <option value="2" ${attr.val === 2 ? 'selected' : ''}>2 - Radiation</option>
                        <option value="3" ${attr.val === 3 ? 'selected' : ''}>3 - Coolant Flow</option>
                        <option value="4" ${attr.val === 4 ? 'selected' : ''}>4 - Fan / Pump</option>
                    </select>
                `;
            } else if (attr.type === 'direction') {
                valHtml = `
                    <select id="ss-val-${attr.key}">
                        <option value="1.0" ${attr.val === 1.0 ? 'selected' : ''}>Node A ➔ Node B</option>
                        <option value="-1.0" ${attr.val === -1.0 ? 'selected' : ''}>Node B ➔ Node A</option>
                    </select>
                `;
            }
            
            tr.innerHTML = `
                <td style="font-weight: 600;">${attr.label}</td>
                <td>${valHtml}</td>
                <td style="color: #64748b; font-family: var(--font-mono);">${attr.unit}</td>
                <td style="color: #64748b;">${attr.desc}</td>
            `;
            sBody.appendChild(tr);
            
            const inputEl = document.getElementById(`ss-val-${attr.key}`);
            if (inputEl) {
                inputEl.addEventListener('change', (e) => {
                    let newVal = parseFloat(e.target.value);
                    if (attr.key === 'type') {
                        link.type = parseInt(newVal);
                        if (link.type === 4) {
                            if (link.p1 === 0.0 || link.p1 === 10.0 || link.p1 === 400.0) link.p1 = 1000.0;
                            if (link.p2 === 0.0 || link.p2 === 1.0) link.p2 = 10.0;
                            if (link.g_a1 === undefined) link.g_a1 = 0.0;
                            if (link.g_a2 === undefined) link.g_a2 = 0.0;
                            if (link.fan_area === undefined) link.fan_area = 0.005;
                        }
                        initSystemOnServer();
                        updateSpreadsheet(); // Rebuild attributes view
                    } else {
                        link[attr.key] = newVal;
                        if (attr.key === 'g_a1' || attr.key === 'g_a2' || attr.key === 'p1' || attr.key === 'p2' || attr.key === 'fan_area') {
                            initSystemOnServer();
                            if (attr.key === 'p2' || attr.key === 'fan_area') {
                                updateSpreadsheet(); // Rebuild attributes view
                            }
                        }
                    }
                    draw();
                });
                
                inputEl.addEventListener('input', (e) => {
                    if (attr.type === 'number') {
                        link[attr.key] = parseFloat(e.target.value) || 0.0;
                        draw();
                    }
                });
            }
        });
    }
    updateFanTabVisibility();
}

// --- FAN MATCHER TAB LOGIC ---
let fanChart = null;

function updateFanTabVisibility() {
    const btn = document.getElementById('tab-btn-fan');
    if (!btn) return;
    
    const isFanSelected = selectedElement && selectedElement.type === 'link' && selectedElement.data && selectedElement.data.type === 4;
    if (isFanSelected) {
        btn.style.display = 'block';
        if (btn.classList.contains('active')) {
            updateFanMatching();
        }
    } else {
        btn.style.display = 'none';
        if (btn.classList.contains('active')) {
            // Switch back to attributes sheet
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            const attrBtn = document.querySelector('.tab-btn[data-tab="tab-props"]');
            if (attrBtn) attrBtn.classList.add('active');
            const attrContent = document.getElementById('tab-props');
            if (attrContent) attrContent.classList.add('active');
        }
    }
}

function updateFanMatching() {
    if (!selectedElement || selectedElement.type !== 'link') return;
    const link = selectedElement.data;
    if (link.type !== 4) return;
    
    const Pmax = Math.abs(link.p1);
    const Vmax = Math.abs(link.p2);
    const K = Math.abs(link.g_a1 || 0.0);
    const R = Math.abs(link.g_a2 || 0.0);
    const A = link.fan_area || 0.005;
    
    let v_oper = 0.0;
    let Q_lmin = 0.0;
    let P_oper = 0.0;
    
    if (Vmax > 0) {
        const B = Pmax / (Vmax * Vmax);
        v_oper = (-R + Math.sqrt(R * R + 4.0 * (K + B) * Pmax)) / (2.0 * (K + B));
        Q_lmin = v_oper * A * 60000.0;
        P_oper = Pmax - B * v_oper * v_oper;
    }
    
    // Update metric values in the UI
    const calcVEl = document.getElementById('fan-calc-v');
    const calcQEl = document.getElementById('fan-calc-q');
    const calcPEl = document.getElementById('fan-calc-p');
    
    if (calcVEl) calcVEl.innerText = (link.p2 >= 0 ? 1.0 : -1.0) * v_oper.toFixed(2) + ' m/s';
    if (calcQEl) calcQEl.innerText = (link.p2 >= 0 ? 1.0 : -1.0) * Q_lmin.toFixed(2) + ' L/min';
    if (calcPEl) calcPEl.innerText = P_oper.toFixed(1) + ' Pa';
    
    const ctxFan = document.getElementById('fan-matching-chart');
    if (!ctxFan) return;
    
    // Generate data points for plotting
    const fanPoints = [];
    const sysPoints = [];
    const limitV = Vmax > 0 ? Vmax * 1.25 : 10.0;
    const steps = 50;
    const B_val = Vmax > 0 ? Pmax / (Vmax * Vmax) : 0;
    
    for (let i = 0; i <= steps; i++) {
        const v = (limitV * i) / steps;
        const p_fan = Math.max(0.0, Pmax - B_val * v * v);
        const p_sys = K * v * v + R * v;
        fanPoints.push({ x: v, y: p_fan });
        sysPoints.push({ x: v, y: p_sys });
    }
    
    if (fanChart) {
        fanChart.destroy();
    }
    
    fanChart = new Chart(ctxFan.getContext('2d'), {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'Fan Pressure Rise (P_fan)',
                    data: fanPoints,
                    borderColor: '#7c3aed',
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false,
                    tension: 0.1
                },
                {
                    label: 'System Resistance (P_sys)',
                    data: sysPoints,
                    borderColor: '#10b981',
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: false,
                    tension: 0.1
                },
                {
                    label: 'Operating Point',
                    data: [{ x: v_oper, y: P_oper }],
                    borderColor: '#ef4444',
                    backgroundColor: '#ef4444',
                    pointRadius: 6,
                    pointHoverRadius: 8,
                    showLine: false
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            scales: {
                x: {
                    type: 'linear',
                    title: { display: true, text: 'Velocity (m/s)', color: '#4b5563', font: { size: 10 } },
                    grid: { color: 'rgba(0, 0, 0, 0.05)' },
                    ticks: { color: '#4b5563', font: { size: 9 } }
                },
                y: {
                    title: { display: true, text: 'Pressure (Pa)', color: '#4b5563', font: { size: 10 } },
                    grid: { color: 'rgba(0, 0, 0, 0.05)' },
                    ticks: { color: '#4b5563', font: { size: 9 } }
                }
            },
            plugins: {
                legend: {
                    display: true,
                    labels: {
                        boxWidth: 12,
                        font: { size: 9 }
                    }
                }
            }
        }
    });
}
