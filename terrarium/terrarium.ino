#include <Arduino.h>
#include <OneWire.h>
#include <NeoPixelBus.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
#include <RTClib.h>

//----------
// Zadratovani

#define PIN_AM2302  2 //teplota a vlhkost
#define PIN_1WIRE   3 //1wire teplomery
#define PIN_LEDS1   4 //prvni ledpasek
#define PIN_LEDS2   5 //druhy ledpasek 

//Konstanty pro tisk polozek
#define PRINT_T1    0x01
#define PRINT_T2    0x02
#define PRINT_T3    0x04
#define PRINT_RH1   0x08
#define PRINT_COLOR 0x10
#define PRINT_DATE  0x20
#define PRINT_TIME  0x40
#define PRINT_ALL   0xFFFF

//----------------------------------------------------------------
// Udalosti
#define EV_PROC    0x01
#define EV_SEC     0x02
#define EV_MIN     0x04
#define EV_INIT    0x08
#define EV_DONE    0x10
#define EV_MEAREQ  0x20
#define EV_STATREQ 0x40

int events=EV_INIT; //stejne v dalsich knihovnach bude staticka inicializace, tak se k tomu pridame

void set_event(int mask) {
  events|=mask;
}

int tstclr_event(int mask) {
  if (events&mask) {
    events&=~mask;
    return mask;
  }
  return 0;
}

//----------------------------------------------------------------
// Nastaveni a aktualni stavy teraria

float t1,t2,t3,rh1;
RgbColor color=new RgbColor();

//----------------------------------------------------------------
// Pomocne kravinky

int sectick;

char printbuffer[64]; //bufik pro tisk
#define get_pritnt_buffer() printbuffer

int bcd2bin(int bcd) {
  return (((bcd>>4)&15)*10)+(bcd&15);
}

int float2int(float f) {
  return (int)(f+0.5);
}

//----------------------------------------------------------------
// RTC

RTC_DS1307 rtc;
DateTime now;

void printbuffer_time(void) {
  sprintf(get_pritnt_buffer(),"%02d:%02d:%02d",now.hour(),now.minute(),now.second()); //"00:00:00"
}

void printbuffer_date(void) {
  sprintf(get_pritnt_buffer(),"%02d:%02d:%04d",now.day(),now.month(),now.year()); //"12.12.2017"
}

void rtc_proc() {
  int sec=now.second(); //schovame si vybrane hodnoty
  int min=now.minute(); // pred zjistenim noveho casu
  now=rtc.now(); //natankujeme cas z RTC
  if (sec!=now.second()) set_event(EV_SEC); //vterinova udalost
  if (min!=now.minute()) set_event(EV_MIN); //minutova udalost
}

void set_rtc(DateTime ts) {
  rtc.adjust(ts); //dle dodaneho razidla nastavime
  rtc_proc(); //a provedeme proceduru, cimz se to nasaje do systemoveho casu
  debug_print(F("set"),PRINT_TIME|PRINT_DATE); //a ukazeme, co jsme provedli
}

//----------------------------------------------------------------
//LED pasek

const uint16_t PixelCount = 60; // make sure to set this to the number of pixels in your strip

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip1(PixelCount, PIN_LEDS1);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip2(PixelCount, PIN_LEDS2);

void set_color(RgbColor c) {      
  color.R=c.R;
  color.G=c.G;
  color.B=c.B;  
  strip1.ClearTo(color);
  strip1.Show();          
  strip2.ClearTo(color);
  strip2.Show();          
  debug_print(F("set"),PRINT_COLOR);
}


//----------------------------------------------------------------
// Dallas 1wire

OneWire wire1(PIN_1WIRE);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature ds18b20(&wire1);

void wire1_read(void) {
  ds18b20.requestTemperatures();
  t2=ds18b20.getTempCByIndex(0);
  t3=ds18b20.getTempCByIndex(1);
  debug_print(F("1wire read"),PRINT_T2|PRINT_T3);
}

//----------------------------------------------------------------
// AM2302 alias DTH22

DHT am2302(PIN_AM2302, DHT22);

void am2302_read() {
  t1=am2302.readTemperature(); //cteme teplotu
  rh1=am2302.readHumidity(); //cteme vlhkost

  // Check if any reads failed and exit early (to try again).
  if (isnan(t1) || isnan(rh1)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  } 
  debug_print(F("AM2302 read"),PRINT_T1|PRINT_RH1); 
}



//----------------------------------------------------------------
//Displej

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

/*
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
  //delay(1000);for (i=0;i<100;i++) {rollLeft();delay(10);}
*/


