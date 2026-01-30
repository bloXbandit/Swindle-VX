type ParameterChangeCallback = (name: string, value: number) => void;

class JUCEBridge {
  private listeners: Set<ParameterChangeCallback> = new Set();

  sendParameterChange(name: string, value: number) {
    if (typeof window !== 'undefined' && (window as any).__JUCE__) {
      (window as any).__JUCE__.backend.emitEvent('parameterChange', { 
        type: 'parameterChange',
        name, 
        value 
      });
    }
  }

  loadVoiceModel(modelId: string, modelType: 'onnx' | 'fish') {
    if (typeof window !== 'undefined' && (window as any).__JUCE__) {
      (window as any).__JUCE__.backend.emitEvent('loadModel', { 
        type: 'loadModel',
        modelId,
        modelType
      });
    }
  }

  convertAudio(model: string, pitchShift: number, formantShift: number) {
    if (typeof window !== 'undefined' && (window as any).__JUCE__) {
      (window as any).__JUCE__.backend.emitEvent('convertAudio', { 
        type: 'convertAudio',
        model,
        pitchShift,
        formantShift
      });
    }
  }

  startCapture() {
    if (typeof window !== 'undefined' && (window as any).__JUCE__) {
      (window as any).__JUCE__.backend.emitEvent('startCapture', {
        type: 'startCapture'
      });
    }
  }

  stopCapture() {
    if (typeof window !== 'undefined' && (window as any).__JUCE__) {
      (window as any).__JUCE__.backend.emitEvent('stopCapture', {
        type: 'stopCapture'
      });
    }
  }

  onParameterChange(callback: ParameterChangeCallback) {
    this.listeners.add(callback);
    return () => this.listeners.delete(callback);
  }

  private handleParameterUpdate(name: string, value: number) {
    this.listeners.forEach(cb => cb(name, value));
  }

  init() {
    if (typeof window !== 'undefined') {
      (window as any).__JUCE_BRIDGE__ = this;
    }
  }
}

export const juceBridge = new JUCEBridge();
juceBridge.init();
