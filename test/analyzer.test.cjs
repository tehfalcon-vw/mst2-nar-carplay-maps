'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const zlib = require('node:zlib');
const crypto = require('node:crypto');
const { analyzeFile } = require('../tools/analyze-firmware.cjs');

// Synthetic USTAR fixture. No vendor bytes or captured device data.
function tar(name = 'extbin/apps.tmp/bin/CarPlay', payload, type = '0') {
  payload ||= Buffer.from([127, 69, 76, 70, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 40, 0]);
  const h = Buffer.alloc(512);
  h.write(name); h.write('0000644\0', 100); h.write('0000000\0', 108); h.write('0000000\0', 116);
  h.write(payload.length.toString(8).padStart(11, '0') + '\0', 124);
  h.write('00000000000\0', 136); h.fill(32, 148, 156); h.write(type, 156); h.write('ustar\0', 257); h.write('00', 263);
  const sum = [...h].reduce((a, b) => a + b, 0);
  h.write(sum.toString(8).padStart(6, '0') + '\0 ', 148);
  return Buffer.concat([h, payload, Buffer.alloc((512 - payload.length % 512) % 512), Buffer.alloc(1024)]);
}
async function fixture(t, data, options) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'delphi-offline-test-'));
  t.after(() => {
    const resolved = fs.realpathSync(dir);
    const tempRoot = fs.realpathSync(os.tmpdir());
    assert.equal(path.dirname(resolved), tempRoot);
    assert.ok(path.basename(resolved).startsWith('delphi-offline-test-'));
    fs.rmSync(resolved, { recursive: true, force: true });
  });
  const input = path.join(dir, 'synthetic.bin'); fs.writeFileSync(input, data);
  const before = crypto.createHash('sha256').update(data).digest('hex');
  const report = await analyzeFile(input, options);
  assert.equal(crypto.createHash('sha256').update(fs.readFileSync(input)).digest('hex'), before);
  assert.equal(report.archiveSHA256, before);
  assert.ok(!JSON.stringify(report).includes(dir));
  return report;
}
test('raw and gzip TAR inventories agree; input stays unchanged and private path is omitted', async t => {
  const raw = tar();
  const a = await fixture(t, raw), b = await fixture(t, zlib.gzipSync(raw));
  assert.deepEqual(a.entries, b.entries);
  assert.equal(a.entries[0].inspection.machine, 40);
  assert.equal(a.entryCount, 1);
});
test('rejects corrupted TAR checksum', async t => {
  const b = tar(); b[0] ^= 1;
  await assert.rejects(fixture(t, b), /checksum/);
});
test('rejects corrupted GZIP trailer even after complete TAR end markers', async t => {
  const b = zlib.gzipSync(tar()); b[b.length - 8] ^= 1;
  await assert.rejects(fixture(t, b), /check|length|CRC/i);
});
test('rejects truncated payload and missing end markers', async t => {
  await assert.rejects(fixture(t, tar().subarray(0, 520)), /Truncated/);
});
test('enforces resource limits before accepting input', async t => {
  await assert.rejects(fixture(t, tar(), { entry: 10 }), /size limit/);
  await assert.rejects(fixture(t, tar(), { total: 600 }), /size limit/);
});
test('rejects traversal paths and unsupported links', async t => {
  await assert.rejects(fixture(t, tar('../outside')), /path/);
  await assert.rejects(fixture(t, tar('link', Buffer.from('x'), '2')), /entry type/);
});
test('rejects nonzero trailing data', async t => {
  await assert.rejects(fixture(t, Buffer.concat([tar(), Buffer.from('hidden')])), /after TAR end/);
});
