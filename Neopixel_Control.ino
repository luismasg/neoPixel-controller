#include <bluefruit.h>
#include <Adafruit_NeoPixel.h>
#define LED_PIN    7
#define LED_COUNT 16
#define BRIGHTNESS 128  // 128 is half   -> max 255  ->  60mA MAX per pixel at full 'white'

BLEDfu bledfu;
BLEUart bleuart;
uint8_t readPacket (BLEUart *ble_uart, uint16_t timeout);
float   parsefloat (uint8_t *buffer);
void    printHex   (const uint8_t * data, const uint32_t numBytes);
extern uint8_t packetbuffer[];


///--------------- Adafruit Class

// Pattern types supported:
enum  pattern { NONE, RAINBOW_CYCLE, THEATER_CHASE, COLOR_WIPE, SCANNER, FADE };
// Patern directions supported:
enum  direction { FORWARD, REVERSE };


// NeoPattern Class - derived from the Adafruit_NeoPixel class
class NeoPatterns : public Adafruit_NeoPixel
{
  public:

    // Member Variables:
    pattern  ActivePattern;  // which pattern is running
    direction Direction;     // direction to run the pattern

    unsigned long Interval;   // milliseconds between updates
    unsigned long lastUpdate; // last update of position

    uint32_t Color1, Color2;  // What colors are in use
    bool isUpdatingColor1 = true;

    uint16_t TotalSteps;  // total number of steps in the pattern
    uint16_t Index;  // current step within the pattern

    void (*OnComplete)();  // Callback on completion of pattern

    // Constructor - calls base-class constructor to initialize strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
      : Adafruit_NeoPixel(pixels, pin, type)
    {
      OnComplete = callback;
    }

    // Update the pattern
    void Update()
    {
      if ((millis() - lastUpdate) > Interval) // time to update
      {
        lastUpdate = millis();
        switch (ActivePattern)
        {
          case RAINBOW_CYCLE:
            RainbowCycleUpdate();
            break;
          case THEATER_CHASE:
            TheaterChaseUpdate();
            break;
          case COLOR_WIPE:
            ColorWipeUpdate();
            break;
          case SCANNER:
            ScannerUpdate();
            break;
          case FADE:
            FadeUpdate();
            break;
          default:
            break;
        }
      }
    }

    // Increment the Index and reset at the end
    void Increment()
    {
      if (Direction == FORWARD)
      {
        Index++;
        if (Index >= TotalSteps)
        {
          Index = 0;
          if (OnComplete != NULL)
          {
            OnComplete(); // call the comlpetion callback
          }
        }
      }
      else // Direction == REVERSE
      {
        --Index;
        if (Index <= 0)
        {
          Index = TotalSteps - 1;
          if (OnComplete != NULL)
          {
            OnComplete(); // call the completion callback
          }
        }
      }
    }

    // Reverse pattern direction
    void Reverse()
    {
      if (Direction == FORWARD)
      {
        Direction = REVERSE;
        Index = TotalSteps - 1;
      }
      else
      {
        Direction = FORWARD;
        Index = 0;
      }
    }

    // Initialize for a RainbowCycle
    void RainbowCycle(uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = RAINBOW_CYCLE;
      Interval = interval;
      TotalSteps = 255;
      Index = 0;
      Direction = dir;
    }

    // Update the Rainbow Cycle Pattern
    void RainbowCycleUpdate()
    {
      for (int i = 0; i < numPixels(); i++)
      {
        setPixelColor(i, Wheel(((i * 256 / numPixels()) + Index) & 255));
      }
      show();
      Increment();
    }

    // Initialize for a Theater Chase
    void TheaterChase(uint32_t color1, uint32_t color2, uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = THEATER_CHASE;
      Interval = interval;
      TotalSteps = numPixels();
      Color1 = color1;
      Color2 = color2;
      Index = 0;
      Direction = dir;
    }

    // Update the Theater Chase Pattern
    void TheaterChaseUpdate()
    {
      for (int i = 0; i < numPixels(); i++)
      {
        if ((i + Index) % 3 == 0)
        {
          setPixelColor(i, Color1);
        }
        else
        {
          setPixelColor(i, Color2);
        }
      }
      show();
      Increment();
    }

