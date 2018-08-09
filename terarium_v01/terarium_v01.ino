#include <U8g2lib.h>
#include <U8x8lib.h>

#include <Arduino.h>
#include <U8x8lib.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);  // Adafruit ESP8266/32u4/ARM Boards + FeatherWing OLED

const uint16_t PixelCount = 30; // make sure to set this to the number of pixels in your strip
const uint16_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

const char* title="Bohuslav I.";

#define LINE_SIZE 8               //pocet bodu na radek
#define LINE1 0                  //prvni radek
#define LINE2 (LINE1+LINE_SIZE*1) //druhy radek
#define LINE3 (LINE1+LINE_SIZE*2) //treti radek
#define LINE4 (LINE1+LINE_SIZE*3) //cvrty radek

#define TEMPS_COLUMN 0        //sloupec pro teploty
#define RH_COLUMN 32  //sloupec pro vlhkosti
#define RTC_COLUMN 69 //cloupec pro cas a datum

static const unsigned char bitmap_hacek[] U8X8_PROGMEM ={0x05,0x02};
static const unsigned char bitmap_stupen[] U8X8_PROGMEM ={0x06,0x09,0x06};

void hacek(int x,int y) {
  u8g2.drawXBMP(x+1,y+1,3,2,bitmap_hacek);
}

void stupen(int x,int y) {
  u8g2.drawXBMP(x+1,y+1,4,3,bitmap_stupen);
  u8g2.drawStr(x+5,y,"C");  
}

void rollLeft() {
  uint8_t *p;
  int sx,sy,i;

  sx=u8g2.getBufferTileWidth()*8;
  
  p=u8g2.getBufferPtr();
  for (i=u8g2.getBufferTileHeight();i!=0;i--) {  
    memcpy(p,p+1,sx-1);
    p[sx-1]=0;
    p+=sx;
  }
  u8g2.sendBuffer();
}


void show(int screen) {    
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_courR08_tr);  
  u8g2.clearBuffer();
  switch (screen) {
    case 1: //zobrazeni aktualnich stavu
      //teploty a RH
      u8g2.drawStr(TEMPS_COLUMN,LINE1,"cidla");
      u8g2.drawStr(TEMPS_COLUMN,LINE2,"1 00   99%");
      u8g2.drawStr(TEMPS_COLUMN,LINE3,"2 88");
      u8g2.drawStr(TEMPS_COLUMN,LINE4,"3 11");
      hacek(TEMPS_COLUMN,LINE1);
      stupen(TEMPS_COLUMN+25,LINE2);
      stupen(TEMPS_COLUMN+25,LINE3);
      stupen(TEMPS_COLUMN+25,LINE4);
      //cas a datum
      u8g2.drawStr(RTC_COLUMN,LINE1,"cas");
      u8g2.drawStr(RTC_COLUMN,LINE2,"00:00:00");
      u8g2.drawStr(RTC_COLUMN,LINE3,"datum");
      u8g2.drawStr(RTC_COLUMN,LINE4,"12.12.2017");
      hacek(RTC_COLUMN,LINE1);
      break;
      
    default: //uvodni displej
      u8g2.drawStr(0,LINE1,"Obyvatel");         
      u8g2.setFont(u8g2_font_timB18_tr);
      u8g2.drawStr(0,LINE1+12,title);
      break;    
  }
  u8g2.sendBuffer();
}

void setup() {
  //Inicializace LED pasku
  strip.Begin();
  strip.Show();  

  //Inicializace dipleje
  u8g2.begin();
  show(0);

  //TODO: inicializace RTC
  //TODO: Inicializace 1WIRE
}

static int ct;

void loop() {
  int i;

  ct++;
  show((ct>>7)&1);
  delay(10);
  // put your main code here, to run repeatedly:
    //delay(1000);for (i=0;i<100;i++) {rollLeft();delay(10);}
}
