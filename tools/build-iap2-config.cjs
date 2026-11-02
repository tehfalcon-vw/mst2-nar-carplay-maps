const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const SEND_IDS = [0x5200, 0x5203];
const RECEIVE_IDS = [0x5201, 0x5202, 0x5204];

function parseIds(text) {
  const values = [];
  for (const token of text.split(',')) {
    const clean = token.trim();
    if (!clean) continue;
    if (!/^(?:0x[0-9a-f]+|[0-9]+)$/i.test(clean)) {
      throw new Error(`invalid iAP2 message ID: ${clean}`);
    }
    const value = Number.parseInt(clean, clean.toLowerCase().startsWith('0x') ? 16 : 10);
    if (value < 0 || value > 0xffff) throw new Error(`iAP2 message ID out of range: ${clean}`);
    if (!values.includes(value)) values.push(value);
  }
  return values;
}

function appendIds(existing, required) {
  const values = parseIds(existing);
  for (const value of required) if (!values.includes(value)) values.push(value);
  return values.map((value) => `0x${value.toString(16).padStart(4, '0')}`).join(',');
}

function rewriteIap2Config(input) {
  if (typeof input !== 'string' || input.includes('\0') || Buffer.byteLength(input) > 65536) {
    throw new Error('invalid iAP2 configuration input');
  }
  const newline = input.includes('\r\n') ? '\r\n' : '\n';
  const lines = input.replace(/\r\n/g, '\n').split('\n');
  const sections = [];
  for (let i = 0; i < lines.length; i += 1) {
    const match = lines[i].match(/^\s*\[([^\]]+)\]\s*(?:[;#].*)?$/);
    if (match) sections.push({ name: match[1].toLowerCase(), start: i });
  }
  const ident = sections.filter((section) => section.name === 'ident');
  if (ident.length > 1) throw new Error('ambiguous repeated [ident] sections');
  let start;
  let end;
  if (!ident.length) {
    if (lines.length && lines.at(-1) !== '') lines.push('');
    start = lines.length;
    lines.push('[ident]');
    end = lines.length;
  } else {
    start = ident[0].start;
    end = sections.find((section) => section.start > start)?.start ?? lines.length;
  }
  const keys = new Map();
  for (let i = start + 1; i < end; i += 1) {
    const match = lines[i].match(/^\s*(sendmsgs|recvmsgs)\s*=\s*([^;#]*)(.*)$/i);
    if (!match) continue;
    const key = match[1].toLowerCase();
    if (keys.has(key)) throw new Error(`ambiguous repeated ${key} entry`);
    keys.set(key, { index: i, value: match[2], suffix: match[3] });
  }
  const replacements = [
    ['sendmsgs', SEND_IDS],
    ['recvmsgs', RECEIVE_IDS],
  ];
  for (const [key, required] of replacements) {
    const old = keys.get(key);
    const spacing = old?.value.match(/\s*$/)?.[0] ?? '';
    const line = `${key}=${appendIds(old?.value ?? '', required)}${spacing}${old?.suffix ?? ''}`;
    if (old) lines[old.index] = line;
    else {
      lines.splice(end, 0, line);
      end += 1;
    }
  }
  let output = lines.join(newline);
  if (!output.endsWith(newline)) output += newline;
  return output;
}

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

function main(argv) {
  if (argv.length !== 4) {
    throw new Error('usage: node tools/build-iap2-config.cjs <stock iap2.cfg> <output iap2.cfg>');
  }
  const inputPath = path.resolve(argv[2]);
  const outputPath = path.resolve(argv[3]);
  if (fs.existsSync(outputPath)) throw new Error(`refusing to overwrite ${outputPath}`);
  const stock = fs.readFileSync(inputPath);
  const manifest = require('../targets/MST2_US_VW_P0867D.json');
  const expected = manifest.requiredEntries.find((entry) => entry.path.endsWith('/cfg/iap2.cfg'));
  if (!expected || stock.length !== expected.bytes || sha256(stock) !== expected.sha256) {
    throw new Error('input is not the exact stock P0867D iap2.cfg');
  }
  const output = rewriteIap2Config(stock.toString('utf8'));
  fs.writeFileSync(outputPath, output, { flag: 'wx', mode: 0o600 });
  process.stdout.write(`${JSON.stringify({ inputSHA256: sha256(stock), outputSHA256: sha256(Buffer.from(output)), output: outputPath })}\n`);
}

if (require.main === module) {
  try { main(process.argv); } catch (error) { process.stderr.write(`${error.message}\n`); process.exitCode = 1; }
}

module.exports = { appendIds, parseIds, rewriteIap2Config };
