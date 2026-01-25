import React from 'react';

interface PianoKeyProps {
  note: string;
  isBlack?: boolean;
  isActive?: boolean;
  onClick: () => void;
}

const PianoKey: React.FC<PianoKeyProps> = ({ note, isBlack, isActive, onClick }) => {
  return (
    <div
      onClick={onClick}
      className={`
        ${isBlack ? 'h-7 w-3 bg-[#111] -mx-1.5 z-10 border border-white/5' : 'h-10 w-4.5 bg-[#222] border border-white/5'}
        ${isActive ? (isBlack ? 'bg-accent border-accent/40 shadow-[0_0_8px_hsl(var(--accent)/0.3)]' : 'bg-accent/80 border-accent/40 shadow-[0_0_8px_hsl(var(--accent)/0.3)]') : ''}
        cursor-pointer transition-all duration-200 rounded-b-[1px] relative overflow-hidden group
      `}
      title={note}
    >
      <div className={`absolute inset-0 bg-white/5 opacity-0 group-hover:opacity-100 transition-opacity`} />
      {isActive && (
        <div className="absolute bottom-0 left-0 right-0 h-1 bg-accent" />
      )}
    </div>
  );
};

export const PianoRoll: React.FC<{ activeNotes: string[]; toggleNote: (note: string) => void }> = ({ activeNotes, toggleNote }) => {
  const keys = [
    { note: 'C', isBlack: false },
    { note: 'C#', isBlack: true },
    { note: 'D', isBlack: false },
    { note: 'D#', isBlack: true },
    { note: 'E', isBlack: false },
    { note: 'F', isBlack: false },
    { note: 'F#', isBlack: true },
    { note: 'G', isBlack: false },
    { note: 'G#', isBlack: true },
    { note: 'A', isBlack: false },
    { note: 'A#', isBlack: true },
    { note: 'B', isBlack: false },
  ];

  return (
    <div className="flex justify-center bg-black/60 p-3 rounded-lg border border-white/5 shadow-inner">
      <div className="flex">
        {keys.map((key) => (
          <PianoKey
            key={key.note}
            note={key.note}
            isBlack={key.isBlack}
            isActive={activeNotes.includes(key.note)}
            onClick={() => toggleNote(key.note)}
          />
        ))}
      </div>
    </div>
  );
};
