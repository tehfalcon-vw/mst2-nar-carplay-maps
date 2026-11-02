'use strict';
const test=require('node:test');const assert=require('node:assert/strict');
const {patchBundles,patchHmi,patchProcman,validateArmElf,validateJar}=require('../tools/build-p0867d-stage.cjs');

test('patchHmi appends exactly one target jar',()=>{
  const result=patchHmi('before\nBOOTCLASSPATH="/ifs1/a.jxe:/x/b.jar"\nafter\n');
  assert.match(result,/BOOTCLASSPATH="\/ifs1\/a\.jxe:\/x\/b\.jar:\/extbin\/apps\/mst2-nar-carplay-maps\/mst2-nar-rgi-hmi\.jar"/);
  assert.throws(()=>patchHmi(result),/already patched/);
});
test('patchProcman scopes the launcher to the stock MediaHandler service',()=>{
  const stock=' -f  MediaHandler           -n"mediahandler"             -l20 -x10 -p12 -c0         -a"-c../cfg/MHConfig.cfg"';
  const result=patchProcman(`before\n${stock}\nafter`);
  assert.match(result,/-f  env .*LD_PRELOAD=\/extbin\/apps\/mst2-nar-carplay-maps\/libmst2_nar_launch\.so/);
  assert.match(result,/-a"MediaHandler" -a"-c\.\.\/cfg\/MHConfig\.cfg"/);
  assert.throws(()=>patchProcman(result),/already patched/);
});
test('patchBundles registers an unused late-load bundle',()=>{
  const result=patchBundles('Bundle.Startup.Lateload = 61,62,75,70,71\nBundle.Name.1 = JDSI\n');
  assert.match(result,/Bundle\.Startup\.Lateload = 61,62,75,70,71,98/);
  assert.match(result,/Bundle\.Activator\.98 = mst2\.nar\.cluster\.Activator/);
  assert.throws(()=>patchBundles(result),/already patched/);
});
test('patchers reject ambiguous input',()=>{
  assert.throws(()=>patchHmi('BOOTCLASSPATH="a"\nBOOTCLASSPATH="b"\n'),/unrecognized/);
  assert.throws(()=>patchBundles('Bundle.Startup.Lateload = nope\n'),/unrecognized/);
});
test('artifact validators reject host or mislabeled files',()=>{
  assert.throws(()=>validateArmElf(Buffer.from('not elf'),'bad.so'),/ELF32/);
  const arm=Buffer.alloc(52);arm.write('\x7fELF',0,'binary');arm[4]=1;arm[5]=1;arm.writeUInt16LE(3,16);arm.writeUInt16LE(40,18);assert.doesNotThrow(()=>validateArmElf(arm,'arm.so'));
  assert.throws(()=>validateJar(Buffer.from('PK fake')),/activator/);
});
