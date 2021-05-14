#include "configuration.h"
#include <Arduino.h>
#include "LedController.h"


void LedController::Update() {
  if (type == RGBLED || type == RGBWLED)
    if (!is_on) { //lamp - switch to off
      digitalWrite(R_pin, 0);
      digitalWrite(G_pin, 0);
      digitalWrite(B_pin, 0);
      if (type == RGBWLED )
        digitalWrite(W_pin, 0);
    } else {
      if  (type == RGBLED)
        hsi2rgb(Hue, Saturation, Brightness, rgb_colors);
      else
        hsi2rgbw(Hue, Saturation, Brightness, rgb_colors);
        
      LOG_D("Updating type=%d, Hue:%d, Saturation:%d, Brightness:%d --> r:%d,g:%d,b:%d,w:%d",type,Hue,Saturation,Brightness,rgb_colors[0],rgb_colors[1],rgb_colors[2],rgb_colors[3]);
      analogWrite(R_pin, rgb_colors[0]);
      analogWrite(G_pin, rgb_colors[1]);
      analogWrite(B_pin, rgb_colors[2]);
      if (type == RGBWLED )
        analogWrite(W_pin, rgb_colors[3]);
    } else {
     // LOG_D("LED switch is_on:%d pi:%d ",is_on,W_pin);
    if (!is_on) //lamp - switch to off
      digitalWrite(W_pin, 0);
    else {
      if (type == DIMMABLELED)
        analogWrite(W_pin, map(Brightness, 0, 100, 0, 255));
      else
        digitalWrite(W_pin, 1);
    }
  }
}

void LedController::hsi2rgb(float H, float S, float I, u_int* rgb) {
  int r, g, b;
  H = fmod(H, 360); // cycle H around to 0-360 degrees
  H = 3.14159 * H / (float)180; // Convert to radians.
  S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
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


void LedController::hsi2rgbw(float H, float S, float I, u_int* rgbw) {
  int r, g, b, w;
  float cos_h, cos_1047_h;
  H = fmod(H, 360); // cycle H around to 0-360 degrees
  H = 3.14159 * H / (float)180; // Convert to radians.
  S = S / 100;
  S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
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
