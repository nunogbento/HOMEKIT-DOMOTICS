// create an empty modbus client
var ModbusRTU = require("modbus-serial");
var client = new ModbusRTU();
 
// open connection to a serial port
client.connectRTUBuffered("/dev/ttyUSB1", { baudRate: 9600 });
client.setID(0x01);
 
// read the values of 10 registers starting at address 0
// on device number 1. and log the values to the console.
 setInterval(function() {
    client.readInputRegisters (0x00, 0x0A, function(err, data) {
		
        console.log(data.data);
    });
}, 1000); 