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

const EP = {
  l1: 10, l2: 11, l3: 12, l4: 13,
  cfg: 14, diag: 15, die: 16, mask: 17, acCfg: 18, room: 20,
  acMode: 30, acTemp: 31, acFan: 32, acSwing: 33,
};
const LIGHT_EPS = [['l1', 10], ['l2', 11], ['l3', 12], ['l4', 13]];

// Must match the firmware's Profile enum order (presentValue == index).
const PROFILES = ['4XDIM', 'CCT_2DIM', '2XCCT', 'RGBW', 'RGB_DIM'];
const PROFILES_IMPLEMENTED = PROFILES;    // all five implemented as of firmware v2

// Must match the firmware's AcMode enum order.
const AC_MODES = ['off', 'cool', 'heat', 'dry', 'fan_only'];
// Zigbee FanControl fanMode enum -> Z2M fan_mode strings.
const FAN_MODES = {0: 'off', 1: 'low', 2: 'medium', 3: 'high', 4: 'on', 5: 'auto', 6: 'smart'};
const FAN_MODES_EXPOSED = ['low', 'medium', 'high', 'auto'];

// One 60 s tick drives every poll; each value has its own cadence in ticks.
const POLL_TICK_MS = 60 * 1000;
const DIE_TEMP_TICKS = 5;    // 5 min
const ROOM_TICKS = 3;        // 3 min (firmware refreshes the AM2320 every 2 min)
const DIAG_TICKS = 30;       // 30 min — only changes on a brownout
const COLOUR_TICKS = 2;      // 2 min — nothing is reported, so colour state is polled

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
/* What KIND of light lives on this endpoint. The firmware declares
 * colorCapabilities per profile — CT-only for the CCT profiles, hue/sat + x/y
 * for RGBW/RGB_DIM — so read that rather than guessing from which endpoints
 * exist (which cannot separate RGB_DIM from CCT_2DIM reliably). configure()
 * reads the attribute so it is in herdsman's cache by the time we get here. */
const CAP_HUE_SAT = 0x01, CAP_XY = 0x08, CAP_COLOR_TEMP = 0x10;
const lightKind = (device, id) => {
  const d = asDevice(device);
  const ep = d && d.getEndpoint(id);
  if (!ep) return 'dimmer';
  if (!(ep.supportsInputCluster && ep.supportsInputCluster('lightingColorCtrl'))) return 'dimmer';
  const caps = ep.getClusterAttributeValue ? ep.getClusterAttributeValue('lightingColorCtrl', 'colorCapabilities') : undefined;
  if (caps === undefined || caps === null) return 'cct';   // unknown: CCT is the only colour-ish profile in the fleet
  if (caps & (CAP_HUE_SAT | CAP_XY)) return (caps & CAP_COLOR_TEMP) ? 'color_cct' : 'color';
  return 'cct';
};

/* genMultistateOutput: EP14 is the output profile, EP30 the AC mode. */
const fzMultistateOutput = {
  cluster: 'genMultistateOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    if (msg.endpoint.ID === EP.cfg) return {profile: PROFILES[v] !== undefined ? PROFILES[v] : v};
    if (msg.endpoint.ID === EP.acMode) return {system_mode: AC_MODES[v] !== undefined ? AC_MODES[v] : v};
    return;
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
    if (msg.endpoint.ID === EP.room) {
      const t = Math.round(v) / 100;
      // The climate expose has no local temperature of its own — the split can't
      // report one over IR — so the room sensor feeds it.
      const hasAc = msg.device && msg.device.getEndpoint && msg.device.getEndpoint(EP.acMode);
      return hasAc ? {temperature: t, local_temperature: t} : {temperature: t};
    }
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

/* genAnalogOutput is used by two very different things, so switch on endpoint:
 * EP17 is the channel mask, EP31 is the AC setpoint. */
const fzAnalogOutput = {
  cluster: 'genAnalogOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    if (msg.endpoint.ID === EP.mask) return {channels: Math.round(v)};
    if (msg.endpoint.ID === EP.acTemp) return {occupied_heating_setpoint: Math.round(v * 10) / 10};
    return;
  },
};

/* Same story for genBinaryOutput: EP18 is the AC-enabled config flag, EP33 the swing. */
const fzBinaryOutput = {
  cluster: 'genBinaryOutput',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.presentValue;
    if (v === undefined) return;
    if (msg.endpoint.ID === EP.acCfg) return {ac_enabled: v ? 'ON' : 'OFF'};
    if (msg.endpoint.ID === EP.acSwing) return {swing: v ? 'ON' : 'OFF'};
    return;
  },
};

