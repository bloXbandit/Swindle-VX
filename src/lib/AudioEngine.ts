export class AudioEngine {
  private ctx: AudioContext | null = null;
  private micStream: MediaStream | null = null;
  private micNode: MediaStreamAudioSourceNode | null = null;
  private inputGain: GainNode | null = null;
  private outputGain: GainNode | null = null;
  private globalMix: GainNode | null = null;
  private dryGain: GainNode | null = null;
  private wetGain: GainNode | null = null;
  private analyzerNode: AnalyserNode | null = null;
  private scriptProcessor: ScriptProcessorNode | null = null;
  private demoSource: AudioBufferSourceNode | null = null;
  private demoBuffer: AudioBuffer | null = null;

  // Real-time data
  public currentPitch = 0;
  public smoothedPitch = 0;
  public targetPitch = 0;
  public inputLevel = 0;
  public outputLevel = 0;
  
  // Settings
  private pitchCorrection = 50;
  private pitchSpeed = 20;
  private pitchShift = 0;
  private pitchKey = 'C';
  private pitchScale = 'Major';
  private activeNotes: string[] = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  private voiceModel = 'none';
  private blend = 0;

  // Modules
  private mainProcessor: ScriptProcessorNode | null = null;
  private saturationNode: WaveShaperNode | null = null;
  private formantFilter: BiquadFilterNode | null = null;
  private resonanceFilter: BiquadFilterNode | null = null;
  private breathGain: GainNode | null = null;
  private breathNoise: AudioBufferSourceNode | null = null;

  private isStarted = false;

  private notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  public isExternalPlugin = false;

  // Recording for Cloning
  private mediaRecorder: MediaRecorder | null = null;
  private recordedChunks: Blob[] = [];

  constructor() {
    this.ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
    
    // Check if running inside JUCE WebBrowserComponent
    if (typeof (window as any).juce !== 'undefined') {
      this.isExternalPlugin = true;
      console.log('Running in VST/AU Plugin Mode');
    }
    
    this.setupNodes();
  }

  // Update parameters to sync with C++ backend if in plugin mode
  private setParameter(name: string, value: number | string) {
    if (this.isExternalPlugin) {
      (window as any).juce.postMessage(JSON.stringify({
        type: 'parameterChange',
        name,
        value
      }));
    }
  }

  async start() {
    if (this.isStarted) return;
    if (!this.ctx) return;

    if (this.ctx.state === 'suspended') {
      await this.ctx.resume();
    }

    try {
      this.micStream = await navigator.mediaDevices.getUserMedia({ audio: true });
      this.micNode = this.ctx.createMediaStreamSource(this.micStream);
      this.micNode.connect(this.inputGain!);
      this.isStarted = true;
    } catch (err) {
      console.warn('Mic input denied, running in internal synth mode');
      this.isStarted = true;
    }
  }

  private setupNodes() {
    if (!this.ctx) return;

    this.inputGain = this.ctx.createGain();
    this.outputGain = this.ctx.createGain();
    this.globalMix = this.ctx.createGain();
    this.dryGain = this.ctx.createGain();
    this.wetGain = this.ctx.createGain();
    this.analyzerNode = this.ctx.createAnalyser();
    this.analyzerNode.fftSize = 1024;

    // Saturation for that "Pro" analog feel
    this.saturationNode = this.ctx.createWaveShaper();
    this.saturationNode.curve = this.makeDistortionCurve(40);

    // Breath Simulation
    const bufferSize = 2 * this.ctx.sampleRate;
    const noiseBuffer = this.ctx.createBuffer(1, bufferSize, this.ctx.sampleRate);
    const output = noiseBuffer.getChannelData(0);
    for (let i = 0; i < bufferSize; i++) output[i] = Math.random() * 2 - 1;
    
    this.breathNoise = this.ctx.createBufferSource();
    this.breathNoise.buffer = noiseBuffer;
    this.breathNoise.loop = true;
    this.breathGain = this.ctx.createGain();
    this.breathGain.gain.value = 0;
    this.breathNoise.connect(this.breathGain);
    this.breathNoise.start();

    // Filters
    this.resonanceFilter = this.ctx.createBiquadFilter();
    this.resonanceFilter.type = 'peaking';
    this.resonanceFilter.frequency.value = 2500;
    this.resonanceFilter.Q.value = 1.0;
    this.resonanceFilter.gain.value = 0;

    this.formantFilter = this.ctx.createBiquadFilter();
    this.formantFilter.type = 'lowpass';
    this.formantFilter.frequency.value = 20000;

    // Main Processor
    this.mainProcessor = this.ctx.createScriptProcessor(2048, 1, 1);
    this.mainProcessor.onaudioprocess = (e) => {
      const input = e.inputBuffer.getChannelData(0);
      const output = e.outputBuffer.getChannelData(0);
      
      let sum = 0;
      for (let i = 0; i < input.length; i++) {
        // Simple robotic simulation of pitch/formant processing
        let val = input[i];
        
        // Basic saturation in code for better control
        if (this.pitchShift !== 0) {
          // Add some harmonic content to simulate shift if we can't do full FFT here
          val += Math.sin(i * (this.pitchShift / 12)) * 0.01;
        }

        output[i] = val;
        sum += val * val;
      }
      this.inputLevel = Math.sqrt(sum / input.length);
      this.outputLevel = this.inputLevel; // Fix: No simulated loss

      this.detectPitch(input);
    };

    // Routing
    this.inputGain.connect(this.dryGain);
    this.inputGain.connect(this.mainProcessor);
    
    this.mainProcessor.connect(this.saturationNode);
    this.saturationNode.connect(this.resonanceFilter);
    this.resonanceFilter.connect(this.formantFilter);
    this.breathGain.connect(this.formantFilter);
    
    this.formantFilter.connect(this.wetGain);
    
    this.dryGain.connect(this.globalMix);
    this.wetGain.connect(this.globalMix);
    
    this.globalMix.connect(this.outputGain);
    this.outputGain.connect(this.analyzerNode);
    this.analyzerNode.connect(this.ctx.destination);
  }

  private makeDistortionCurve(amount: number) {
    const k = amount;
    const n_samples = 44100;
    const curve = new Float32Array(n_samples);
    const deg = Math.PI / 180;
    for (let i = 0; i < n_samples; ++i) {
      const x = (i * 2) / n_samples - 1;
      curve[i] = ((3 + k) * x * 20 * deg) / (Math.PI + k * Math.abs(x));
    }
    return curve;
  }

  private detectPitch(data: Float32Array) {
    // Optimized YIN-like autocorrelation for smoother UI feedback
    let bestOffset = -1;
    let bestCorrelation = 0;
    
    // Search range: 80Hz to 2000Hz
    const minOffset = Math.floor(this.ctx!.sampleRate / 2000);
    const maxOffset = Math.floor(this.ctx!.sampleRate / 80);
    
    // Downsample for faster processing if needed, but for 2048 it's okay
    for (let offset = minOffset; offset < maxOffset; offset++) {
      let correlation = 0;
      for (let i = 0; i < data.length - offset; i++) {
        correlation += data[i] * data[i + offset];
      }
      
      // Normalize correlation
      if (correlation > bestCorrelation) {
        bestCorrelation = correlation;
        bestOffset = offset;
      }
    }

    if (bestOffset !== -1 && bestCorrelation > 0.1) {
      const freq = this.ctx!.sampleRate / bestOffset;
      
      // Frequency smoothing
      this.currentPitch = freq;
      this.smoothedPitch = this.smoothedPitch * 0.7 + freq * 0.3;
      
      // Calculate correction target
      const target = this.calculateTargetPitch(this.smoothedPitch);
      
      // Apply correction amount (0-100%)
      const amount = this.pitchCorrection / 100;
      this.targetPitch = this.smoothedPitch * (1 - amount) + target * amount;
    } else {
      this.currentPitch = 0;
      this.targetPitch = 0;
    }
  }

  // Preset Application
  applySettings(settings: any) {
    this.setPitchCorrection(settings.pitchCorrection);
    this.setPitchSpeed(settings.pitchSpeed);
    this.setPitchShift(settings.pitchShift);
    this.setPitchKey(settings.pitchKey);
    this.setPitchScale(settings.pitchScale);
    this.setActiveNotes(settings.activeNotes);
    this.setFormant(settings.formant);
    this.setBreath(settings.breath);
    this.setResonance(settings.resonance);
    this.setVoiceModel(settings.voiceModel);
    this.setBlend(settings.blend);
    this.setInputGain(settings.inputGain);
    this.setOutputGain(settings.outputGain);
    this.setGlobalMix(settings.globalMix);
  }

  private calculateTargetPitch(freq: number): number {
    if (freq <= 0) return 0;
    
    // Map freq to midi note
    const midi = 69 + 12 * Math.log2(freq / 440);
    const roundedMidi = Math.round(midi);
    const noteName = this.notes[roundedMidi % 12];
    
    // If note is in active scale, snap to it
    if (this.activeNotes.includes(noteName)) {
      return 440 * Math.pow(2, (roundedMidi - 69) / 12);
    }
    
    // Otherwise find closest active note
    let closestMidi = roundedMidi;
    for (let i = 1; i < 12; i++) {
      if (this.activeNotes.includes(this.notes[(roundedMidi + i) % 12])) {
        closestMidi = roundedMidi + i;
        break;
      }
      if (this.activeNotes.includes(this.notes[(roundedMidi - i + 12) % 12])) {
        closestMidi = roundedMidi - i;
        break;
      }
    }
    
    return 440 * Math.pow(2, (closestMidi - 69) / 12);
  }

  async loadDemo() {
    if (!this.ctx) return;
    try {
      const response = await fetch('https://cdn.pixabay.com/audio/2022/03/10/audio_c36294e073.mp3');
      const arrayBuffer = await response.arrayBuffer();
      this.demoBuffer = await this.ctx.decodeAudioData(arrayBuffer);
    } catch (err) {
      console.error('Failed to load demo:', err);
    }
  }

  toggleDemo(play: boolean) {
    if (!this.ctx || !this.inputGain) return;

    if (play) {
      if (!this.demoBuffer) {
        this.loadDemo().then(() => this.toggleDemo(true));
        return;
      }
      this.demoSource = this.ctx.createBufferSource();
      this.demoSource.buffer = this.demoBuffer;
      this.demoSource.loop = true;
      this.demoSource.connect(this.inputGain);
      this.demoSource.start();
    } else {
      if (this.demoSource) {
        this.demoSource.stop();
        this.demoSource.disconnect();
        this.demoSource = null;
      }
    }
  }

  setBreath(value: number) {
    if (this.breathGain) {
      this.breathGain.gain.setTargetAtTime(value / 200, this.ctx!.currentTime, 0.01);
    }
  }

  setPitchCorrection(value: number) {
    this.pitchCorrection = value;
    this.setParameter('correction', value / 100);
  }

  setPitchSpeed(value: number) {
    this.pitchSpeed = value;
    this.setParameter('pitchSpeed', value);
  }

  setPitchShift(value: number) {
    this.pitchShift = value;
    this.setParameter('pitch', value);
    // In a real VST/AU, this would use a high-quality phase vocoder.
    // In Web Audio, we'll simulate the "Character" change associated with pitch shifting.
    if (this.resonanceFilter && this.ctx) {
      const shiftFreq = 2500 * Math.pow(2, value / 24);
      this.resonanceFilter.frequency.setTargetAtTime(shiftFreq, this.ctx.currentTime, 0.1);
    }
  }

  setPitchKey(value: string) {
    this.pitchKey = value;
  }

  setPitchScale(value: string) {
    this.pitchScale = value;
  }

  setActiveNotes(value: string[]) {
    this.activeNotes = value;
  }

  setVoiceModel(value: string) {
    this.voiceModel = value;
    this.setParameter('voiceModel', value);
    if (this.resonanceFilter && this.ctx) {
      switch (value) {
        case 'singer_m':
          this.resonanceFilter.frequency.setTargetAtTime(400, this.ctx.currentTime, 0.1);
          this.resonanceFilter.gain.setTargetAtTime(6, this.ctx.currentTime, 0.1);
          break;
        case 'singer_f':
          this.resonanceFilter.frequency.setTargetAtTime(1200, this.ctx.currentTime, 0.1);
          this.resonanceFilter.gain.setTargetAtTime(4, this.ctx.currentTime, 0.1);
          break;
        case 'ethereal':
          this.resonanceFilter.frequency.setTargetAtTime(5000, this.ctx.currentTime, 0.1);
          this.resonanceFilter.gain.setTargetAtTime(8, this.ctx.currentTime, 0.1);
          break;
        case 'vintage':
          this.resonanceFilter.frequency.setTargetAtTime(2000, this.ctx.currentTime, 0.1);
          this.resonanceFilter.gain.setTargetAtTime(-10, this.ctx.currentTime, 0.1);
          break;
        default:
          this.resonanceFilter.frequency.setTargetAtTime(2500, this.ctx.currentTime, 0.1);
          this.resonanceFilter.gain.setTargetAtTime(0, this.ctx.currentTime, 0.1);
      }
    }
  }

  setBlend(value: number) {
    this.blend = value;
    this.setParameter('blend', value / 100);
    if (this.wetGain && this.dryGain && this.ctx) {
      const ratio = value / 100;
      this.wetGain.gain.setTargetAtTime(ratio, this.ctx.currentTime, 0.01);
      this.dryGain.gain.setTargetAtTime(1 - ratio, this.ctx.currentTime, 0.01);
    }
  }

  // Parameter Setters
  setInputGain(value: number) {
    if (this.inputGain && this.ctx) {
      const gain = Math.pow(10, value / 20);
      this.inputGain.gain.setTargetAtTime(gain, this.ctx.currentTime, 0.01);
    }
  }

  // Recording Methods
  async startRecording() {
    if (!this.micStream) return;
    this.recordedChunks = [];
    this.mediaRecorder = new MediaRecorder(this.micStream);
    this.mediaRecorder.ondataavailable = (e) => {
      if (e.data.size > 0) this.recordedChunks.push(e.data);
    };
    this.mediaRecorder.start();
  }

  async stopRecording(): Promise<File | null> {
    return new Promise((resolve) => {
      if (!this.mediaRecorder) return resolve(null);
      this.mediaRecorder.onstop = () => {
        const blob = new Blob(this.recordedChunks, { type: 'audio/wav' });
        const file = new File([blob], `clone-sample-${Date.now()}.wav`, { type: 'audio/wav' });
        resolve(file);
      };
      this.mediaRecorder.stop();
    });
  }

  setOutputGain(value: number) {
    if (this.outputGain && this.ctx) {
      const gain = Math.pow(10, value / 20);
      this.outputGain.gain.setTargetAtTime(gain, this.ctx.currentTime, 0.01);
    }
  }

  setGlobalMix(value: number) {
    if (this.globalMix && this.ctx) {
      const ratio = value / 100;
      this.globalMix.gain.setTargetAtTime(ratio, this.ctx.currentTime, 0.01);
    }
  }

  setResonance(value: number) {
    if (this.resonanceFilter && this.ctx) {
      this.resonanceFilter.gain.setTargetAtTime(value / 4, this.ctx.currentTime, 0.01);
      this.resonanceFilter.Q.setTargetAtTime(1 + (value / 20), this.ctx.currentTime, 0.01);
    }
  }

  setFormant(value: number) {
    this.setParameter('formant', value);
    if (this.formantFilter && this.ctx) {
      const freq = 20000 * Math.pow(2, -value / 12);
      this.formantFilter.frequency.setTargetAtTime(Math.max(100, Math.min(20000, freq)), this.ctx.currentTime, 0.01);
    }
  }

  getAnalyzerData(dataArray: Uint8Array) {
    if (this.analyzerNode) {
      this.analyzerNode.getByteFrequencyData(dataArray);
    }
  }

  stop() {
    if (this.micStream) {
      this.micStream.getTracks().forEach(track => track.stop());
    }
    if (this.ctx) {
      this.ctx.close();
    }
    this.isStarted = false;
  }
}

export const audioEngine = new AudioEngine();