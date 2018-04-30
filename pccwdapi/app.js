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

const mqttClient = mqtt.connect('mqtt://127.0.0.1')

var self=this;

var status={};

serialPort.on('error', function(err) {
  console.log('Error: ', err.message);  
});
	
const parser = serialPort.pipe(new ReadLine({ delimiter: [0x46] }));

self.SendQueue = [];



const SerialWriteQueue = setInterval(() => {  //BECAUSE PCWDD is realy slow...
	if(self.SendQueue.length > 0){		
		serialPort.write([self.SendQueue.shift()]);		
	}
  }, 100);

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

const initPccwd=[
0xaa,0x02,0x8a,0xaa,0x02,0x8b,0xaa,0x02,0x8a,0xaa,0x02,0x8c,0xaa,0x02,0x8a,0xaa,0x02,0x8d,
0xaa,0x02,0x8a,0xaa,0x02,0x8e,0xaa,0x02,0x8a,0xaa,0x02,0x8f,0xaa,0x02,0x8a,0xaa,0x02,0x90,
0xaa,0x02,0x8a,0xaa,0x02,0x91,0xaa,0x02,0x8a,0xaa,0x02,0x92,0xaa,0x02,0xc9,0xaa,0x02,0xc9
];
    
parser.on('data', function(data){		
	if(data.charCodeAt(0)==0x49 && data.charCodeAt(1)==65533) {		
		if(data.charCodeAt(2)==0x01 && data.charCodeAt(3)==0x67){
			console.log('Connected to PCCwd'); 
			clearInterval(pingInterval);
			self.SendQueue=self.SendQueue.concat(initPccwd);	
			console.log('Pccwd Initialized');
		}else{
			var key={
				Id:data.charCodeAt(2),
				Count:data.charCodeAt(3)
			};
			setStatus(key);
			try {
				mqttClient.publish('pccwd/keypress', JSON.stringify(key) )
			}catch(err) {
				 console.log('Error: ', err.message);  
			}
		}
	}else{
		console.log('->'+ data);  		
	}
});

mqttClient.on('message', function (topic, message) {
  // message is Buffer
  console.log(message.toString())
  
  client.end()
})


const pingInterval = setInterval(() => {
	self.SendQueue=self.SendQueue.concat([0x57,0x01,0x64]);
    console.log('.');
}, 800);

var app = express();

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

var routes = require("./routes/routes.js")(app,this,status);

var server = app.listen(3000, function () {
    console.log("Listening on port %s...", server.address().port);
});


