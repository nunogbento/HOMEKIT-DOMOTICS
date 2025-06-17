#ifndef LIGHTBULBSERVICE_H_
#define LIGHTBULBSERVICE_H_

struct LightBulbService : Service::LightBulb {
  u_int _pin;
  SpanCharacteristic *On;

  LightBulbService(u_int pin)
    : Service::LightBulb() {

    On = new Characteristic::On();
    _pin = pin;
    pinMode(this->_pin, OUTPUT);
  }

  boolean update() {
    digitalWrite(_pin, On->getNewVal());
    return (true);
  }
};

class DimmableLightBulbService : Service::LightBulb {
  LedPin *_pin;  // NEW! Create reference to LED Pin instantiated below


  SpanCharacteristic *On;
  SpanCharacteristic *Brightness;

  DimmableLightBulbService(u_int pin)
    : Service::LightBulb() {

    On = new Characteristic::On();

    Brightness = new Characteristic::Brightness(50);  // NEW! Instantiate the Brightness Characteristic with an initial value of 50% (same as we did in Example 4)
    Brightness->setRange(5, 100, 1);                  // NEW! This sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1% (different from Example 4 values)

    _pin = new LedPin(pin);
  }

  boolean update() {
    _pin->set(On->getNewVal() * Brightness->getNewVal());
    return (true);
  }
};


struct CCTLightBulbService : Service::LightBulb {
  LedPin *_cPin;
  LedPin *_wPin;  // NEW! Create reference to LED Pin instantiated below


  SpanCharacteristic *On;
  SpanCharacteristic *Brightness;
  SpanCharacteristic *ColorTemperature;

  CCTLightBulbService(u_int cPin, u_int wPin)
    : Service::LightBulb() {

    On = new Characteristic::On();

    Brightness = new Characteristic::Brightness(50);  // NEW! Instantiate the Brightness Characteristic with an initial value of 50% (same as we did in Example 4)
    Brightness->setRange(5, 100, 1);                  // NEW! This sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1% (different from Example 4 values)
    ColorTemperature = new Characteristic::ColorTemperature(400, true);
    ColorTemperature->setRange(140, 500);
    _cPin = new LedPin(cPin);
    _wPin = new LedPin(wPin);
  }

  boolean update() {
    if (!On->getNewVal()) {
      _wPin->fade(0, 200);
      _cPin->fade(0, 200);
      return true;
    }

    float level = Brightness->getNewVal() / 100.0;

    // Map mireds (140–500) to blend factor (0 = cool only, 1 = warm only)
    float mireds = ColorTemperature->getNewVal();
    float blend = (mireds - 140.0) / (500.0 - 140.0);
    blend = constrain(blend, 0.0, 1.0);

    // Warm = blend * brightness; Cool = (1 - blend) * brightness
    float wPWM = blend * level;
    float cPWM = (1.0 - blend) * level;

    _wPin->fade(wPWM, 200);
    _cPin->fade(cPWM, 200);
    return (true);
  }
};


class RGBLightBulbService : Service::LightBulb {
  LedPin *_rPin;
  LedPin *_gPin;
  LedPin *_bPin;  // NEW! Create reference to LED Pin instantiated below
  LedPin *_wPin;

  bool isRGBW = false;
  u_int rgb_colors[4] = { 0, 0, 0, 0 };

  SpanCharacteristic *On;
  SpanCharacteristic *Brightness;
  SpanCharacteristic *Hue;
  SpanCharacteristic *Saturation;

  RGBLightBulbService(u_int rPin, u_int gPin, u_int bPin)
    : Service::LightBulb() {

    On = new Characteristic::On();
    Brightness = new Characteristic::Brightness(50);  // NEW! Instantiate the Brightness Characteristic with an initial value of 50% (same as we did in Example 4)
    Brightness->setRange(5, 100, 1);                  // NEW! This sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1% (different from Example 4 values)
    Hue = new Characteristic::Hue(0);                 // instantiate the Hue Characteristic with an initial value of 0 out of 360
    Saturation = new Characteristic::Saturation(0);   // instantiate the Saturation Characteristic with an initial value of 0%

    _rPin = new LedPin(rPin);
    _gPin = new LedPin(gPin);
    _bPin = new LedPin(bPin);
  }

