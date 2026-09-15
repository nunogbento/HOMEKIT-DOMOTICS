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
const globalStore = require('zigbee-herdsman-converters/lib/store');
const e = exposes.presets;
const ea = exposes.access;

// Die temperature is POLLED, not reported: every Zigbee temp-report path faults the
// device's zboss build (app reportTemperature() asserts; setReporting / coordinator
// ConfigureReporting null-deref the stack's zb_zcl_send_report_attr_command). The
// firmware keeps the attribute fresh with setTemperature(); we read it on a timer.
const TEMP_POLL_MS = 5 * 60 * 1000;

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

/* Post-mortem, ported from the MultiAccessory converter. It arrives in the
 * `description` (0x001C) attributes of the STANDARD analog input and output
 * clusters on EP13 — a custom cluster was tried there first and the device never
 * answered reads on it (esp-zigbee-sdk #278/#406/#434/#811). Two 32-char fields:
 *   A (input)  rst=PANIC n=13 t=Zigbee_mai
 *   B (output) pc=42000128 mc=7 s=f3a91996
 * Joined for display, symbolised with personal-ops/tools/esp32/crashdecode.sh. */
const fzDiagA = {
  cluster: 'genAnalogInput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg, publish, options, meta) => {
    if (msg.endpoint.ID !== 13 || msg.data.description === undefined) return;
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
    if (msg.endpoint.ID !== 13 || msg.data.description === undefined) return;
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
    const ep = meta.device.getEndpoint(13);
    if (!ep) return;
    await ep.read('genAnalogInput', ['description']);
    await ep.read('genAnalogOutput', ['description']);
  },
};

/* Writing 13579 to EP13's analog output panics the board on purpose. The write
 * will report a timeout because the board is mid-panic — that is expected. */
const tzCrashTest = {
  key: ['crash_test'],
  convertSet: async (entity, key, value, meta) => {
    if (String(value).toUpperCase() !== 'ON' && value !== true) return {state: {crash_test: 'OFF'}};
    const ep = meta.device.getEndpoint(13);
    if (ep) {
      try { await ep.write('genAnalogOutput', {presentValue: 13579}); } catch (err) { /* expected */ }
    }
    return {state: {crash_test: 'OFF'}};
  },
};

const tzMode = {
  key: ['mode_1', 'mode_2'],
  convertSet: async (entity, key, value, meta) => {
    const idx = MODES.indexOf(value);
    if (idx < 0) throw new Error(`mode must be one of: ${MODES.join(', ')}`);
    const ep = meta.device.getEndpoint(key === 'mode_1' ? 10 : 11);
    await ep.write('genMultistateOutput', {presentValue: idx});
    return {state: {[key]: value}};
  },
  convertGet: async (entity, key, meta) => {
    const ep = meta.device.getEndpoint(key === 'mode_1' ? 10 : 11);
    await ep.read('genMultistateOutput', ['presentValue']);
  },
};