const fzFanMode = {
  cluster: 'hvacFanCtrl',
  type: ['attributeReport', 'readResponse'],
  convert: (model, msg) => {
    const v = msg.data.fanMode;
    if (v === undefined) return;
    return {fan_mode: FAN_MODES[v] !== undefined ? FAN_MODES[v] : v};
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

const tzMask = {
  key: ['channels'],
  convertSet: async (entity, key, value, meta) => {
    const v = Number(value);
    if (!Number.isInteger(v) || v < 1 || v > 15) {
      throw new Error('channels must be an integer 1..15 (bit0=CH1 .. bit3=CH4)');
    }
    const ep = meta.device.getEndpoint(EP.mask);
    if (!ep) throw new Error('device has no channel-mask endpoint (EP17)');
    await ep.write('genAnalogOutput', {presentValue: v});
    // Like the profile: stored in NVS, the board REBOOTS, and Z2M must
    // re-interview before the new endpoint set shows up.
    return {state: {[key]: v}};
  },
  convertGet: async (entity, key, meta) => {
    const ep = meta.device.getEndpoint(EP.mask);
    if (ep) await ep.read('genAnalogOutput', ['presentValue']);
  },
};

const writeEp = async (meta, epId, cluster, payload, what) => {
  const ep = meta.device.getEndpoint(epId);
  if (!ep) throw new Error(`device has no ${what} endpoint (EP${epId})`);
  await ep.write(cluster, payload);
};

const tzAc = {
  key: ['system_mode', 'occupied_heating_setpoint', 'fan_mode', 'swing'],
  /* Z2M calls a toZigbee converter ONCE per payload, not once per key, and skips
   * it for the remaining keys it owns — the same contract tz.light_onoff_brightness
   * relies on to handle state+brightness together. HA sets mode, temperature and
   * fan in a single service call, so read them all off meta.message; handling only
   * the `key` argument silently dropped everything but the first. Setpoint and fan
   * go out before the mode so the coalesced IR frame already carries them. */
  convertSet: async (entity, key, value, meta) => {
    const msg = (meta && meta.message) || {[key]: value};
    const state = {};

    if (msg.occupied_heating_setpoint !== undefined) {
      const t = Number(msg.occupied_heating_setpoint);
      if (!(t >= 16 && t <= 30)) throw new Error('setpoint must be 16..30 C');
      await writeEp(meta, EP.acTemp, 'genAnalogOutput', {presentValue: t}, 'AC setpoint');
      state.occupied_heating_setpoint = t;
    }

    if (msg.fan_mode !== undefined) {
      const wanted = String(msg.fan_mode).toLowerCase();
      const num = Object.keys(FAN_MODES).find((k) => FAN_MODES[k] === wanted);
      if (num === undefined) throw new Error(`fan_mode must be one of: ${FAN_MODES_EXPOSED.join(', ')}`);
      await writeEp(meta, EP.acFan, 'hvacFanCtrl', {fanMode: Number(num)}, 'AC fan');
      state.fan_mode = wanted;
    }

    if (msg.swing !== undefined) {
      const on = String(msg.swing).toUpperCase() === 'ON' || msg.swing === true;
      await writeEp(meta, EP.acSwing, 'genBinaryOutput', {presentValue: on ? 1 : 0}, 'AC swing');
      state.swing = on ? 'ON' : 'OFF';
    }

    if (msg.system_mode !== undefined) {
      const idx = AC_MODES.indexOf(String(msg.system_mode).toLowerCase());
      if (idx < 0) throw new Error(`system_mode must be one of: ${AC_MODES.join(', ')}`);
      await writeEp(meta, EP.acMode, 'genMultistateOutput', {presentValue: idx}, 'AC mode');
      state.system_mode = AC_MODES[idx];
    }

    return {state};
  },
  convertGet: async (entity, key, meta) => {
    const map = {
      system_mode: [EP.acMode, 'genMultistateOutput', ['presentValue']],
      occupied_heating_setpoint: [EP.acTemp, 'genAnalogOutput', ['presentValue']],
      fan_mode: [EP.acFan, 'hvacFanCtrl', ['fanMode']],
      swing: [EP.acSwing, 'genBinaryOutput', ['presentValue']],
    };
    const m = map[key];
    if (!m) return;
    const ep = meta.device.getEndpoint(m[0]);
    if (ep) await ep.read(m[1], m[2]);
  },
};

const tzAcEnabled = {
  key: ['ac_enabled'],
  convertSet: async (entity, key, value, meta) => {
    const on = String(value).toUpperCase() === 'ON' || value === true;
    await writeEp(meta, EP.acCfg, 'genBinaryOutput', {presentValue: on ? 1 : 0}, 'AC config');
    // Stored in NVS; the board REBOOTS and must be re-interviewed before the
    // AC endpoints appear (or disappear).
    return {state: {ac_enabled: on ? 'ON' : 'OFF'}};
  },
  convertGet: async (entity, key, meta) => {
    const ep = meta.device.getEndpoint(EP.acCfg);
    if (ep) await ep.read('genBinaryOutput', ['presentValue']);
  },
};

module.exports = [
  {
    zigbeeModel: ['ESP32C6-MULTIACCESSORY'],
    model: 'ESP32C6-MULTIACCESSORY',
    vendor: 'DIY',
    description: 'ESP32-C6 MultiAccessory — 4 PWM channels, IR AC and AM2320, Zigbee router',
    fromZigbee: [fz.on_off, fz.brightness, fz.color_colortemp, fzMultistateOutput, fzAnalogOutput,
                 fzBinaryOutput, fzFanMode, fzTemperature, fzHumidity, fzBrownout],
    toZigbee: [tz.light_onoff_brightness, tz.light_color_colortemp, tz.light_colortemp_startup,
               tzProfile, tzMask, tzAc, tzAcEnabled],
    ota: true,
    meta: {multiEndpoint: true},
    /* color_sync (default ON) makes Z2M recompute every colour representation
     * after each update, using BOTH the new and the CACHED state to decide what
     * to synthesise. On an endpoint that has been a CCT light and an hs light at
     * various times, that means every xy you set gets round-tripped through
     * hue/saturation and colour temperature the endpoint no longer has — which
     * is what dragged the UI sliders away from where they were clicked. Exposing
     * the option lets it be turned off so the commanded value is kept verbatim. */
    options: [exposes.options.color_sync()],
    endpoint: () => ({...EP}),
    exposes: (device) => {
      const list = [];
      // A light per channel endpoint that exists, colour-temperature capable
      // when the firmware declared lightingColorCtrl on it.
      for (const [name, id] of LIGHT_EPS) {
        if (!hasEp(device, id)) continue;
        const kind = lightKind(device, id);
        /* XY ONLY for colour, deliberately. The device is xy-native: it reports
         * colorMode 1 and its currentX/currentY round-trip EXACTLY (verified on
         * the bench), but it never updates currentHue/currentSaturation — they
         * sit at 0/0 whatever colour is set. Exposing color_hs as well gave the
         * UI a second colour model that was permanently wrong, so the picker
         * jumped somewhere else after every click. One model, the true one. */
        const expose =
          kind === 'cct'       ? e.light_brightness_colortemp([153, 500]) :
          kind === 'color'     ? e.light_brightness_colorxy() :
          kind === 'color_cct' ? e.light_brightness_colortemp_colorxy([153, 500]) :
                                 e.light_brightness();
        list.push(expose.withEndpoint(name));
      }
      list.push(exposes.enum('profile', ea.ALL, PROFILES)
        .withDescription('Output profile. Stored in NVS; the board REBOOTS to apply it and must then be ' +
          're-interviewed in Z2M, because Zigbee fixes endpoint composition at interview time. ' +
          `Implemented in firmware v1: ${PROFILES_IMPLEMENTED.join(', ')} — anything else falls back to 4XDIM.`));
      list.push(exposes.numeric('channels', ea.ALL).withValueMin(1).withValueMax(15)
        .withDescription('Which PWM channels are actually populated on this board, as a bitmask: ' +
          'bit0=CH1, bit1=CH2, bit2=CH3, bit3=CH4. A board with one strip is 1; the studio board, ' +
          'wired on CH1+CH3, is 5; all four is 15. A CCT pair needs both of its channels set. ' +
          'Stored in NVS; the board REBOOTS to apply and must then be re-interviewed.'));
      list.push(exposes.numeric('brownout_count', ea.STATE)
        .withDescription('Brownout/unexpected resets since flash'));
      list.push(e.device_temperature().withDescription('MCU die temperature (diagnostic, not ambient)'));
      // AC: one climate expose assembled from EP30..EP33, so HA gets a real
      // climate entity (and HomeKit a thermostat) despite there being no
      // thermostat server cluster on the device.
      if (hasEp(device, EP.acMode)) {
        list.push(exposes.climate()
          .withSystemMode(AC_MODES)
          /* MUST be occupied_heating_setpoint (or current_heating_setpoint): Z2M's
           * HA-discovery extension asserts on those property names specifically,
           * and occupied_cooling_setpoint made the WHOLE HomeAssistant extension
           * fail to start — house-wide, not just this device. The name is a Z2M
           * convention for a single-setpoint climate; HA maps it to `temperature`
           * regardless of whether the unit is heating or cooling. */
          .withSetpoint('occupied_heating_setpoint', 16, 30, 1)
          .withLocalTemperature()
          .withFanMode(FAN_MODES_EXPOSED)
          .withDescription('LG split driven over IR. One-way: the state shown is what was last ' +
            'commanded, not read back from the unit. local_temperature comes from the AM2320.'));
        list.push(exposes.binary('swing', ea.ALL, 'ON', 'OFF').withDescription('Vertical swing'));
      }
      list.push(exposes.binary('ac_enabled', ea.ALL, 'ON', 'OFF')
        .withDescription('Is an LG split wired to this board\'s IR LED? IR cannot be probed, so this ' +
          'is a stored setting. The board REBOOTS to apply and must then be re-interviewed.'));
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
        // Cache colorCapabilities so lightKind() can tell CCT from RGB. Harmless
        // on a plain dimmer endpoint (no such cluster -> the read just fails).
        await readSafe(id, 'lightingColorCtrl', ['colorCapabilities']);
        // ...and the actual colour state, so the UI picker starts populated. The
        // device never reports, so without this there is no colour state at all.
        await readSafe(id, 'lightingColorCtrl',
                       ['colorMode', 'currentX', 'currentY', 'currentHue', 'currentSaturation', 'colorTemperature']);
      }
      await readSafe(EP.cfg, 'genMultistateOutput', ['presentValue']);
      await readSafe(EP.mask, 'genAnalogOutput', ['presentValue']);
      await readSafe(EP.acCfg, 'genBinaryOutput', ['presentValue']);
      await readSafe(EP.acMode, 'genMultistateOutput', ['presentValue']);
      await readSafe(EP.acTemp, 'genAnalogOutput', ['presentValue']);
      await readSafe(EP.acFan, 'hvacFanCtrl', ['fanMode']);
      await readSafe(EP.acSwing, 'genBinaryOutput', ['presentValue']);
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
      const exposesChanged = typeof a === 'string' ? undefined : a?.data?.deviceExposesChanged;
      if (!device) return;
      if (type === 'stop' || type === 'deviceLeave') {
        const h = globalStore.getValue(device, 'poll');
        if (h) { clearInterval(h); globalStore.clearValue(device, 'poll'); }
        return;
      }
      /* Z2M computes exposes right after the interview, BEFORE configure() runs,
       * so colorCapabilities isn't cached yet and lightKind() falls back to CCT —
       * an RGBW board would render as a colour-temperature light. Read the
       * capability here and ask Z2M to recompute the exposes afterwards.
       *
       * This MUST re-run on every deviceInterview, not just once per session:
       * changing the output profile changes what EP10 is (a CCT light in
       * CCT_2DIM, a colour light in RGBW), and the cached capability from the
       * previous profile is then wrong — a CCT strip rendered as hue/saturation. */
      /* A profile / channel-mask / ac_enabled write REBOOTS the board, and nothing
       * pushes the new values back afterwards (this firmware never reports — see
       * the header). Z2M would therefore keep showing the pre-reboot values until
       * something read them. deviceAnnounce fires when the board rejoins after
       * that reboot, so re-read the config there and the UI self-heals.
       * NOTE the endpoint SET still needs a re-interview — Zigbee only discovers
       * endpoints at interview time — but at least the values stop lying. */
      if (type === 'deviceAnnounce') {
        (async () => {
          for (const [id, cluster, attrs] of [
            [EP.cfg, 'genMultistateOutput', ['presentValue']],
            [EP.mask, 'genAnalogOutput', ['presentValue']],
            [EP.acCfg, 'genBinaryOutput', ['presentValue']],
          ]) {
            const ep = device.getEndpoint(id);
            if (!ep) continue;
            try { await ep.read(cluster, attrs); } catch (err) { /* retried on the next announce */ }
          }
        })();
      }

      if (type === 'deviceInterview' || type === 'deviceJoined' || !globalStore.hasValue(device, 'caps')) {
        globalStore.putValue(device, 'caps', true);
        (async () => {
          let got = false;
          for (const [, id] of LIGHT_EPS) {
            const ep = device.getEndpoint(id);
            if (!ep || !(ep.supportsInputCluster && ep.supportsInputCluster('lightingColorCtrl'))) continue;
            try {
              await ep.read('lightingColorCtrl', ['colorCapabilities']);
              got = true;
            } catch (err) { /* endpoint offline; the next event retries */ }
          }
          if (got && exposesChanged) exposesChanged();
        })();
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
        // Colour state, for the same reason: nothing is reported, so poll it.
        if (tick % COLOUR_TICKS === 0) {
          for (const [, id] of LIGHT_EPS) {
            const ep = device.getEndpoint(id);
            if (!ep || !(ep.supportsInputCluster && ep.supportsInputCluster('lightingColorCtrl'))) continue;
            await read(id, 'lightingColorCtrl',
                       ['colorMode', 'currentX', 'currentY', 'currentHue', 'currentSaturation', 'colorTemperature']);
          }
        }
      }, POLL_TICK_MS);
      globalStore.putValue(device, 'poll', h);
    },
  },
];
