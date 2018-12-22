#include <SPI.h>
#include <Wire.h> // this is needed even tho we aren't using it
#include <Adafruit_NeoPixel.h>
#define PIN            12
#define NUMPIXELS     5
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
int circleSize = 0;
bool ping = 0;
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <Adafruit_ILI9341.h> // Hardware-specific library
#include <Adafruit_STMPE610.h>
#include <SD.h>
int i=0;
#if defined (__AVR_ATmega32U4__) || defined(ARDUINO_SAMD_FEATHER_M0) || defined (__AVR_ATmega328P__) 
   #define STMPE_CS 6
   #define TFT_CS   9
   #define TFT_DC   10
   #define SD_CS    5
#endif

int largeCircle = 0;
// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750

#define BUFFPIXEL 20
////#include <SPI.h>
//#include <Wire.h>      // this is needed even tho we aren't using it
bool touched = 0;


Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);


// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750
float timerOne = 0;
// Size of the color selection boxes and the paintbrush size
//#define BOXSIZE 40
#define PENRADIUS 3
int oldcolor, currentcolor;
#define BUFFPIXEL 20
//function to display the choosen photo
 void bmpDraw(char *filename, int16_t x, int16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);

        for (row=0; row<h; row++) { // For each scanline...

          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            tft.endWrite();
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              tft.endWrite();
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              tft.startWrite();
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
          tft.endWrite();
        } // end scanline
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
  delay(5000);
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
void reset(void){
  tft.setRotation(0);
   tft.fillScreen(ILI9341_WHITE);
  // make the color selection boxes
  tft.fillCircle(215, 245,20, ILI9341_RED);
  
  tft.fillCircle(215, 145, 20, ILI9341_BLUE);
  tft.drawCircle(215, 45, 20, ILI9341_BLACK);
  
 
  // select the current color 'red'
  
  currentcolor = ILI9341_RED;
}
void resetPicture(void) {
  tft.setRotation(3);
  bmpDraw("P10.bmp", 0, 0);
  
}

void setup(void) {
  Serial.begin(115200);
  pixels.begin();
  delay(10);
  Serial.println("FeatherWing TFT");
  if (!ts.begin()) {
    Serial.println("Couldn't start touchscreen controller");
    while (1);
  }
  Serial.println("Touchscreen started");
  
  tft.begin();
  //tft.fillScreen(ILI9341_WHITE);
  //tft.setRotation(3);
  //tft.fillScreen(ILI9341_BLACK);
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  }
  Serial.println("OK!");
  for(int i=0;i<NUMPIXELS;i++){

    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(127,80,0)); // Moderately bright green color.
   }
    pixels.show(); // This sends the updated pixel color to the hardware.

 
  resetPicture();
  // select the current color 'red'
  
  
}

void loop() {
  
  
  if(ts.touched()){
    touched = 1;
    reset();
    timerOne = millis();
    
  }
  while(touched == 1){
  if(ts.touched()) timerOne = millis();
  // Retrieve a point  
  TS_Point p = ts.getPoint();
  
  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print("\tPressure = "); Serial.println(p.z);  
 
 
  // Scale from ~0->4000 to tft.width using the calibration #'s
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

  if (p.x > 200) {
    largeCircle = 0;
     oldcolor = currentcolor;

     if (p.y > 200) { 
       currentcolor = ILI9341_RED;
       largeCircle = 0; 
       tft.fillCircle(215, 245,5, ILI9341_GREEN);
     } else if (p.y > 75) {
       currentcolor = ILI9341_BLUE;
       largeCircle = 0;
       tft.fillCircle(215, 145, 5, ILI9341_GREEN);
     } else if (p.y >5) {
       currentcolor = ILI9341_WHITE;
       largeCircle = 1;
       tft.fillCircle(215, 45,5, ILI9341_GREEN);
     }

     if (oldcolor != currentcolor) {
        if (oldcolor == ILI9341_RED) 
         tft.fillCircle(215, 245, 20, ILI9341_RED);
        
        else  if (oldcolor == ILI9341_WHITE){ 
         tft.fillCircle(215, 45,5, ILI9341_WHITE);
         tft.drawCircle(215, 45,20,ILI9341_BLACK);
        }
         else if (oldcolor == ILI9341_BLUE) 
         tft.fillCircle(215, 145, 20, ILI9341_BLUE);
        
     }
     
  }
circleSize = PENRADIUS + (20 * largeCircle);
  if (((p.y-circleSize) > 0) && ((p.y+circleSize) < tft.height())) {
    tft.fillCircle(p.x, p.y, circleSize, currentcolor);
  }
  if(millis() - timerOne > 10000){ 
    touched = 0;
    resetPicture();
  }
  }
}
