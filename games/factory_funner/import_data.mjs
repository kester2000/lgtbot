// Development-only importer. The plugin has no JavaScript runtime dependency.
// Usage: node games/factory_funner/import_data.mjs D:/code/FactoryFunner
import {pathToFileURL} from 'node:url';
import {resolve, join} from 'node:path';
import {writeFileSync} from 'node:fs';
const source = resolve(process.argv[2]);
const {MACHINES} = await import(pathToFileURL(join(source, 'src/data.js')));
const colors = {pink:1, yellow:2, green:4, blue:8, black:16};
const rows = MACHINES.map(m => `    {${m.id}, "${m.name}", ${m.revenue}, {${m.ports.map(p => `{${p.edge}, ${p.colors.reduce((n,c)=>n|colors[c],0)}, ${p.amount}, ${p.kind === 'in' ? 'true' : 'false'}}`).join(', ')}}}`);
writeFileSync(new URL('machines.h', import.meta.url), `// Generated from FactoryFunner/src/data.js; see PORTING.md.\n#pragma once\n#include <vector>\n#include <string>\nnamespace factory_funner {\nstruct MachinePort { int edge, colors, amount; bool input; };\nstruct Machine { int id; std::string name; int revenue; std::vector<MachinePort> ports; };\ninline const std::vector<Machine> kMachines {\n${rows.join(',\n')}\n};\n}\n`);
