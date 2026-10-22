#!/usr/bin/env node
// Install dependencies in an external build directory, never viewer runtime:
// npm install --prefix <deps> --ignore-scripts --no-audit --no-fund --save-exact
//   @giphy/js-fetch-api@5.8.0 @giphy/js-util@5.2.0 @giphy/js-types@5.1.0 uuid@9.0.1 esbuild@0.25.12
// node scripts/build_tasia_media_sdk.mjs <deps>
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv[2]) throw new Error('Supply an external dependency directory');
const dependencies = resolve(process.argv[2]);
const require = createRequire(resolve(dependencies, 'package.json'));
const versions = { '@giphy/js-fetch-api': '5.8.0', '@giphy/js-util': '5.2.0',
    '@giphy/js-types': '5.1.0', uuid: '9.0.1', esbuild: '0.25.12' };
const lock = JSON.parse(readFileSync(resolve(dependencies, 'package-lock.json')));
const packages = {};
for (const [name, version] of Object.entries(versions)) {
    const item = lock.packages['node_modules/' + name];
    if (item?.version !== version || !item.integrity) throw new Error('Dependency mismatch: ' + name);
    packages[name] = { version, integrity: item.integrity };
}
const output = resolve(root, 'indra/newview/skins/default/html/en/tasia_media');
const bundle = resolve(output, 'picker.bundle.js');
const result = await require('esbuild').build({
    entryPoints: [resolve(root, 'scripts/tasia_media_picker.mjs')],
    nodePaths: [resolve(dependencies, 'node_modules')],
    outfile: bundle, bundle: true, minify: true, target: 'chrome110',
    legalComments: 'eof', metafile: true, logLevel: 'warning',
});
const previewBundle = resolve(output, 'preview.bundle.js');
await require('esbuild').build({ entryPoints: [resolve(root, 'scripts/tasia_media_preview.mjs')],
    outfile: previewBundle, bundle: true, minify: true, target: 'chrome110', logLevel: 'warning' });
const sha256 = file => createHash('sha256').update(readFileSync(file)).digest('hex');
const manifest = { packages, bundle_sha256: sha256(bundle), preview_bundle_sha256: sha256(previewBundle),
    sources: Object.fromEntries(['scripts/tasia_media_picker.mjs', 'scripts/tasia_media_policy.mjs', 'scripts/tasia_media_preview.mjs']
        .map(file => [file, sha256(resolve(root, file))])),
    bundledInputCount: Object.keys(result.metafile.inputs).length };
writeFileSync(resolve(output, 'sdk-manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
console.log('Built pinned GIPHY Web fetch SDK and picker; no API keys included.');
