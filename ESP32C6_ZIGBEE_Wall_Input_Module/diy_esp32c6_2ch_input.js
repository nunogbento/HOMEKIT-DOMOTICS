// External converter: DIY ESP32-C6 2-channel in-wall SCENE / BUTTON module.
// Deploy to: ~/zigbee2mqtt/data/external_converters/diy_esp32c6_2ch_input.js  (Z2M auto-loads .js there).
//
// Each wall switch is reported as button ACTIONS (single / double / long),
// the same as a Hue dimmer or a Modomus scene switch — so HA maps each channel
// to a Stateless Programmable Switch (Single / Double / Long press) instead of
// a door/contact sensor.
//
// Wire protocol: each channel is a Multistate Input endpoint
//   EP 10 -> button_1, EP 11 -> button_2
// On a gesture the firmware writes genMultistateInput.presentValue then resets
// it to 0:  1 = single, 2 = double, 3 = long.

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;

const GESTURE = {1: 'single', 2: 'double', 3: 'hold'};

// The firmware delivers each report to every coordinator binding, and a
// double-configure can leave two of them, so the same action can arrive twice
// a few ms apart. The gesture timing (single needs a 300ms window, double/long
// are single events) means an IDENTICAL action can never legitimately repeat
// within ~250ms — so we drop repeats inside that window. Keyed per device+action.
const lastAction = new Map();
const DEDUP_MS = 250;

const fzAction = {
    cluster: 'genMultistateInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const v = msg.data.presentValue;
        if (v === undefined || v === 0) return;          // 0 = idle reset, ignore
        const g = GESTURE[v];
        if (!g) return;
        const btn = msg.endpoint.ID === 10 ? 'button_1' : 'button_2';
        const action = `${btn}_${g}`;
        const key = `${meta.device.ieeeAddr}:${action}`;
        const now = Date.now();
        const prev = lastAction.get(key);
        lastAction.set(key, now);
        if (prev !== undefined && (now - prev) < DEDUP_MS) return; // duplicate report
        return {action};
    },
};

const ACTIONS = [
    'button_1_single', 'button_1_double', 'button_1_hold',
    'button_2_single', 'button_2_double', 'button_2_hold',
];

module.exports = [
  {
    zigbeeModel: ['ESP32C6-2CH-INPUT'],
    model: 'ESP32C6-2CH-INPUT',
    vendor: 'DIY',
    description: 'ESP32-C6 mains-powered 2-channel in-wall scene switch (Hue/Modomus-style, single/double/long, Zigbee router)',
    fromZigbee: [fzAction],
    toZigbee: [],
    ota: true,   // firmware carries a Zigbee OTA client (EP10); images served via the OTA override index
    exposes: [e.action(ACTIONS)],
    // The firmware sends manual attribute reports, which are delivered to the
    // cluster's BIND destination — so we must bind genMultistateInput on each
    // endpoint to the coordinator, or the button presses never arrive.
    configure: async (device, coordinatorEndpoint) => {
      for (const epId of [10, 11]) {
        const ep = device.getEndpoint(epId);
        if (!ep) continue;
        await ep.bind('genMultistateInput', coordinatorEndpoint);
        try {
          await ep.configureReporting('genMultistateInput', [{
            attribute: 'presentValue',
            minimumReportInterval: 0,
            maximumReportInterval: 0,   // manual reports only; no periodic
            reportableChange: 0,
          }]);
        } catch (e) { /* device reports manually; binding alone is enough */ }
      }
    },
  },
];
