(function() {
  'use strict';

  // State management
  let state = {
    fileName: '',
    rawText: '',
    delimiter: ',',
    hasHeader: true,
    rows: [], // Array of string arrays
    headers: [],
    expectedColumnCount: 0,
    columnTypes: {}, // colIndex -> 'auto' | 'integer' | 'float' | 'date' | 'text'
    anomalies: [], // Array of { rowIndex, colIndex, type, message }
    filter: 'all', // 'all' | 'anomalies'
    selectedCell: null // { rowIndex, colIndex }
  };

  // DOM Elements
  const uploadSection = document.getElementById('uploadSection');
  const workspace = document.getElementById('workspace');
  const dropZone = document.getElementById('dropZone');
  const fileInput = document.getElementById('fileInput');
  const loadSampleBtn = document.getElementById('loadSampleBtn');
  const newFileBtn = document.getElementById('newFileBtn');
  const exportBtn = document.getElementById('exportBtn');
  const fileNameBadge = document.getElementById('fileNameBadge');
  const fileRowsBadge = document.getElementById('fileRowsBadge');
  const fileColsBadge = document.getElementById('fileColsBadge');
  const anomalyCountBadge = document.getElementById('anomalyCountBadge');
  const delimiterSelect = document.getElementById('delimiterSelect');
  const headerCheck = document.getElementById('headerCheck');
  const schemaConfigList = document.getElementById('schemaConfigList');
  const tableHead = document.getElementById('tableHead');
  const tableBody = document.getElementById('tableBody');
  const cellModal = document.getElementById('cellModal');
  const modalBackdrop = document.getElementById('modalBackdrop');
  const modalCloseBtn = document.getElementById('modalCloseBtn');
  const modalSaveBtn = document.getElementById('modalSaveBtn');
  const modalCellInput = document.getElementById('modalCellInput');
  const modalCellCoords = document.getElementById('modalCellCoords');
  const modalAnomalyInfo = document.getElementById('modalAnomalyInfo');

  // Repair Buttons
  const repairTrimBtn = document.getElementById('repairTrimBtn');
  const repairQuotesBtn = document.getElementById('repairQuotesBtn');
  const repairNonAsciiBtn = document.getElementById('repairNonAsciiBtn');
  const repairFillEmptyBtn = document.getElementById('repairFillEmptyBtn');
  const repairAllBtn = document.getElementById('repairAllBtn');

  // Initialize Event Listeners
  function init() {
    // Drag and drop
    dropZone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropZone.style.borderColor = 'var(--primary)';
    });
    dropZone.addEventListener('dragleave', (e) => {
      e.preventDefault();
      dropZone.style.borderColor = 'var(--panel-border)';
    });
    dropZone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropZone.style.borderColor = 'var(--panel-border)';
      if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        handleFile(e.dataTransfer.files[0]);
      }
    });

    dropZone.addEventListener('click', () => fileInput.click());
    dropZone.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        fileInput.click();
      }
    });

    fileInput.addEventListener('change', (e) => {
      if (e.target.files && e.target.files.length > 0) {
        handleFile(e.target.files[0]);
      }
    });

    loadSampleBtn.addEventListener('click', loadSampleData);
    newFileBtn.addEventListener('click', resetToUpload);
    exportBtn.addEventListener('click', exportCSV);

    delimiterSelect.addEventListener('change', (e) => {
      state.delimiter = e.target.value;
      parseCSVText(state.rawText);
    });

    headerCheck.addEventListener('change', (e) => {
      state.hasHeader = e.target.checked;
      parseCSVText(state.rawText);
    });

    // Filter buttons
    document.querySelectorAll('.filter-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
        e.target.classList.add('active');
        state.filter = e.target.getAttribute('data-filter');
        renderTable();
      });
    });

    // Quick repairs
    repairTrimBtn.addEventListener('click', () => { repairWhitespace(); });
    repairQuotesBtn.addEventListener('click', () => { repairQuotes(); });
    repairNonAsciiBtn.addEventListener('click', () => { repairNonAscii(); });
    repairFillEmptyBtn.addEventListener('click', () => { repairFillEmpty(); });
    repairAllBtn.addEventListener('click', () => { 
      repairWhitespace();
      repairQuotes();
      repairNonAscii();
      repairFillEmpty();
    });

    // Modal actions
    modalCloseBtn.addEventListener('click', closeModal);
    modalBackdrop.addEventListener('click', closeModal);
    modalSaveBtn.addEventListener('click', saveModalCell);
  }

  // Load Sample Messy CSV
  function loadSampleData() {
    const sampleCSV = `id,name,signup_date,score,status
1,Alice Smith,2023-01-15,95.5,active
2,Bob Jones,01/20/2023,82,active
3,Charlie Brown,2023-02-10,invalid_score,pending
4,Diana Prince,2023-03-05,88.0,active
5,Extra Column Rogue,2023-03-12,91,active,extra_val
6,Eve Café,,79.5,inactive
7,Frank Castle,2023-05-01,-100,banned
,Grace Kelly,2023-06-15,85,active`;
    
    state.fileName = 'sample_messy_data.csv';
    state.rawText = sampleCSV;
    state.delimiter = ',';
    state.hasHeader = true;
    delimiterSelect.value = ',';
    headerCheck.checked = true;
    
    parseCSVText(sampleCSV);
  }

  function handleFile(file) {
    state.fileName = file.name;
    const reader = new FileReader();
    reader.onload = (e) => {
      state.rawText = e.target.result;
      detectDelimiter(state.rawText);
      parseCSVText(state.rawText);
    };
    reader.readAsText(file);
  }

  function detectDelimiter(text) {
    const firstLine = text.split(/\r?\n/)[0] || '';
    const counts = {
      ',': (firstLine.match(/,/g) || []).length,
      ';': (firstLine.match(/;/g) || []).length,
      '\t': (firstLine.match(/\t/g) || []).length,
      '|': (firstLine.match(/\|/g) || []).length
    };
    let best = ',';
    let max = -1;
    for (const [delim, count] of Object.entries(counts)) {
      if (count > max) {
        max = count;
        best = delim;
      }
    }
    if (max > 0) {
      state.delimiter = best;
      delimiterSelect.value = best;
    } else {
      state.delimiter = ',';
      delimiterSelect.value = ',';
    }
  }

  // Robust CSV parser handling quoted fields and escaped quotes
  function parseCSV(text, delimiter) {
    const rows = [];
    let currentRow = [];
    let currentField = '';
    let inQuotes = false;
    
    for (let i = 0; i < text.length; i++) {
      const c = text[i];
      const nextC = text[i + 1];

      if (inQuotes) {
        if (c === '"') {
          if (nextC === '"') {
            currentField += '"';
            i++;
          } else {
            inQuotes = false;
          }
        } else {
          currentField += c;
        }
      } else {
        if (c === '"') {
          inQuotes = true;
        } else if (c === delimiter) {
          currentRow.push(currentField);
          currentField = '';
        } else if (c === '\r' && nextC === '\n') {
          currentRow.push(currentField);
          rows.push(currentRow);
          currentRow = [];
          currentField = '';
          i++;
        } else if (c === '\n' || c === '\r') {
          currentRow.push(currentField);
          rows.push(currentRow);
          currentRow = [];
          currentField = '';
        } else {
          currentField += c;
        }
      }
    }
    currentRow.push(currentField);
    if (currentRow.length > 1 || currentRow[0] !== '') {
      rows.push(currentRow);
    }
    return rows;
  }

  function parseCSVText(text) {
    let delim = state.delimiter;
    if (delim === 'auto') delim = ',';
    
    const parsed = parseCSV(text, delim);
    if (parsed.length === 0) return;

    if (state.hasHeader) {
      state.headers = parsed[0];
      state.rows = parsed.slice(1);
    } else {
      state.headers = parsed[0].map((_, idx) => `Column ${idx + 1}`);
      state.rows = parsed;
    }

    // Determine expected column count based on mode
    const colCounts = state.rows.map(r => r.length);
    colCounts.push(state.headers.length);
    // Find most frequent column count
    const countsMap = {};
    colCounts.forEach(c => countsMap[c] = (countsMap[c] || 0) + 1);
    let maxFreq = 0;
    let modeCount = state.headers.length;
    for (const [cnt, freq] of Object.entries(countsMap)) {
      if (freq > maxFreq) {
        maxFreq = freq;
        modeCount = parseInt(cnt, 10);
      }
    }
    state.expectedColumnCount = modeCount;

    // Initialize column types if not set
    if (Object.keys(state.columnTypes).length === 0) {
      state.headers.forEach((_, idx) => {
        state.columnTypes[idx] = inferColumnType(idx);
      });
    }

    runAudit();
    showWorkspace();
  }

  function inferColumnType(colIndex) {
    const values = state.rows.map(r => r[colIndex]).filter(v => v !== undefined && v.trim() !== '');
    if (values.length === 0) return 'text';

    let intCount = 0;
    let floatCount = 0;
    let dateCount = 0;

    const intRegex = /^-?\d+$/;
    const floatRegex = /^-?\d*\.\d+$/;
    const dateRegexes = [
      /^\d{4}-\d{2}-\d{2}$/,
      /^\d{2}\/\d{2}\/\d{4}$/,
      /^\d{4}\/\d{2}\/\d{2}$/
    ];

    values.forEach(v => {
      const trimmed = v.trim();
      if (intRegex.test(trimmed)) intCount++;
      else if (floatRegex.test(trimmed)) floatCount++;
      else if (dateRegexes.some(rx => rx.test(trimmed))) dateCount++;
    });

    const threshold = values.length * 0.7;
    if (intCount >= threshold) return 'integer';
    if ((intCount + floatCount) >= threshold) return 'float';
    if (dateCount >= threshold) return 'date';
    return 'text';
  }

  function runAudit() {
    state.anomalies = [];

    state.rows.forEach((row, rowIndex) => {
      // Check column count mismatch
      if (row.length !== state.expectedColumnCount) {
        state.anomalies.push({
          rowIndex,
          colIndex: -1,
          type: 'col-mismatch',
          message: `Row has ${row.length} columns (expected ${state.expectedColumnCount})`
        });
      }

      row.forEach((cell, colIndex) => {
        if (cell === undefined) return;
        const trimmed = cell.trim();

        // Check empty cell
        if (trimmed === '') {
          state.anomalies.push({
            rowIndex,
            colIndex,
            type: 'empty',
            message: 'Cell is empty'
          });
          return;
        }

        // Check non-ASCII
        if (/[^\x00-\x7F]/.test(cell)) {
          state.anomalies.push({
            rowIndex,
            colIndex,
            type: 'non-ascii',
            message: 'Contains non-ASCII characters'
          });
        }

        // Check type mismatch
        const expectedType = state.columnTypes[colIndex] || 'text';
        if (expectedType === 'integer') {
          if (!/^-?\d+$/.test(trimmed)) {
            state.anomalies.push({
              rowIndex,
              colIndex,
              type: 'type-mismatch',
              message: `Expected Integer, got "${cell}"`
            });
          }
        } else if (expectedType === 'float') {
          if (!/^-?\d*\.?\d+$/.test(trimmed)) {
            state.anomalies.push({
              rowIndex,
              colIndex,
              type: 'type-mismatch',
              message: `Expected Numeric, got "${cell}"`
            });
          }
        } else if (expectedType === 'date') {
          const dateRegexes = [/^\d{4}-\d{2}-\d{2}$/, /^\d{2}\/\d{2}\/\d{4}$/, /^\d{4}\/\d{2}\/\d{2}$/];
          if (!dateRegexes.some(rx => rx.test(trimmed))) {
            state.anomalies.push({
              rowIndex,
              colIndex,
              type: 'type-mismatch',
              message: `Expected Date format, got "${cell}"`
            });
          }
        }
      });
    });

    updateStats();
    renderSchemaConfig();
    renderTable();
  }

  function updateStats() {
    fileNameBadge.textContent = state.fileName || 'data.csv';
    fileRowsBadge.textContent = `${state.rows.length} rows`;
    fileColsBadge.textContent = `${state.expectedColumnCount} cols`;
    anomalyCountBadge.textContent = `${state.anomalies.length} anomalies`;
  }

  function renderSchemaConfig() {
    schemaConfigList.innerHTML = '';
    state.headers.forEach((header, idx) => {
      const item = document.createElement('div');
      item.className = 'schema-item';
      
      const headerRow = document.createElement('div');
      headerRow.className = 'schema-item-header';
      headerRow.innerHTML = `<span>${escapeHTML(header)}</span><span class="col-type">Col ${idx+1}</span>`;
      item.appendChild(headerRow);

      const select = document.createElement('select');
      ['text', 'integer', 'float', 'date'].forEach(t => {
        const opt = document.createElement('option');
        opt.value = t;
        opt.textContent = t.charAt(0).toUpperCase() + t.slice(1);
        if ((state.columnTypes[idx] || 'text') === t) opt.selected = true;
        select.appendChild(opt);
      });

      select.addEventListener('change', (e) => {
        state.columnTypes[idx] = e.target.value;
        runAudit();
      });

      item.appendChild(select);
      schemaConfigList.appendChild(item);
    });
  }

  function renderTable() {
    // Build header
    tableHead.innerHTML = '';
    const headerTr = document.createElement('tr');
    const rowNumTh = document.createElement('th');
    rowNumTh.textContent = '#';
    headerTr.appendChild(rowNumTh);

    state.headers.forEach((header, idx) => {
      const th = document.createElement('th');
      const type = state.columnTypes[idx] || 'text';
      th.innerHTML = `${escapeHTML(header)}<span class="col-type">${type}</span>`;
      headerTr.appendChild(th);
    });
    tableHead.appendChild(headerTr);

    // Build body
    tableBody.innerHTML = '';
    
    state.rows.forEach((row, rowIndex) => {
      // Check filter
      if (state.filter === 'anomalies') {
        const hasAnomaly = state.anomalies.some(a => a.rowIndex === rowIndex);
        if (!hasAnomaly) return;
      }

      const tr = document.createElement('tr');
      
      // Row index cell
      const tdIdx = document.createElement('td');
      tdIdx.textContent = rowIndex + 1;
      tdIdx.style.color = 'var(--text-muted)';
      tr.appendChild(tdIdx);

      // Check if row has col-mismatch
      const rowColMismatch = state.anomalies.find(a => a.rowIndex === rowIndex && a.type === 'col-mismatch');

      for (let colIndex = 0; colIndex < state.expectedColumnCount; colIndex++) {
        const td = document.createElement('td');
        const cellValue = row[colIndex] !== undefined ? row[colIndex] : '';
        td.textContent = cellValue;
        td.className = 'cell-editable';

        // Find anomalies for this cell
        const cellAnomalies = state.anomalies.filter(a => a.rowIndex === rowIndex && a.colIndex === colIndex);
        
        if (rowColMismatch && colIndex >= row.length) {
          td.classList.add('has-col-mismatch');
          td.title = 'Missing column (row too short)';
        } else if (cellAnomalies.length > 0) {
          const types = cellAnomalies.map(a => a.type);
          if (types.includes('type-mismatch')) {
            td.classList.add('has-type-mismatch');
          } else if (types.includes('non-ascii')) {
            td.classList.add('has-non-ascii');
          } else if (types.includes('empty')) {
            td.classList.add('has-empty');
          }
          td.title = cellAnomalies.map(a => a.message).join('; ');
        }

        // Click to edit
        const capturedRowIndex = rowIndex;
        const capturedColIndex = colIndex;
        td.addEventListener('click', () => {
          openCellModal(capturedRowIndex, capturedColIndex);
        });

        tr.appendChild(td);
      }

      // If row has extra columns, append them or note them
      if (row.length > state.expectedColumnCount) {
        for (let colIndex = state.expectedColumnCount; colIndex < row.length; colIndex++) {
          const td = document.createElement('td');
          td.textContent = row[colIndex];
          td.className = 'cell-editable has-col-mismatch';
          td.title = 'Extra column (row too long)';
          const capturedRowIndex = rowIndex;
          const capturedColIndex = colIndex;
          td.addEventListener('click', () => {
            openCellModal(capturedRowIndex, capturedColIndex);
          });
          tr.appendChild(td);
        }
      }

      tableBody.appendChild(tr);
    });
  }

  function openCellModal(rowIndex, colIndex) {
    state.selectedCell = { rowIndex, colIndex };
    const val = state.rows[rowIndex][colIndex] !== undefined ? state.rows[rowIndex][colIndex] : '';
    modalCellInput.value = val;
    modalCellCoords.textContent = `Row ${rowIndex + 1}, Column ${colIndex + 1} (${state.headers[colIndex] || 'Extra'})`;
    
    const cellAnomalies = state.anomalies.filter(a => a.rowIndex === rowIndex && (a.colIndex === colIndex || a.colIndex === -1));
    if (cellAnomalies.length > 0) {
      modalAnomalyInfo.textContent = cellAnomalies.map(a => a.message).join(' | ');
      modalAnomalyInfo.style.display = 'block';
    } else {
      modalAnomalyInfo.style.display = 'none';
    }

    cellModal.classList.remove('hidden');
    cellModal.setAttribute('aria-hidden', 'false');
    modalCellInput.focus();
  }

  function closeModal() {
    cellModal.classList.add('hidden');
    cellModal.setAttribute('aria-hidden', 'true');
    state.selectedCell = null;
  }

  function saveModalCell() {
    if (!state.selectedCell) return;
    const { rowIndex, colIndex } = state.selectedCell;
    if (!state.rows[rowIndex]) state.rows[rowIndex] = [];
    state.rows[rowIndex][colIndex] = modalCellInput.value;
    closeModal();
    runAudit();
  }

  // Repair Functions
  function repairWhitespace() {
    state.rows = state.rows.map(row => row.map(cell => cell !== undefined ? cell.trim() : ''));
    runAudit();
  }

  function repairQuotes() {
    state.rows = state.rows.map(row => row.map(cell => {
      if (cell === undefined) return '';
      // Replace unescaped rogue quotes or clean wrapping quotes
      let cleaned = cell;
      if (cleaned.startsWith('"') && cleaned.endsWith('"') && cleaned.length >= 2) {
        cleaned = cleaned.substring(1, cleaned.length - 1);
      }
      return cleaned;
    }));
    runAudit();
  }

  function repairNonAscii() {
    state.rows = state.rows.map(row => row.map(cell => {
      if (cell === undefined) return '';
      // Strip non-ASCII characters or transliterate common ones
      return cell.replace(/[^\x00-\x7F]/g, '');
    }));
    runAudit();
  }

  function repairFillEmpty() {
    state.rows = state.rows.map(row => {
      const newRow = [...row];
      for (let i = 0; i < state.expectedColumnCount; i++) {
        if (newRow[i] === undefined || newRow[i].trim() === '') {
          const type = state.columnTypes[i] || 'text';
          if (type === 'integer' || type === 'float') {
            newRow[i] = '0';
          } else if (type === 'date') {
            newRow[i] = '1970-01-01';
          } else {
            newRow[i] = 'N/A';
          }
        }
      }
      // Trim extra columns if row is too long
      if (newRow.length > state.expectedColumnCount) {
        return newRow.slice(0, state.expectedColumnCount);
      }
      // Pad if row is too short
      while (newRow.length < state.expectedColumnCount) {
        newRow.push('N/A');
      }
      return newRow;
    });
    runAudit();
  }

  function exportCSV() {
    let csvContent = '';
    const delim = state.delimiter === 'auto' ? ',' : state.delimiter;

    const escapeField = (field) => {
      if (field === undefined) return '';
      const str = String(field);
      if (str.includes(delim) || str.includes('"') || str.includes('\n') || str.includes('\r')) {
        return `"${str.replace(/"/g, '""')}"`;
      }
      return str;
    };

    if (state.hasHeader && state.headers.length > 0) {
      csvContent += state.headers.map(escapeField).join(delim) + '\r\n';
    }

    state.rows.forEach(row => {
      csvContent += row.map(escapeField).join(delim) + '\r\n';
    });

    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.setAttribute('href', url);
    link.setAttribute('download', `sanitized_${state.fileName || 'data.csv'}`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  }

  function showWorkspace() {
    uploadSection.classList.add('hidden');
    workspace.classList.remove('hidden');
  }

  function resetToUpload() {
    state = {
      fileName: '',
      rawText: '',
      delimiter: ',',
      hasHeader: true,
      rows: [],
      headers: [],
      expectedColumnCount: 0,
      columnTypes: {},
      anomalies: [],
      filter: 'all',
      selectedCell: null
    };
    fileInput.value = '';
    workspace.classList.add('hidden');
    uploadSection.classList.remove('hidden');
  }

  function escapeHTML(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;');
  }

  // Initialize on load
  init();

})();
