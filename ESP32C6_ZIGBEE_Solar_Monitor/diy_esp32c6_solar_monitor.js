// External converter: DIY ESP32-C6 solar-thermal monitor (v1).
// Deploy to: ~/zigbee2mqtt/data/external_converters/diy_esp32c6_solar_monitor.js
//
// EP10/11/12 msTemperatureMeasurement = DS18B20 probes A/B/C (sorted by ROM, stable across reboots)
// EP13       msTemperatureMeasurement = C6 die temperature (diagnostic)
// EP14       genAnalogInput           = electric heater current, A RMS
// EP15       genAnalogInput           = solar circulator current, A RMS
// EP16       genAnalogInput/Output    = crash post-mortem (descriptions) + crash test (output write)
//
// EVERYTHING IS POLLED. No bind/configureReporting on any endpoint: every report path faults the
// device's zboss build (see the wall module converter). The firmware refreshes attribute values every
// 60 s; onEvent reads them at the same cadence. Reads are radio-safe.
//
// Probe roles: the firmware cannot know which probe is strapped where. Set the device option
// `probe_roles` (default "tank,hot_out,cold_in") to the roles of EP10, EP11, EP12 in that order after
// commissioning (warm one probe, see which temperature moves).

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const globalStore = require('zigbee-herdsman-converters/lib/store');
const e = exposes.presets;
const ea = exposes.access;

const POLL_MS = 60 * 1000;
const DIAG_EVERY_N_TICKS = 30;          // every 30 min
const ROLES = ['tank', 'hot_out', 'cold_in'];
const HEATER_ON_A = 1.0, CIRC_ON_A = 0.1;

function roles(options) {
  const s = (options && options.probe_roles) ? String(options.probe_roles) : ROLES.join(',');
  const r = s.split(',').map((x) => x.trim()).filter(Boolean);
  return r.length === 3 ? r : ROLES;
}

const fzTemperature = {
  cluster: 'msTemperatureMeasurement',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options) => {
    const v = msg.data.measuredValue;
    if (v === undefined) return;
    const ep = msg.endpoint.ID;
    if (ep === 13) return {device_temperature: Math.round(v / 100)};
    if (ep >= 10 && ep <= 12) {
      const role = roles(options)[ep - 10];
      return {[`${role}_temperature`]: Math.round(v) / 100};
    }
  },
};

const fzCurrent = {
  cluster: 'genAnalogInput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    const ep = msg.endpoint.ID;
    const a = Math.round(v * 1000) / 1000;
    if (ep === 14) return {heater_current: a, heater_running: a > HEATER_ON_A};
    if (ep === 15) return {circulator_current: a, circulator_running: a > CIRC_ON_A};
    if (ep === 16) return {brownout_count: Math.round(v)};
  },
};

// Post-mortem in the description attributes of EP16 (see wall module converter for the why).
const fzDiagA = {
  cluster: 'genAnalogInput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    if (msg.endpoint.ID !== 16 || msg.data.description === undefined) return;
    const dev = (meta && meta.device) || msg.endpoint.getDevice();
    const a = String(msg.data.description).trim();
    globalStore.putValue(dev, 'diagA', a);
    const b = globalStore.getValue(dev, 'diagB') || '';
    return {crash_summary: (a + (b ? ' ' + b : '')).trim()};
  },
};
const fzDiagB = {
  cluster: 'genAnalogOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    if (msg.endpoint.ID !== 16 || msg.data.description === undefined) return;
    const dev = (meta && meta.device) || msg.endpoint.getDevice();
    const b = String(msg.data.description).trim();
    globalStore.putValue(dev, 'diagB', b);
    const a = globalStore.getValue(dev, 'diagA') || '';
    return {crash_summary: ((a ? a + ' ' : '') + b).trim()};
  },
};

const tzDiag = {
  key: ['crash_summary'],
  convertGet: async (entity, key, meta) => {
    const ep = meta.device.getEndpoint(16);
    if (!ep) return;
    await ep.read('genAnalogInput', ['description']);
    await ep.read('genAnalogOutput', ['description']);
  },
};
const tzCrashTest = {
  key: ['crash_test'],
  convertSet: async (entity, key, value, meta) => {
    if (String(value).toUpperCase() !== 'ON' && value !== true) return {state: {crash_test: 'OFF'}};
    const ep = meta.device.getEndpoint(16);
    if (ep) { try { await ep.write('genAnalogOutput', {presentValue: 13579}); } catch (err) { /* mid-panic */ } }
    return {state: {crash_test: 'OFF'}};
  },
};
const tzRefresh = {
  key: ['tank_temperature', 'hot_out_temperature', 'cold_in_temperature', 'heater_current', 'circulator_current'],
  convertGet: async (entity, key, meta) => {
    for (const ep of [10, 11, 12]) { const x = meta.device.getEndpoint(ep); if (x) await x.read('msTemperatureMeasurement', ['measuredValue']); }
    for (const ep of [14, 15]) { const x = meta.device.getEndpoint(ep); if (x) await x.read('genAnalogInput', ['presentValue']); }
  },
};

