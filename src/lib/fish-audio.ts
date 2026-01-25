interface ReferenceAudio {
  audio: ArrayBuffer;
  text: string;
}

interface VoiceModel {
  id: string;
  title: string;
  description?: string;
  tags?: string[];
  languages?: string[];
  visibility?: 'public' | 'unlist' | 'private';
}

interface VoiceListResponse {
  items: VoiceModel[];
  total: number;
}

class FishAudioClient {
  private apiKey: string = '';
  private baseUrl = 'https://api.fish.audio/v1';

  setApiKey(key: string) {
    this.apiKey = key;
  }

  hasApiKey(): boolean {
    return this.apiKey.length > 0;
  }

  private async request(endpoint: string, options: RequestInit = {}) {
    const headers = {
      'Authorization': `Bearer ${this.apiKey}`,
      'Content-Type': 'application/json',
      ...options.headers,
    };

    const response = await fetch(`${this.baseUrl}${endpoint}`, {
      ...options,
      headers,
    });

    if (!response.ok) {
      throw new Error(`Fish Audio API error: ${response.status} ${response.statusText}`);
    }

    return response;
  }

  async cloneVoiceInstant(audioBlob: Blob, transcript: string, targetText: string): Promise<Blob> {
    const audioBuffer = await audioBlob.arrayBuffer();
    const audioBase64 = btoa(String.fromCharCode(...new Uint8Array(audioBuffer)));

    const response = await this.request('/tts/convert', {
      method: 'POST',
      body: JSON.stringify({
        text: targetText,
        references: [{
          audio: audioBase64,
          text: transcript
        }]
      })
    });

    return await response.blob();
  }

  async createVoiceModel(
    title: string,
    audioSamples: Blob[],
    transcripts?: string[],
    options?: {
      description?: string;
      tags?: string[];
      visibility?: 'public' | 'unlist' | 'private';
      enhanceAudioQuality?: boolean;
    }
  ): Promise<VoiceModel> {
    const voices: string[] = [];
    
    for (const sample of audioSamples) {
      const buffer = await sample.arrayBuffer();
      const base64 = btoa(String.fromCharCode(...new Uint8Array(buffer)));
      voices.push(base64);
    }

    const body: any = {
      title,
      voices,
      description: options?.description,
      tags: options?.tags,
      visibility: options?.visibility || 'private',
      enhance_audio_quality: options?.enhanceAudioQuality || false
    };

    if (transcripts && transcripts.length === audioSamples.length) {
      body.texts = transcripts;
    }

    const response = await this.request('/voices', {
      method: 'POST',
      body: JSON.stringify(body)
    });

    return await response.json();
  }

  async listVoices(options?: {
    tags?: string[];
    language?: string;
    selfOnly?: boolean;
    pageSize?: number;
  }): Promise<VoiceListResponse> {
    const params = new URLSearchParams();
    
    if (options?.tags) {
      options.tags.forEach(tag => params.append('tag', tag));
    }
    if (options?.language) {
      params.append('language', options.language);
    }
    if (options?.selfOnly) {
      params.append('self_only', 'true');
    }
    if (options?.pageSize) {
      params.append('page_size', options.pageSize.toString());
    }

    const response = await this.request(`/voices?${params.toString()}`);
    return await response.json();
  }

  async getVoice(voiceId: string): Promise<VoiceModel> {
    const response = await this.request(`/voices/${voiceId}`);
    return await response.json();
  }

  async updateVoice(
    voiceId: string,
    updates: {
      title?: string;
      description?: string;
      visibility?: 'public' | 'unlist' | 'private';
      tags?: string[];
    }
  ): Promise<void> {
    await this.request(`/voices/${voiceId}`, {
      method: 'PATCH',
      body: JSON.stringify(updates)
    });
  }

  async deleteVoice(voiceId: string): Promise<void> {
    await this.request(`/voices/${voiceId}`, {
      method: 'DELETE'
    });
  }

  async convertWithVoice(voiceId: string, text: string): Promise<Blob> {
    const response = await this.request('/tts/convert', {
      method: 'POST',
      body: JSON.stringify({
        text,
        voice_id: voiceId
      })
    });

    return await response.blob();
  }
}

export const fishAudio = new FishAudioClient();
export type { VoiceModel, VoiceListResponse };
