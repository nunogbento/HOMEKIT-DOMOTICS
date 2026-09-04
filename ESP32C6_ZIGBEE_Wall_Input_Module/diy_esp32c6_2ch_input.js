// External converter: DIY ESP32-C6 2-channel in-wall SCENE / BUTTON module (v4).
// Deploy to: ~/zigbee2mqtt/data/external_converters/diy_esp32c6_2ch_input.js
//
// EP10/EP11 = Multistate Input (button actions) + Multistate Output (per-channel mode)
// EP12       = die temperature (msTemperatureMeasurement) -> device_temperature
// EP13       = analog input (genAnalogInput)             -> brownout_count
//
// Action presentValue: 1 single, 2 double, 3 hold, 4 toggle, 5 on, 6 off  (0 = idle).
// Mode presentValue (genMultistateOutput): 0 momentary, 1 toggle,
//                                          2 toggle_directional, 3 toggle_scenes.

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const ea = exposes.access;

const GESTURE = {1: 'single', 2: 'double', 3: 'hold', 4: 'toggle', 5: 'on', 6: 'off'};
const MODES = ['momentary', 'toggle', 'toggle_directional', 'toggle_scenes'];

const ACTIONS = [];
for (const b of ['button_1', 'button_2'])
  for (const g of ['single', 'double', 'hold', 'toggle', 'on', 'off']) ACTIONS.push(`${b}_${g}`);

// Same-action reports can arrive twice ms apart (duplicate coordinator binding);
// the gesture timing means an identical action can't legitimately repeat <250ms.
const lastAction = new Map();
const DEDUP_MS = 250;

const fzAction = {
  cluster: 'genMultistateInput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    const v = msg.data.presentValue;
    if (v === undefined || v === 0) return;
    const g = GESTURE[v];
    if (!g) return;
    const btn = msg.endpoint.ID === 10 ? 'button_1' : 'button_2';
    const action = `${btn}_${g}`;
    const key = `${meta.device.ieeeAddr}:${action}`;
    const now = Date.now();
    const prev = lastAction.get(key);
    lastAction.set(key, now);
    if (prev !== undefined && (now - prev) < DEDUP_MS) return;
    return {action};
  },
};

const fzMode = {
  cluster: 'genMultistateOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    const ep = msg.endpoint.ID === 10 ? '1' : '2';
    return {[`mode_${ep}`]: (MODES[v] !== undefined ? MODES[v] : v)};
  },
};

const fzTemp = {
  cluster: 'msTemperatureMeasurement',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.measuredValue;
    if (v === undefined) return;
    return {device_temperature: Math.round(v / 100)};
  },
};

const fzBrownout = {
  cluster: 'genAnalogInput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    return {brownout_count: Math.round(v)};
  },
};

const tzMode = {
  key: ['mode'],
  convertSet: async (entity, key, value, meta) => {
    const idx = MODES.indexOf(value);
    if (idx < 0) throw new Error(`mode must be one of: ${MODES.join(', ')}`);
    await entity.write('genMultistateOutput', {presentValue: idx});
    return {state: {[`mode_${meta.endpoint_name}`]: value}};
  },
  convertGet: async (entity, key, meta) => {
    await entity.read('genMultistateOutput', ['presentValue']);
  },
};

module.exports = [
  {
    zigbeeModel: ['ESP32C6-2CH-INPUT'],
    model: 'ESP32C6-2CH-INPUT',
    vendor: 'DIY',
    description: 'ESP32-C6 mains-powered 2-channel in-wall scene switch (momentary/toggle, Zigbee router)',
    fromZigbee: [fzAction, fzMode, fzTemp, fzBrownout],
    toZigbee: [tzMode],
    ota: true,
    exposes: [
      e.action(ACTIONS),
      exposes.enum('mode', ea.ALL, MODES).withEndpoint('1')
        .withDescription('Input 1 behaviour: momentary=single/double/hold; toggle=one action per flip; ' +
          'toggle_directional=on/off; toggle_scenes=single/double from flip count'),
      exposes.enum('mode', ea.ALL, MODES).withEndpoint('2')
        .withDescription('Input 2 behaviour (see input 1)'),
      e.device_temperature().withDescription('MCU die temperature (diagnostic, not ambient)'),
      exposes.numeric('brownout_count', ea.STATE).withDescription('Brownout/unexpected resets since flash'),
    ],
    endpoint: (device) => ({'1': 10, '2': 11}),
    meta: {multiEndpoint: true},
    configure: async (device, coordinatorEndpoint) => {
      // Actions: bind each Multistate Input to the coordinator (manual reports go to binds).
      for (const epId of [10, 11]) {
        const ep = device.getEndpoint(epId);
        if (!ep) continue;
        await ep.bind('genMultistateInput', coordinatorEndpoint);
        try { await ep.read('genMultistateOutput', ['presentValue']); } catch (e) { /* mode readback */ }
      }
      // Diagnostics: bind temp + analog so their manual reports arrive.
      const t = device.getEndpoint(12);
      if (t) {
        await t.bind('msTemperatureMeasurement', coordinatorEndpoint);
        try { await t.read('msTemperatureMeasurement', ['measuredValue']); } catch (e) {}
      }
      const a = device.getEndpoint(13);
      if (a) {
        await a.bind('genAnalogInput', coordinatorEndpoint);
        try { await a.read('genAnalogInput', ['presentValue']); } catch (e) {}
      }
    },
  },
];
