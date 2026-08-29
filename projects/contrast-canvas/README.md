# Contrast Canvas

Contrast Canvas is a local-first browser application designed for UI/UX Designers and Frontend Developers to test WCAG accessibility contrast ratios for complex UI components such as gradients, background images, and layered text elements.

## The Problem
Testing WCAG accessibility contrast ratios for complex UI components like gradients, text over images, and multi-state buttons is tedious with standard single-color pickers. Designers and developers often skip rigorous accessibility checks or fail compliance because current tools only support flat color-to-color comparison, leading to late-stage design refactoring or non-compliant releases.

## Features
- **Interactive Canvas**: Drag and drop or create text overlays directly onto custom solid colors, linear/radial gradients, or uploaded background images.
- **Real-Time Relative Luminance Mapping**: Analyzes exact pixel regions beneath text elements to calculate relative luminance and contrast ratios instantly.
- **Instant WCAG Compliance Scoring**: Evaluates text against WCAG AA and AAA standards for normal and large text sizes.
- **Smart Color Suggestions**: Automatically suggests lighter or darker text color adjustments to meet required compliance levels.
- **Local-First Privacy**: All image processing and contrast calculations happen entirely within your browser using HTML5 Canvas and pure mathematical algorithms. No data ever leaves your device.

## How to Use
1. Open `index.html` in any modern web browser.
2. Configure your canvas background using the Background tab (Solid, Gradient, or Image Upload).
3. Add text layers using the Text Overlays toolbar, and drag them anywhere on the canvas.
4. View real-time WCAG contrast scores, bounding box luminance heatmaps, and one-click fix suggestions.

## Limitations
- Processing very large background images may experience minor performance throttling depending on client hardware.
- Calculations are based on sRGB relative luminance formulas specified in WCAG 2.1 guidelines.
