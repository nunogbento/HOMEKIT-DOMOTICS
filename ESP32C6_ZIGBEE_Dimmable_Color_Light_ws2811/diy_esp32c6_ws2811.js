// External converter: DIY ESP32-C6 + WS2811 LED-strip controller, with a Z2M-settable antenna selector.
// Deploy to the Pi: ~/zigbee2mqtt/data/external_converters/diy_esp32c6_ws2811.js  (Z2M auto-loads .js there).
// Endpoints: 11 = WS2811 colour-dimmable light, 12 = antenna on/off (ON = external U.FL, OFF = ceramic).
// NOTE: toggling the antenna reboots the device (it re-inits Zigbee on the new RF path).

const {light, onOff, deviceEndpoints} = require('zigbee-herdsman-converters/lib/modernExtend');

module.exports = [
  {
    zigbeeModel: ['ESP32C6-WS2811'],
    model: 'ESP32C6-WS2811',
    vendor: 'DIY',
    description: 'ESP32-C6 + WS2811 colour light, with antenna selector',
    extend: [
      deviceEndpoints({endpoints: {light: 11, antenna: 12}}),
      light({
        color: {modes: ['xy', 'hs']},
        colorTemp: {range: [153, 500]},
        powerOnBehavior: false,
        effect: false,
        endpointNames: ['light'],
      }),
      onOff({
        endpointNames: ['antenna'],
        powerOnBehavior: false,
        description: 'Antenna: ON = external (U.FL), OFF = internal (ceramic). Toggling reboots the device.',
      }),
    ],
    meta: {multiEndpoint: true},
  },
];
