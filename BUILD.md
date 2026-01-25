# Build Instructions

## Prerequisites
- Xcode with Command Line Tools
- CMake 3.15+
- Node.js (for UI dev server)

## Build Plugin (AU + VST3)

```bash
cd plugin/
cmake -S . -B build
cmake --build build
```

This automatically installs to:
- AU: `~/Library/Audio/Plug-Ins/Components/Vocal Suite Pro.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/Vocal Suite Pro.vst3`

## Run UI Dev Server

```bash
cd /Users/wonkasworld/Downloads/project-vocal-suite-plugin-k660u1pj-ver-0yw1yzsg
npm run dev -- --port 5173 --strictPort
```

Plugin loads UI from `http://localhost:5173` (dev server must be running).

## Notes
- Logic AU WebView doesn't support embedded UI (ResourceProvider limitation)
- VST3 may support embedded mode in other DAWs
- Dev server mode is the working solution for Logic
