const assert = require('node:assert/strict');
const test = require('node:test');
const { rewriteIap2Config } = require('../tools/build-iap2-config.cjs');

test('adds a bounded ident section without changing stock sections', () => {
  const stock = '[accessory]\nname=Volkswagen\n[power]\nenable=yes\n';
  const output = rewriteIap2Config(stock);
  assert.equal(output, `${stock}\n[ident]\nsendmsgs=0x5200,0x5203\nrecvmsgs=0x5201,0x5202,0x5204\n`);
  assert.equal(rewriteIap2Config(output), output);
});

test('preserves unrelated IDs and comments in an existing ident section', () => {
  const stock = '[ident]\nsendmsgs=0x5000 ; stock\nrecvmsgs=20481,0x5202 # keep\n[next]\nx=1\n';
  const output = rewriteIap2Config(stock);
  assert.match(output, /sendmsgs=0x5000,0x5200,0x5203 ; stock/);
  assert.match(output, /recvmsgs=0x5001,0x5202,0x5201,0x5204 # keep/);
  assert.match(output, /\[next\]\nx=1/);
});

test('rejects ambiguous or malformed message declarations', () => {
  assert.throws(() => rewriteIap2Config('[ident]\nrecvmsgs=wat\n'), /invalid/);
  assert.throws(() => rewriteIap2Config('[ident]\nrecvmsgs=1\nrecvmsgs=2\n'), /repeated recvmsgs/);
  assert.throws(() => rewriteIap2Config('[ident]\n[ident]\n'), /repeated \[ident\]/);
});
