'use strict';
const esbuild = require('esbuild');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const output = path.join(root, 'app/build/generated/engine');
const asset = path.join(output, 'assets/engine/agent.js');
esbuild.buildSync({
  absWorkingDir: __dirname, entryPoints: ['src/silph-entry.ts'], bundle: true, preserveSymlinks: true,
  format: 'iife', platform: 'neutral', mainFields: ['main', 'module'],
  target: 'es2020', charset: 'utf8', legalComments: 'inline', outfile: asset,
});
const hash = crypto.createHash('sha256').update(fs.readFileSync(asset)).digest('hex');
const generated = path.join(output, 'java/dev/rootcatch/ugphone/AgentBuild.java');
fs.mkdirSync(path.dirname(generated), {recursive: true});
fs.writeFileSync(generated, `package dev.rootcatch.ugphone;\nfinal class AgentBuild {\n` +
  `    static final String SHA256 = "${hash}";\n    private AgentBuild() { }\n}\n`);
fs.copyFileSync(path.join(root, 'tools/native-runtime.json'), path.join(output, 'assets/engine/injector.json'));
process.stdout.write(JSON.stringify({agentSha256: hash, bytes: fs.statSync(asset).size}) + '\n');
