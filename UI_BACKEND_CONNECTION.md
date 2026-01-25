# UI ↔ Backend Connection

## Status: ✅ CONNECTED

The React UI now sends parameter changes to the C++ audio processor in real-time.

---

## How It Works

### UI → Backend (Parameter Changes)

**Flow:**
1. User moves a knob/slider in React UI
2. React state updates (e.g., `setPitchCorrection(50)`)
3. `useEffect` hook triggers
4. `juceBridge.sendParameterChange('correction', 0.5)` called
5. JUCE WebView bridge sends event to C++
6. `PluginEditor::handleMessage()` receives message
7. Parameter value updated in APVTS
8. Audio processing uses new value immediately

**Parameter Mapping:**

| UI Control | UI Value | C++ Parameter | C++ Range | Conversion |
|------------|----------|---------------|-----------|------------|
| Pitch Correction | 0-100 | `correction` | 0.0-1.0 | `/100` |
| Correction Speed | 0-100 | `speed` | 0.0-1.0 | `/100` |
| Pitch Shift | -24 to 24 | `pitch` | -24.0-24.0 | direct |
| Formant Shift | -12 to 12 | `formant` | -12.0-12.0 | direct |
| Breath Amount | 0-100 | `breath` | 0.0-1.0 | `/100` |
| Resonance | 0-100 | `resonance` | 0.0-1.0 | `/100` |
| AI Blend | 0-100 | `blend` | 0.0-1.0 | `/100` |
| Key | C, C#, D... | `key` | 0-11 (int) | map to index |
| Scale | Major, Minor... | `scale` | 0-8 (int) | map to index |

---

## Implementation Details

### Frontend (`src/lib/juce-bridge.ts`)
```typescript
juceBridge.sendParameterChange(name: string, value: number)
```
Sends parameter changes via `window.__JUCE__.backend.emitEvent()`

### Backend (`plugin/Source/PluginEditor.cpp`)
```cpp
void handleMessage(const juce::var& message)
```
Receives messages with format:
```json
{
  "type": "parameterChange",
  "name": "correction",
  "value": 0.5
}
```

Converts to 0-1 normalized range and updates APVTS parameter.

---

## Testing

**To verify connection:**
1. Run dev server: `npm run dev -- --port 5173 --strictPort`
2. Open plugin in Logic
3. Move a knob in the UI
4. **Expected:** Audio processing changes immediately
5. **Verify:** Automate parameter in Logic → UI should NOT update (one-way only for now)

---

## Known Limitations

- **One-way sync only:** UI → Backend works, but Backend → UI not implemented yet
- **No parameter automation feedback:** If you automate parameters in the DAW, UI won't reflect changes
- **No preset recall sync:** Loading presets in DAW won't update UI

---

## Future Enhancements

### Backend → UI (Parameter Updates)
To implement bidirectional sync:
1. Add timer in `PluginEditor` to poll parameter values
2. Send updates to UI via `webView.evaluateJavascript()`
3. UI listens for updates and updates state without triggering `useEffect` loops

### Visualization Data
Send real-time pitch detection and spectrum data to UI:
```cpp
webView.evaluateJavascript("window.__updatePitch(" + String(currentPitch) + ")");
```
