class AppState {
    constructor() {
        this.bgType = 'gradient';
        this.solidColor = '#1e293b';
        this.gradColor1 = '#3b82f6';
        this.gradColor2 = '#8b5cf6';
        this.gradAngle = 90;
        this.imageObj = null;
        this.canvasWidth = 800;
        this.canvasHeight = 500;
        this.showHeatmap = true;
        this.layers = [
            {
                id: 'layer-1',
                text: 'Accessible UI Header',
                color: '#ffffff',
                fontSize: 32,
                fontWeight: '700',
                x: 60,
                y: 80
            },
            {
                id: 'layer-2',
                text: 'Testing complex gradients & contrast ratios.',
                color: '#cbd5e1',
                fontSize: 16,
                fontWeight: '400',
                x: 60,
                y: 150
            }
        ];
        this.activeLayerId = 'layer-1';
    }
}

const state = new AppState();

// DOM Elements
const bgTypeSelect = document.getElementById('bg-type');
const controlSolid = document.getElementById('control-solid');
const controlGradient = document.getElementById('control-gradient');
const controlImage = document.getElementById('control-image');
const solidColorInput = document.getElementById('solid-color');
const solidColorText = document.getElementById('solid-color-text');
const gradColor1Input = document.getElementById('grad-color1');
const gradColor2Input = document.getElementById('grad-color2');
const gradAngleInput = document.getElementById('grad-angle');
const angleValSpan = document.getElementById('angle-val');
const imageFileInput = document.getElementById('image-file');
const canvasWidthInput = document.getElementById('canvas-width');
const canvasHeightInput = document.getElementById('canvas-height');
const toggleHeatmapCheck = document.getElementById('toggle-heatmap');

const bgCanvas = document.getElementById('bg-canvas');
const ctx = bgCanvas.getContext('2d', { willReadFrequently: true });
const overlaysContainer = document.getElementById('overlays-container');

const addTextBtn = document.getElementById('add-text-btn');
const layersListEl = document.getElementById('layers-list');
const layerPropertiesEl = document.getElementById('layer-properties');
const activeLayerIdInput = document.getElementById('active-layer-id');
const layerTextContent = document.getElementById('layer-text-content');
const layerTextColor = document.getElementById('layer-text-color');
const layerTextColorText = document.getElementById('layer-text-color-text');
const layerFontSize = document.getElementById('layer-font-size');
const layerFontWeight = document.getElementById('layer-font-weight');
const deleteLayerBtn = document.getElementById('delete-layer-btn');
const auditResultsContainer = document.getElementById('audit-results-container');
const exportReportBtn = document.getElementById('export-report-btn');

// Tabs navigation
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
        document.querySelectorAll('.tab-btn').forEach(b => {
            b.classList.remove('active');
            b.setAttribute('aria-selected', 'false');
        });
        document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));

        e.target.classList.add('active');
        e.target.setAttribute('aria-selected', 'true');
        const panelId = e.target.getAttribute('aria-controls');
        document.getElementById(panelId).classList.add('active');
    });
});

// Initialize UI controls from state
function initUI() {
    solidColorInput.value = state.solidColor;
    solidColorText.value = state.solidColor;
    gradColor1Input.value = state.gradColor1;
    gradColor2Input.value = state.gradColor2;
    gradAngleInput.value = state.gradAngle;
    angleValSpan.textContent = state.gradAngle;
    canvasWidthInput.value = state.canvasWidth;
    canvasHeightInput.value = state.canvasHeight;
    bgTypeSelect.value = state.bgType;

    updateSubControls();
    renderCanvas();
    renderLayersList();
    renderLayerProperties();
    runAudit();
}

function updateSubControls() {
    controlSolid.classList.add('hidden');
    controlGradient.classList.add('hidden');
    controlImage.classList.add('hidden');

    if (state.bgType === 'solid') controlSolid.classList.remove('hidden');
    if (state.bgType === 'gradient') controlGradient.classList.remove('hidden');
    if (state.bgType === 'image') controlImage.classList.remove('hidden');
}

