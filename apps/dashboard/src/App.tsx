import { useState } from 'react';
import { NavRail } from './components/NavRail';
import type { ViewId } from './views';
import { CryptoView } from './views/CryptoView';
import { ResearchView } from './views/ResearchView';
import { FleetView } from './views/FleetView';
import { MissionView } from './views/MissionView';
import { SimulatorView } from './views/SimulatorView';
import { ConfidentialityView } from './views/ConfidentialityView';
import { SecurityView } from './views/SecurityView';
import { PlatformView } from './views/PlatformView';

const VIEWS: Record<ViewId, () => React.ReactElement> = {
  crypto: CryptoView,
  research: ResearchView,
  fleet: FleetView,
  mission: MissionView,
  simulator: SimulatorView,
  confidentiality: ConfidentialityView,
  security: SecurityView,
  platform: PlatformView,
};

export default function App() {
  const [active, setActive] = useState<ViewId>('crypto');
  const View = VIEWS[active];

  return (
    <div style={{ display: 'flex', height: '100%' }}>
      <NavRail active={active} onSelect={setActive} />
      <main style={{ flex: 1, display: 'flex', flexDirection: 'column', overflowY: 'auto', minWidth: 0 }}>
        <View />
      </main>
    </div>
  );
}
