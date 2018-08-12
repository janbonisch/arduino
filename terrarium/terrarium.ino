
#include <Arduino.h>

//----------
// Zadratovani

#define PIN_AM2302  2 //teplota a vlhkost
#define PIN_1WIRE   3 //1wire teplomery
#define PIN_LEDS    4 //ledpasek


//----------------------------------------------------------------
// Nastaveni a aktualni stavy teraria

float t1,t2,t3,rh1;

//----------------------------------------------------------------
// Dallas 1wire

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire wire1(PIN_1WIRE);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature ds18b20(&wire1);

void wire1_read(void) {
  ds18b20.requestTemperatures();
  t2=ds18b20.getTempCByIndex(0);
  t3=ds18b20.getTempCByIndex(1);
  
  
  Serial.print("t2=");
  Serial.print(t2);
  Serial.print(" t3=");
  Serial.print(t3);
  Serial.print("\n");  
}



//----------------------------------------------------------------
// AM2302 alias DTH22

#include "DHT.h"

void println(void) {
  Serial.print(F("\n"));
}

DHT am2302(PIN_AM2302, DHT22);

void am2302_read() {
  t1=am2302.readTemperature(); //cteme teplotu
  rh1=am2302.readHumidity(); //cteme vlhkost

  // Check if any reads failed and exit early (to try again).
  if (isnan(t1) || isnan(rh1)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
}  

  Serial.print(F("t1="));
  Serial.print(t1);
  Serial.print(F(" rh1="));
  Serial.print(rh1);
  println();
  
}


//----------------------------------------------------------------
//LED pasek

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

const uint16_t PixelCount = 30; // make sure to set this to the number of pixels in your strip

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PIN_LEDS);

//----------------------------------------------------------------
//Displej
#include <U8g2lib.h>
#include <U8x8lib.h>

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);  // Adafruit ESP8266/32u4/ARM Boards + FeatherWing OLED

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

void display_proc(int screen) {    
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
      u8g2.drawStr(0,LINE1+12,"Bohuslav I.");      
      break;    
  }
  u8g2.sendBuffer();
}

//----------------------------------------------------------------
//Seriak
#define SERIAL_TIMEOUT (-1)

int read_char(void) {
  char c;

  c=Serial.read();  
  return tolower(c);
}

//Cteme hexa cislici, pokud neni, vraci SERIAL_TIMEOUT
int read_hexn(void) {
  char c;

  c=read_char(); //cteme pismeno
  if (c==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //timeout, slus
  if ((c>='0')&&(c<='9')) return c-'9';
  if ((c>='a')&&(c<='f')) return c-'a'+10;
  return SERIAL_TIMEOUT;  
}

int read_hexb(void) {
  int n1,n2;

  n1=read_hexn(); //horni pulka
  if (n1==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //pokud timeout, tak slus
  n2=read_hexn(); //dolni pulka
  if (n2==SERIAL_TIMEOUT) return SERIAL_TIMEOUT;  //pokud timeout, tak slus
  return (n1<<4)+n2; //vyrobime vysledek
}

long read_hexw(void) {
  int b1,b2;

  b1=read_hexb(); //horni pulka
  if (b1==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //pokud timeout, tak slus
  b2=read_hexb(); //dolni pulka
  if (b2==SERIAL_TIMEOUT) return SERIAL_TIMEOUT;  //pokud timeout, tak slus
  return (b1<<8)+b2; //vyrobime vysledek  
}


void show_head(void) {
  Serial.println(F("Terrarium v1.0\n==============\n"));  
}

void show_help(void) {
  show_head();
  Serial.println(F(
  "\n(h)elp"   \
  "\n")); //na zaver dvojity odradkovani (druhy je od println);
}

//Akce na seriovem portu
void serial_proc(void) {
  char c;
  
  if (Serial.available()==0) return; //pokud na seriaku klid, tak slus
  c=read_char(); //nacteme prikaz
  switch (c) { //rozeskok dle prikazu
    case 'h':
    case '?':
      show_help();
      break;    
  }
}

//----------------------------------------------------------------
//Start a hlavni smyce arduina

void setup() {
  //Inicializace seriaku
  Serial.begin(9600);
  show_head();
    
  //Inicializace LED pasku
  strip.Begin();
  strip.Show();  

  //Inicializace dipleje
  u8g2.begin();
  display_proc(0);

  //inicializace vlhkoa teplomeru
  am2302.begin();
  am2302_read();

  //inicializace dallas teplomeru
  ds18b20.begin();
  wire1_read();

  //TODO: inicializace RTC
}

static int ct;
void loop() {
  int i;

  display_proc(((++ct)>>7)&1);
  serial_proc();
  delay(10);
  //delay(1000);for (i=0;i<100;i++) {rollLeft();delay(10);}
}
