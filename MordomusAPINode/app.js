var express = require("express");
var bodyParser = require("body-parser");
var SerialPort = require("serialport");

var Service, Characteristic;
var serialPort;

var serialPort = new SerialPort('/dev/ttyUSB0', {
    baudrate: 14400,
    stopBits: 1
});

serialPort.on('error', function(err) {
  console.log('Error: ', err.message);
})

var app = express();

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

var routes = require("./routes/routes.js")(app,serialPort);


var server = app.listen(3000, function () {
    console.log("Listening on port %s...", server.address().port);
});