// Background Type Change
bgTypeSelect.addEventListener('change', (e) => {
    state.bgType = e.target.value;
    updateSubControls();
    renderCanvas();
    runAudit();
});

solidColorInput.addEventListener('input', (e) => {
    state.solidColor = e.target.value;
    solidColorText.value = e.target.value;
    renderCanvas();
    runAudit();
});
solidColorText.addEventListener('input', (e) => {
    if (/^#[0-9A-Fa-f]{6}$/.test(e.target.value)) {
        state.solidColor = e.target.value;
        solidColorInput.value = e.target.value;
        renderCanvas();
        runAudit();
    }
});

gradColor1Input.addEventListener('input', (e) => {
    state.gradColor1 = e.target.value;
    renderCanvas();
    runAudit();
});
gradColor2Input.addEventListener('input', (e) => {
    state.gradColor2 = e.target.value;
    renderCanvas();
    runAudit();
});
gradAngleInput.addEventListener('input', (e) => {
    state.gradAngle = parseInt(e.target.value, 10);
    angleValSpan.textContent = state.gradAngle;
    renderCanvas();
    runAudit();
});

canvasWidthInput.addEventListener('change', (e) => {
    state.canvasWidth = Math.max(300, Math.min(1600, parseInt(e.target.value, 10) || 800));
    e.target.value = state.canvasWidth;
    renderCanvas();
    runAudit();
});
canvasHeightInput.addEventListener('change', (e) => {
    state.canvasHeight = Math.max(200, Math.min(1200, parseInt(e.target.value, 10) || 500));
    e.target.value = state.canvasHeight;
    renderCanvas();
    runAudit();
});

imageFileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (event) => {
        const img = new Image();
        img.onload = () => {
            state.imageObj = img;
            renderCanvas();
            runAudit();
        };
        img.src = event.target.result;
    };
    reader.readAsDataURL(file);
});

toggleHeatmapCheck.addEventListener('change', (e) => {
    state.showHeatmap = e.target.checked;
    renderCanvas();
});

// Canvas Rendering Pipeline
function renderCanvas() {
    bgCanvas.width = state.canvasWidth;
    bgCanvas.height = state.canvasHeight;
    overlaysContainer.style.width = state.canvasWidth + 'px';
    overlaysContainer.style.height = state.canvasHeight + 'px';

    ctx.clearRect(0, 0, state.canvasWidth, state.canvasHeight);

    if (state.bgType === 'solid') {
        ctx.fillStyle = state.solidColor;
        ctx.fillRect(0, 0, state.canvasWidth, state.canvasHeight);
    } else if (state.bgType === 'gradient') {
        const rad = (state.gradAngle * Math.PI) / 180;
        const x1 = state.canvasWidth / 2 - Math.cos(rad) * state.canvasWidth;
        const y1 = state.canvasHeight / 2 - Math.sin(rad) * state.canvasHeight;
        const x2 = state.canvasWidth / 2 + Math.cos(rad) * state.canvasWidth;
        const y2 = state.canvasHeight / 2 + Math.sin(rad) * state.canvasHeight;

        const gradient = ctx.createLinearGradient(x1, y1, x2, y2);
        gradient.addColorStop(0, state.gradColor1);
        gradient.addColorStop(1, state.gradColor2);

        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, state.canvasWidth, state.canvasHeight);
    } else if (state.bgType === 'image' && state.imageObj) {
        ctx.drawImage(state.imageObj, 0, 0, state.canvasWidth, state.canvasHeight);
    } else {
        ctx.fillStyle = '#1e293b';
        ctx.fillRect(0, 0, state.canvasWidth, state.canvasHeight);
        ctx.fillStyle = '#94a3b8';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('No image loaded. Select an image or switch background.', state.canvasWidth / 2, state.canvasHeight / 2);
    }

    renderTextOverlaysDOM();
}

