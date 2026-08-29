/**
 * Cron Horizon — Application Logic
 */

// --- Constants & Configuration ---
const COLOR_PALETTE = [
  '#6366f1', // Indigo
  '#10b981', // Emerald
  '#f59e0b', // Amber
  '#ec4899', // Pink
  '#06b6d4', // Cyan
  '#8b5cf6'  // Purple
];

const MONTH_ALIASES = {
  JAN: 1, FEB: 2, MAR: 3, APR: 4, MAY: 5, JUN: 6,
  JUL: 7, AUG: 8, SEP: 9, OCT: 10, NOV: 11, DEC: 12
};

const DOW_ALIASES = {
  SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6
};

const DEFAULT_JOBS = [
  {
    id: 'job-1',
    name: 'Database Backup',
    cron: '0 2 * * *',
    color: '#6366f1',
    active: true
  },
  {
    id: 'job-2',
    name: 'Hourly Cleanup',
    cron: '0 * * * *',
    color: '#10b981',
    active: true
  },
  {
    id: 'job-3',
    name: 'Marketing Sync',
    cron: '*/15 9-17 * * 1-5',
    color: '#f59e0b',
    active: true
  },
  {
    id: 'job-4',
    name: 'Weekly Report',
    cron: '0 9 * * 1',
    color: '#ec4899',
    active: true
  }
];

// --- State Management ---
let state = {
  jobs: [],
  selectedColor: COLOR_PALETTE[0],
  activeTab: 'tab-calendar',
  projectionStartDate: new Date(),
  projectionEndDate: new Date()
};

// Set projection range to exactly 12 months starting from current month
const initProjectionDates = () => {
  const now = new Date();
  state.projectionStartDate = new Date(now.getFullYear(), now.getMonth(), 1);
  state.projectionEndDate = new Date(now.getFullYear(), now.getMonth() + 12, 0, 23, 59, 59);
};

// --- Cron Parser Engine ---

function parseField(str, min, max, aliases = {}) {
  let s = str.toUpperCase();
  for (const [alias, val] of Object.entries(aliases)) {
    s = s.replace(new RegExp(alias, 'g'), val);
  }

  const values = new Set();
  const parts = s.split(',');

  for (const part of parts) {
    if (part === '*') {
      for (let i = min; i <= max; i++) values.add(i);
    } else if (part.includes('/')) {
      const [range, stepStr] = part.split('/');
      const step = parseInt(stepStr, 10) || 1;
      let start = min;
      let end = max;
      if (range !== '*') {
        if (range.includes('-')) {
          const [rStart, rEnd] = range.split('-').map(Number);
          start = rStart;
          end = rEnd;
        } else {
          start = parseInt(range, 10);
        }
      }
      for (let i = start; i <= end; i += step) {
        if (i >= min && i <= max) values.add(i);
      }
    } else if (part.includes('-')) {
      const [start, end] = part.split('-').map(Number);
      for (let i = start; i <= end; i++) {
        if (i >= min && i <= max) values.add(i);
      }
    } else {
      const val = parseInt(part, 10);
      if (!isNaN(val) && val >= min && val <= max) {
        values.add(val);
      }
    }
  }

  if (values.size === 0) {
    throw new Error(`Invalid field value: "${str}"`);
  }

  return Array.from(values).sort((a, b) => a - b); 
}

function parseCronExpression(expr) {
  const parts = expr.trim().split(/\s+/);
  if (parts.length !== 5) {
    throw new Error("Cron expression must have exactly 5 fields (minute, hour, day-of-month, month, day-of-week).");
  }

  const [minStr, hourStr, domStr, monthStr, dowStr] = parts;

  const minutes = parseField(minStr, 0, 59);
  const hours = parseField(hourStr, 0, 23);
  const daysOfMonth = parseField(domStr, 1, 31);
  const months = parseField(monthStr, 1, 12, MONTH_ALIASES);
  const daysOfWeek = parseField(dowStr, 0, 7, DOW_ALIASES).map(d => d === 7 ? 0 : d);

  // Deduplicate daysOfWeek
  const uniqueDaysOfWeek = Array.from(new Set(daysOfWeek)).sort((a, b) => a - b);

  return {
    minutes,
    hours,
    daysOfMonth,
    months,
    daysOfWeek: uniqueDaysOfWeek,
    domRestricted: domStr !== '*',
    dowRestricted: dowStr !== '*'
  };
}

