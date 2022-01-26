#ifndef LEDCONTROLLER_H_
#define LEDCONTROLLER_H_

#include "configuration.h"


enum LedType {
  ONOFFLED = 0,
  DIMMABLELED,
  RGBLED,
  RGBWLED
};

class LedController {
    LedType type;
    u_int W_pin;
    u_int R_pin;
    u_int G_pin;
    u_int B_pin;

    bool is_on = false;
    u_int Brightness = 0;
    u_int Hue = 0;
    u_int Saturation = 0;

    u_int rgb_colors[4] = {0, 0, 0, 0};
  public:

    LedController(u_int w_pin, bool dimmable) {
      type = (dimmable) ? DIMMABLELED : ONOFFLED;
      W_pin = w_pin;
      pinMode(W_pin, OUTPUT);
      Update();
    }

    LedController(u_int r_pin, u_int g_pin, u_int b_pin) {
      type = RGBLED;
      R_pin = r_pin;
      G_pin = g_pin;
      B_pin = b_pin;

      pinMode(R_pin, OUTPUT);
      pinMode(G_pin, OUTPUT);
      pinMode(B_pin, OUTPUT);
    }

    LedController(u_int r_pin, u_int g_pin, u_int b_pin, u_int w_pin) {
      type = RGBWLED;
      R_pin = r_pin;
      G_pin = g_pin;
      B_pin = b_pin;
      W_pin = w_pin;
      pinMode(W_pin, OUTPUT);
      pinMode(R_pin, OUTPUT);
      pinMode(G_pin, OUTPUT);
      pinMode(B_pin, OUTPUT);
    }

    void TurnOn() {
      is_on = true;
      if (Brightness == 0 && type > ONOFFLED )
        Brightness = 100;
      Update();
    }

    void TurnOff() {
      is_on = false;
      Update();
    }

    void SetBrightness(u_int brightness) {
      Brightness = brightness;
      Update();
    }
    void SetHue(u_int hue) {
      Hue = hue;
      Update();
    }
    void SetSaturation(u_int saturation) {
      Saturation = saturation;
      Update();
    }

  private:
    static void hsi2rgb(float H, float S, float I, u_int* rgb);
    static void hsi2rgbw(float H, float S, float I, u_int* rgb);
    void Update();
};
#endif