void display_proc(int screen) {    
  int x1,x2;
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_courR08_tr);  
  u8g2.clearBuffer();
  switch (screen) {
    case 1: //zobrazeni aktualnich stavu, bohuzel arduino neumi formatovat float, tak nejpreve potupny prevod na int
      //teploty a RH
      u8g2.drawStr(TEMPS_COLUMN,LINE1,"cidla");
      x1=float2int(t1);
      x2=float2int(rh1);
      sprintf(get_pritnt_buffer(),"1 %02d   %02d%%",x1,x2); //"1 00   99%"
      u8g2.drawStr(TEMPS_COLUMN,LINE2,get_pritnt_buffer());
      x1=float2int(t2);
      sprintf(get_pritnt_buffer(),"2 %02d",x1); //"2 88"
      u8g2.drawStr(TEMPS_COLUMN,LINE3,get_pritnt_buffer());
      x1=float2int(t3);
      sprintf(get_pritnt_buffer(),"2 %02d",x1); //"3 88"      
      u8g2.drawStr(TEMPS_COLUMN,LINE4,get_pritnt_buffer());
      hacek(TEMPS_COLUMN,LINE1);
      stupen(TEMPS_COLUMN+25,LINE2);
      stupen(TEMPS_COLUMN+25,LINE3);
      stupen(TEMPS_COLUMN+25,LINE4);
      //cas a datum
      u8g2.drawStr(RTC_COLUMN,LINE1,"cas");
      printbuffer_time();      
      u8g2.drawStr(RTC_COLUMN,LINE2,get_pritnt_buffer());      
      u8g2.drawStr(RTC_COLUMN,LINE3,"datum");
      printbuffer_date();      
      u8g2.drawStr(RTC_COLUMN,LINE4,get_pritnt_buffer());
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

void println(void) {
  Serial.print(F("\n"));
}

void print_item(char* name, float value) {
  Serial.print(' ');
  Serial.print(name);
  Serial.print('=');
  Serial.print(value);  
}

void print_rgb(char* name, RgbColor c) {
  Serial.print(' ');
  Serial.print(name);
  Serial.print('=');
  Serial.print(c.R);
  Serial.print(',');
  Serial.print(c.G);
  Serial.print(',');
  Serial.print(c.B);
}

//procedura tisku podle masky
void print_proc(int mask) {
  if (mask&PRINT_T1) print_item("t1",t1);
  if (mask&PRINT_T2) print_item("t2",t2);
  if (mask&PRINT_T3) print_item("t3",t3);
  if (mask&PRINT_RH1) print_item("rh1",rh1);
  if (mask&PRINT_COLOR) print_rgb("color",color);   
  if (mask&PRINT_TIME) {    
    Serial.print(F(" time="));
    printbuffer_time();
    Serial.print(get_pritnt_buffer());    
  }
  if (mask&PRINT_DATE) {    
    Serial.print(F(" date="));
    printbuffer_date();
    Serial.print(get_pritnt_buffer());    
  }
  Serial.print("\n");  
}

//tisk ladicich informaci
void debug_print(__FlashStringHelper* msg, int mask) {
  Serial.print(msg);
  print_proc(mask);
}

//cte prave jeden znak, prevede na maly pismenka
int read_char(void) {
  char c;

  c=Serial.read();    
  if ((c>='A')&&(c<='Z')) c=c-'A'+'a';
  return c;
}

//Cteme hexa cislici, pokud neni, vraci SERIAL_TIMEOUT
int read_hexn(void) {
  char c;

  c=read_char(); //cteme pismeno
  if (c==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //timeout, slus
  if ((c>='0')&&(c<='9')) return c-'0';
  if ((c>='a')&&(c<='f')) return c-'a'+10;
  return SERIAL_TIMEOUT;  
}

//cte hexa bajt (8 bitu)
int read_hexb(void) {
  int n1,n2;

  n1=read_hexn(); //horni pulka
  if (n1==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //pokud timeout, tak slus
  n2=read_hexn(); //dolni pulka
  if (n2==SERIAL_TIMEOUT) return SERIAL_TIMEOUT;  //pokud timeout, tak slus
  return (n1<<4)+n2; //vyrobime vysledek
}

//cte hexa word (16 bitu)
long read_hexw(void) {
  int b1,b2;

  b1=read_hexb(); //horni pulka
  if (b1==SERIAL_TIMEOUT) return SERIAL_TIMEOUT; //pokud timeout, tak slus
  b2=read_hexb(); //dolni pulka
  if (b2==SERIAL_TIMEOUT) return SERIAL_TIMEOUT;  //pokud timeout, tak slus
  return (b1<<8)+b2; //vyrobime vysledek  
}

void show_head(void) {
  Serial.println(F("\n\nTerrarium v1.0\n==============\n"));  
}

void show_help(void) {
  show_head();
  Serial.println(F(
  "(h)elp"\
  "\n(s)tatus"\
  "\n(m)easure"\  
  "\n(c)olor RRGGBB in hex"\
  "\n(t)ime hhmmssDDMMYY in bcd"\
  "\n")); //na zaver dvojity odradkovani (druhy je od println);
}

DateTime read_rtc(void) {
  int x1,x2,x3,x4,x5,x6;
  
  if ((x1=read_hexb())>=0)
    if ((x2=read_hexb())>=0)
      if ((x3=read_hexb())>=0)
        if ((x4=read_hexb())>=0)
          if ((x5=read_hexb())>=0)
            if ((x6=read_hexb())>=0) {
              //rtc.adjust(DateTime(bcd2bin(x6)+2000,bcd2bin(x5),bcd2bin(x4),bcd2bin(x1),bcd2bin(x2),bcd2bin(x3)));
              return DateTime(bcd2bin(x6)+2000,bcd2bin(x5),bcd2bin(x4),bcd2bin(x1),bcd2bin(x2),bcd2bin(x3));              
            }            
  return now;
}

RgbColor read_color(void) {
  int x1,x2,x3;  
  Serial.println("Co je sakra");  
  if ((x1=read_hexb())>=0)
    if ((x2=read_hexb())>=0)
      if ((x3=read_hexb())>=0) {        
        return RgbColor(x1,x2,x3);        
      }
  return color;
}

//Akce na seriovem portu
void serial_proc(void) {
  char c;
  int x1,x2,x3,x4,x5,x6;
  
  if (Serial.available()==0) return; //pokud na seriaku klid, tak slus
  c=read_char(); //nacteme prikaz
  switch (c) { //rozeskok dle prikazu
    case 't': //nastaveni rtc
      set_rtc(read_rtc()); //ze seriaku nacteme DateTime a to nastavime
      break;      
    case 'c': //nastaveni barvy
      set_color(read_color()); //ze seriaku nabereme barvu a tu nastavime
      break; 
    case 'm': //pozadavek mereni  
      set_event(EV_MEAREQ); //nahodime pozadavek na mereni     
      break;    
    case 's': //pozadavek na data
      set_event(EV_STATREQ); //nahodime pozadavek na mereni     
      break;
    case 'h': //napoveda
    case '?':
      show_help(); //ukaz napovedu
      break;    
  }
}

//----------------------------------------------------------------
//Prilepeni udalosti na arduinosystem

//pocatek vsehomira
void setup() {  
  //resime po svem, tudis at si trhnou ploutvi ;-)
}

//hlavni smyce adruina, tady jen rozdeleni do hlavnich udalosti
void loop() {  
  if (tstclr_event(EV_INIT)) {
    ev_init(); //inicializace
  } else if (tstclr_event(EV_SEC)) {
    ev_sec(); //vterina
  } else if (tstclr_event(EV_MIN)) {
    ev_min(); //minuta
  } else if (tstclr_event(EV_DONE)) {
    //TODO: jak se dozvim o vypadku? a co budeme delat?
  } else if (tstclr_event(EV_PROC)) {
    proc(); //furtakce
  } else { //vsechny udalosti vyreseny
    set_event(EV_PROC); //takhe si zase nahodime takovy to furtnecodelam
    delay(50); //ale nebudem to prehanet, mozna to poslat chrapat, at to nezere, nebo proste nevim
  } //zvlastni je, ze kdyz je to min nez 10, tak zacne zlobit seriova komunikace. Proc? nemam paru
}

//----------------------------------------------------------------
// Udalosti a jejich reseni

//Start celyho blazince
void ev_init() {
  //Inicializace seriaku
  Serial.begin(9600);
  show_head();
    
  //Inicializace LED pasku
  strip1.Begin();
  strip2.Begin();
  set_color(RgbColor(3,3,3));

  //Inicializace dipleje
  u8g2.begin();
  display_proc(0);

  //inicializace vlhko a teplomeru
  am2302.begin();
  am2302_read();

  //inicializace dallas teplomeru
  ds18b20.begin();
  wire1_read();

  //inicializace RTC
  if (rtc.begin()) { //startujem
    if (! rtc.isrunning()) { //pokud nebezelo,
      set_rtc(DateTime(2018,1,1,0,0,0));      
    }    
  } else {
    Serial.println(F("Couldn't find RTC"));
  }  
}

//nova minuta
void ev_min() {
  set_event(EV_MEAREQ); //jednou za minutu muzeme zmerit okoli, to nas asik nezabije  
}

//nova vterina
void ev_sec() {
  sectick++;  
  display_proc(((sectick)>>2)&1);
}

//nudime se
void proc() {  
  if (tstclr_event(EV_MEAREQ)) { //pozadavek mereni
    am2302_read(); //cteme vlhko a teplo
    wire1_read(); //cteme jednodraty
  } else if (tstclr_event(EV_STATREQ)) { //pozadavek na vyplivnuti dat
    Serial.print(F("status:"));
    print_proc(PRINT_ALL);    
  } else { //nuda v brne
    serial_proc();
    rtc_proc();      
  }
}
