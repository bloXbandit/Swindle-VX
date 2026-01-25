import { blink } from './blink';

export interface Preset {
  id?: string;
  name: string;
  userId: string;
  settings: {
    pitchCorrection: number;
    pitchSpeed: number;
    pitchShift: number;
    pitchKey: string;
    pitchScale: string;
    activeNotes: string[];
    formant: number;
    breath: number;
    resonance: number;
    voiceModel: string;
    blend: number;
    inputGain: number;
    outputGain: number;
    globalMix: number;
    pitchEnabled: boolean;
    characterEnabled: boolean;
    modelEnabled: boolean;
  };
  createdAt?: string;
}

export class PresetManager {
  static async savePreset(name: string, settings: any) {
    const user = await blink.auth.me();
    if (!user) throw new Error('Authentication required');

    const preset = await (blink.db as any).presets.create({
      userId: user.id,
      name,
      settings: JSON.stringify(settings)
    });

    return preset;
  }

  static async listPresets(): Promise<Preset[]> {
    const user = await blink.auth.me();
    if (!user) return [];

    const records = await (blink.db as any).presets.list({
      where: { userId: user.id },
      orderBy: { createdAt: 'desc' }
    });

    return records.map((r: any) => ({
      id: r.id,
      name: r.name,
      userId: r.userId,
      settings: JSON.parse(r.settings),
      createdAt: r.createdAt
    }));
  }

  static async deletePreset(id: string) {
    await (blink.db as any).presets.delete(id);
  }
}
