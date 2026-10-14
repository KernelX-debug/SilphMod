const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const directory = path.join(root, 'app/src/main/assets/pokemon');
async function get(url, binary = false) {
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      const response = await fetch(url, {signal: AbortSignal.timeout(25000), headers: {'User-Agent': 'SilphMod-asset-preparation'}});
      if (!response.ok) throw Error(`${response.status}: ${url}`);
      return binary ? Buffer.from(await response.arrayBuffer()) : response.json();
    } catch (error) { if (attempt === 2) throw error; }
  }
}
async function main() {
  const releasedUrl = 'https://pogoapi.net/api/v1/released_pokemon.json';
  const released = await get(releasedUrl);
  const entries = Object.values(released).sort((a, b) => a.id - b.id);
  if (entries.length < 800 || entries.some((entry, i) => !Number.isInteger(entry.id) || entry.id < 1 || entry.id > 1025
      || !entry.name || /[\t\r\n]/.test(entry.name) || i && entries[i - 1].id >= entry.id)) throw Error('Invalid species catalog');
  const commit = (await get('https://api.github.com/repos/PokeMiners/pogo_assets/commits/master')).sha;
  const listing = await get(`https://api.github.com/repos/PokeMiners/pogo_assets/contents/Images/Pokemon?ref=${commit}`);
  const addressable = listing.find(entry => entry.name === 'Addressable Assets');
  if (!addressable) throw Error('Pokémon asset directory missing');
  const tree = await get(`https://api.github.com/repos/PokeMiners/pogo_assets/git/trees/${addressable.sha}`);
  if (tree.truncated) throw Error('Incomplete image inventory');
  const available = new Set(tree.tree.filter(entry => entry.type === 'blob').map(entry => entry.path));
  const spriteCommit = (await get('https://api.github.com/repos/PokeAPI/sprites/commits/master')).sha;
  fs.mkdirSync(path.join(directory, 'icons'), {recursive: true});
  const metadata = {catalogUrl: releasedUrl, acquiredAt: new Date().toISOString(), pogoAssetsCommit: commit,
    pokeApiSpritesCommit: spriteCommit, entries: []};
  let cursor = 0, completed = 0;
  await Promise.all(Array.from({length: 8}, async () => {
    while (cursor < entries.length) {
      const entry = entries[cursor++];
      const image = `pm${entry.id}.icon.png`;
      const url = available.has(image)
        ? `https://raw.githubusercontent.com/PokeMiners/pogo_assets/${commit}/Images/Pokemon/Addressable%20Assets/${image}`
        : `https://raw.githubusercontent.com/PokeAPI/sprites/${spriteCommit}/sprites/pokemon/${entry.id}.png`;
      const destination = path.join(directory, 'icons', String(entry.id).padStart(4, '0') + '.png');
      const bytes = fs.existsSync(destination) ? fs.readFileSync(destination) : await get(url, true);
      if (!bytes.subarray(0, 8).equals(Buffer.from([137,80,78,71,13,10,26,10])) || bytes.length < 100
          || bytes.readUInt32BE(16) > 1024 || bytes.readUInt32BE(20) > 1024) throw Error('Invalid PNG: ' + entry.id);
      fs.writeFileSync(destination, bytes);
      metadata.entries.push({id: entry.id, url, sha256: crypto.createHash('sha256').update(bytes).digest('hex'), bytes: bytes.length});
      if (++completed % 100 === 0) process.stdout.write(`${completed}/${entries.length} Pokémon icons\n`);
    }
  }));
  metadata.entries.sort((a, b) => a.id - b.id);
  fs.writeFileSync(path.join(directory, 'catalog.tsv'), entries.map(entry => `${entry.id}\t${entry.name}`).join('\n') + '\n');
  fs.writeFileSync(path.join(root, 'tools/pokemon-assets.json'), JSON.stringify(metadata, null, 2) + '\n');
  const license = await fetch(`https://raw.githubusercontent.com/PokeAPI/sprites/${spriteCommit}/LICENCE.txt`);
  if (!license.ok) throw Error('Sprite license unavailable');
  fs.writeFileSync(path.join(root, 'app/src/main/assets/licenses/PokeAPI-sprites.txt'), await license.text());
  const mined = metadata.entries.filter(entry => entry.url.includes('/PokeMiners/')).length;
  console.log(JSON.stringify({species: entries.length, pogoIcons: mined, fallbackIcons: entries.length - mined,
    imageBytes: metadata.entries.reduce((total, entry) => total + entry.bytes, 0)}));
}
main().catch(error => { console.error(error); process.exitCode = 1; });
