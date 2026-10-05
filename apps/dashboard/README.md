# E1-CIPHER dashboard

A Vite + React visualization layer over this repo's real `results/*.json`
artifacts. It reads them; it does not simulate new numbers. See the parent
repo's [README → Dashboard](../../README.md#dashboard) for what each view
shows and why.

## Run it

From the repo root, generate (or use the already-committed) result
artifacts, then:

```sh
cd apps/dashboard
npm install
npm run dev
```

`predev`/`prebuild` automatically copy `../../results/*-latest.json`,
`security-manifest.json`, and `cra-evidence.json` into `public/data/` (see
`scripts/sync-data.mjs`). Run `npm run sync-data` manually to refresh them
without restarting the dev server.
