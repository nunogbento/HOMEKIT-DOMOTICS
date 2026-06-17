// Zigbee2MQTT external converter for the DIY ESP32-C6 in-wall input module.
// Copy to: /home/pi/zigbee2mqtt/data/external_converters/diy_esp32c6_2ch_input.js
//
// Exposes two contact states (contact_1 / contact_2) from the two IAS zone
// endpoints (10 and 11). contact = false means the wall switch is closed
// (shorted to GND) — trigger your "toggle the lights" automation on ANY
// state change so both rocker positions work.

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;

const fzLocal = {
    contact_multi: {
        cluster: 'ssIasZone',
        type: ['commandStatusChangeNotification', 'attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const zoneStatus = msg.data.zonestatus !== undefined ? msg.data.zonestatus : msg.data.zoneStatus;
            if (zoneStatus === undefined) return;
            const suffix = msg.endpoint.ID === 10 ? '1' : '2';
            // IAS alarm1 bit set = zone open (same convention as stock contact sensors)
            return {[`contact_${suffix}`]: !((zoneStatus & 1) > 0)};
        },
    },
};

const definition = {
    zigbeeModel: ['ESP32C6-2CH-INPUT'],
    model: 'ESP32C6-2CH-INPUT',
    vendor: 'DIY',
    description: 'ESP32-C6 mains-powered in-wall dual switch input module (Hue Wall Switch Module alternative, Zigbee router)',
    fromZigbee: [fzLocal.contact_multi],
    toZigbee: [],
    exposes: [e.contact().withEndpoint('1'), e.contact().withEndpoint('2')],
    meta: {multiEndpoint: true},
    endpoint: (device) => {
        return {'1': 10, '2': 11};
    },
};

module.exports = definition;
