'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { Tablet } = require('../app/core.cjs');
const resources = path.resolve(__dirname, '../app');
const guard = fs.readFileSync(path.join(resources, 'check-tablet-runtime.sh'), 'utf8');
const own = '/home/root/reply-runtime';

function preflight({ unit = '', env = '', maps = '', files = '', pid = '32',
  launch = '{ path=/usr/bin/xochitl ; argv[]=/usr/bin/xochitl ; }' } = {}) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'reply-guard-'));
  try {
    const processDir = path.join(tmp, '32');
    fs.mkdirSync(processDir);
    fs.writeFileSync(path.join(processDir, 'environ'), env);
    fs.writeFileSync(path.join(processDir, 'maps'), maps);
    fs.writeFileSync(path.join(tmp, 'systemctl'), [
      '#!/bin/sh', 'case "$4" in',
      'MainPID) printf "%s\\n" "$FIXTURE_PID";;',
      'Environment) printf "%s\\n" "$FIXTURE_UNIT";;',
      'EnvironmentFiles) printf "%s\\n" "$FIXTURE_FILES";;',
      'ExecStart) printf "%s\\n" "$FIXTURE_LAUNCH";;',
      '*) exit 9;;', 'esac',
    ].join('\n'), { mode:0o700 });
    const result = spawnSync('sh', ['-s', '--', '--inspect'], {
      input:guard.replaceAll('/proc/', tmp + '/'), encoding:'utf8', timeout:5000,
      env:{ PATH:tmp + ':/usr/bin:/bin', FIXTURE_UNIT:unit, FIXTURE_FILES:files,
        FIXTURE_PID:pid, FIXTURE_LAUNCH:launch },
    });
    assert.ifError(result.error);
    return result;
  } finally {
    fs.rmSync(tmp, { recursive:true, force:true });
  }
}

test('preflight accepts stock and an exact, already running rePly setup', () => {
  assert.equal(preflight().status, 0);
  const pair = ['XOVI_ROOT=' + own, 'LD_PRELOAD=' + own + '/xovi.so'];
  assert.equal(preflight({ unit:pair.join(' '), env:pair.join('\0') + '\0',
    maps:'address r-xp offset dev inode ' + own + '/xovi.so\n' }).status, 0);
});

test('preflight refuses an active or staged foreign runtime and withholds environment data', () => {
  for (const fixture of [
    { unit:'XOVI_ROOT=/home/root/custom PRIVATE=not-for-output' },
    { env:'LD_PRELOAD=/home/root/custom/xovi.so\0PRIVATE=not-for-output\0' },
    { maps:'address r-xp offset dev inode /home/root/custom/xovi.so\n' },
    { unit:'XOVI_ROOT=' + own + ' LD_PRELOAD=' + own + '/xovi.so:/tmp/other.so' },
    { unit:'XOVI_ROOT=' + own },
    { unit:'"XOVI_ROOT=/home/root/custom runtime"' },
    { files:'/tmp/only-a-test' },
    { launch:'{ path=/home/root/manager-launch ; argv[]=other ; }' },
    { pid:'0' },
  ]) {
    const result = preflight(fixture);
    assert.equal(result.status, 1, JSON.stringify(fixture));
    assert(!result.stderr.includes('not-for-output'));
    assert.equal(result.stdout, '');
  }
});

test('the desktop install and start stop at a rejected preflight, without deployment', async () => {
  const tablet = new Tablet({ resources });
  tablet.device = {};
  const commands = [];
  tablet.exec = async (cmd, input) => {
    commands.push(cmd);
    assert.equal(String(input), guard);
    throw Error('Another XOVI setup');
  };
  tablet.put = tablet.dependency = async () => assert.fail('No deployment after rejection');
  await assert.rejects(tablet.install({
    provider:'openai', api_key:'sk-placeholder-not-a-real-key-12345', model:'gpt-5-mini',
    reasoning_effort:'low', timezone:'UTC',
  }), /Another XOVI/);
  await assert.rejects(tablet.start(), /Another XOVI/);
  assert.deepEqual(commands, ['sh -s', 'sh -s']);
});