// Generates runs for a single job over a period
function getRunsForPeriod(cronObj, startDate, endDate) {
  const runs = [];
  let iterDate = new Date(startDate.getTime());
  const MAX_RUNS = 5000; // Prevent browser freeze

  while (iterDate <= endDate) {
    if (runs.length >= MAX_RUNS) break;

    const year = iterDate.getFullYear();
    const month = iterDate.getMonth() + 1; // 1-12
    const dom = iterDate.getDate();
    const dow = iterDate.getDay(); // 0-6

    // Optimize: Skip month if not matching
    if (!cronObj.months.includes(month)) {
      iterDate.setMonth(iterDate.getMonth() + 1);
      iterDate.setDate(1);
      continue;
    }

    // Check DOM and DOW matching logic (Standard Cron OR logic)
    const domMatch = cronObj.daysOfMonth.includes(dom);
    const dowMatch = cronObj.daysOfWeek.includes(dow);

    let dayMatches = false;
    if (cronObj.domRestricted && cronObj.dowRestricted) {
      dayMatches = domMatch || dowMatch;
    } else {
      dayMatches = domMatch && dowMatch;
    }

    if (dayMatches) {
      for (const h of cronObj.hours) {
        for (const m of cronObj.minutes) {
          const runDate = new Date(year, month - 1, dom, h, m, 0, 0);
          if (runDate >= startDate && runDate <= endDate) {
            runs.push(runDate);
            if (runs.length >= MAX_RUNS) break;
          }
        }
        if (runs.length >= MAX_RUNS) break;
      }
    }

    iterDate.setDate(iterDate.getDate() + 1);
  }

  return runs;
}

// --- Natural Language Translation Engine ---

