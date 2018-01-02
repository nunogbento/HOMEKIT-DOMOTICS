var appRouter = function(app,serialPort,status) {
  var self=this;
  self.serialPort=serialPort;
  self.status=status;
  
  app.get("/turnon", function(req, res) {
    if(!req.query.address) {
        return res.send({"status": "error", "message": "missing address"});
    }
    var addr = parseInt(req.query.address);

    
        self.serialPort.write([0xAA]);
        self.serialPort.write([addr]);
        self.serialPort.write([0x64]);
		self.status[req.query.address]=true;
    
        
    res.sendStatus(200);
  });

  app.get("/turnoff", function(req, res) {
    if(!req.query.address) {
        return res.send({"status": "error", "message": "missing address"});
    }
    var addr = parseInt(req.query.address);
    
        self.serialPort.write([0xAA]);
        self.serialPort.write([addr]);
        self.serialPort.write([0x01]);
		self.status[req.query.address]=false;
    
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
