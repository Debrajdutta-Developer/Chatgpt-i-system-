# Cron Horizon

Cron Horizon is a local-first, developer-centric cron visualizer, 12-month calendar projector, and concurrency conflict detector. It helps developers, DevOps engineers, and system administrators design, debug, and optimize cron schedules without risking server crashes or resource-heavy job overlaps.

## The Problem
Cron expressions are notoriously difficult to read, debug, and predict. A single typo or misunderstanding of cron syntax can trigger a heavy script every minute instead of every hour, or cause multiple resource-intensive jobs to run simultaneously, leading to server exhaustion, database locks, or unexpected downtime.

## Features
- **Interactive 12-Month Calendar Projection**: Visualizes scheduled runs across a full year with color-coded indicators, allowing you to click any day to view a detailed chronological timeline.
- **Confrontation & Overlap Detector**: Scans your active schedules to identify when multiple jobs run at the exact same minute. It groups conflicts, assesses risk levels (Critical, High, Medium, Low), and provides actionable recommendations to stagger schedules.
- **Human-Readable Translation**: Instantly translates complex cron expressions into clear, natural English.
- **Interactive Sandbox & Verification**: Paste any standard cron expression (e.g., `*/15 9-17 * * 1-5`) to verify its behavior. The sandbox automatically analyzes the schedule to confirm if it matches expected intervals (like 15-minute intervals during business hours on weekdays).
- **Local-First & Private**: All parsing, calculations, and storage happen entirely in your browser. No data is sent to external servers.

## How to Use
1. **Manage Jobs**: Use the sidebar to add, edit, or toggle your cron jobs. You can assign custom colors to each job to easily distinguish them on the calendar.
2. **Explore the Calendar**: Switch to the **12-Month Calendar** tab to see a bird's-eye view of your scheduled runs. Click on any highlighted day to see the exact execution times.
3. **Detect Conflicts**: Open the **Conflict Detector** tab to see if any jobs are scheduled to run at the exact same minute, which could cause CPU or database spikes.
4. **Verify Schedules**: Use the **Sandbox & Verification** tab to test individual expressions and view an automated verification report.

## Limitations
- Supports standard 5-field cron syntax (`minute hour day-of-month month day-of-week`).
- Does not support non-standard extensions like `@reboot`, `@yearly`, or seconds/years fields.
- To prevent browser performance degradation, the projection is capped at a maximum of 5,000 runs per job over the 12-month period.
