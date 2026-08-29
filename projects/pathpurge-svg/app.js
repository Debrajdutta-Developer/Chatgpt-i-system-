const input = document.getElementById('path-input');
const output = document.getElementById('output');
const displayPath = document.getElementById('display-path');
const precision = document.getElementById('precision');
const precVal = document.getElementById('prec-val');

function optimizePath(d, prec) {
    const regex = /([a-zA-Z])|([-+]?[0-9]*\.?[0-9]+)/g;
    return d.replace(regex, (match, cmd, num) => {
        if (cmd) return cmd;
        return parseFloat(num).toFixed(prec);
    });
}

function update() {
    const val = input.value;
    const p = parseInt(precision.value);
    precVal.textContent = p;
    const optimized = optimizePath(val, p);
    output.value = optimized;
    displayPath.setAttribute('d', optimized);
}

input.addEventListener('input', update);
precision.addEventListener('input', update);

document.getElementById('convert-btn').addEventListener('click', () => {
    // Simple relative conversion logic
    let d = input.value.replace(/([A-Z])/g, (m) => m.toLowerCase());
    input.value = d;
    update();
});
