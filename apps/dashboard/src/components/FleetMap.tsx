import styles from './FleetMap.module.css';
import { sequentialBlue } from '../lib/sequentialBlue';
import type { DroneTelemetrySample } from '../../../../lib/telemetry/types.js';

const SIZE = 320;
const PAD = 24;

/** Live SVG scatter of the fleet's current positions, colored by infraAnomalyScore (sequential, one hue). */
export function FleetMap({
  samples,
  selectedId,
  onSelect,
}: {
  samples: DroneTelemetrySample[];
  selectedId: string | null;
  onSelect: (id: string) => void;
}) {
  const lats = samples.map((s) => s.gps.lat);
  const lons = samples.map((s) => s.gps.lon);
  const latRange = [Math.min(...lats), Math.max(...lats)];
  const lonRange = [Math.min(...lons), Math.max(...lons)];
  const latSpan = latRange[1] - latRange[0] || 1;
  const lonSpan = lonRange[1] - lonRange[0] || 1;

  const toXY = (s: DroneTelemetrySample) => {
    const x = PAD + ((s.gps.lon - lonRange[0]) / lonSpan) * (SIZE - 2 * PAD);
    const y = PAD + (1 - (s.gps.lat - latRange[0]) / latSpan) * (SIZE - 2 * PAD);
    return [x, y];
  };

  return (
    <div className={styles.wrap}>
      <svg className={styles.svgBox} width={SIZE} height={SIZE} viewBox={`0 0 ${SIZE} ${SIZE}`} role="img" aria-label="Fleet positions, colored by local infrastructure anomaly score">
        {samples.map((s) => {
          const [x, y] = toXY(s);
          return (
            <circle
              key={s.droneId}
              cx={x}
              cy={y}
              r={s.droneId === selectedId ? 7 : 5}
              fill={sequentialBlue(s.infraAnomalyScore)}
              className={s.droneId === selectedId ? styles.droneSelected : styles.drone}
              onClick={() => onSelect(s.droneId)}
            >
              <title>
                {s.droneId}: anomaly {s.infraAnomalyScore.toFixed(2)}, battery {s.batteryPct.toFixed(0)}%
              </title>
            </circle>
          );
        })}
      </svg>
      <div>
        <div className={styles.legend}>
          <span>infra anomaly score</span>
          <span className={styles.ramp} />
          <span>0 -&gt; 1</span>
        </div>
        <p style={{ fontSize: 11, color: 'var(--ink-faint)', maxWidth: 220, marginTop: 8 }}>
          Click a drone to see its live policy decision. Positions and scores come from{' '}
          <code className="mono">lib/telemetry/generator.ts</code>, running unmodified in this browser tab.
        </p>
      </div>
    </div>
  );
}
