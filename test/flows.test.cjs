const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const nodes = JSON.parse(fs.readFileSync(path.join(__dirname, '../std_lab_data/nodered/flows.json'), 'utf8'));

test('flow IDs and references are valid, with one MQTT input feeding the router', () => {
  const ids = new Set(nodes.map(n => n.id));
  assert.equal(ids.size, nodes.length);
  for (const n of nodes) {
    for (const id of (n.wires || []).flat()) assert(ids.has(id), `missing wire target ${id}`);
    for (const key of ['z', 'group', 'tab', 'influxdb']) {
      if (n[key]) assert(ids.has(n[key]), `missing ${key} reference ${n[key]}`);
    }
    if (['mqtt in', 'mqtt out'].includes(n.type)) assert(ids.has(n.broker));
  }
  const inputs = nodes.filter(n => n.type === 'mqtt in');
  assert.equal(inputs.length, 1);
  assert.equal(inputs[0].topic, 'smart_farm/site_01/zone_A/node_01/#');
  assert.equal(inputs[0].datatype, 'json');
  assert(inputs[0].wires.flat().includes('router_func'));
});

const route = new Function('msg', nodes.find(n => n.id === 'router_func').func);
test('telemetry reaches InfluxDB and gauges with correct fields and tags', () => {
  const payload = { temperature: 25.5, humidity: 60, pump_state: 1, valve_state: 0 };
  const result = route({ topic: 'smart_farm/site_01/zone_A/node_01/telemetry', payload });
  assert.equal(result[0].measurement, 'sensor_data');
  assert.deepEqual(result[0].payload, [
    { temperature: 25.5, humidity: 60, pump: 1, valve: 0 },
    { site: 'site_01', zone: 'zone_A', device: 'node_01' }
  ]);
  assert.deepEqual(result[1], { payload });
  assert.equal(result[2], null);
});

test('status routes separately and ACK/non-object messages do not become sensor data', () => {
  for (const status of ['online', 'offline']) {
    assert.deepEqual(route({ topic: 'smart_farm/site_01/zone_A/node_01/status', payload: { status } }),
      [null, null, { payload: status }]);
  }
  for (const msg of [
    { topic: 'smart_farm/site_01/zone_A/node_01/ack', payload: { ack: 'pump' } },
    { topic: 'smart_farm/site_01/zone_A/node_01/telemetry', payload: 'bad' }
  ]) assert.deepEqual(route(msg), [null, null, null]);
});