// Text Overlays DOM & Dragging
function renderTextOverlaysDOM() {
    overlaysContainer.innerHTML = '';

    state.layers.forEach(layer => {
        const div = document.createElement('div');
        div.className = 'canvas-text-overlay';
        if (layer.id === state.activeLayerId) div.classList.add('selected');
        div.style.left = layer.x + 'px';
        div.style.top = layer.y + 'px';
        div.style.color = layer.color;
        div.style.fontSize = layer.fontSize + 'px';
        div.style.fontWeight = layer.fontWeight;
        div.textContent = layer.text;

        div.addEventListener('mousedown', (e) => {
            selectLayer(layer.id);
            startDrag(e, layer);
        });

        overlaysContainer.appendChild(div);
    });
}

function selectLayer(id) {
    state.activeLayerId = id;
    renderLayersList();
    renderLayerProperties();
    renderTextOverlaysDOM();
}

function startDrag(e, layer) {
    const startX = e.clientX;
    const startY = e.clientY;
    const origX = layer.x;
    const origY = layer.y;

    function onMouseMove(moveEvent) {
        const dx = moveEvent.clientX - startX;
        const dy = moveEvent.clientY - startY;
        layer.x = Math.max(0, Math.min(state.canvasWidth - 50, origX + dx));
        layer.y = Math.max(0, Math.min(state.canvasHeight - 20, origY + dy));
        renderTextOverlaysDOM();
    }

    function onMouseUp() {
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
        runAudit();
    }

    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
}

// Layers sidebar management
function renderLayersList() {
    layersListEl.innerHTML = '';
    state.layers.forEach(layer => {
        const item = document.createElement('div');
        item.className = 'layer-item';
        if (layer.id === state.activeLayerId) item.classList.add('active');

        const title = document.createElement('span');
        title.className = 'layer-item-title';
        title.textContent = layer.text || 'Untitled Layer';

        item.addEventListener('click', () => {
            selectLayer(layer.id);
        });

        item.appendChild(title);
        layersListEl.appendChild(item);
    });
}

function renderLayerProperties() {
    const layer = state.layers.find(l => l.id === state.activeLayerId);
    if (!layer) {
        layerPropertiesEl.classList.add('hidden');
        return;
    }
    layerPropertiesEl.classList.remove('hidden');
    activeLayerIdInput.value = layer.id;
    layerTextContent.value = layer.text;
    layerTextColor.value = layer.color;
    layerTextColorText.value = layer.color;
    layerFontSize.value = layer.fontSize;
    layerFontWeight.value = layer.fontWeight;
}

addTextBtn.addEventListener('click', () => {
    const newLayer = {
        id: 'layer-' + Date.now(),
        text: 'New Text Layer',
        color: '#ffffff',
        fontSize: 20,
        fontWeight: '600',
        x: Math.floor(state.canvasWidth / 4),
        y: Math.floor(state.canvasHeight / 2)
    };
    state.layers.push(newLayer);
    selectLayer(newLayer.id);
    runAudit();
});

deleteLayerBtn.addEventListener('click', () => {
    state.layers = state.layers.filter(l => l.id !== state.activeLayerId);
    state.activeLayerId = state.layers.length > 0 ? state.layers[0].id : null;
    renderLayersList();
    renderLayerProperties();
    renderTextOverlaysDOM();
    runAudit();
});

layerTextContent.addEventListener('input', (e) => {
    const layer = state.layers.find(l => l.id === state.activeLayerId);
    if (layer) {
        layer.text = e.target.value;
        renderTextOverlaysDOM();
        renderLayersList();
        runAudit();
    }
});

layerTextColor.addEventListener('input', (e) => {
    const layer = state.layers.find(l => l.id === state.activeLayerId);
    if (layer) {
        layer.color = e.target.value;
        layerTextColorText.value = e.target.value;
        renderTextOverlaysDOM();
        runAudit();
    }
});

