import React, { useEffect, useRef } from 'react';
import { audioEngine } from '../../lib/AudioEngine';

export const SpectrumVisualizer: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationFrameId: number;
    const bars = 60;
    const dataArray = new Uint8Array(bars);

    const render = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      audioEngine.getAnalyzerData(dataArray);
      const barWidth = canvas.width / bars;
      
      for (let i = 0; i < bars; i++) {
        const val = dataArray[i] / 255;
        const h = val * canvas.height * 0.8;
        const x = i * barWidth;
        const y = canvas.height - h;

        const gradient = ctx.createLinearGradient(0, y, 0, canvas.height);
        gradient.addColorStop(0, 'hsl(180, 100%, 50%)');
        gradient.addColorStop(1, 'hsl(180, 100%, 20%)');

        ctx.fillStyle = gradient;
        ctx.globalAlpha = 0.5;
        ctx.fillRect(x + 1, y, barWidth - 2, h);
        ctx.globalAlpha = 1;
      }

      animationFrameId = requestAnimationFrame(render);
    };

    render();

    return () => {
      cancelAnimationFrame(animationFrameId);
    };
  }, []);

  return (
    <canvas 
      ref={canvasRef} 
      width={800} 
      height={150} 
      className="absolute inset-0 w-full h-full opacity-40 pointer-events-none" 
    />
  );
};

export const PitchVisualizer: React.FC = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationFrameId: number;
    const points: { x: number; y: number }[] = [];

    const render = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      // Detected pitch from engine
      const pitch = audioEngine.smoothedPitch;
      const target = audioEngine.targetPitch;
      
      let currentY = -100;
      let targetY = -100;
      
      const minFreq = 50;
      const maxFreq = 1000;
      
      if (pitch > 0) {
        const logPitch = Math.log2(pitch / minFreq) / Math.log2(maxFreq / minFreq);
        currentY = canvas.height - (logPitch * canvas.height);
      }
      
      if (target > 0) {
        const logTarget = Math.log2(target / minFreq) / Math.log2(maxFreq / minFreq);
        targetY = canvas.height - (logTarget * canvas.height);
      }
      
      points.push({ x: canvas.width, currentY, targetY });
      if (points.length > 200) points.shift();

      // Background Piano Lines
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.03)';
      ctx.lineWidth = 1;
      for (let i = 0; i < 12; i++) {
        const y = (i / 12) * canvas.height;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvas.width, y);
        ctx.stroke();
      }

      // Draw Target (Corrected) Pitch - Glowy line
      ctx.beginPath();
      ctx.strokeStyle = 'hsl(var(--accent) / 0.2)';
      ctx.lineWidth = 4;
      ctx.setLineDash([2, 4]);
      points.forEach((p, i) => {
        p.x -= 3;
        if (p.targetY > 0) {
          if (i === 0) ctx.moveTo(p.x, p.targetY);
          else ctx.lineTo(p.x, p.targetY);
        }
      });
      ctx.stroke();
      ctx.setLineDash([]);

      // Draw Current (Input) Pitch - Solid bright line
      ctx.beginPath();
      ctx.strokeStyle = 'hsl(var(--accent))';
      ctx.lineWidth = 2;
      ctx.shadowBlur = 15;
      ctx.shadowColor = 'hsl(var(--accent))';
      
      points.forEach((p, i) => {
        if (p.currentY > 0) {
          if (i === 0) ctx.moveTo(p.x, p.currentY);
          else ctx.lineTo(p.x, p.currentY);
        }
      });
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Pulse at current position
      if (pitch > 0) {
        ctx.beginPath();
        ctx.arc(canvas.width - 10, currentY, 4, 0, Math.PI * 2);
        ctx.fillStyle = 'hsl(var(--accent))';
        ctx.fill();
        ctx.shadowBlur = 20;
        ctx.shadowColor = 'hsl(var(--accent))';
        ctx.stroke();
      }

      animationFrameId = requestAnimationFrame(render);
    };

    render();

    return () => {
      cancelAnimationFrame(animationFrameId);
    };
  }, []);

  return (
    <canvas 
      ref={canvasRef} 
      width={800} 
      height={150} 
      className="absolute inset-0 w-full h-full pointer-events-none" 
    />
  );
};