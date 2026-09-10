const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname,
    '../clawbrowser/verify/resources/verify.js'), 'utf8');
const assignment = source.match(/const expectedOrientation =[^;]+;/);
assert.ok(assignment, 'verification orientation calculation must exist');
for (const [width, height, wanted] of [
  ['1280', '800', 'landscape-primary'],
  ['800', '1280', 'portrait-primary'],
  ['1920', '1080', 'landscape-primary'],
  ['800', '800', 'portrait-primary'],
]) {
  const actual = vm.runInNewContext(assignment[0] + '\nexpectedOrientation', {
    expected: {screen_width: width, screen_height: height},
  });
  assert.equal(actual, wanted, `${width}x${height}`);
}
console.log('PASS');
