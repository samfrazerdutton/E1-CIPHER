import { useEffect, useState } from 'react';

export type DataState<T> = { status: 'loading' } | { status: 'error'; message: string } | { status: 'ready'; data: T };

/**
 * Fetches a JSON file this app's data/ directory (synced from the parent
 * repo's results/ via scripts/sync-data.mjs — see that script's header
 * comment). Never fabricates a fallback value: a missing file surfaces as
 * an explicit error state the view must render, not a silently empty chart.
 */
export function useResultData<T>(filename: string): DataState<T> {
  const [state, setState] = useState<DataState<T>>({ status: 'loading' });

  useEffect(() => {
    let cancelled = false;
    setState({ status: 'loading' });
    fetch(`/data/${filename}`)
      .then((res) => {
        if (!res.ok) throw new Error(`${filename}: HTTP ${res.status}`);
        return res.json();
      })
      .then((data) => {
        if (!cancelled) setState({ status: 'ready', data });
      })
      .catch((err: Error) => {
        if (!cancelled) setState({ status: 'error', message: err.message });
      });
    return () => {
      cancelled = true;
    };
  }, [filename]);

  return state;
}
