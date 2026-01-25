import React, { useState, useEffect, useRef } from 'react';

interface KnobProps {
  label: string;
  min: number;
  max: number;
  value: number;
  onChange: (value: number) => void;
  unit?: string;
  size?: number;
}

export const Knob: React.FC<KnobProps> = ({
  label,
  min,
  max,
  value,
  onChange,
  unit = '',
  size = 60,
}) => {
  const [isDragging, setIsDragging] = useState(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  const angle = ((value - min) / (max - min)) * 270 - 135;

  const handleMouseDown = (e: React.MouseEvent) => {
    setIsDragging(true);
    startY.current = e.clientY;
    startValue.current = value;
  };

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (!isDragging) return;

      const deltaY = startY.current - e.clientY;
      const range = max - min;
      const step = range / 200; // sensitivity
      let newValue = startValue.current + deltaY * step;
      
      newValue = Math.max(min, Math.min(max, newValue));
      onChange(newValue);
    };

    const handleMouseUp = () => {
      setIsDragging(false);
    };

    if (isDragging) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', handleMouseUp);
    }

    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isDragging, max, min, onChange]);

  return (
    <div className="flex flex-col items-center gap-1 group">
      <span className="knob-label">{label}</span>
      <div
        className="relative cursor-ns-resize"
        onMouseDown={handleMouseDown}
        style={{ width: size, height: size }}
      >
        {/* Outer Ring Shadow */}
        <div className="absolute inset-0 rounded-full bg-black/40 shadow-inner border border-white/5" />
        
        {/* Progress Arc */}
        <svg className="absolute inset-0 -rotate-90" viewBox="0 0 100 100">
          <circle
            cx="50"
            cy="50"
            r="44"
            fill="none"
            stroke="rgba(255,255,255,0.03)"
            strokeWidth="6"
            strokeDasharray="276"
            strokeDashoffset="69"
            strokeLinecap="round"
          />
          <circle
            cx="50"
            cy="50"
            r="44"
            fill="none"
            stroke="hsl(var(--accent))"
            strokeWidth="6"
            strokeDasharray="276"
            strokeDashoffset={276 - ((value - min) / (max - min)) * 207}
            strokeLinecap="round"
            style={{ 
              filter: 'drop-shadow(0 0 4px hsl(var(--accent) / 0.5))',
              transition: isDragging ? 'none' : 'stroke-dashoffset 0.2s ease-out'
            }}
          />
        </svg>

        {/* Knob Body */}
        <div 
          className="absolute inset-[15%] rounded-full transition-transform duration-75 ease-out"
          style={{ 
            transform: `rotate(${angle}deg)`,
            background: 'linear-gradient(135deg, #3f3f46 0%, #18181b 100%)',
            boxShadow: '0 4px 10px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.2)',
            border: '1px solid rgba(255,255,255,0.05)'
          }}
        >
          {/* Grip Lines */}
          <div className="absolute inset-0 rounded-full opacity-20" style={{ backgroundImage: 'repeating-conic-gradient(from 0deg, transparent 0deg 10deg, #000 10deg 20deg)' }} />
          
          {/* Indicator Dot */}
          <div className="absolute top-1 left-1/2 -translate-x-1/2 w-1.5 h-1.5 rounded-full bg-accent shadow-[0_0_8px_hsl(var(--accent))]" />
          
          {/* Surface highlight */}
          <div className="absolute inset-0 rounded-full bg-gradient-to-br from-white/10 to-transparent pointer-events-none" />
        </div>
      </div>
      <div className="mt-2 font-mono text-[10px] text-accent/80 font-bold tabular-nums tracking-tight">
        {Math.round(value * 10) / 10}<span className="text-[8px] ml-0.5 opacity-60 uppercase">{unit}</span>
      </div>
    </div>
  );
};