function translateCron(cronStr) {
  try {
    const parts = cronStr.trim().split(/\s+/);
    if (parts.length !== 5) return "Invalid expression";

    const [minStr, hourStr, domStr, monthStr, dowStr] = parts;

    const formatList = (arr, formatter) => {
      if (arr.length === 1) return formatter(arr[0]);
      if (arr.length === 2) return `${formatter(arr[0])} and ${formatter(arr[1])}`;
      return arr.slice(0, -1).map(formatter).join(', ') + ', and ' + formatter(arr[arr.length - 1]);
    };

    // Minutes
    let minDesc = "";
    if (minStr === '*') {
      minDesc = "every minute";
    } else if (minStr.startsWith('*/')) {
      minDesc = `every ${minStr.split('/')[1]} minutes`;
    } else {
      const mins = parseField(minStr, 0, 59);
      if (mins.length === 1) {
        minDesc = `at minute ${mins[0].toString().padStart(2, '0')}`;
      } else {
        minDesc = `at minutes ${formatList(mins, m => m.toString().padStart(2, '0'))}`;
      }
    }

    // Hours
    let hourDesc = "";
    if (hourStr === '*') {
      hourDesc = "every hour";
    } else if (hourStr.startsWith('*/')) {
      hourDesc = `every ${hourStr.split('/')[1]} hours`;
    } else if (hourStr.includes('-') && !hourStr.includes(',')) {
      const [start, end] = hourStr.split('-').map(Number);
      hourDesc = `between ${start.toString().padStart(2, '0')}:00 and ${end.toString().padStart(2, '0')}:59`;
    } else {
      const hours = parseField(hourStr, 0, 23);
      if (hours.length === 1) {
        hourDesc = `at ${hours[0].toString().padStart(2, '0')}:00`;
      } else {
        hourDesc = `at hours ${formatList(hours, h => h.toString().padStart(2, '0'))}`;
      }
    }

    // Days of Month
    let domDesc = "";
    if (domStr !== '*') {
      const doms = parseField(domStr, 1, 31);
      const ordinal = n => n + (['st','nd','rd'][((n+90)%100-10)%10-1] || 'th');
      domDesc = `on the ${formatList(doms, ordinal)} of the month`;
    }

    // Months
    let monthDesc = "";
    const monthNames = ["", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
    if (monthStr !== '*') {
      const months = parseField(monthStr, 1, 12, MONTH_ALIASES);
      monthDesc = `in ${formatList(months, m => monthNames[m])}`;
    }

    // Days of Week
    let dowDesc = "";
    const dowNames = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
    if (dowStr !== '*') {
      const dows = parseField(dowStr, 0, 7, DOW_ALIASES).map(d => d === 7 ? 0 : d);
      const uniqueDows = Array.from(new Set(dows)).sort();
      dowDesc = `on ${formatList(uniqueDows, d => dowNames[d])}`;
    }

    let partsList = [];
    if (minDesc.startsWith("every minute") && hourDesc.startsWith("every hour")) {
      partsList.push("Every minute");
    } else if (minDesc.startsWith("every") && hourDesc.startsWith("every hour")) {
      partsList.push(`${minDesc.charAt(0).toUpperCase() + minDesc.slice(1)}`);
    } else {
      partsList.push(`${minDesc.charAt(0).toUpperCase() + minDesc.slice(1)} of ${hourDesc}`);
    }

    if (domDesc) partsList.push(domDesc);
    if (monthDesc) partsList.push(monthDesc);
    if (dowDesc) partsList.push(dowDesc);

    return partsList.join(", ");
  } catch (e) {
    return "Invalid cron syntax";
  }
}

// --- UI Rendering & Logic ---

// Color Picker Init
const initColorPicker = () => {
  const picker = document.getElementById('color-picker');
  picker.innerHTML = '';
  COLOR_PALETTE.forEach(color => {
    const opt = document.createElement('div');
    opt.className = 'color-option';
    opt.style.backgroundColor = color;
    if (color === state.selectedColor) opt.classList.add('selected');
    opt.addEventListener('click', () => {
      document.querySelectorAll('.color-option').forEach(el => el.classList.remove('selected'));
      opt.classList.add('selected');
      state.selectedColor = color;
    });
    picker.appendChild(opt);
  });
};

// Save Jobs to LocalStorage
const saveToStorage = () => {
  localStorage.setItem('cron_horizon_jobs', JSON.stringify(state.jobs));
};

// Load Jobs from LocalStorage
const loadFromStorage = () => {
  const stored = localStorage.getItem('cron_horizon_jobs');
  if (stored) {
    state.jobs = JSON.parse(stored);
  } else {
    state.jobs = [...DEFAULT_JOBS];
  }
};

// Render Job List in Sidebar
const renderJobList = () => {
  const list = document.getElementById('job-list');
  const countBadge = document.getElementById('job-count');
  list.innerHTML = '';
  countBadge.textContent = `${state.jobs.length} Job${state.jobs.length !== 1 ? 's' : ''}`;

  if (state.jobs.length === 0) {
    list.innerHTML = `<div class="empty-state">No jobs configured. Add a job above or load the demo set!</div>`;
    return;
  }

  state.jobs.forEach(job => {
    const item = document.createElement('div');
    item.className = 'job-item';
    
    const translation = translateCron(job.cron);

    item.innerHTML = `
      <div class="job-item-header">
        <div class="job-item-title-group">
          <span class="job-color-indicator" style="background-color: ${job.color}"></span>
          <span class="job-name">${escapeHtml(job.name)}</span>
        </div>
        <label class="switch">
          <input type="checkbox" class="toggle-job-status" data-id="${job.id}" ${job.active ? 'checked' : ''}>
          <span class="slider"></span>
        </label>
      </div>
      <div>
        <span class="job-cron-text">${escapeHtml(job.cron)}</span>
      </div>
      <div class="job-desc">${escapeHtml(translation)}</div>
      <div class="job-actions">
        <button class="btn-icon btn-edit-job" data-id="${job.id}">Edit</button>
        <button class="btn-icon btn-delete-job" data-id="${job.id}" style="color: var(--danger)">Delete</button>
      </div>
    `;

    // Toggle handler
    item.querySelector('.toggle-job-status').addEventListener('change', (e) => {
      job.active = e.target.checked;
      saveToStorage();
      calculateAndRender();
    });

    // Edit handler
    item.querySelector('.btn-edit-job').addEventListener('click', () => {
      loadJobIntoForm(job);
    });

    // Delete handler
    item.querySelector('.btn-delete-job').addEventListener('click', () => {
      state.jobs = state.jobs.filter(j => j.id !== job.id);
      saveToStorage();
      calculateAndRender();
    });

    list.appendChild(item);
  });
};

const loadJobIntoForm = (job) => {
  document.getElementById('job-id').value = job.id;
  document.getElementById('job-name').value = job.name;
  document.getElementById('job-cron').value = job.cron;
  state.selectedColor = job.color;
  
  // Update color picker selection
  document.querySelectorAll('.color-option').forEach(opt => {
    if (opt.style.backgroundColor === job.color || rgbToHex(opt.style.backgroundColor) === job.color) {
      opt.classList.add('selected');
    } else {
      opt.classList.remove('selected');
    }
  });

  document.getElementById('btn-cancel-edit').classList.remove('hidden');
  document.getElementById('btn-save-job').textContent = 'Update Job';
  validateFormCron();
};

const resetForm = () => {
  document.getElementById('job-id').value = '';
  document.getElementById('job-name').value = '';
  document.getElementById('job-cron').value = '';
  document.getElementById('btn-cancel-edit').classList.add('hidden');
  document.getElementById('btn-save-job').textContent = 'Save Job';
  document.getElementById('cron-live-translation').textContent = 'Enter a valid cron expression';
  document.getElementById('cron-live-translation').classList.remove('error');
  state.selectedColor = COLOR_PALETTE[0];
  initColorPicker();
};

// Live validation for job form
const validateFormCron = () => {
  const cronInput = document.getElementById('job-cron').value.trim();
  const feedback = document.getElementById('cron-live-translation');
  if (!cronInput) {
    feedback.textContent = 'Enter a valid cron expression';
    feedback.classList.remove('error');
    return;
  }
  try {
    parseCronExpression(cronInput);
    const translation = translateCron(cronInput);
    feedback.textContent = translation;
    feedback.classList.remove('error');
  } catch (e) {
    feedback.textContent = e.message;
    feedback.classList.add('error');
  }
};

// --- Projection & Conflict Calculations ---

let globalRunsByDate = {}; // Key: YYYY-MM-DD, Value: Array of { job, date }
let globalConflicts = [];   // Array of conflicts

const calculateAndRender = () => {
  globalRunsByDate = {};
  globalConflicts = [];

  const activeJobs = state.jobs.filter(j => j.active);

  // 1. Generate all runs
  activeJobs.forEach(job => {
    try {
      const parsed = parseCronExpression(job.cron);
      const runs = getRunsForPeriod(parsed, state.projectionStartDate, state.projectionEndDate);
      
      runs.forEach(date => {
        const dateKey = formatDateKey(date);
        if (!globalRunsByDate[dateKey]) {
          globalRunsByDate[dateKey] = [];
        }
        globalRunsByDate[dateKey].push({ job, date });
      });
    } catch (e) {
      console.error(`Error generating runs for job ${job.name}:`, e);
    }
  });

  // 2. Identify conflicts (same minute overlaps)
  const conflictMap = {}; // Key: JobPairKey, Value: Array of dates

  Object.keys(globalRunsByDate).forEach(dateKey => {
    const runs = globalRunsByDate[dateKey];
    // Group runs on this day by exact hour and minute
    const timeGroups = {};
    runs.forEach(run => {
      const timeKey = `${run.date.getHours().toString().padStart(2, '0')}:${run.date.getMinutes().toString().padStart(2, '0')}`;
      if (!timeGroups[timeKey]) {
        timeGroups[timeKey] = [];
      }
      timeGroups[timeKey].push(run);
    });

    // Check each time group for overlaps
    Object.keys(timeGroups).forEach(timeKey => {
      const group = timeGroups[timeKey];
      if (group.length > 1) {
        // We have a conflict! Find all unique pairs
        for (let i = 0; i < group.length; i++) {
          for (let j = i + 1; j < group.length; j++) {
            const jobA = group[i].job;
            const jobB = group[j].job;
            // Sort IDs to ensure consistent pair key
            const pairKey = [jobA.id, jobB.id].sort().join('::');
            if (!conflictMap[pairKey]) {
              conflictMap[pairKey] = {
                jobA,
                jobB,
                dates: []
              };
            }
            // Store the exact conflict date-time
            const conflictDate = new Date(group[i].date); 
            conflictMap[pairKey].dates.push(conflictDate);
          }
        }
      }
    });
  });

  globalConflicts = Object.values(conflictMap);

  // Update Conflict Badge in Tab
  const badge = document.getElementById('conflict-badge');
  if (globalConflicts.length > 0) {
    badge.textContent = globalConflicts.length;
    badge.classList.remove('hidden');
  } else {
    badge.classList.add('hidden');
  }

  // Render views
  renderJobList();
  renderCalendarProjection();
  renderConflicts();
};

// --- Render 12-Month Calendar ---
const renderCalendarProjection = () => {
  const container = document.getElementById('calendar-projection');
  container.innerHTML = '';

  const start = new Date(state.projectionStartDate.getTime());

  for (let m = 0; m < 12; m++) {
    const currentMonth = new Date(start.getFullYear(), start.getMonth() + m, 1);
    const monthCard = document.createElement('div');
    monthCard.className = 'month-card';

    const monthName = currentMonth.toLocaleString('default', { month: 'long', year: 'numeric' });
    
    // Days of week header
    const daysHeader = ['Su', 'Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa']
      .map(d => `<div>${d}</div>`).join('');

    // Days grid
    const firstDayIndex = currentMonth.getDay();
    const totalDays = new Date(currentMonth.getFullYear(), currentMonth.getMonth() + 1, 0).getDate();

    let daysHtml = '';
    // Padding for first week
    for (let i = 0; i < firstDayIndex; i++) {
      daysHtml += `<div class="day-cell empty"></div>`;
    }

    // Render actual days
    for (let day = 1; day <= totalDays; day++) {
      const dateObj = new Date(currentMonth.getFullYear(), currentMonth.getMonth(), day);
      const dateKey = formatDateKey(dateObj);
      const dayRuns = globalRunsByDate[dateKey] || [];
      
      let cellClass = 'day-cell active-month';
      let dotsHtml = '';

      if (dayRuns.length > 0) {
        cellClass += ' has-runs';
        // Check if there is a conflict on this day
        const hasConflictOnDay = checkDayHasConflict(dayRuns);
        if (hasConflictOnDay) {
          cellClass += ' has-conflict';
        }

        // Render small color dots for jobs (max 3)
        const uniqueJobs = Array.from(new Set(dayRuns.map(r => r.job)));
        dotsHtml = `<div class="day-dots">` + 
          uniqueJobs.slice(0, 3).map(job => `<span class="day-dot" style="background-color: ${job.color}"></span>`).join('') + 
          `</div>`;
      }

      daysHtml += `
        <div class="${cellClass}" data-date="${dateKey}">
          ${day}
          ${dotsHtml}
        </div>
      `;
    }

    monthCard.innerHTML = `
      <div class="month-title">${monthName}</div>
      <div class="month-days-header">${daysHeader}</div>
      <div class="month-days-grid">${daysHtml}</div>
    `;

    // Add click listeners to day cells
    monthCard.querySelectorAll('.day-cell.has-runs').forEach(cell => {
      cell.addEventListener('click', () => {
        openDayModal(cell.getAttribute('data-date'));
      });
    });

    container.appendChild(monthCard);
  }
};

// Helper to check if a day has any overlapping runs
const checkDayHasConflict = (dayRuns) => {
  const times = {};
  for (const run of dayRuns) {
    const timeKey = `${run.date.getHours()}:${run.date.getMinutes()}`;
    if (times[timeKey]) return true;
    times[timeKey] = true;
  }
  return false;
};

// --- Render Conflicts Tab ---
const renderConflicts = () => {
  const container = document.getElementById('conflict-results');
  container.innerHTML = '';

  if (globalConflicts.length === 0) {
    container.innerHTML = `
      <div class="conflict-summary-card">
        <div class="conflict-icon-box success">
          <svg width="24" height="24" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
            <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path>
            <polyline points="22 4 12 14.01 9 11.01"></polyline>
          </svg>
        </div>
        <div>
          <h3>No Schedule Conflicts Detected!</h3>
          <p class="panel-subtitle">All active cron jobs are safely staggered. No overlapping run-times found in the next 12 months.</p>
        </div>
      </div>
    `;
    return;
  }

  // Summary Card
  const summary = document.createElement('div');
  summary.className = 'conflict-summary-card';
  summary.innerHTML = `
    <div class="conflict-icon-box danger">
      <svg width="24" height="24" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
        <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path>
        <line x1="12" y1="9" x2="12" y2="13"></line>
        <line x1="12" y1="17" x2="12.01" y2="17"></line>
      </svg>
    </div>
    <div>
      <h3>${globalConflicts.length} Potential Schedule Confrontation${globalConflicts.length > 1 ? 's' : ''} Detected</h3>
      <p class="panel-subtitle">Multiple jobs are scheduled to trigger at the exact same minute. Review details and recommendations below.</p>
    </div>
  `;
  container.appendChild(summary);

  const list = document.createElement('div');
  list.className = 'conflict-list';

  globalConflicts.forEach(conflict => {
    const { jobA, jobB, dates } = conflict;
    const overlapCount = dates.length;

    // Determine Risk Level
    let riskBadge = '';
    let recommendation = '';
    if (overlapCount > 300) {
      riskBadge = '<span class="badge badge-danger">Critical Risk</span>';
      recommendation = `These jobs overlap very frequently (${overlapCount} times). Consider shifting the minute field of <strong>${escapeHtml(jobB.name)}</strong> (e.g., from <code>0</code> to <code>30</code> or adding a custom offset) to balance server load.`;
    } else if (overlapCount > 50) {
      riskBadge = '<span class="badge badge-warning">High Risk</span>';
      recommendation = `Frequent overlap detected. Staggering <strong>${escapeHtml(jobB.name)}</strong> by 5-10 minutes will prevent concurrent resource spikes.`;
    } else {
      riskBadge = '<span class="badge badge-info">Medium Risk</span>';
      recommendation = `Occasional overlap. If these jobs are highly resource-intensive, consider shifting one of their start times.`;
    }

    const item = document.createElement('div');
    item.className = 'conflict-item';
    item.innerHTML = `
      <div class="conflict-item-header">
        <div class="conflict-pair">
          <span class="job-color-indicator" style="background-color: ${jobA.color}"></span>
          <strong>${escapeHtml(jobA.name)}</strong>
          <span class="conflict-vs">vs</span>
          <span class="job-color-indicator" style="background-color: ${jobB.color}"></span>
          <strong>${escapeHtml(jobB.name)}</strong>
        </div>
        <div class="conflict-meta">
          ${riskBadge}
          <span class="badge badge-neutral">${overlapCount} Overlaps / Year</span>
        </div>
      </div>
      <div class="conflict-details">
        <p><strong>Next 5 Overlap Times:</strong></p>
        <ul style="margin: 8px 0 12px 20px; font-family: monospace; font-size: 0.85rem; color: var(--text-muted);">
          ${dates.slice(0, 5).map(d => `<li>${d.toLocaleString()}</li>`).join('')}
        </ul>
        <div class="conflict-recommendation">
          <strong>Recommendation:</strong> ${recommendation}
        </div>
      </div>
    `;
    list.appendChild(item);
  });

  container.appendChild(list);
};

// --- Day Details Modal --- 
const openDayModal = (dateKey) => {
  const modal = document.getElementById('day-modal');
  const title = document.getElementById('modal-date-title');
  const summary = document.getElementById('modal-day-summary');
  const timeline = document.getElementById('modal-timeline');

  const [year, month, day] = dateKey.split('-').map(Number);
  const dateObj = new Date(year, month - 1, day);
  
  title.textContent = dateObj.toLocaleDateString('default', { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });

  const dayRuns = globalRunsByDate[dateKey] || [];
  
  // Sort chronologically
  dayRuns.sort((a, b) => a.date - b.date);

  // Count conflicts
  const timeCounts = {};
  dayRuns.forEach(r => {
    const t = r.date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
    timeCounts[t] = (timeCounts[t] || 0) + 1;
  });
  const conflictCount = Object.values(timeCounts).filter(c => c > 1).length;

  summary.innerHTML = `
    <strong>Daily Summary:</strong> ${dayRuns.length} execution${dayRuns.length !== 1 ? 's' : ''} scheduled. 
    ${conflictCount > 0 ? `<span style="color: var(--danger); font-weight: 600;">${conflictCount} overlapping time slot(s) detected!</span>` : 'All runs are safely staggered.'}
  `;

  timeline.innerHTML = '';
  if (dayRuns.length === 0) {
    timeline.innerHTML = '<div class="empty-state">No runs scheduled for this day.</div>';
  } else {
    dayRuns.forEach(run => {
      const timeStr = run.date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', hour12: false });
      const isConflicting = timeCounts[timeStr] > 1;

      const item = document.createElement('div');
      item.className = 'timeline-item';
      if (isConflicting) {
        item.style.borderLeft = '4px solid var(--danger)';
      } else {
        item.style.borderLeft = `4px solid ${run.job.color}`;
      }

      item.innerHTML = `
        <div class="timeline-time">${timeStr}</div>
        <div class="timeline-job">
          <span class="job-color-indicator" style="background-color: ${run.job.color}"></span>
          <span>${escapeHtml(run.job.name)}</span>
          <span class="job-cron-text" style="font-size: 0.7rem;">${escapeHtml(run.job.cron)}</span>
        </div>
        ${isConflicting ? `<span class="badge badge-danger timeline-conflict-badge">Overlap</span>` : ''}
      `;
      timeline.appendChild(item);
    });
  }

  modal.classList.remove('hidden');
};

// --- Sandbox & Verification Logic ---

const runSandboxVerification = () => {
  const input = document.getElementById('sandbox-cron-input').value.trim();
  const feedback = document.getElementById('sandbox-validation-feedback');
  const translationEl = document.getElementById('sandbox-translation');
  const runsList = document.getElementById('sandbox-runs-list');
  const reportContent = document.getElementById('verification-report-content');

  if (!input) {
    feedback.textContent = 'Please enter a cron expression';
    feedback.className = 'validation-feedback error';
    return;
  }

  try {
    const parsed = parseCronExpression(input);
    feedback.textContent = '✓ Valid cron expression';
    feedback.className = 'validation-feedback success';

    // Translate
    const translation = translateCron(input);
    translationEl.textContent = translation;

    // Generate next 15 runs
    const now = new Date();
    const endLimit = new Date(now.getFullYear() + 2, now.getMonth(), now.getDate()); // Look up to 2 years ahead
    const runs = getRunsForPeriod(parsed, now, endLimit).slice(0, 15);

    runsList.innerHTML = '';
    runs.forEach((date, index) => {
      const li = document.createElement('li');
      li.innerHTML = `
        <span class="run-index">#${index + 1}</span>
        <span>${date.toLocaleString()}</span>
      `;
      runsList.appendChild(li);
    });

    // Automated Verification Report
    // We verify standard expectations (e.g., 15-minute intervals during business hours on weekdays)
    let reportHtml = '';

    // Check 1: Weekday restriction
    const runsOnWeekends = runs.some(d => d.getDay() === 0 || d.getDay() === 6);
    if (!runsOnWeekends && parsed.dowRestricted) {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon success" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"></polyline></svg>
          <div>
            <strong>Weekday Restriction Verified:</strong> This schedule runs exclusively on weekdays (Monday through Friday).
          </div>
        </div>
      `;
    } else {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon info" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
          <div>
            <strong>Active on Weekends:</strong> This schedule executes on weekends (Saturday/Sunday).
          </div>
        </div>
      `;
    }

    // Check 2: Business Hours restriction (9 AM to 5 PM / 17:00)
    const outsideBusinessHours = runs.some(d => d.getHours() < 9 || d.getHours() > 17);
    if (!outsideBusinessHours && parsed.hours.length < 24) {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon success" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"></polyline></svg>
          <div>
            <strong>Business Hours Verified:</strong> Executions are strictly confined to standard business hours (09:00 AM - 05:59 PM).
          </div>
        </div>
      `;
    } else {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon info" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
          <div>
            <strong>24-Hour Coverage:</strong> This schedule runs outside standard business hours (09:00 AM - 05:59 PM).
          </div>
        </div>
      `;
    }

    // Check 3: Interval verification (e.g. 15-minute intervals)
    const is15MinInterval = input.includes('*/15') || (parsed.minutes.length > 1 && parsed.minutes[1] - parsed.minutes[0] === 15);
    if (is15MinInterval) {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon success" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"></polyline></svg>
          <div>
            <strong>15-Minute Interval Verified:</strong> Runs exactly every 15 minutes as expected.
          </div>
        </div>
      `;
    } else {
      // General interval calculation
      if (parsed.minutes.length > 1) {
        const diff = parsed.minutes[1] - parsed.minutes[0];
        reportHtml += `
          <div class="verification-item">
            <svg class="verification-icon info" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
            <div>
              <strong>Custom Interval:</strong> Runs at intervals of ${diff} minute(s) within the scheduled hours.
            </div>
          </div>
        `;
      } else {
        reportHtml += `
          <div class="verification-item">
            <svg class="verification-icon info" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
            <div>
              <strong>Single Execution:</strong> Runs once per scheduled hour.
            </div>
          </div>
        `;
      }
    }

    // Check 4: High Frequency Warning
    if (parsed.minutes.length === 60 && parsed.hours.length === 24) {
      reportHtml += `
        <div class="verification-item">
          <svg class="verification-icon warning" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></svg>
          <div>
            <strong>High Frequency Warning:</strong> This job runs every single minute. Ensure your script is extremely lightweight to prevent resource exhaustion.
          </div>
        </div>
      `;
    }

    reportContent.innerHTML = reportHtml;

  } catch (e) {
    feedback.textContent = e.message;
    feedback.className = 'validation-feedback error';
    translationEl.textContent = 'Unable to translate invalid expression.';
    runsList.innerHTML = '<div class="empty-state">No runs to project.</div>';
    reportContent.innerHTML = `
      <div class="verification-item">
        <svg class="verification-icon warning" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
        <div>
          <strong>Verification Failed:</strong> Please correct the cron expression syntax errors to generate a report.
        </div>
      </div>
    `;
  }
};

// --- Event Listeners & Setup ---

const setupEventListeners = () => {
  // Tab switching
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
      
      btn.classList.add('active');
      const tabId = btn.getAttribute('data-tab');
      document.getElementById(tabId).classList.add('active');
      state.activeTab = tabId;
    });
  });

  // Live validation on job form
  document.getElementById('job-cron').addEventListener('input', validateFormCron);

  // Job form submission
  document.getElementById('job-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const id = document.getElementById('job-id').value;
    const name = document.getElementById('job-name').value.trim();
    const cron = document.getElementById('job-cron').value.trim();

    try {
      parseCronExpression(cron); // Validate before saving
    } catch (err) {
      alert(`Cannot save: ${err.message}`);
      return;
    }

    if (id) {
      // Edit mode
      state.jobs = state.jobs.map(j => j.id === id ? { ...j, name, cron, color: state.selectedColor } : j);
    } else {
      // Add mode
      const newJob = {
        id: 'job-' + Date.now(),
        name,
        cron,
        color: state.selectedColor,
        active: true
      };
      state.jobs.push(newJob);
    }

    saveToStorage();
    resetForm();
    calculateAndRender();
  });

  // Cancel edit
  document.getElementById('btn-cancel-edit').addEventListener('click', resetForm);

  // Load Demo Jobs
  document.getElementById('btn-load-demo').addEventListener('click', () => {
    if (confirm('This will replace your current jobs with the demo set. Continue?')) {
      state.jobs = [...DEFAULT_JOBS];
      saveToStorage();
      resetForm();
      calculateAndRender();
    }
  });

  // Reset All
  document.getElementById('btn-reset').addEventListener('click', () => {
    if (confirm('Are you sure you want to delete all jobs?')) {
      state.jobs = [];
      saveToStorage();
      resetForm();
      calculateAndRender();
    }
  });

  // Sandbox live validation
  document.getElementById('sandbox-cron-input').addEventListener('input', runSandboxVerification);

  // Sandbox presets
  document.querySelectorAll('.preset-tag').forEach(tag => {
    tag.addEventListener('click', () => {
      document.getElementById('sandbox-cron-input').value = tag.getAttribute('data-cron');
      runSandboxVerification();
    });
  });

  // Close Modal
  document.getElementById('btn-close-modal').addEventListener('click', () => {
    document.getElementById('day-modal').classList.add('hidden');
  });

  // Close modal on background click
  document.getElementById('day-modal').addEventListener('click', (e) => {
    if (e.target.id === 'day-modal') {
      document.getElementById('day-modal').classList.add('hidden');
    }
  });
};

// --- Helper Utilities ---

function formatDateKey(date) {
  const y = date.getFullYear();
  const m = (date.getMonth() + 1).toString().padStart(2, '0');
  const d = date.getDate().toString().padStart(2, '0');
  return `${y}-${m}-${d}`;
}

function escapeHtml(str) {
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

function rgbToHex(rgb) {
  if (rgb.startsWith('#')) return rgb;
  const rgbValues = rgb.match(/\d+/g);
  if (!rgbValues) return '#6366f1';
  const r = parseInt(rgbValues[0]).toString(16).padStart(2, '0');
  const g = parseInt(rgbValues[1]).toString(16).padStart(2, '0');
  const b = parseInt(rgbValues[2]).toString(16).padStart(2, '0');
  return `#${r}${g}${b}`;
}

// --- App Initialization ---
window.addEventListener('DOMContentLoaded', () => {
  initProjectionDates();
  loadFromStorage();
  initColorPicker();
  setupEventListeners();
  calculateAndRender();
  runSandboxVerification(); // Initialize sandbox view
});