async function pollAll(device, tick) {
  for (const ep of [10, 11, 12, 13]) {
    const x = device.getEndpoint(ep);
    if (x) { try { await x.read('msTemperatureMeasurement', ['measuredValue']); } catch (e) { /* offline */ } }
  }
  for (const ep of [14, 15]) {
    const x = device.getEndpoint(ep);
    if (x) { try { await x.read('genAnalogInput', ['presentValue']); } catch (e) { /* offline */ } }
  }
  if (tick % DIAG_EVERY_N_TICKS === 0) {
    const d = device.getEndpoint(16);
    if (d) {
      try { await d.read('genAnalogInput', ['presentValue']); } catch (e) {}
      try { await d.read('genAnalogInput', ['description']); } catch (e) {}
      try { await d.read('genAnalogOutput', ['description']); } catch (e) {}
    }
  }
}

module.exports = [
  {
    zigbeeModel: ['ESP32C6-SOLAR-MON'],
    model: 'ESP32C6-SOLAR-MON',
    vendor: 'DIY',
    description: 'ESP32-C6 solar-thermal monitor: tank + hot-out + cold-in DS18B20, heater and circulator CTs (Zigbee router)',
    fromZigbee: [fzTemperature, fzCurrent, fzDiagA, fzDiagB],
    toZigbee: [tzRefresh, tzDiag, tzCrashTest],
    ota: true,
    options: [
      exposes.text('probe_roles', ea.SET)
        .withDescription('Roles of the three DS18B20 probes on EP10,EP11,EP12 (sorted by ROM), comma separated. ' +
          'Default tank,hot_out,cold_in. Set after commissioning by warming each probe.'),
    ],
    exposes: [
      exposes.numeric('tank_temperature', ea.STATE_GET).withUnit('°C').withDescription('500 L tank probe'),
      exposes.numeric('hot_out_temperature', ea.STATE_GET).withUnit('°C').withDescription('DHW outlet pipe (strap-on) — jumps on a draw'),
      exposes.numeric('cold_in_temperature', ea.STATE_GET).withUnit('°C').withDescription('Mains inlet pipe (strap-on) — drops on a draw'),
      exposes.numeric('heater_current', ea.STATE_GET).withUnit('A').withDescription('Electric backup heater phase, RMS'),
      exposes.numeric('circulator_current', ea.STATE_GET).withUnit('A').withDescription('Solar circulator phase, RMS'),
      exposes.binary('heater_running', ea.STATE, true, false).withDescription(`heater_current > ${HEATER_ON_A} A`),
      exposes.binary('circulator_running', ea.STATE, true, false).withDescription(`circulator_current > ${CIRC_ON_A} A`),
      exposes.numeric('brownout_count', ea.STATE).withDescription('Brownout resets since flash').withCategory('diagnostic'),
      exposes.text('crash_summary', ea.STATE_GET)
        .withDescription('Last reset reason, all-cause restart count and core-dump post-mortem (pc/mcause/ELF hash). ' +
          'Symbolise with personal-ops/tools/esp32/crashdecode.sh.').withCategory('diagnostic'),
      exposes.binary('crash_test', ea.SET, 'ON', 'OFF')
        .withDescription('Deliberately panic the board to prove the post-mortem path. It REBOOTS; the write times out.')
        .withCategory('config'),
      e.device_temperature().withDescription('MCU die temperature (diagnostic)'),
    ],
    configure: async (device) => {
      // Read everything once so values appear right after interview. NO bind / configureReporting.
      await pollAll(device, 0);
    },
    onEvent: async (a, b, c) => {
      // Z2M >= 2.x passes ONE object; legacy passed (type, data, device). Accept both.
      const type = typeof a === 'string' ? a : a?.type;
      const device = typeof a === 'string' ? c : a?.data?.device;
      if (!device) return;
      if (type === 'stop' || type === 'deviceLeave') {
        const h = globalStore.getValue(device, 'solar_poll');
        if (h) { clearInterval(h); globalStore.clearValue(device, 'solar_poll'); }
        return;
      }
      if (globalStore.hasValue(device, 'solar_poll')) return;
      let tick = 0;
      const h = setInterval(async () => { tick++; await pollAll(device, tick); }, POLL_MS);
      globalStore.putValue(device, 'solar_poll', h);
    },
  },
];
