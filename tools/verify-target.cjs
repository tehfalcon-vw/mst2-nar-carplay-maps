#!/usr/bin/env node
'use strict';

const fs = require('node:fs');
const path = require('node:path');

function loadJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function verifyReport(report, target) {
  const errors = [];
  if (target.platform !== 'MST2' || target.vendor !== 'DELPHI' || target.region !== 'NAR') {
    errors.push('target manifest is outside the MST2 Delphi NAR scope');
  }
  if (String(report.archiveSHA256).toLowerCase() !== String(target.extbinAppsSHA256).toLowerCase()) {
    errors.push(`EXTBIN_APPS hash mismatch: expected ${target.extbinAppsSHA256}, got ${report.archiveSHA256}`);
  }
  const entries = new Map((report.entries || []).map((entry) => [entry.path, entry]));
  for (const expected of target.requiredEntries || []) {
    const actual = entries.get(expected.path);
    if (!actual) {
      errors.push(`missing required entry: ${expected.path}`);
      continue;
    }
    if (actual.bytes !== expected.bytes) {
      errors.push(`${expected.path}: expected ${expected.bytes} bytes, got ${actual.bytes}`);
    }
    if (String(actual.sha256).toLowerCase() !== String(expected.sha256).toLowerCase()) {
      errors.push(`${expected.path}: SHA-256 mismatch`);
    }
  }
  return {
    ok: errors.length === 0,
    target: target.id,
    checkedEntries: (target.requiredEntries || []).length,
    errors
  };
}

function main(argv) {
  if (argv.length !== 2) {
    throw new Error('Usage: node tools/verify-target.cjs REPORT.json TARGET.json');
  }
  const reportPath = path.resolve(argv[0]);
  const targetPath = path.resolve(argv[1]);
  const result = verifyReport(loadJson(reportPath), loadJson(targetPath));
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
  if (!result.ok) process.exitCode = 1;
}

if (require.main === module) {
  try {
    main(process.argv.slice(2));
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 2;
  }
}

module.exports = { verifyReport };

