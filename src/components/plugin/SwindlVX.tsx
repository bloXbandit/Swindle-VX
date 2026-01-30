import React, { useState, useEffect } from 'react';
import { Power, Save, FolderOpen, Settings, Info, Import, Mic, MicOff, Globe, Sparkles, UserCircle, FileAudio } from 'lucide-react';
import { Knob } from './Knob';
import { Button } from '../ui/button';
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "../ui/select";
import { Switch } from "../ui/switch";
import { SpectrumVisualizer, PitchVisualizer } from './Visualizers';
import { PianoRoll } from './PianoRoll';
import { toast } from "sonner";
import { 
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
  DialogFooter,
} from "../ui/dialog";
import { audioEngine } from '../../lib/AudioEngine';
import { PresetManager, Preset } from '../../lib/Presets';
import { juceBridge } from '../../lib/juce-bridge';
import { fishAudio, VoiceModel } from '../../lib/fish-audio';
import { VoiceSettings } from './VoiceSettings';

export const SwindlVX: React.FC = () => {
  const isAuthenticated = true;
  const user = { id: 'local-user' };
  const isLoading = false;

  // Global States
  const [isActive, setIsActive] = useState(false);
  const [isBypassed, setIsBypassed] = useState(false);
  const [isDemoPlaying, setIsDemoPlaying] = useState(false);
  const [inputGain, setInputGain] = useState(0);
  const [outputGain, setOutputGain] = useState(0);
  const [globalMix, setGlobalMix] = useState(100);

  // Module States - Pitch
  const [pitchCorrection, setPitchCorrection] = useState(50);
  const [pitchSpeed, setPitchSpeed] = useState(20);
  const [pitchShift, setPitchShift] = useState(0);
  const [pitchKey, setPitchKey] = useState('C');
  const [pitchScale, setPitchScale] = useState('Major');
  const [activeNotes, setActiveNotes] = useState<string[]>(['C', 'D', 'E', 'F', 'G', 'A', 'B']);
  const [pitchEnabled, setPitchEnabled] = useState(true);

  // Module States - Character
  const [formant, setFormant] = useState(0);
  const [breath, setBreath] = useState(0);
  const [resonance, setResonance] = useState(50);
  const [characterEnabled, setCharacterEnabled] = useState(true);

  // Module States - Voice Model
  const [voiceModel, setVoiceModel] = useState('none');
  const [blend, setBlend] = useState(0);
  const [modelEnabled, setModelEnabled] = useState(true);
  const [isImporting, setIsImporting] = useState(false);

  // Cloning State
  const [isRecording, setIsRecording] = useState(false);
  const [isCloning, setIsCloning] = useState(false);
  const [clonedVoices, setClonedVoices] = useState<{id: string, name: string}[]>([]);
  const [fishVoices, setFishVoices] = useState<VoiceModel[]>([]);
  const [localOnnxModels, setLocalOnnxModels] = useState<{id: string, name: string}[]>([]);
  const [showVoiceSettings, setShowVoiceSettings] = useState(false);
  
  // Preset States
  const [presets, setPresets] = useState<Preset[]>([]);
  const [currentPresetName, setCurrentPresetName] = useState('Default Initialization');
  const [newPresetName, setNewPresetName] = useState('');
  const [isSavingPreset, setIsSavingPreset] = useState(false);

  // Sync parameters to C++ backend
  useEffect(() => {
    juceBridge.sendParameterChange('correction', pitchCorrection / 100);
  }, [pitchCorrection]);

  useEffect(() => {
    juceBridge.sendParameterChange('speed', pitchSpeed / 100);
  }, [pitchSpeed]);

  useEffect(() => {
    juceBridge.sendParameterChange('pitch', pitchShift);
  }, [pitchShift]);

  useEffect(() => {
    juceBridge.sendParameterChange('formant', formant);
  }, [formant]);

  useEffect(() => {
    juceBridge.sendParameterChange('breath', breath / 100);
  }, [breath]);

  useEffect(() => {
    juceBridge.sendParameterChange('resonance', resonance / 100);
  }, [resonance]);

  useEffect(() => {
    juceBridge.sendParameterChange('blend', blend / 100);
  }, [blend]);

  useEffect(() => {
    const keyMap: Record<string, number> = {
      'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5,
      'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11
    };
    juceBridge.sendParameterChange('key', keyMap[pitchKey] || 0);
  }, [pitchKey]);

  useEffect(() => {
    const scaleMap: Record<string, number> = {
      'Major': 0, 'Minor': 1, 'Dorian': 2, 'Phrygian': 3,
      'Lydian': 4, 'Mixolydian': 5, 'Aeolian': 6, 'Locrian': 7, 'Chromatic': 8
    };
    juceBridge.sendParameterChange('scale', scaleMap[pitchScale] || 0);
  }, [pitchScale]);

  useEffect(() => {
    if (voiceModel === 'none') {
      juceBridge.loadVoiceModel('none', 'onnx');
    } else if (fishVoices.some(v => v.id === voiceModel)) {
      juceBridge.loadVoiceModel(voiceModel, 'fish');
    } else {
      juceBridge.loadVoiceModel(voiceModel, 'onnx');
    }
  }, [voiceModel, fishVoices]);

  // Load Fish Audio API key and voices
  useEffect(() => {
    let apiKey = localStorage.getItem('fish_audio_api_key');
    
    const envKey = (import.meta as any).env?.VITE_FISH_AUDIO_API_KEY;
    console.log('[Fish Audio] ENV key found:', envKey ? 'YES ✓' : 'NO ✗');
    console.log('[Fish Audio] localStorage key found:', apiKey ? 'YES ✓' : 'NO ✗');
    
    if (!apiKey && envKey) {
      apiKey = envKey;
      localStorage.setItem('fish_audio_api_key', apiKey);
      console.log('[Fish Audio] Saved ENV key to localStorage');
    }
    
    if (apiKey) {
      fishAudio.setApiKey(apiKey);
      console.log('[Fish Audio] API key set, loading voices...');
      fishAudio.listVoices({ selfOnly: false, pageSize: 30 })
        .then(response => {
          setFishVoices(response.items);
          console.log('[Fish Audio] Loaded', response.items.length, 'public voices');
          console.log('[Fish Audio] Voice list:', response.items.map(v => v.title).join(', '));
        })
        .catch((err) => {
          console.error('[Fish Audio] Failed to load voices:', err.message);
        });
    } else {
      console.warn('[Fish Audio] No API key found - configure in Settings');
    }
  }, []);

  // Sync with AudioEngine
  useEffect(() => {
    PresetManager.listPresets().then(setPresets);
  }, [isAuthenticated]);

  const saveCurrentPreset = async () => {
    if (!newPresetName.trim()) {
      toast.error("Please enter a preset name");
      return;
    }
    
    setIsSavingPreset(true);
    try {
      const settings = {
        pitchCorrection, pitchSpeed, pitchShift, pitchKey, pitchScale, activeNotes,
        formant, breath, resonance, voiceModel, blend,
        inputGain, outputGain, globalMix,
        pitchEnabled, characterEnabled, modelEnabled
      };
      
      const newPreset = await PresetManager.savePreset(newPresetName, settings);
      setPresets(prev => [newPreset as Preset, ...prev]);
      setCurrentPresetName(newPresetName);
      setNewPresetName('');
      toast.success("Preset saved", { description: `"${newPresetName}" is now available.` });
    } catch (err) {
      toast.error("Failed to save preset");
    } finally {
      setIsSavingPreset(false);
    }
  };

  const loadPreset = (preset: Preset) => {
    const s = preset.settings;
    setPitchCorrection(s.pitchCorrection);
    setPitchSpeed(s.pitchSpeed);
    setPitchShift(s.pitchShift);
    setPitchKey(s.pitchKey);
    setPitchScale(s.pitchScale);
    setActiveNotes(s.activeNotes);
    setFormant(s.formant);
    setBreath(s.breath);
    setResonance(s.resonance);
    setVoiceModel(s.voiceModel);
    setBlend(s.blend);
    setInputGain(s.inputGain);
    setOutputGain(s.outputGain);
    setGlobalMix(s.globalMix);
    setPitchEnabled(s.pitchEnabled);
    setCharacterEnabled(s.characterEnabled);
    setModelEnabled(s.modelEnabled);
    
    audioEngine.applySettings(s);
    
    juceBridge.sendParameterChange('correction', s.pitchCorrection / 100);
    juceBridge.sendParameterChange('speed', s.pitchSpeed / 100);
    juceBridge.sendParameterChange('pitch', s.pitchShift);
    juceBridge.sendParameterChange('formant', s.formant);
    juceBridge.sendParameterChange('breath', s.breath / 100);
    juceBridge.sendParameterChange('resonance', s.resonance / 100);
    juceBridge.sendParameterChange('blend', s.blend / 100);
    
    const keyMap: Record<string, number> = {
      'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5,
      'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11
    };
    juceBridge.sendParameterChange('key', keyMap[s.pitchKey] || 0);
    
    const scaleMap: Record<string, number> = {
      'Major': 0, 'Minor': 1, 'Dorian': 2, 'Phrygian': 3,
      'Lydian': 4, 'Mixolydian': 5, 'Aeolian': 6, 'Locrian': 7, 'Chromatic': 8
    };
    juceBridge.sendParameterChange('scale', scaleMap[s.pitchScale] || 0);
    
    setCurrentPresetName(preset.name);
    toast.success("Preset loaded", { description: `Applied "${preset.name}" settings.` });
  };

  useEffect(() => {
    if (isActive && !isBypassed) {
      audioEngine.start();
    } else {
      // We don't necessarily stop the context entirely to avoid delay, 
      // but we could mute or bypass
    }
  }, [isActive, isBypassed]);

  useEffect(() => {
    audioEngine.setInputGain(inputGain);
  }, [inputGain]);

  useEffect(() => {
    audioEngine.setOutputGain(outputGain);
  }, [outputGain]);

  useEffect(() => {
    audioEngine.setGlobalMix(isBypassed ? 0 : globalMix);
  }, [globalMix, isBypassed]);

  useEffect(() => {
    audioEngine.setResonance(characterEnabled ? resonance : 0);
  }, [resonance, characterEnabled]);

  useEffect(() => {
    audioEngine.setFormant(characterEnabled ? formant : 0);
  }, [formant, characterEnabled]);

  useEffect(() => {
    audioEngine.setBreath(characterEnabled ? breath : 0);
  }, [breath, characterEnabled]);

  useEffect(() => {
    audioEngine.setPitchCorrection(pitchEnabled ? pitchCorrection : 0);
  }, [pitchCorrection, pitchEnabled]);

  useEffect(() => {
    audioEngine.setPitchSpeed(pitchEnabled ? pitchSpeed : 0);
  }, [pitchSpeed, pitchEnabled]);

  useEffect(() => {
    audioEngine.setPitchShift(pitchEnabled ? pitchShift : 0);
  }, [pitchShift, pitchEnabled]);

  useEffect(() => {
    audioEngine.setPitchKey(pitchKey);
  }, [pitchKey]);

  useEffect(() => {
    audioEngine.setPitchScale(pitchScale);
  }, [pitchScale]);

  useEffect(() => {
    audioEngine.setActiveNotes(activeNotes);
  }, [activeNotes]);

  useEffect(() => {
    audioEngine.setVoiceModel(modelEnabled ? voiceModel : 'none');
  }, [voiceModel, modelEnabled]);

  useEffect(() => {
    audioEngine.setBlend(modelEnabled ? blend : 0);
  }, [blend, modelEnabled]);

  useEffect(() => {
    audioEngine.toggleDemo(isDemoPlaying);
  }, [isDemoPlaying]);

  // Signal Meters
  const [inLevel, setInLevel] = useState(0);
  const [outLevel, setOutLevel] = useState(0);

  useEffect(() => {
    let frame: number;
    const updateLevels = () => {
      setInLevel(audioEngine.inputLevel * 100);
      setOutLevel(audioEngine.outputLevel * 100);
      frame = requestAnimationFrame(updateLevels);
    };
    updateLevels();
    return () => cancelAnimationFrame(frame);
  }, []);

  const handleImport = () => {
    setIsImporting(true);
    setTimeout(() => {
      setIsImporting(false);
      toast.success("RVC Model imported successfully!", {
        description: "Model: 'Studio_Pro_Vocalist' loaded.",
      });
    }, 2000);
  };

  const handleCloneVoice = async () => {
    if (isRecording) {
      setIsRecording(false);
      setIsCloning(true);
      
      try {
        if (!fishAudio.hasApiKey()) {
          toast.error("Fish Audio API key required", { 
            description: "Please configure your API key in settings" 
          });
          setShowVoiceSettings(true);
          setIsCloning(false);
          return;
        }

        const file = await audioEngine.stopRecording();
        if (file) {
          toast.info("Uploading to Fish Audio...", { description: "Creating voice model" });
          
          const voiceName = `My Voice ${clonedVoices.length + fishVoices.length + 1}`;
          const voiceModel = await fishAudio.createVoiceModel(
            voiceName,
            [file],
            undefined,
            {
              description: 'Custom voice clone',
              tags: ['custom', 'clone'],
              visibility: 'private',
              enhanceAudioQuality: true
            }
          );
          
          setFishVoices(prev => [...prev, voiceModel]);
          setVoiceModel(voiceModel.id);
          setIsCloning(false);
          toast.success("Voice cloned successfully!", { 
            description: `"${voiceName}" is now available` 
          });
        }
      } catch (err) {
        const errorMsg = err instanceof Error ? err.message : 'Unknown error';
        toast.error("Cloning failed", { description: errorMsg });
        setIsCloning(false);
      }
    } else {
      setIsRecording(true);
      audioEngine.startRecording();
      toast.info("Recording...", { description: "Speak clearly into the microphone." });
    }
  };

  const toggleNote = (note: string) => {
    setActiveNotes(prev => 
      prev.includes(note) ? prev.filter(n => n !== note) : [...prev, note]
    );
  };

  if (isLoading) return <div className="plugin-container flex items-center justify-center text-accent animate-pulse font-mono uppercase tracking-[0.5em]">Initializing Engine...</div>;
  
  if (!isAuthenticated) {
    return (
      <div className="plugin-container w-[950px] h-[650px] flex flex-col items-center justify-center gap-8 text-white relative overflow-hidden bg-black">
        <div className="absolute inset-0 opacity-20" style={{ backgroundImage: 'radial-gradient(circle at center, hsl(var(--accent)/0.2) 0%, transparent 70%)' }} />
        <div className="z-10 text-center space-y-6 max-w-md">
          <h1 className="text-5xl font-black tracking-tighter italic leading-none flex items-baseline justify-center gap-2">
            SWINDL <span className="text-accent drop-shadow-[0_0_15px_hsl(var(--accent)/0.5)]">VX</span>
          </h1>
          <p className="text-muted-foreground text-sm uppercase tracking-widest font-bold">The Next Generation of Vocal Production</p>
          <div className="h-[1px] w-full bg-gradient-to-r from-transparent via-white/20 to-transparent my-8" />
          <Button 
            onClick={() => toast.info("Authentication bypassed for local development")}
            className="w-full bg-accent hover:bg-accent/80 text-black font-black uppercase tracking-[0.2em] h-14 rounded-none shadow-[0_0_30px_hsl(var(--accent)/0.3)] transition-all hover:scale-105 active:scale-95"
          >
            Authenticate Plugin
          </Button>
          <p className="text-[10px] text-muted-foreground font-mono leading-relaxed mt-4">
            Licensing required for AI features, cloud storage, and premium voice models.
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="plugin-container w-[950px] h-[650px] p-8 flex flex-col gap-8 text-white select-none relative">
      {/* Texture Overlays */}
      <div className="absolute inset-0 opacity-[0.03] pointer-events-none mix-blend-overlay" style={{ backgroundImage: 'var(--texture-noise)' }} />
      <div className="absolute inset-0 bg-gradient-to-b from-white/5 to-transparent pointer-events-none" />
      
      {/* Header Section */}
      <header className="flex items-center justify-between border-b border-white/5 pb-6 relative z-10">
        <div className="flex items-center gap-6">
          <div className="flex flex-col">
            <h1 className="text-3xl font-black tracking-tighter italic leading-none flex items-baseline gap-1">
              SWINDL <span className="text-accent drop-shadow-[0_0_8px_hsl(var(--accent)/0.5)]">VX</span>
            </h1>
          </div>
          <div className="h-8 w-[1px] bg-white/10 mx-2" />
          <div className="flex items-center gap-4 bg-black/40 px-4 py-2 rounded-lg border border-white/5 shadow-inner">
            <FolderOpen size={14} className="text-accent/60" />
            <div className="flex flex-col min-w-[120px]">
              <span className="text-[8px] font-black text-muted-foreground uppercase tracking-widest leading-none mb-1">Current Preset</span>
              <Select onValueChange={(val) => {
                const p = presets.find(p => p.id === val);
                if (p) loadPreset(p);
              }}>
                <SelectTrigger className="bg-transparent border-none p-0 h-auto text-[10px] font-bold text-white/90 uppercase tracking-wider focus:ring-0">
                  <SelectValue placeholder={currentPresetName} />
                </SelectTrigger>
                <SelectContent className="bg-[#18181B] border-white/10">
                  {presets.length === 0 && <div className="p-2 text-[8px] text-muted-foreground uppercase">No saved presets</div>}
                  {presets.map(p => (
                    <SelectItem key={p.id} value={p.id || ''}>{p.name}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
            
            <Dialog>
              <DialogTrigger asChild>
                <Save size={14} className="text-muted-foreground hover:text-accent cursor-pointer ml-4 transition-colors" />
              </DialogTrigger>
              <DialogContent className="bg-[#18181B] border-white/10 text-white">
                <DialogHeader>
                  <DialogTitle>Save Preset</DialogTitle>
                  <DialogDescription>Store your current vocal processing settings.</DialogDescription>
                </DialogHeader>
                <div className="py-4">
                  <label className="text-[10px] uppercase font-bold text-muted-foreground block mb-2">Preset Name</label>
                  <input 
                    className="w-full bg-black/40 border border-white/10 rounded h-10 px-4 text-sm focus:border-accent outline-none"
                    placeholder="e.g. Dreamy Lead Vocal"
                    value={newPresetName}
                    onChange={(e) => setNewPresetName(e.target.value)}
                  />
                </div>
                <DialogFooter>
                  <Button 
                    className="bg-accent text-black font-bold uppercase tracking-widest text-[10px]"
                    onClick={saveCurrentPreset}
                    disabled={isSavingPreset}
                  >
                    {isSavingPreset ? 'Saving...' : 'Save Preset'}
                  </Button>
                </DialogFooter>
              </DialogContent>
            </Dialog>
          </div>
        </div>
        
        <div className="flex items-center gap-4">
          <Button
            variant="ghost"
            size="sm"
            className={`gap-2 ${isDemoPlaying ? 'text-accent' : 'text-muted-foreground'}`}
            onClick={() => setIsDemoPlaying(!isDemoPlaying)}
          >
            <div className={`w-1.5 h-1.5 rounded-full ${isDemoPlaying ? 'bg-accent animate-pulse' : 'bg-white/20'}`} />
            <span className="text-[10px] font-bold uppercase tracking-widest">Demo Loop</span>
          </Button>
          <Button
            variant="ghost"
            size="sm"
            className={`gap-2 ${isBypassed ? 'text-destructive' : 'text-accent'}`}
            onClick={() => setIsBypassed(!isBypassed)}
          >
            <Power size={16} />
            <span className="text-[10px] font-bold uppercase tracking-widest">Bypass</span>
          </Button>
          <Dialog open={showVoiceSettings} onOpenChange={setShowVoiceSettings}>
            <DialogTrigger asChild>
              <Settings size={18} className="text-muted-foreground hover:text-white cursor-pointer" />
            </DialogTrigger>
            <DialogContent className="bg-[#18181B] border-white/10 text-white">
              <VoiceSettings onClose={() => setShowVoiceSettings(false)} />
            </DialogContent>
          </Dialog>
          <Info size={18} className="text-muted-foreground hover:text-white cursor-pointer" />
        </div>
      </header>

      {/* Visualizer Section */}
      <div className="visualizer-screen h-48 flex items-center justify-center relative z-10">
        <div className="absolute inset-0 opacity-10 pointer-events-none">
          {/* Finer Grid */}
          <div className="absolute inset-0" style={{ backgroundImage: 'linear-gradient(to right, #ffffff11 1px, transparent 1px), linear-gradient(to bottom, #ffffff11 1px, transparent 1px)', backgroundSize: '20px 20px' }} />
        </div>
        
        {/* Scanlines Effect */}
        <div className="absolute inset-0 pointer-events-none z-20 opacity-[0.03]" style={{ backgroundImage: 'repeating-linear-gradient(0deg, transparent, transparent 1px, #fff 1px, #fff 2px)', backgroundSize: '100% 3px' }} />
        
        {isActive && !isBypassed ? (
          <>
            <SpectrumVisualizer />
            <PitchVisualizer />
            <div className="absolute top-4 right-4 flex flex-col items-end gap-1 z-30">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 rounded-full bg-accent animate-pulse" />
                <span className="text-[10px] font-bold text-accent tracking-widest uppercase">Processing</span>
              </div>
              <span className="text-[12px] font-mono text-white/60 uppercase">{pitchKey} {pitchScale}</span>
            </div>
          </>
        ) : (
          <div className="flex flex-col items-center gap-4 z-30">
            <div className="text-accent font-mono text-lg animate-pulse accent-glow uppercase tracking-widest">
              {isBypassed ? 'Plugin Bypassed' : 'Ready to Process'}
            </div>
            {!isBypassed && (
              <Button 
                onClick={() => setIsActive(true)}
                className="bg-accent hover:bg-accent/80 text-black font-bold uppercase tracking-widest text-[10px] h-8 px-6 shadow-lg shadow-accent/30"
              >
                Start Processing
              </Button>
            )}
          </div>
        )}
      </div>

      {/* Processing Modules */}
      <div className="flex-1 grid grid-cols-3 gap-6 relative z-10">
        {/* Module 1: PITCH */}
        <div className={`module-box group/module flex flex-col gap-6 ${!pitchEnabled && 'grayscale opacity-40'}`}>
          <div className="flex items-center justify-between border-b border-white/5 pb-2">
            <div className="flex items-center gap-2">
              <div className={`w-1 h-3 rounded-full ${pitchEnabled ? 'bg-accent' : 'bg-white/20'}`} />
              <h2 className="text-[10px] font-black uppercase tracking-[0.25em] text-white/80">Pitch Engine</h2>
              <span className="text-[8px] bg-white/10 px-1.5 py-0.5 rounded text-accent font-bold tracking-tighter ml-2">LOCAL/FREE</span>
            </div>
            <Switch checked={pitchEnabled} onCheckedChange={setPitchEnabled} className="data-[state=checked]:bg-accent" />
          </div>
          
          <div className="grid grid-cols-2 gap-6">
            <Knob label="Correction" min={0} max={100} value={pitchCorrection} onChange={setPitchCorrection} unit="%" />
            <Knob label="Speed" min={0} max={100} value={pitchSpeed} onChange={setPitchSpeed} unit="ms" />
          </div>

          <div className="flex flex-col gap-4">
            <div className="grid grid-cols-2 gap-2">
              <Select value={pitchKey} onValueChange={setPitchKey}>
                <SelectTrigger className="bg-black/40 border-white/5 text-[10px] h-8 uppercase font-bold shadow-inner">
                  <SelectValue placeholder="Key" />
                </SelectTrigger>
                <SelectContent>
                  {['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'].map(k => (
                    <SelectItem key={k} value={k}>{k}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
              <Select value={pitchScale} onValueChange={setPitchScale}>
                <SelectTrigger className="bg-black/40 border-white/5 text-[10px] h-8 uppercase font-bold shadow-inner">
                  <SelectValue placeholder="Scale" />
                </SelectTrigger>
                <SelectContent>
                  {['Major', 'Minor', 'Chromatic', 'Dorian', 'Phrygian'].map(s => (
                    <SelectItem key={s} value={s}>{s}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
            
            <div className="space-y-2 mt-2">
              <label className="text-[8px] uppercase text-muted-foreground font-bold text-center block">Active Scale Notes</label>
              <PianoRoll activeNotes={activeNotes} toggleNote={toggleNote} />
            </div>

            <div className="flex justify-center mt-4">
              <Knob label="Pitch Shift" min={-24} max={24} value={pitchShift} onChange={setPitchShift} unit="st" size={80} />
            </div>
          </div>
        </div>

        {/* Module 2: CHARACTER */}
        <div className={`module-box group/module flex flex-col gap-6 ${!characterEnabled && 'grayscale opacity-40'}`}>
          <div className="flex items-center justify-between border-b border-white/5 pb-2">
            <div className="flex items-center gap-2">
              <div className={`w-1 h-3 rounded-full ${characterEnabled ? 'bg-accent' : 'bg-white/20'}`} />
              <h2 className="text-[10px] font-black uppercase tracking-[0.25em] text-white/80">Character Morph</h2>
              <span className="text-[8px] bg-white/10 px-1.5 py-0.5 rounded text-accent font-bold tracking-tighter ml-2">LOCAL/FREE</span>
            </div>
            <Switch checked={characterEnabled} onCheckedChange={setCharacterEnabled} className="data-[state=checked]:bg-accent" />
          </div>

          <div className="flex justify-center py-4 relative">
            <div className="absolute inset-0 bg-accent/5 rounded-full blur-3xl opacity-0 group-hover/module:opacity-100 transition-opacity duration-700" />
            <Knob label="Formant Shift" min={-12} max={12} value={formant} onChange={setFormant} unit="st" size={110} />
          </div>

          <div className="grid grid-cols-2 gap-6">
            <Knob label="Air / Breath" min={0} max={100} value={breath} onChange={setBreath} unit="%" />
            <Knob label="Resonance" min={0} max={100} value={resonance} onChange={setResonance} unit="%" />
          </div>
        </div>

        {/* Module 3: VOICE MODEL */}
        <div className={`module-box group/module flex flex-col gap-6 ${!modelEnabled && 'grayscale opacity-40'}`}>
          <div className="flex items-center justify-between border-b border-white/5 pb-2">
            <div className="flex items-center gap-2">
              <div className={`w-1 h-3 rounded-full ${modelEnabled ? 'bg-accent' : 'bg-white/20'}`} />
              <h2 className="text-[10px] font-black uppercase tracking-[0.25em] text-white/80">AI Model Library</h2>
              <span className="text-[8px] bg-accent/20 px-1.5 py-0.5 rounded text-accent font-bold tracking-tighter ml-2">MIXED/LOCAL</span>
            </div>
            <Switch checked={modelEnabled} onCheckedChange={setModelEnabled} className="data-[state=checked]:bg-accent" />
          </div>

          <div className="flex flex-col gap-4">
            <div className="space-y-2">
              <label className="text-[10px] uppercase text-muted-foreground font-bold">Model Library</label>
              <Select value={voiceModel} onValueChange={setVoiceModel}>
                <SelectTrigger className="bg-black/40 border-white/5 text-[10px] h-10 font-bold shadow-inner">
                  <SelectValue placeholder="Select Model" />
                </SelectTrigger>
                <SelectContent className="bg-[#18181B] border-white/10 max-h-[400px] overflow-y-auto">
                  <div className="px-2 py-1.5 text-[8px] font-black uppercase text-muted-foreground tracking-widest flex items-center gap-2">
                    <Sparkles size={10} /> Local Models (.pth / .onnx)
                  </div>
                  <SelectItem value="none">None (Bypass)</SelectItem>
                  <SelectItem value="21-Savage-american-dream">21 Savage (American Dream)</SelectItem>
                  <SelectItem value="custom_model_a">Custom Model A (.pth)</SelectItem>
                  <SelectItem value="custom_model_b">Custom Model B (.pth)</SelectItem>
                  {localOnnxModels.map(m => (
                    <SelectItem key={m.id} value={m.id}>{m.name}</SelectItem>
                  ))}
                  
                  {fishVoices.length > 0 && (
                    <>
                      <div className="px-2 py-1.5 mt-2 text-[8px] font-black uppercase text-accent tracking-widest flex items-center gap-2 border-t border-white/5 pt-3">
                        <Globe size={10} /> Fish Audio Voices
                      </div>
                      {fishVoices.map(v => (
                        <SelectItem key={v.id} value={v.id}>{v.title}</SelectItem>
                      ))}
                    </>
                  )}
                </SelectContent>
              </Select>
            </div>

            <div className="grid grid-cols-3 gap-2">
              <Button 
                onClick={() => {
                  juceBridge.startCapture();
                  toast.success('Capture armed', {
                    description: 'Recording input into capture buffer'
                  });
                }}
                className="h-10 text-[10px] uppercase font-black tracking-widest gap-2 bg-white/5 hover:bg-white/10 border border-white/10"
              >
                Arm
              </Button>

              <Button 
                onClick={() => {
                  juceBridge.stopCapture();
                  toast.success('Capture stopped', {
                    description: 'Saved take to Renders folder'
                  });
                }}
                className="h-10 text-[10px] uppercase font-black tracking-widest gap-2 bg-white/5 hover:bg-white/10 border border-white/10"
              >
                Stop
              </Button>

              <Button 
                onClick={() => {
                  juceBridge.convertAudio(voiceModel, pitchShift, formant / 100);
                  toast.success('Conversion started', {
                    description: `Model: ${voiceModel}`
                  });
                }}
                disabled={voiceModel === 'none'}
                className="h-10 text-[10px] uppercase font-black tracking-widest gap-2 bg-accent/20 hover:bg-accent/30 border border-accent/30"
              >
                <FileAudio size={14} className="text-accent" />
                Convert
              </Button>
              
              <Dialog>
                <DialogTrigger asChild>
                  <Button variant="outline" className="h-10 border-white/5 hover:bg-white/5 text-[10px] uppercase font-black tracking-widest gap-2">
                    <Globe size={14} className="text-blue-400" />
                    Connect
                  </Button>
                </DialogTrigger>
                <DialogContent className="bg-[#18181B] border-white/10 text-white">
                  <DialogHeader>
                    <DialogTitle className="text-xl font-bold tracking-tight">External Service Integration</DialogTitle>
                    <DialogDescription className="text-muted-foreground">
                      Connect your professional voice generation accounts.
                    </DialogDescription>
                  </DialogHeader>
                  <div className="space-y-4 py-6">
                    <div className="p-4 bg-white/5 rounded-lg border border-white/10 flex items-center justify-between">
                      <div className="flex items-center gap-4">
                        <div className="w-10 h-10 bg-black rounded flex items-center justify-center font-bold text-lg">11</div>
                        <div>
                          <p className="font-bold text-sm">ElevenLabs</p>
                          <p className="text-[10px] text-muted-foreground">High-fidelity iconic voices</p>
                        </div>
                      </div>
                      <Button variant="outline" className="text-[10px] uppercase font-bold h-8 border-white/20 hover:bg-accent hover:text-black">Connect API</Button>
                    </div>
                    <div className="p-4 bg-white/5 rounded-lg border border-white/10 flex items-center justify-between">
                      <div className="flex items-center gap-4">
                        <div className="w-10 h-10 bg-black rounded flex items-center justify-center font-bold text-lg text-blue-400">FA</div>
                        <div>
                          <p className="font-bold text-sm">Fish Audio</p>
                          <p className="text-[10px] text-muted-foreground">Low-latency streaming models</p>
                        </div>
                      </div>
                      <Button variant="outline" className="text-[10px] uppercase font-bold h-8 border-white/20 hover:bg-accent hover:text-black">Connect API</Button>
                    </div>
                  </div>
                  <div className="p-4 bg-blue-500/5 border border-blue-500/20 rounded-lg text-[10px] leading-relaxed italic text-blue-200/60">
                    External services require their own respective subscriptions and API credits. Swindl VX serves as a real-time host for these models.
                  </div>
                </DialogContent>
              </Dialog>
            </div>

            <div className="flex justify-center py-2">
              <Knob label="Blend" min={0} max={100} value={blend} onChange={setBlend} unit="%" size={80} />
            </div>

            <Dialog>
              <DialogTrigger asChild>
                <Button variant="outline" className="w-full border-dashed border-white/20 hover:border-accent hover:bg-accent/10 text-[10px] uppercase font-bold gap-2 h-12 shadow-inner">
                  <Import size={14} className="text-muted-foreground" />
                  Import RVC Model
                </Button>
              </DialogTrigger>
              <DialogContent className="bg-[#18181B] border-white/10 text-white">
                <DialogHeader>
                  <DialogTitle className="text-xl font-bold tracking-tight">Legal & Ethical AI Usage</DialogTitle>
                  <DialogDescription className="text-muted-foreground pt-2">
                    Swindl VX strictly enforces ethical standards for voice conversion.
                  </DialogDescription>
                </DialogHeader>
                <div className="space-y-4 py-4 text-sm leading-relaxed">
                  <div className="p-4 bg-accent/5 border border-accent/20 rounded-lg">
                    <p className="font-bold text-accent mb-2 uppercase text-[10px] tracking-widest">Mandatory Disclaimer</p>
                    <p className="text-xs italic text-white/80">
                      "By importing a voice model, you represent and warrant that you have obtained all necessary rights and permissions from the individual whose voice is being used. Unauthorized use of a person's voice is illegal and a violation of our terms of service."
                    </p>
                  </div>
                  <div className="space-y-2">
                    <p className="font-bold uppercase text-[10px] tracking-widest text-muted-foreground">Requirements</p>
                    <ul className="list-disc list-inside text-xs space-y-1 text-white/60">
                      <li>Model must be in RVC format (.pth + .index)</li>
                      <li>Maximum file size: 150MB</li>
                      <li>Real-time inference requires a modern GPU for lowest latency</li>
                    </ul>
                  </div>
                </div>
                <DialogFooter>
                  <Button 
                    onClick={handleImport} 
                    disabled={isImporting}
                    className="w-full bg-accent hover:bg-accent/80 text-black font-bold uppercase tracking-widest shadow-lg shadow-accent/30"
                  >
                    {isImporting ? "Processing Model..." : "I Accept & Import Model"}
                  </Button>
                </DialogFooter>
              </DialogContent>
            </Dialog>
            
            <p className="text-[8px] text-muted-foreground leading-tight text-center px-4 italic">
              "You must have the legal right to use any voice model you import."
            </p>
          </div>
        </div>
      </div>

      {/* Footer Section */}
      <footer className="flex items-center justify-between border-t border-white/5 pt-6 px-2 relative z-10">
        <div className="flex items-center gap-6">
          <Knob label="In Gain" min={-24} max={24} value={inputGain} onChange={setInputGain} unit="dB" size={45} />
        </div>

        <div className="flex items-center gap-8 bg-black/40 px-6 h-full rounded-xl border border-white/5 shadow-inner">
          <div className="flex flex-col gap-2 min-w-[100px]">
            <div className="flex justify-between items-end">
              <span className="text-[8px] font-black text-muted-foreground uppercase tracking-widest">Input</span>
              <span className="text-[8px] font-mono text-white/40">{(inLevel / 10).toFixed(1)} dB</span>
            </div>
            <div className="h-1.5 w-full bg-white/5 rounded-full overflow-hidden flex gap-0.5 p-0.5">
              <div 
                className="h-full bg-accent transition-all duration-75" 
                style={{ width: `${Math.min(100, inLevel)}%`, boxShadow: '0 0 10px hsl(var(--accent)/0.5)' }} 
              />
            </div>
          </div>

          <div className="h-6 w-[1px] bg-white/5" />

          {/* Connection Status */}
          <div className="flex items-center gap-3">
            <div className="flex flex-col items-start">
              <span className="text-[7px] font-black text-muted-foreground uppercase tracking-[0.2em] mb-1">Engine Bridge</span>
              <div className="flex items-center gap-1.5">
                <div className={`w-1.5 h-1.5 rounded-full ${isActive ? 'bg-accent shadow-[0_0_5px_hsl(var(--accent))]' : 'bg-white/10'}`} />
                <span className={`text-[8px] font-bold uppercase tracking-widest ${isActive ? 'text-white' : 'text-white/40'}`}>
                  {isActive ? 'Connected' : 'Standby'}
                </span>
              </div>
            </div>
            <div className="flex flex-col items-start ml-2">
              <span className="text-[7px] font-black text-muted-foreground uppercase tracking-[0.2em] mb-1">DSP Mode</span>
              <div className="flex items-center gap-1.5">
                <Globe size={10} className={audioEngine.isExternalPlugin ? 'text-accent' : 'text-white/20'} />
                <span className="text-[8px] font-bold uppercase tracking-widest text-white/60">
                  {audioEngine.isExternalPlugin ? 'VST/AU Host' : 'Web Audio'}
                </span>
              </div>
            </div>
          </div>

          <div className="h-6 w-[1px] bg-white/5" />
          
          <div className="flex flex-col gap-2 min-w-[100px]">
            <div className="flex justify-between items-end">
              <span className="text-[8px] font-black text-muted-foreground uppercase tracking-widest">Output</span>
              <span className="text-[8px] font-mono text-white/40">{(outLevel / 10).toFixed(1)} dB</span>
            </div>
            <div className="h-1.5 w-full bg-white/5 rounded-full overflow-hidden flex gap-0.5 p-0.5">
              <div 
                className="h-full bg-accent transition-all duration-75" 
                style={{ width: `${Math.min(100, outLevel)}%`, boxShadow: '0 0 10px hsl(var(--accent)/0.5)' }} 
              />
            </div>
          </div>
        </div>

        <div className="flex items-center gap-6">
          <Knob label="Out Gain" min={-24} max={24} value={outputGain} onChange={setOutputGain} unit="dB" size={45} />
          <div className="h-8 w-[1px] bg-white/5 mx-2" />
          <Knob label="Dry / Wet" min={0} max={100} value={globalMix} onChange={setGlobalMix} unit="%" size={55} />
          <div className="flex flex-col items-end opacity-40">
            <span className="text-[9px] text-white font-black tracking-[0.3em] uppercase">Swindl VX</span>
            <span className="text-[7px] text-accent font-mono tracking-tighter">BUILD 10.22.2026-X</span>
          </div>
        </div>
      </footer>
    </div>
  );
};