  RGBLightBulbService(u_int rPin, u_int gPin, u_int bPin, u_int wPin)
    :RGBLightBulbService(rPin, gPin, bPin) {
    isRGBW = true;
    _wPin = new LedPin(wPin);
  }

  boolean update() {
    if (!On->getNewVal()) {
      _rPin->fade(0, 200);
      _gPin->fade(0, 200);
      _bPin->fade(0, 200);
      if (isRGBW)
        _wPin->fade(0, 200);
      return true;
    }

    float h = Hue->getNewVal();
    float s = Saturation->getNewVal();
    float v = Brightness->getNewVal();

    if (isRGBW)
      hsi2rgbw(h, s, v, rgb_colors);
    else
      hsi2rgb(h, s, v, rgb_colors);

    _rPin->fade(rgb_colors[0], 200);
    _gPin->fade(rgb_colors[1], 200);
    _bPin->fade(rgb_colors[2], 200);
    if (isRGBW)
      _wPin->fade(rgb_colors[3], 200);

    return (true);
  }
private:

  static void hsi2rgb(float H, float S, float I, u_int *rgb) {
    int r, g, b;
    H = fmod(H, 360);                 // cycle H around to 0-360 degrees
    H = 3.14159 * H / (float)180;     // Convert to radians.
    S = S > 0 ? (S < 1 ? S : 1) : 0;  // clamp S and I to interval [0,1]
    I = I > 0 ? (I < 1 ? I : 1) : 0;

    // Math! Thanks in part to Kyle Miller.
    if (H < 2.09439) {
      r = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
      g = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
      b = 255 * I / 3 * (1 - S);
    } else if (H < 4.188787) {
      H = H - 2.09439;
      g = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
      b = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
      r = 255 * I / 3 * (1 - S);
    } else {
      H = H - 4.188787;
      b = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
      r = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
      g = 255 * I / 3 * (1 - S);
    }
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
  }


  static void hsi2rgbw(float H, float S, float I, u_int *rgbw) {
    int r, g, b, w;
    float cos_h, cos_1047_h;
    H = fmod(H, 360);              // cycle H around to 0-360 degrees
    H = 3.14159 * H / (float)180;  // Convert to radians.
    S = S / 100;
    S = S > 0 ? (S < 1 ? S : 1) : 0;  // clamp S and I to interval [0,1]
    I = I / 100;
    I = I > 0 ? (I < 1 ? I : 1) : 0;

    if (H < 2.09439) {
      cos_h = cos(H);
      cos_1047_h = cos(1.047196667 - H);
      r = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
      g = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
      b = 0;
      w = 255 * (1 - S) * I;
    } else if (H < 4.188787) {
      H = H - 2.09439;
      cos_h = cos(H);
      cos_1047_h = cos(1.047196667 - H);
      g = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
      b = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
      r = 0;
      w = 255 * (1 - S) * I;
    } else {
      H = H - 4.188787;
      cos_h = cos(H);
      cos_1047_h = cos(1.047196667 - H);
      b = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
      r = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
      g = 0;
      w = 255 * (1 - S) * I;
    }
    rgbw[0] = r;
    rgbw[1] = g;
    rgbw[2] = b;
    rgbw[3] = w;
  }
};

struct LightBulbAccessoryInformation : Service::AccessoryInformation {

  LedPin *_pin;

  LightBulbAccessoryInformation(int pin)
    : Service::AccessoryInformation() {
    new Characteristic::Identify();
    char c[64];
    sprintf(c, "LightBulb-%d", pin);
    new Characteristic::Name(c);
    _pin = new LedPin(pin);
  }

  boolean update() {

    for (int i = 0; i < 3; i++) {
      _pin->fade(100, 300);
      delay(200);
      _pin->fade(0, 300);
      delay(200);
    }
    return (true);
  }
};


#endif
