# Voice Model Setup Guide

## Overview

The plugin supports two types of voice models:
1. **Local ONNX Models** - Pre-made voice presets stored on your computer
2. **Fish Audio Voices** - Cloud-based voices (cloned or from Fish Audio library)

Both types appear in the same dropdown and work seamlessly together.

---

## Setup Local ONNX Models

### 1. Create Models Directory

The plugin looks for ONNX models in:
```
~/Documents/VocalSuitePro/Models/
```

Create this directory:
```bash
mkdir -p ~/Documents/VocalSuitePro/Models
```

### 2. Add ONNX Model Files

Place your `.onnx` voice models in this directory. The model ID in the UI should match the filename (without `.onnx` extension).

**Example:**
- File: `~/Documents/VocalSuitePro/Models/singer_m.onnx`
- UI shows: "Pro Singer (Male)"
- Model ID: `singer_m`

### 3. Supported Models

The plugin currently lists these pre-made models:
- `singer_m.onnx` - Pro Singer (Male)
- `singer_f.onnx` - Pro Singer (Female)
- `ethereal.onnx` - Ethereal Synthetic
- `vintage.onnx` - Vintage Tube

**You need to provide these `.onnx` files yourself.** The plugin doesn't ship with models.

### 4. Where to Get ONNX Models

**RVC (Retrieval-based Voice Conversion) Models:**
- [RVC Models Hub](https://huggingface.co/models?search=rvc)
- Train your own using [RVC-Project](https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI)

**Convert to ONNX:**
Most RVC models are PyTorch `.pth` files. Convert to ONNX:
```python
import torch
import onnx

# Load your RVC model
model = torch.load('voice_model.pth')
model.eval()

# Export to ONNX
dummy_input = torch.randn(1, 1, 16000)  # Adjust shape for your model
torch.onnx.export(model, dummy_input, 'singer_m.onnx')
```

---

## Setup Fish Audio Voices

### 1. Get API Key

1. Visit [fish.audio/dashboard](https://fish.audio/dashboard)
2. Sign up or log in
3. Navigate to API settings
4. Copy your API key

### 2. Configure in Plugin

1. Open plugin in your DAW
2. Click **Settings** icon (⚙️) in top-right
3. Paste your Fish Audio API key
4. Click **Save & Verify**

### 3. Clone Your Voice

1. Click **"Clone Voice"** button
2. Speak clearly for 10-30 seconds
3. Click **"Stop Clone"**
4. Plugin uploads to Fish Audio and creates voice model
5. Your voice appears in dropdown under "Fish Audio Voices"

### 4. Use Fish Audio Library

Your cloned voices and any voices in your Fish Audio account automatically appear in the dropdown.

---

## Using Voice Models

### In the UI:

1. Open the **AI Model Library** section
2. Click the **Model Library** dropdown
3. Select a voice:
   - **Local ONNX Models** - Instant, offline, free
   - **Fish Audio Voices** - Your cloned voices
4. Adjust **AI Blend** knob (0-100%)
5. Enable the module with the switch

### Model Selection Flow:

```
User selects model in UI
    ↓
UI sends loadModel command to C++
    ↓
C++ checks model type:
    - ONNX → Loads from ~/Documents/VocalSuitePro/Models/
    - Fish → Stores Fish Audio voice ID for API calls
    ↓
Audio processing uses selected model
```

---

## Troubleshooting

### ONNX Models Not Loading

**Check file location:**
```bash
ls ~/Documents/VocalSuitePro/Models/
```

**Verify filename matches model ID:**
- UI model ID: `singer_m`
- File must be: `singer_m.onnx`

**Check ONNX Runtime:**
The plugin needs ONNX Runtime compiled in. If models don't load, ONNX Runtime may not be enabled in the build.

### Fish Audio Not Working

**No API key:**
- Configure API key in Settings (⚙️ icon)

**Clone button does nothing:**
- Check that API key is valid
- Check internet connection
- Look for error toasts in UI

**Voices not appearing:**
- Refresh by reopening plugin
- Check Fish Audio dashboard for your voices

---

## Model Performance

### Local ONNX:
- ✅ No internet required
- ✅ Low latency
- ✅ Free
- ❌ CPU intensive
- ❌ Need to source/train models

### Fish Audio:
- ✅ High quality voice cloning
- ✅ Easy to use
- ✅ No local processing
- ❌ Requires internet
- ❌ API costs
- ❌ Higher latency

---

## Advanced: Custom Model Paths

To use models from a different location, modify `PluginProcessor.cpp`:

```cpp
juce::File modelsDir = juce::File("/path/to/your/models");
```

Then rebuild the plugin.
