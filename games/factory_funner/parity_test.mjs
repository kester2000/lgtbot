import {pathToFileURL} from 'node:url';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
const source=resolve(process.argv[2]);
const {newGame,place,ports,neighbor,validate,installationCost}=await import(pathToFileURL(join(source,'src/engine.js')));
const skins=['Cian','Green','Red','Violet','White','Yellow'];
let input='',expected=[];
for(let skin=0;skin<6;skin++)for(let id=1;id<=48;id++)for(let r=0;r<7;r++)for(let q=0;q<7;q++)for(let rot=0;rot<6;rot++) {
  input+=`${skin} ${id} ${q} ${r} ${rot}\n`;
  const f=newGame('parity',false,{boardSkin:skins[skin]}); f.deck[0]=id;
  try {
    const p=place(f,{kind:'machine'},q,r,rot);
    for(const port of ports(p))place(f,port.kind==='in'?{kind:'supply',color:port.colors[0]}:{kind:port.colors[0]==='black'?'black':'collector'},...neighbor(q,r,port.edge),(port.edge+3)%6);
    const v=validate(f);expected.push(`${v.valid?1:0} ${installationCost(f)} ${v.bonus}`);
  } catch {expected.push('-1');}
}
const actual=spawnSync(process.argv[3],[...process.argv.slice(4),'--parity'],{input,encoding:'utf8',maxBuffer:20*1024*1024});
assert.equal(actual.status,0,actual.stderr);
const lines=actual.stdout.trim().split(/\r?\n/),cases=input.trim().split('\n');
assert.equal(lines.length,expected.length);
for(let i=0;i<lines.length;i++)assert.equal(lines[i],expected[i],`case ${cases[i]}`);
console.log(`Parity passed: ${lines.length} machine/board/position/rotation cases`);
