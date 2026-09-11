const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname,
  '../clawbrowser/verify/resources/verify.js'), 'utf8');
const start = source.indexOf('async function detectExpectedFonts(');
assert.ok(start >= 0);
const helper = source.slice(start, source.indexOf('function formatProxyLocation(', start));
const document = {
  body: {appendChild() {}},
  createElement() {return {
    style: {}, remove() {},
    getBoundingClientRect() {
      assert.match(this.style.cssText, /font-size:72px/);
      const family = this.style.fontFamily;
      if (family.startsWith('"Arimo"')) return {width: 11};
      const base = family.split(',').at(-1).trim();
      return {width: {monospace: 10, serif: 20, 'sans-serif': 30}[base]};
    },
  };},
};
class FontFace {
  constructor(_, src) { this.src = src; }
  async load() {
    if (this.src === 'local("Noto Sans Thai Regular")') return this;
    throw new Error('No such local face');
  }
}
(async () => {
  const result = await vm.runInNewContext(helper +
    '\ndetectExpectedFonts(["Arimo", "Noto Sans Thai", "Missing-XYZ"])',
    {document, FontFace});
  assert.deepEqual(Array.from(result), ['Arimo', 'Noto Sans Thai']);
  console.log('PASS');
})().catch(error => {console.error(error); process.exitCode = 1;});