    // Initialize for a ColorWipe
    void ColorWipe(uint32_t color, uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = COLOR_WIPE;
      Interval = interval;
      TotalSteps = numPixels();
      Color1 = color;
      Index = 0;
      Direction = dir;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
      setPixelColor(Index, Color1);
      show();
      Increment();
    }

    // Initialize for a SCANNNER
    void Scanner(uint32_t color1, uint8_t interval)
    {
      ActivePattern = SCANNER;
      Interval = interval;
      TotalSteps = (numPixels() - 1) * 2;
      Color1 = color1;
      Index = 0;
    }

    // Update the Scanner Pattern
    void ScannerUpdate()
    {
      for (int i = 0; i < numPixels(); i++)
      {
        if (i == Index)  // Scan Pixel to the right
        {
          setPixelColor(i, Color1);
        }
        else if (i == TotalSteps - Index) // Scan Pixel to the left
        {
          setPixelColor(i, Color1);
        }
        else // Fading tail
        {
          setPixelColor(i, DimColor(getPixelColor(i)));
        }
      }
      show();
      Increment();
    }

    // Initialize for a Fade
    void Fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = FADE;
      Interval = interval;
      TotalSteps = steps;
      Color1 = color1;
      Color2 = color2;
      Index = 0;
      Direction = dir;
    }

    // Update the Fade Pattern
    void FadeUpdate()
    {
      // Calculate linear interpolation between Color1 and Color2
      // Optimise order of operations to minimize truncation error
      uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
      uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
      uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;

      ColorSet(Color(red, green, blue));
      show();
      Increment();
    }

    // Calculate 50% dimmed version of a color (used by ScannerUpdate)
    uint32_t DimColor(uint32_t color)
    {
      // Shift R, G and B components one bit to the right
      uint32_t dimColor = Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
      return dimColor;
    }

    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
      for (int i = 0; i < numPixels(); i++)
      {
        setPixelColor(i, color);
      }
      show();
    }

    // Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
      return (color >> 16) & 0xFF;
    }

    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
      return (color >> 8) & 0xFF;
    }

    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
      return color & 0xFF;
    }

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
      WheelPos = 255 - WheelPos;
      if (WheelPos < 85)
      {
        return Color(255 - WheelPos * 3, 0, WheelPos * 3);
      }
      else if (WheelPos < 170)
      {
        WheelPos -= 85;
        return Color(0, WheelPos * 3, 255 - WheelPos * 3);
      }
      else
      {
        WheelPos -= 170;
        return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
      }
    }
};

void Ring1Complete();

// Define some NeoPatterns for the two rings and the stick
//  as well as some completion routines


//NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
NeoPatterns Ring1(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800, &Ring1Complete);



////
void setup(void)
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println(F("Foro RGB Controller"));
  Serial.println(F("-------------------------------------------"));

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("ForoRGB");
  bledfu.begin();
  bleuart.begin();
  startAdv();

  Serial.println(F("Please use Foro Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then use color picker, game controller, etc!"));
  Serial.println(F("Neopixels up and running"));
  Serial.println();


  // Initialize all the pixelStrips
  Ring1.begin();
  Ring1.setBrightness(BRIGHTNESS);   // modify for production to 255
  // Ring1.Color2 = Ring1.Color(0, 0, 255);
  //  Ring1.ColorWipe(Ring1.Color(255, 255, 255), 100);
  Ring1.ColorWipe( Ring1.Color(255, 255, 255), 150);

}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}


