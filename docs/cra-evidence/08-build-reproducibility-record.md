# Build / Reproducibility Record

- Git commit: `263fa0f8938df775bd631bd00fd1590e1bcc6a87`
- Node version: `v25.6.1`
- Platform: `win32-x64`
- `package-lock.json` SHA-256: `6b4996d753e963f30de7d75af490dbe7dafd753a3de39dd6737f335f6df5d341`
- Generated: 2026-10-05T01:42:39.121Z

## Reproduction

```sh
npm install          # uses the exact package-lock.json hashed above
npm run bench:ckks
npm run bench:kernels
npm run fleet:compare
npm run demo:killer
npm run redteam:intercept
npm run security:sbom
npm run security:manifest
npm run security:evidence   # this document
```

Every `results/*.json` artifact embeds its own experiment ID, git commit, and host info
(`lib/platform/hostInfo.ts`) independent of this record.
