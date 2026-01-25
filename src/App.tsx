import { SwindlVX } from './components/plugin/SwindlVX';
import { Toaster } from './components/ui/sonner';

function App() {
  return (
    <div className="min-h-screen bg-[#020202] flex items-center justify-center p-4">
      <SwindlVX />
      <Toaster position="bottom-right" theme="dark" />
    </div>
  );
}

export default App;