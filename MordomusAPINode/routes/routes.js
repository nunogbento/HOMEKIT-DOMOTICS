var appRouter = function(app,serialPort) {
  var self=this;
  self.serialPort=serialPort;
  self.status={};
  
  app.get("/turnon", function(req, res) {
    if(!req.query.address) {
        return res.send({"status": "error", "message": "missing address"});
    }
    var addr = parseInt(req.query.address);

    if (self.serialPort.isOpen()) {
        self.serialPort.write([0xAA]);
        self.serialPort.write([addr]);
        self.serialPort.write([0x64]);
		self.status[req.query.address]=true;
    } else
        console.log('Can not send command serial port is closed');
    res.sendStatus(200);
  });

  app.get("/turnoff", function(req, res) {
    if(!req.query.address) {
        return res.send({"status": "error", "message": "missing address"});
    }
    var addr = parseInt(req.query.address);
    if (self.serialPort.isOpen()) {
        self.serialPort.write([0xAA]);
        self.serialPort.write([addr]);
        self.serialPort.write([0x01]);
		self.status[req.query.address]=false;
    } else
        console.log('Can not send command serial port is closed');
    res.sendStatus(200);
    });
	
  app.get("/status", function(req, res) {
    if(!req.query.address) {
        return res.send({"status": "error", "message": "missing address"});
    }
    res.send((self.status.hasOwnProperty(req.query.address) && self.status[req.query.address])? "1":"0");
    });

}

module.exports = appRouter;
