// External converter: DIY ESP32-C6 MultiAccessory (v1) — T-0036.
// Deploy to: ~/zigbee2mqtt/data/external_converters/diy_esp32c6_multiaccessory.js
//
// EP10..EP13  Dimmable Light, one per PWM channel (OTA client on EP10)
// EP14        Multistate Output -> output profile selector (NVS, reboots to apply)
// EP15        Analog Input      -> brownout/reset counter
// EP16        Temperature       -> C6 die temperature (diagnostic)
// EP20        Temperature + Humidity -> AM2320, ONLY PRESENT IF THE SENSOR IS FITTED
//
// The exposes list is a FUNCTION of the device, so one converter serves every
// board and every profile: a light appears for each of EP10..EP13 that actually
// exists, and it becomes a colour-temperature light automatically when that
// endpoint carries lightingColorCtrl (profile CCT_2DIM, phase 2). Room
// temperature/humidity only appear on boards where the AM2320 was detected.
//
// ⚠️ NOTHING HERE MAY CONFIGURE REPORTING. Every Zigbee report path faults the
// device's zboss build (T-0012): the app's report*() asserts in
// esp_zigbee_zcl_command.c:263, and setReporting()/ConfigureReporting null-deref
// the stack's own send path. So: no bind+configureReporting on any cluster, and
// the diagnostics are POLLED with reads (the read-response path is the one that
// works). Lights do not need reporting — Z2M is optimistic after a command.

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const globalStore = require('zigbee-herdsman-converters/lib/store');
const e = exposes.presets;
const ea = exposes.access;

const EP = {l1: 10, l2: 11, l3: 12, l4: 13, cfg: 14, diag: 15, die: 16, room: 20};
const LIGHT_EPS = [['l1', 10], ['l2', 11], ['l3', 12], ['l4', 13]];

// Must match the firmware's Profile enum order (presentValue == index).
const PROFILES = ['4XDIM', 'CCT_2DIM', '2XCCT', 'RGBW', 'RGB_DIM'];
const PROFILES_IMPLEMENTED = ['4XDIM'];   // firmware v1; extend as phases land

// One 60 s tick drives every poll; each value has its own cadence in ticks.
const POLL_TICK_MS = 60 * 1000;
const DIE_TEMP_TICKS = 5;    // 5 min
const ROOM_TICKS = 3;        // 3 min (firmware refreshes the AM2320 every 2 min)
const DIAG_TICKS = 30;       // 30 min — only changes on a brownout

// exposes() is called from Z2M's resolveDevicesDefinitions() as well as for real
// devices, and not every caller passes a zigbee-herdsman Device — some pass an
// object with no getEndpoint(). Calling it blind throws
// "device.getEndpoint is not a function", and because loadJS() re-resolves ALL
// definitions after registering each converter, one throwing definition gets
// every converter loaded after it renamed to .invalid as well. So: only probe
// endpoints on something that actually quacks like a Device, and fall back to
// advertising the full set when we cannot tell.
const asDevice = (device) => (device && typeof device.getEndpoint === 'function' ? device : null);
const hasEp = (device, id) => {
  const d = asDevice(device);
  return d ? !!d.getEndpoint(id) : true;   // unknown -> assume present
};
const isCct = (device, id) => {
  const d = asDevice(device);
  const ep = d && d.getEndpoint(id);
  return !!(ep && ep.supportsInputCluster && ep.supportsInputCluster('lightingColorCtrl'));
};

const fzProfile = {
  cluster: 'genMultistateOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    return {profile: PROFILES[v] !== undefined ? PROFILES[v] : v};
  },
};

// One converter for both temperature endpoints: EP16 is the MCU die (a
// diagnostic), EP20 is the room. Returning fixed keys keeps the multiEndpoint
// postfixing off these two, which would otherwise produce temperature_die.
const fzTemperature = {
  cluster: 'msTemperatureMeasurement',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.measuredValue;
    if (v === undefined) return;
    if (msg.endpoint.ID === EP.die) return {device_temperature: Math.round(v / 100)};
    if (msg.endpoint.ID === EP.room) return {temperature: Math.round(v) / 100};
    return;
  },
};

