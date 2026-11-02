'use strict';
const fs = require('node:fs');
const zlib = require('node:zlib');
const crypto = require('node:crypto');

const targets = new Set([
  'bin/CarPlay', 'bin/CarPlayServices', 'bin/ClusterServices', 'bin/Most',
  'lib/libAirPlay.so', 'lib/libAirPlaySupport.so', 'lib/libScreenStream.so'
].map(p => 'extbin/apps.tmp/' + p));

class Reader {
  constructor(stream, limit) {
    this.iterator = stream[Symbol.asyncIterator]();
    this.buffer = Buffer.alloc(0);
    this.position = 0;
    this.limit = limit;
  }
  async read(n) {
    const parts = [];
    let received = 0;
    while (received < n) {
      if (!this.buffer.length) {
        const next = await this.iterator.next();
        if (next.done) break;
        this.buffer = next.value;
      }
      const count = Math.min(n - received, this.buffer.length);
      parts.push(this.buffer.subarray(0, count));
      this.buffer = this.buffer.subarray(count);
      received += count;
      this.position += count;
      if (this.position > this.limit) throw Error('Decompressed size limit exceeded');
    }
    return Buffer.concat(parts, received);
  }
  async exact(n) {
    const b = await this.read(n);
    if (b.length !== n) throw Error('Truncated TAR stream');
    return b;
  }
}

function text(b, start, length) {
  return b.subarray(start, start + length).toString('utf8').split('\0')[0];
}
function octal(b, start, length) {
  if (b[start] & 128) throw Error('Unsupported TAR base-256 number');
  const s = text(b, start, length).trim();
  if (!/^[0-7]*$/.test(s)) throw Error('Invalid TAR octal number');
  const n = s ? Number.parseInt(s, 8) : 0;
  if (!Number.isSafeInteger(n)) throw Error('TAR number exceeds safe integer range');
  return n;
}
function inspect(b) {
  const elf = b.length >= 20 && b.subarray(0, 4).equals(Buffer.from([127, 69, 76, 70]));
  const strings = b.toString('latin1').match(/[\x20-\x7e]{6,}/g) || [];
  // Exclude embedded build paths and excessively long diagnostic text.
  const indicators = [...new Set(strings.filter(s =>
    s.length <= 180 && !s.includes('/') && !s.includes('\\') &&
    /carplay|navguidance|navigationinfo|gfxstream|connectionmaster|distancetonextmaneuver|eta_status|screenstream|h264|alt.?screen|route.?guidance|is_map_most_streaming|^lib.*\.so/i.test(s)
  ))].slice(0, 80);
  return {
    elf,
    ...(elf ? {
      elfClass: b[4], endianness: b[5],
      machine: b[5] === 1 ? b.readUInt16LE(18) : b[5] === 2 ? b.readUInt16BE(18) : null
    } : {}),
    indicators
  };
}

async function analyzeFile(input, options = {}) {
  const limits = { total: 2 * 1024 ** 3, entry: 512 * 1024 ** 2, entries: 50000, ...options };
  const fd = fs.openSync(input, 'r');
  const signature = Buffer.alloc(2);
  try { fs.readSync(fd, signature, 0, 2, 0); } finally { fs.closeSync(fd); }
  const gzip = signature[0] === 31 && signature[1] === 139;
  const source = fs.createReadStream(input);
  const archiveHash = crypto.createHash('sha256');
  source.on('data', b => archiveHash.update(b));
  const stream = gzip ? source.pipe(zlib.createGunzip()) : source;
  if (gzip) source.on('error', e => stream.destroy(e));
  const reader = new Reader(stream, limits.total);
  const entries = [];
  const paths = new Set();
  try {
    while (true) {
      const header = await reader.exact(512);
      if (header.every(v => v === 0)) {
        if (!(await reader.exact(512)).every(v => v === 0)) throw Error('Invalid TAR end markers');
        // Drain the decoder: verifies GZIP trailer/CRC and rejects hidden nonzero data.
        while (true) {
          const tail = await reader.read(65536);
          if (!tail.length) break;
          if (!tail.every(v => v === 0)) throw Error('Nonzero data after TAR end');
        }
        break;
      }
      if (entries.length >= limits.entries) throw Error('TAR entry count limit exceeded');
      let checksum = 0;
      for (let i = 0; i < 512; i++) checksum += i >= 148 && i < 156 ? 32 : header[i];
      if (checksum !== octal(header, 148, 8)) throw Error('TAR header checksum mismatch');
      if (text(header, 257, 5) !== 'ustar') throw Error('Unsupported TAR header format');
      const prefix = text(header, 345, 155);
      const name = (prefix ? prefix + '/' : '') + text(header, 0, 100);
      if (!name || name.startsWith('/') || name.includes('\\') || /^[A-Za-z]:/.test(name) || name.split('/').includes('..')) throw Error('Unsafe or unsupported TAR path');
      if (paths.has(name)) throw Error('Duplicate TAR entry path');
      paths.add(name);
      const type = text(header, 156, 1) || '0';
      if (!['0', '5'].includes(type)) throw Error('Unsupported TAR entry type: ' + type);
      const size = octal(header, 124, 12);
      if (size > limits.entry) throw Error('TAR entry size limit exceeded');
      if (type === '5' && size !== 0) throw Error('Directory has nonzero payload');
      const hash = crypto.createHash('sha256');
      const capture = targets.has(name) && size <= 16 * 1024 ** 2;
      const parts = [];
      for (let left = size; left > 0;) {
        const b = await reader.exact(Math.min(left, 65536));
        hash.update(b);
        if (capture) parts.push(b);
        left -= b.length;
      }
      const padding = await reader.exact((512 - size % 512) % 512);
      if (!padding.every(v => v === 0)) throw Error('Nonzero TAR entry padding');
      entries.push({
        path: name, type, bytes: size, sha256: hash.digest('hex'),
        ...(capture ? { inspection: inspect(Buffer.concat(parts)) } : {})
      });
    }
  } finally {
    source.destroy();
    if (stream !== source) stream.destroy();
  }
  return {
    schemaVersion: 1,
    scope: 'Static offline observations only; no RGI, video, signature or device compatibility validation.',
    container: gzip ? 'gzip+tar' : 'tar',
    archiveSHA256: archiveHash.digest('hex'),
    decompressedBytes: reader.position,
    entryCount: entries.length,
    entries
  };
}

if (require.main === module) {
  const [input, output, ...extra] = process.argv.slice(2);
  if (!input || !output || extra.length) {
    console.error('Usage: node tools/analyze-firmware.cjs INPUT.bin NEW_REPORT.json');
    process.exitCode = 1;
  } else if (fs.existsSync(output)) {
    console.error('Output already exists; refusing to overwrite');
    process.exitCode = 1;
  } else {
    analyzeFile(input).then(report => {
      fs.writeFileSync(output, JSON.stringify(report, null, 2) + '\n', { flag: 'wx' });
      console.log(`Analyzed ${report.entryCount} entries; report created. No files extracted or executed.`);
    }).catch(error => {
      console.error(error.code ? `I/O error: ${error.code}` : error.message);
      process.exitCode = 1;
    });
  }
}
module.exports = { analyzeFile };
