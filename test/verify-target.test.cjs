'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const target = require('../targets/MST2_US_VW_P0867D.json');
const { verifyReport } = require('../tools/verify-target.cjs');

function matchingReport() {
  return {
    archiveSHA256: target.extbinAppsSHA256,
    entries: target.requiredEntries.map((entry) => ({ ...entry }))
  };
}

test('accepts the exact P0867D target fingerprints', () => {
  const result = verifyReport(matchingReport(), target);
  assert.equal(result.ok, true);
  assert.equal(result.checkedEntries, target.requiredEntries.length);
  assert.deepEqual(result.errors, []);
});

test('fails closed on an EXTBIN or component mismatch', () => {
  const report = matchingReport();
  report.archiveSHA256 = '00'.repeat(32);
  report.entries[0].sha256 = 'ff'.repeat(32);
  const result = verifyReport(report, target);
  assert.equal(result.ok, false);
  assert.equal(result.errors.length, 2);
});

test('fails closed when a required component is missing', () => {
  const report = matchingReport();
  report.entries.pop();
  const result = verifyReport(report, target);
  assert.equal(result.ok, false);
  assert.match(result.errors[0], /missing required entry/);
});

