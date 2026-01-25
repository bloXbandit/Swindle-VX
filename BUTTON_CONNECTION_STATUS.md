# Button & Control Connection Status

## ✅ FULLY CONNECTED (UI → C++ Backend)

### Parameter Controls (Real-time Audio Processing)
- **Pitch Correction Knob** → `correction` parameter (0-1)
- **Correction Speed Knob** → `speed` parameter (0-1)
- **Pitch Shift Knob** → `pitch` parameter (-24 to +24 semitones)
- **Formant Shift Knob** → `formant` parameter (-12 to +12 semitones)
- **Breath Amount Knob** → `breath` parameter (0-1)
- **Resonance Knob** → `resonance` parameter (0-1)
- **AI Blend Knob** → `blend` parameter (0-1)
- **Key Selector** → `key` parameter (0-11)
- **Scale Selector** → `scale` parameter (0-8)

**Status:** All knobs send parameter changes to C++ via `juceBridge.sendParameterChange()`

---

## ⚠️ UI-ONLY (No Backend Connection)

### Module Toggles
- **Pitch Engine Switch** → Only updates UI state (`pitchEnabled`)
- **Character Morph Switch** → Only updates UI state (`characterEnabled`)
- **AI Model Library Switch** → Only updates UI state (`modelEnabled`)

**Issue:** These switches don't disable/enable the actual DSP modules in C++. Audio processing always runs regardless of switch state.

**Fix Needed:** Add enable/disable parameters to C++ or implement bypass logic.

---

### Voice Cloning
- **Clone Voice Button** → ✅ Calls Fish Audio API
  - Records audio via `audioEngine.startRecording()`
  - Uploads to Fish Audio via `fishAudio.createVoiceModel()`
  - Creates persistent voice model
  - **Status:** WORKING (requires Fish Audio API key)

---

### Voice Model Selection
- **Model Library Dropdown** → ⚠️ PARTIALLY CONNECTED
  - Shows local ONNX models (singer_m, singer_f, etc.)
  - Shows Fish Audio voices (loaded from API)
  - **Issue:** Selected model ID stored in UI state but NOT sent to C++ backend
  - **C++ Side:** `ONNXInference` can load models but no parameter to trigger loading
  
**Fix Needed:** 
1. Add model loading mechanism in C++
2. Send model path/ID from UI to C++
3. Implement model switching in `ONNXInference`

---

### Import Model Button
- **Import RVC Model** → ❌ MOCK ONLY
  - Shows success toast after 2 seconds
  - Doesn't actually import any files
  - No file picker implemented
  
**Fix Needed:** Implement file picker and model loading

---

### Preset System
- **Save Preset Button** → ✅ WORKING
  - Saves all UI state to `PresetManager`
  - Stores in localStorage
  
- **Load Preset Dropdown** → ✅ WORKING
  - Loads preset and updates UI state
  - **Issue:** Doesn't sync loaded values to C++ backend
  
**Fix Needed:** After loading preset, trigger parameter sync to C++

---

### Utility Buttons
- **Demo Loop Button** → ⚠️ UI-ONLY
  - Toggles `isDemoPlaying` state
  - Calls `audioEngine.toggleDemo()`
  - **Issue:** `audioEngine` is web-only, doesn't affect C++ plugin
  
- **Bypass Button** → ⚠️ UI-ONLY
  - Toggles `isBypassed` state
  - Doesn't bypass C++ audio processing
  
**Fix Needed:** Add bypass parameter to C++

- **Start Processing Button** → ⚠️ UI-ONLY
  - Sets `isActive = true`
  - Visual feedback only, no backend effect

---

### Settings
- **Settings Icon** → ✅ WORKING
  - Opens Fish Audio API key configuration dialog
  - Saves key to localStorage
  - Verifies key with Fish Audio API

---

## Summary

### Working Connections:
✅ **9 audio parameters** → C++ backend (real-time processing)
✅ **Voice cloning** → Fish Audio API
✅ **Preset save/load** → localStorage
✅ **Settings** → API key management

### Missing Connections:
❌ **Module enable/disable switches** → No C++ bypass
❌ **Voice model selection** → No C++ model loading
❌ **Import model** → Not implemented
❌ **Demo/Bypass buttons** → UI-only
❌ **Preset load** → Doesn't sync to C++

---

## Priority Fixes

### High Priority:
1. **Preset load sync** - After loading preset, send all parameters to C++
2. **Voice model loading** - Connect dropdown selection to C++ ONNX loader

### Medium Priority:
3. **Module bypass** - Add enable/disable parameters for each DSP module
4. **Global bypass** - Add master bypass parameter

### Low Priority:
5. **Import model** - Implement file picker and model loading
6. **Demo loop** - Connect to C++ or remove button
