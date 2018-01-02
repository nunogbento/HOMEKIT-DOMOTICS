var express = require("express");
var bodyParser = require("body-parser");
var SerialPort = require("serialport");
var mqtt = require('mqtt')

var Service, Characteristic;
var serialPort;
const ReadLine=SerialPort.parsers.Readline;

var serialPort = new SerialPort('/dev/ttyUSB0', {
    baudRate: 14400,
    stopBits: 1
});

const client = mqtt.connect('mqtt://127.0.0.1')


var status={};

serialPort.on('error', function(err) {
  console.log('Error: ', err.message);  
});
	
const parser = serialPort.pipe(new ReadLine({ delimiter: [0x46] }));

function setStatus(key){
	switch (key.Id)
	{
		case 75:
			status[174]=!status[174];
			break;
		case 76:
			status[170]=!status[170];
			break;
		case 77:
			status[169]=!status[169];
			break;
		case 78:
			status[171]=!status[171];
			break;
		case 81: 
			status[168]=!status[168];
			break;
		case 82: 
			status[172]=!status[172];
			break;
		case 83: 
			status[173]=!status[173];
			break;
		case 84: 
		case 80: 
		case 79: 
			status[167]=!status[167];	
			break;	   
	}	
}  

parser.on('data', function(data){	
	
	if(data.charCodeAt(0)==0x49 && data.charCodeAt(1)==65533) {
		var key={
			Id:data.charCodeAt(2),
			Count:data.charCodeAt(3)
		};
		setStatus(key);
		try {
			client.publish('pccwd/keypress', JSON.stringify(key) )
		}catch(err) {
			 console.log('Error: ', err.message);  
		}
	}else{
		console.log('->'+ data);  		
	}
});


var app = express();

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

var routes = require("./routes/routes.js")(app,serialPort,status);


var server = app.listen(3000, function () {
    console.log("Listening on port %s...", server.address().port);
});