layerTextColorText.addEventListener('input', (e) => {
    if (/^#[0-9A-Fa-f]{6}$/.test(e.target.value)) {
        const layer = state.layers.find(l => l.id === state.activeLayerId);
        if (layer) {
            layer.color = e.target.value;
            layerTextColor.value = e.target.value;
            renderTextOverlaysDOM();
            runAudit();
        }
    }
});

layerFontSize.addEventListener('input', (e) => {
    const layer = state.layers.find(l => l.id === state.activeLayerId);
    if (layer) {
        layer.fontSize = parseInt(e.target.value, 10) || 16;
        renderTextOverlaysDOM();
        runAudit();
    }
});

layerFontWeight.addEventListener('change', (e) => {
    const layer = state.layers.find(l => l.id === state.activeLayerId);
    if (layer) {
        layer.fontWeight = e.target.value;
        renderTextOverlaysDOM();
        runAudit();
    }
});

// WCAG Math & Contrast Calculations
function parseHexColor(hex) {
    let c = hex.replace('#', '');
    if (c.length === 3) {
        c = c.split('').map(char => char + char).join('');
    }
    const num = parseInt(c, 16);
    return [(num >> 16) & 255, (num >> 8) & 255, num & 255];
}

function getRelativeLuminance(r, g, b) {
    const sRGB = [r, g, b].map(v => {
        v /= 255;
        return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
    });
    return 0.2126 * sRGB[0] + 0.7152 * sRGB[1] + 0.0722 * sRGB[2];
}

function getContrastRatio(lum1, lum2) {
    const lighter = Math.max(lum1, lum2);
    const darker = Math.min(lum1, lum2);
    return (lighter + 0.05) / (darker + 0.05);
}

function sampleBackgroundLuminance(layer) {
    // Approximate bounding box of text element
    const approxWidth = Math.min(layer.text.length * layer.fontSize * 0.6, state.canvasWidth - layer.x);
    const approxHeight = layer.fontSize * 1.2;
    
    const sampleX = Math.floor(layer.x);
    const sampleY = Math.floor(layer.y);
    const sampleW = Math.max(10, Math.floor(approxWidth));
    const sampleH = Math.max(10, Math.floor(approxHeight));

    const imgData = ctx.getImageData(
        Math.max(0, Math.min(state.canvasWidth - 1, sampleX)),
        Math.max(0, Math.min(state.canvasHeight - 1, sampleY)),
        Math.max(1, Math.min(state.canvasWidth - sampleX, sampleW)),
        Math.max(1, Math.min(state.canvasHeight - sampleY, sampleH))
    );

    let totalLum = 0;
    const data = imgData.data;
    const pixelCount = data.length / 4;

    for (let i = 0; i < data.length; i += 4) {
        totalLum += getRelativeLuminance(data[i], data[i+1], data[i+2]);
    }

    return pixelCount > 0 ? totalLum / pixelCount : 0.18;
}

function suggestBetterColor(textColorHex, bgLum, isLargeText) {
    const [r, g, b] = parseHexColor(textColorHex);
    const currentLum = getRelativeLuminance(r, g, b);
    const targetRatio = isLargeText ? 3.0 : 4.5;

    let step = currentLum > bgLum ? 0.02 : -0.02;
    let testR = r, testG = g, testB = b;
    let bestColor = textColorHex;
    let bestRatio = getContrastRatio(currentLum, bgLum);

    for (let i = 0; i < 50; i++) {
        testR = Math.max(0, Math.min(255, testR + (step > 0 ? 5 : -5)));
        testG = Math.max(0, Math.min(255, testG + (step > 0 ? 5 : -5)));
        testB = Math.max(0, Math.min(255, testB + (step > 0 ? 5 : -5)));
        
        const lum = getRelativeLuminance(testR, testG, testB);
        const ratio = getContrastRatio(lum, bgLum);
        
        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestColor = '#' + [testR, testG, testB].map(x => x.toString(16).padStart(2, '0')).join('');
        }
        if (ratio >= targetRatio) break;
    }
    return { color: bestColor, ratio: bestRatio };
}

