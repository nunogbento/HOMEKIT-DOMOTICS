    1  passwd
    2  sudo nano /etc/hostname
    3  sudo nano /etc/hosts
    4  sudo shutdown -r now
    6  hostname
    7  sudo apt-get update
    8  sudo apt-get upgrade
    9  sudo apt-get install git make
  SETUP NODEJS from Bynaries FROM
  curl -sL https://deb.nodesource.com/setup_9.x | sudo -E bash -
  sudo apt install -y nodejs
  //node-red
  sudo npm install -g --unsafe-perm node-red
  sudo wget https://raw.githubusercontent.com/node-red/raspbian-deb-package/master/resources/nodered.service -O /lib/systemd/system/nodered.service
sudo wget https://raw.githubusercontent.com/node-red/raspbian-deb-package/master/resources/node-red-start -O /usr/bin/node-red-start
sudo wget https://raw.githubusercontent.com/node-red/raspbian-deb-package/master/resources/node-red-stop -O /usr/bin/node-red-stop
sudo chmod +x /usr/bin/node-red-st*
sudo systemctl daemon-reload
sudo systemctl enable nodered.service
sudo systemctl start nodered.service

   20  sudo apt-get install libavahi-compat-libdnssd-dev
       142  sudo apt-get install ffmpeg
	   
   21  cd /usr/local
   22  cd lib
   23  sudo npm install -g --unsafe-perm homebridge
   24  cd /home/pi
   
   
   27  sudo mv homebridge /etc/default
   28  sudo nano /etc/default/homebridge 
   30  sudo mv homebridge /etc/default
   32  sudo mv homebridge.service /etc/systemd/system
   34  sudo useradd --system homebridge
   35  sudo mkdir /var/homebridge
   36  sudo cp config.json /var/homebridge/
   37  sudo chmod -R 0777 /var/homebridge
   38  sudo systemctl daemon-reload
   39  sudo systemctl enable homebridge
   40  sudo systemctl start homebridge
   43  sudo systemctl status homebridge
   52  sudo npm install -g homebridge-hue
   53  sudo npm install -g homebridge-magichome
   54  sudo npm install -g homebridge-better-http-rgb
   55  sudo npm install -g homebridge-ip-camera   
   56  cd /var/homebridge
   58  sudo nano /etc/systemd/system/homebridge.service
   62  sudo systemctl daemon-reload
   63  sudo systemctl start homebridge
   64  sudo systemctl status homebridge
   84  sudo useradd --system pccwdapi
   86  sudo npm install serialport --unsafe-perm --build-from-source
   88  sudo npm install -g express
   89  sudo npm install -g body-parser
   134  sudo mv pccwdapi.service /etc/systemd/system
   139  sudo usermod -a -G dialout pccwdapi 
   135  sudo systemctl daemon-reload
  136  sudo systemctl start pccwdapi
  137  sudo systemctl status pccwdapi


