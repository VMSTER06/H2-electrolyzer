const fs = require('node:fs');
const path = require('node:path');

(async () => {
  const results = [];
  for (const [file, tests] of [
    ['game_test.wasm', ['test_clicks', 'test_dino', 'test_persistent_record']],
    ['controls_test.wasm', ['test_controls_regression', 'test_music_game_integration']],
  ]) {
    const {instance} = await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname, file)));
    for (const name of tests) {
      const line = instance.exports[name]();
      results.push({name, passed: line === 0, failedLine: line || null});
      console.log(`${name}: ${line ? `FAIL at line ${line}` : 'PASS'}`);
    }
  }
  fs.writeFileSync(path.join(__dirname, 'test_results.json'), JSON.stringify(results, null, 2));
  if (results.some(r => !r.passed)) process.exitCode = 1;
})().catch(e => { console.error(e); process.exitCode = 1; });