function runAudit() {
    auditResultsContainer.innerHTML = '';

    if (state.layers.length === 0) {
        auditResultsContainer.innerHTML = '<p class="help-text">No text layers found. Add a text overlay to run accessibility audits.</p>';
        return;
    }

    state.layers.forEach(layer => {
        const [r, g, b] = parseHexColor(layer.color);
        const textLum = getRelativeLuminance(r, g, b);
        const bgLum = sampleBackgroundLuminance(layer);
        const ratio = getContrastRatio(textLum, bgLum);

        const isLarge = layer.fontSize >= 24 || (layer.fontSize >= 18.5 && parseInt(layer.fontWeight, 10) >= 700);
        const aaPass = isLarge ? ratio >= 3.0 : ratio >= 4.5;
        const aaaPass = isLarge ? ratio >= 4.5 : ratio >= 7.0;

        const card = document.createElement('div');
        card.className = `audit-card ${aaPass ? 'pass' : 'fail'}`;

        const header = document.createElement('div');
        header.className = 'audit-header';

        const title = document.createElement('div');
        title.className = 'audit-title';
        title.textContent = layer.text || 'Untitled Layer';

        const score = document.createElement('div');
        score.className = 'audit-score';
        score.textContent = ratio.toFixed(2) + ':1';

        header.appendChild(title);
        header.appendChild(score);
        card.appendChild(header);

        const details = document.createElement('div');
        details.className = 'audit-details';
        details.innerHTML = `<span>AA: ${aaPass ? 'PASS' : 'FAIL'}</span><span>AAA: ${aaaPass ? 'PASS' : 'FAIL'}</span><span>Type: ${isLarge ? 'Large' : 'Normal'}</span>`;
        card.appendChild(details);

        if (!aaPass) {
            const suggestion = suggestBetterColor(layer.color, bgLum, isLarge);
            const suggestBtn = document.createElement('button');
            suggestBtn.className = 'suggest-btn';
            suggestBtn.textContent = `Quick Fix: Suggest ${suggestion.color} (${suggestion.ratio.toFixed(1)}:1)`;
            suggestBtn.addEventListener('click', () => {
                layer.color = suggestion.color;
                renderTextOverlaysDOM();
                renderLayerProperties();
                runAudit();
            });
            card.appendChild(suggestBtn);
        }

        auditResultsContainer.appendChild(card);
    });
}

// Export Report
exportReportBtn.addEventListener('click', () => {
    const report = {
        timestamp: new Date().toISOString(),
        canvasSettings: {
            width: state.canvasWidth,
            height: state.canvasHeight,
            backgroundType: state.bgType,
            solidColor: state.solidColor,
            gradient: { color1: state.gradColor1, color2: state.gradColor2, angle: state.gradAngle }
        },
        layers: state.layers.map(layer => {
            const [r, g, b] = parseHexColor(layer.color);
            const textLum = getRelativeLuminance(r, g, b);
            const bgLum = sampleBackgroundLuminance(layer);
            const ratio = getContrastRatio(textLum, bgLum);
            const isLarge = layer.fontSize >= 24 || (layer.fontSize >= 18.5 && parseInt(layer.fontWeight, 10) >= 700);
            return {
                text: layer.text,
                color: layer.color,
                fontSize: layer.fontSize,
                fontWeight: layer.fontWeight,
                position: { x: layer.x, y: layer.y },
                contrastRatio: ratio.toFixed(2) + ':1',
                wcagAA: isLarge ? ratio >= 3.0 : ratio >= 4.5,
                wcagAAA: isLarge ? ratio >= 4.5 : ratio >= 7.0
            };
        })
    };

    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(report, null, 2));
    const downloadAnchor = document.createElement('a');
    downloadAnchor.setAttribute("href", dataStr);
    downloadAnchor.setAttribute("download", "contrast-canvas-accessibility-report.json");
    document.body.appendChild(downloadAnchor);
    downloadAnchor.click();
    downloadAnchor.remove();
});

// Run initialization
window.addEventListener('DOMContentLoaded', () => {
    initUI();
});