void loop(void)
{
  Ring1.Update();
  uint8_t len = readPacket(&bleuart, 1);
//  Serial.println("--Debug Status---");
//  Serial.println("-------------");
//  Serial.print("interval: ");
//  Serial.println(Ring1.Interval);
//  Serial.print("TotalSteps: ");
//  Serial.println(Ring1.TotalSteps);
//  Serial.print("Index: ");
//  Serial.println(Ring1.Index);
//  Serial.print("ActivePattern: ");
//  Serial.println(Ring1.ActivePattern);
//  Serial.println();
  
  if (len != 0) {
    if (packetbuffer[1] == 'C') {
      setColor();
    } else if (packetbuffer[1] == 'B' ) {

      uint8_t buttnum = packetbuffer[2] - '0';
      boolean pressed = packetbuffer[3] - '0';
      //    Serial.print ("Button "); Serial.print(buttnum);
      //    if (pressed) {
      //      Serial.println(" pressed");
      //    } else {
      //      Serial.println(" released");
      //    }

      if (buttnum == 1 && !pressed) {
        Serial.println("COLOR_WIPE llamado");
        Ring1.Interval = 50;
        Ring1.ActivePattern = COLOR_WIPE;
        Ring1.TotalSteps = Ring1.numPixels();
      } else if (buttnum == 2 && !pressed) {
        Serial.println("ok RAINBOW_CYCLE llamado");
        Ring1.ActivePattern = RAINBOW_CYCLE;
        Ring1.TotalSteps = 255;
        Ring1.Interval = 5;
      } else if (buttnum == 3 && !pressed) {
        Serial.println("ok THEATER_CHASE llamado");
        Ring1.Interval = 100;
        Ring1.ActivePattern = THEATER_CHASE;
        Ring1.TotalSteps = Ring1.numPixels();
        Ring1.Index = 0;
      } else if (buttnum == 4 && !pressed) {
        Serial.println("ok SCANNER");
        Ring1.Interval = 100;
        Ring1.TotalSteps = (Ring1.numPixels() - 1) * 2;
        Ring1.Index = 0;
        Ring1.ActivePattern = SCANNER;
      } else if (buttnum == 5 && !pressed) {
        //up
        if (Ring1.ActivePattern ==  RAINBOW_CYCLE) {
          increaseSpeedbyOne();
        } else {
          increaseSpeedbyTen();
        }
      } else if (buttnum == 6 && !pressed) {
        //down
        if (Ring1.ActivePattern ==  RAINBOW_CYCLE) {
          decreaseSpeedByOne();
        } else {
          decreaseSpeedByTen();
        }
      } else if (buttnum == 7 && !pressed) {
        Serial.println("boton Left");
        Ring1.Interval = 50;
        Ring1.TotalSteps = 100;
        Ring1.Index = 0;
        Ring1.Color1 = Ring1.Color(255, 255, 255);
        Ring1.Color2 = Ring1.Color(0, 0, 0);
        Ring1.ActivePattern = FADE;
      } else if (buttnum == 8 && !pressed) {
        Ring1.Reverse();
      }
    }
  }
}



//------------------------------------------------------------
//Completion Routines - get called on completion of a pattern
//------------------------------------------------------------

// Ring1 Completion Callback
void Ring1Complete()
{
  if (Ring1.ActivePattern == FADE ) {
   // Serial.println("reversing");
    Ring1.Reverse();
  }

}


void setColor () {
  uint8_t red = packetbuffer[2];
  uint8_t green = packetbuffer[3];
  uint8_t blue = packetbuffer[4];
  if (Ring1.ActivePattern == COLOR_WIPE) {
    Ring1.Color1 = Ring1.Color(red, green, blue);
  } else if (Ring1.isUpdatingColor1 == true) {
    Ring1.Color1 = Ring1.Color(red, green, blue);
    Ring1.isUpdatingColor1 = false;
  } else {
    Ring1.Color2 = Ring1.Color(red, green, blue);
    Ring1.isUpdatingColor1 = true;
  }

}


void increaseSpeedbyTen() {
  if (Ring1.Interval - 10 > 0 &&  Ring1.Interval - 10  < 4294967286) {
    Ring1.Interval -= 10;
//    Serial.println("interval decreased");
//    Serial.print("new interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  } else {
    Ring1.Interval = 0;
//    Serial.print("old interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  }

}


void decreaseSpeedByTen () {

//  Serial.println("boton up");
  if (Ring1.Interval + 10 < 200) {
    Ring1.Interval += 10;
//    Serial.println("interval increased");
//    Serial.print("new interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  } else {
//    Serial.println("keep old interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  }



}


void increaseSpeedbyOne() {
  if (Ring1.Interval - 1 > 0 &&  Ring1.Interval - 1  < 4294967295) {
    Ring1.Interval --;
//    Serial.println("interval decreased");
//    Serial.print("new interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  } else {
    Ring1.Interval = 0;
//    Serial.print("old interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  }

}

void decreaseSpeedByOne () {

  Serial.println("boton up");
  if (Ring1.Interval + 1 < 30) {
    Ring1.Interval ++;
//    Serial.println("interval increased");
//    Serial.print("new interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  } else {
//    Serial.println("keep old interval: ");
//    Serial.print(Ring1.Interval);
//    Serial.print("ms");
//    Serial.println();
  }



}