const fzHumidity = {
  cluster: 'msRelativeHumidity',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.measuredValue;
    if (v === undefined) return;
    return {humidity: Math.round(v) / 100};
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

// Resolve EP14 EXPLICITLY. The wall-input module hit a Z2M framework crash
// (publish.ts: reading 'ID' of undefined) using modernExtend's withEndpoint()
// on a multistate-output set, so do it by hand here too.
const tzProfile = {
  key: ['profile'],
  convertSet: async (entity, key, value, meta) => {
    const idx = PROFILES.indexOf(value);
    if (idx < 0) throw new Error(`profile must be one of: ${PROFILES.join(', ')}`);
    const ep = meta.device.getEndpoint(EP.cfg);
    if (!ep) throw new Error('device has no config endpoint (EP14)');
    await ep.write('genMultistateOutput', {presentValue: idx});
    // The device stores this in NVS and REBOOTS: endpoint composition is fixed
    // at interview time, so Z2M must re-interview it before the new endpoints
    // appear. Nothing to do here but report what we wrote.
    return {state: {[key]: value}};
  },
  convertGet: async (entity, key, meta) => {
    const ep = meta.device.getEndpoint(EP.cfg);
    if (ep) await ep.read('genMultistateOutput', ['presentValue']);
  },
};

module.exports = [
  {
    zigbeeModel: ['ESP32C6-MULTIACCESSORY'],
    model: 'ESP32C6-MULTIACCESSORY',
    vendor: 'DIY',
    description: 'ESP32-C6 MultiAccessory — 4 PWM channels, IR AC and AM2320, Zigbee router',
    fromZigbee: [fz.on_off, fz.brightness, fz.color_colortemp, fzProfile, fzTemperature, fzHumidity, fzBrownout],
    toZigbee: [tz.light_onoff_brightness, tz.light_colortemp, tzProfile],
    ota: true,
    meta: {multiEndpoint: true},
    endpoint: () => ({...EP}),
    exposes: (device) => {
      const list = [];
      // A light per channel endpoint that exists, colour-temperature capable
      // when the firmware declared lightingColorCtrl on it.
      for (const [name, id] of LIGHT_EPS) {
        if (!hasEp(device, id)) continue;
        list.push(isCct(device, id)
          ? e.light_brightness_colortemp([153, 500]).withEndpoint(name)
          : e.light_brightness().withEndpoint(name));
      }
      list.push(exposes.enum('profile', ea.ALL, PROFILES)
        .withDescription('Output profile. Stored in NVS; the board REBOOTS to apply it and must then be ' +
          're-interviewed in Z2M, because Zigbee fixes endpoint composition at interview time. ' +
          `Implemented in firmware v1: ${PROFILES_IMPLEMENTED.join(', ')} — anything else falls back to 4XDIM.`));
      list.push(exposes.numeric('brownout_count', ea.STATE)
        .withDescription('Brownout/unexpected resets since flash'));
      list.push(e.device_temperature().withDescription('MCU die temperature (diagnostic, not ambient)'));
      // Room sensor only on boards where the AM2320 was auto-detected.
      if (hasEp(device, EP.room)) {
        list.push(e.temperature(), e.humidity());
      }
      return list;
    },
    configure: async (device, coordinatorEndpoint) => {
      // NO bind + configureReporting anywhere (see the header). Just seed values
      // with reads so the entities are populated straight after the interview.
      const readSafe = async (id, cluster, attrs) => {
        const ep = device.getEndpoint(id);
        if (!ep) return;
        try { await ep.read(cluster, attrs); } catch (err) { /* the poller will fill it */ }
      };
      for (const [, id] of LIGHT_EPS) {
        await readSafe(id, 'genOnOff', ['onOff']);
        await readSafe(id, 'genLevelCtrl', ['currentLevel']);
      }
      await readSafe(EP.cfg, 'genMultistateOutput', ['presentValue']);
      await readSafe(EP.diag, 'genAnalogInput', ['presentValue']);
      await readSafe(EP.die, 'msTemperatureMeasurement', ['measuredValue']);
      await readSafe(EP.room, 'msTemperatureMeasurement', ['measuredValue']);
      await readSafe(EP.room, 'msRelativeHumidity', ['measuredValue']);
    },
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
        const h = globalStore.getValue(device, 'poll');
        if (h) { clearInterval(h); globalStore.clearValue(device, 'poll'); }
        return;
      }
      if (globalStore.hasValue(device, 'poll')) return;

      let tick = 0;
      const h = setInterval(async () => {
        tick++;
        const read = async (id, cluster, attrs) => {
          const ep = device.getEndpoint(id);
          if (!ep) return;
          try { await ep.read(cluster, attrs); } catch (err) { /* asleep/offline */ }
        };
        if (tick % DIE_TEMP_TICKS === 0) await read(EP.die, 'msTemperatureMeasurement', ['measuredValue']);
        if (tick % ROOM_TICKS === 0) {
          await read(EP.room, 'msTemperatureMeasurement', ['measuredValue']);
          await read(EP.room, 'msRelativeHumidity', ['measuredValue']);
        }
        if (tick % DIAG_TICKS === 0) await read(EP.diag, 'genAnalogInput', ['presentValue']);
      }, POLL_TICK_MS);
      globalStore.putValue(device, 'poll', h);
    },
  },
];