module.exports = [
  {
    zigbeeModel: ['ESP32C6-2CH-INPUT'],
    model: 'ESP32C6-2CH-INPUT',
    vendor: 'DIY',
    description: 'ESP32-C6 mains-powered 2-channel in-wall scene switch (momentary/toggle, Zigbee router)',
    fromZigbee: [fzAction, fzMode, fzTemp, fzBrownout, fzDiagA, fzDiagB],
    toZigbee: [tzMode, tzDiag, tzCrashTest],
    ota: true,
    exposes: [
      e.action(ACTIONS),
      exposes.enum('mode_1', ea.ALL, MODES)
        .withDescription('Input 1 behaviour: momentary=single/double/hold; toggle=one action per flip; ' +
          'toggle_directional=on/off; toggle_scenes=single/double from flip count'),
      exposes.enum('mode_2', ea.ALL, MODES)
        .withDescription('Input 2 behaviour (see input 1)'),
      exposes.numeric('brownout_count', ea.STATE).withDescription('Brownout/unexpected resets since flash')
        .withCategory('diagnostic'),
      exposes.text('crash_summary', ea.STATE_GET)
        .withDescription('Last reset reason, the all-cause restart count, and — if the board ' +
          'panicked — the faulting task, pc, mcause and the crashing build\'s ELF hash. ' +
          'brownout_count only ever counted what the BOD classified, which is why the ' +
          '2026-09-09 restart was unexplainable. Symbolise with tools/esp32/crashdecode.sh.')
        .withCategory('diagnostic'),
      exposes.binary('crash_test', ea.SET, 'ON', 'OFF')
        .withDescription('Deliberately panic the board to prove the post-mortem path works. ' +
          'It REBOOTS. The write will report a timeout — that is expected.')
        .withCategory('config'),
      e.device_temperature().withDescription('MCU die temperature (diagnostic, not ambient)'),
    ],
    configure: async (device, coordinatorEndpoint) => {
      // Post-mortem: read both description fields once, so it shows up immediately.
      {
        const ep13 = device.getEndpoint(13);
        if (ep13) {
          try { await ep13.read('genAnalogInput', ['description']); } catch (e) { /* offline */ }
          try { await ep13.read('genAnalogOutput', ['description']); } catch (e) { /* offline */ }
        }
      }

      // Actions: bind each Multistate Input to the coordinator (manual reports go to binds).
      for (const epId of [10, 11]) {
        const ep = device.getEndpoint(epId);
        if (!ep) continue;
        await ep.bind('genMultistateInput', coordinatorEndpoint);
        try { await ep.read('genMultistateOutput', ['presentValue']); } catch (e) { /* mode readback */ }
      }
      // Die temperature: NEVER bind/configureReporting here — the coordinator's
      // ConfigureReporting faults the device's stack report path. We POLL instead
      // (see onEvent). Just do one read so a value appears right after interview.
      const t = device.getEndpoint(12);
      if (t) {
        try { await t.read('msTemperatureMeasurement', ['measuredValue']); } catch (e) { /* first poll fills it */ }
      }
      // Diagnostics: bind the analog (brownout counter) so its report arrives.
      const a = device.getEndpoint(13);
      if (a) {
        await a.bind('genAnalogInput', coordinatorEndpoint);
        try { await a.read('genAnalogInput', ['presentValue']); } catch (e) {}
      }
    },
    // Poll die temperature (report paths crash the device — see header). A timer
    // reads measuredValue every TEMP_POLL_MS; fzTemp turns the read-response into
    // device_temperature. Interval is stored per-device and cleared on stop.
    /* Z2M >= 2.x calls onEvent({type, data:{device, state, options, ...}}) — a
     * SINGLE object. The legacy signature was (type, data, device), so reading
     * the 3rd argument yields undefined on every event and any poller guarded on
     * it silently never starts (and, unguarded, globalStore throws
     * "Cannot read properties of undefined (reading 'constructor')" — that is
     * what the OnEvent/deviceInterview EventBus errors were). Accept both. */
    onEvent: async (a, b, c) => {
      const type = typeof a === 'string' ? a : a?.type;
      const device = typeof a === 'string' ? c : a?.data?.device;
      if (!device) return;
      if (type === 'stop' || type === 'deviceLeave') {
        const h = globalStore.getValue(device, 'temp_poll');
        if (h) { clearInterval(h); globalStore.clearValue(device, 'temp_poll'); }
        return;
      }
      if (globalStore.hasValue(device, 'temp_poll')) return;
      const ep = device.getEndpoint(12);
      if (!ep) return;
      const h = setInterval(async () => {
        try { await ep.read('msTemperatureMeasurement', ['measuredValue']); } catch (e) { /* device asleep/offline */ }
      }, TEMP_POLL_MS);
      globalStore.putValue(device, 'temp_poll', h);
    },
  },
];
