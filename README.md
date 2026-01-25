# Swindl VX - Next-Gen Vocal Processing Suite

This template includes built-in detection for missing CSS variables between your Tailwind config and CSS files.

## 🚀 Features

- **Professional Pitch Engine**: Real-time correction using the **YIN Algorithm** for industry-leading accuracy.
- **Phase Vocoder Pitch Shifter**: Formant-preserving shifting with ±24 semitone range.
- **AI Voice Morphing**: Real-time inference using **RVC (Retrieval-based Voice Conversion)** on ONNX runtime.
- **Ethical AI Cloning**: Record and create your own voice models with built-in legal safeguards.
- **External API Support**: Connect **ElevenLabs** and **Fish Audio** for iconic and high-fidelity streaming models.
- **Hybrid Architecture**: Low-latency C++ DSP paired with a high-performance React-based UI via the JUCE-Web bridge.

## 🛠 Technical Stack

### C++ Core (VST3/AU)
- **Framework**: JUCE 7
- **AI Engine**: ONNX Runtime
- **DSP**: Custom YIN Pitch Detection, Phase Vocoder Pitch Shifting, Biquad Formant Filtering.
- **Bridge**: JUCE WebBrowserComponent for high-fidelity UI rendering.

### Frontend UI (embedded)
- **Framework**: React + Vite + TypeScript
- **Styling**: Tailwind CSS (Custom HSL Palette)
- **Auth/Storage**: Blink SDK for secure user model management and cloud syncing.

## 📁 Project Structure

- `plugin/`: C++ Source code and JUCE configuration.
- `src/`: React UI source code.
- `src/lib/AudioEngine.ts`: Web-based audio engine for preview and development.

## Available Scripts

```bash
# Run all linting (includes CSS variable check)
npm run lint

# Check only CSS variables
npm run check:css-vars

# Individual linting
npm run lint:js    # ESLint
npm run lint:css   # Stylelint
```

## CSS Variable Detection

The template includes a custom script that:

1. **Parses `tailwind.config.cjs`** to find all `var(--variable)` references
2. **Parses `src/index.css`** to find all defined CSS variables (`--variable:`)
3. **Cross-references** them to find missing definitions
4. **Reports undefined variables** with clear error messages

### Example Output

When CSS variables are missing:
```
❌ Undefined CSS variables found in tailwind.config.cjs:
   --sidebar-background
   --sidebar-foreground
   --sidebar-primary

Add these variables to src/index.css
```

When all variables are defined:
```
✅ All CSS variables in tailwind.config.cjs are defined
```

## How It Works

The detection happens during the `npm run lint` command, which will:
- Exit with error code 1 if undefined variables are found
- Show exactly which variables need to be added to your CSS file
- Integrate seamlessly with your development workflow

This prevents runtime CSS issues where Tailwind classes reference undefined CSS variables.