import React, { useState, useEffect } from 'react';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { toast } from 'sonner';
import { fishAudio } from '../../lib/fish-audio';

interface VoiceSettingsProps {
  onClose: () => void;
}

export const VoiceSettings: React.FC<VoiceSettingsProps> = ({ onClose }) => {
  const [apiKey, setApiKey] = useState('');
  const [isTestingKey, setIsTestingKey] = useState(false);

  useEffect(() => {
    const savedKey = localStorage.getItem('fish_audio_api_key');
    if (savedKey) {
      setApiKey(savedKey);
      fishAudio.setApiKey(savedKey);
    }
  }, []);

  const handleSaveKey = async () => {
    if (!apiKey.trim()) {
      toast.error('Please enter an API key');
      return;
    }

    setIsTestingKey(true);
    try {
      fishAudio.setApiKey(apiKey);
      await fishAudio.listVoices({ pageSize: 1 });
      
      localStorage.setItem('fish_audio_api_key', apiKey);
      toast.success('API key saved and verified!');
      onClose();
    } catch (error) {
      toast.error('Invalid API key', { 
        description: 'Could not connect to Fish Audio API' 
      });
    } finally {
      setIsTestingKey(false);
    }
  };

  const handleClearKey = () => {
    setApiKey('');
    localStorage.removeItem('fish_audio_api_key');
    fishAudio.setApiKey('');
    toast.info('API key cleared');
  };

  return (
    <div className="space-y-6 p-6">
      <div>
        <h3 className="text-lg font-bold text-white mb-2">Fish Audio API Settings</h3>
        <p className="text-sm text-muted-foreground">
          Enter your Fish Audio API key to enable voice cloning features.
        </p>
      </div>

      <div className="space-y-4">
        <div className="space-y-2">
          <Label htmlFor="apiKey" className="text-white">API Key</Label>
          <Input
            id="apiKey"
            type="password"
            value={apiKey}
            onChange={(e) => setApiKey(e.target.value)}
            placeholder="Enter your Fish Audio API key"
            className="bg-black/50 border-white/10 text-white"
          />
          <p className="text-xs text-muted-foreground">
            Get your API key from{' '}
            <a 
              href="https://fish.audio/dashboard" 
              target="_blank" 
              rel="noopener noreferrer"
              className="text-accent hover:underline"
            >
              fish.audio/dashboard
            </a>
          </p>
        </div>

        <div className="flex gap-2">
          <Button
            onClick={handleSaveKey}
            disabled={isTestingKey || !apiKey.trim()}
            className="flex-1 bg-accent hover:bg-accent/80 text-black font-bold"
          >
            {isTestingKey ? 'Verifying...' : 'Save & Verify'}
          </Button>
          <Button
            onClick={handleClearKey}
            variant="outline"
            className="border-white/10 text-white hover:bg-white/5"
          >
            Clear
          </Button>
        </div>
      </div>

      <div className="pt-4 border-t border-white/10">
        <h4 className="text-sm font-bold text-white mb-2">Features</h4>
        <ul className="text-xs text-muted-foreground space-y-1">
          <li>• Instant voice cloning from recordings</li>
          <li>• Create persistent voice models</li>
          <li>• Access Fish Audio voice library</li>
          <li>• Real-time voice conversion</li>
        </ul>
      </div>
    </div>
  );
};
