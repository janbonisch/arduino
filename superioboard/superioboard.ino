#include <Arduino.h>
#include <OneWire.h>              //http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
#include <DallasTemperature.h>    //https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <U8x8lib.h>              //https://github.com/olikraus/u8g2
#include <ModbusRTUSlave.h>       //!!!see https://forum.arduino.cc/index.php?topic=663329.0

// Ruzne poznamky:
// Radeji pouzivam stdin typy, jeden nikdy nevi, do ceho ten kod budu rvat.

#define DEBUG_PLN_HEX(msg,val)  {Serial.print(F(msg));Serial.println(val,HEX);}
#define DEBUG_PLN_DEC(msg,val)  {Serial.print(F(msg));Serial.println(val,DEC);}

//----------------------------------------------------------------
// Zadratovani a dalsi konstanty

#define TICK_MS       10  //kolik milisekund ma jeden tik

#define STR4094       A0  //4094 strobe         1=stale prenos
#define DATA4094      A1  //4094 data           normalne data, co jinyho
#define CLK4094       A2  //4094 clock          nabezna vsunuje
#define OE4094        A3  //4094 output enable  1=povoleni
#define U_RELAY       A6  //mereni napeti relatek
#define AMBIENT_LIGHT A7  //cidlo osvetleni
#define RS485_DE      3   //ovladani vysilace rs485
#define BIN_READ      4   //cteni vstupu
#define WDOG          5   //hlidaci cokl
#define RELE_A        6   //aktivace buzeni relat do polohy A
#define RELE_B        7   //aktivace buzeni relat do polohy B
#define WIRE1         8   //stado 1Wire teplomeru
#define RES2          9
#define SPI_SS        10
#define SPI_MOSI      11
#define SPI_MISO      12
#define SPI_CLK       13

//tohle asik neni ani potreba
//#define PIN_SDA 
//#define PIN_SCL

//volitelne vstupovystupy
#define BUTTON1       9
#define BUTTON2       2
#define LED1          12
#define LED2          11

//Prevod mezi poradim na retezu 4094 a logickym cislovanim od 0 z leva
//const uint8_t hwid2logid_tab[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}; //prevod mezi pozici na posuvaku a poradim z leva do prava na svorkach
uint8_t hwid2logid(uint8_t id) {
  //return hwid2logid_tab[id&0x0f];
  return (id&0x0f)^0x08;
}

//----------------------------------------------------------------
// Zakladni IO nastaveni, operace, datova rozhrani

// Prima manipulace s pinama, aby se to pripadne dalo optimalizovat
#define STR4094_1()     {digitalWrite(STR4094,HIGH);}
#define STR4094_0()     {digitalWrite(STR4094,LOW);}
#define DATA4094_1()    {digitalWrite(DATA4094,HIGH);}
#define DATA4094_0()    {digitalWrite(DATA4094,LOW);}
#define CLK4094_1()     {digitalWrite(CLK4094,HIGH);}
#define CLK4094_0()     {digitalWrite(CLK4094,LOW);}
#define OE4094_1()      {digitalWrite(OE4094,HIGH);}
#define OE4094_0()      {digitalWrite(OE4094,LOW);}
#define RELE_A_1()      {digitalWrite(RELE_A,HIGH);}
#define RELE_A_0()      {digitalWrite(RELE_A,LOW);}
#define RELE_B_1()      {digitalWrite(RELE_B,HIGH);}
#define RELE_B_0()      {digitalWrite(RELE_B,LOW);}
#define WDOG_1()        {digitalWrite(WDOG,HIGH);}
#define WDOG_0()        {digitalWrite(WDOG,LOW);}

#define CLK4094_PULSE() {CLK4094_1();CLK4094_0();}
#define STR4094_PULSE() {STR4094_1();STR4094_0();}
#define WDOG_PULSE()    {WDOG_1();WDOG_0();}
#define IS_BIN_READ()   (digitalRead(BIN_READ)!=LOW)  

#ifdef BUTTON1
#define IS_BUTTON1()    (digitalRead(BUTTON1)!=HIGH)
#endif //BUTTON1
#ifdef BUTTON2
#define IS_BUTTON2()    (digitalRead(BUTTON2)!=HIGH)
#endif //BUTTON2

//modbus mapovani
uint16_t modbus_data[16]; //blok modbus dat
#define inputs_status  modbus_data[0] //bits  0-15 stav vstupu
#define relays_status  modbus_data[1] //bits 16-31 stav relat

//Podporgramek pro pro nastaveni pinu(pin_id) jako vystup a inicializace na pozadovanou hodnotu (value).
void pinOutput(uint8_t pin_id, uint8_t value) {
  pinMode(pin_id,OUTPUT);
  digitalWrite(pin_id,value);  
} //puvodne jako makro, usetrime 55 bajtu! ;-) // #define pinOutput(pin_id,value) {pinMode(pin_id,OUTPUT);digitalWrite(pin_id,value);} //makro pro nastaveni pinu(pin_id) jako vystup a inicializace na pozadovanou hodnotu (value).

void io_setup(void) {
  //watchdog
  pinOutput(WDOG,LOW); //je to vystup, dame do nuly  
  WDOG_PULSE(); //a hnedle si pusneme
  //rizeni relatek
  pinOutput(RELE_A,HIGH); //buzeni je aktivni v
  pinOutput(RELE_B,HIGH); // log 0  
  //LEDky, volitelny preklad pokud je fakt chceme
#ifdef LED1  
  pinOutput(LED1,HIGH);
#endif //LED1
#ifdef LED2  
  pinOutput(LED2,HIGH);
#endif //LED2  
  //Cudliky, volitelny preklad pokud je fakt chceme
#ifdef BUTTON1
  pinMode(BUTTON1,INPUT_PULLUP);
#endif //BUTTON1
#ifdef BUTTON2
  pinMode(BUTTON2,INPUT_PULLUP);
#endif //BUTTON2
  //rizeni retezu posuvaku
  pinMode(BIN_READ,INPUT_PULLUP);    
  pinOutput(STR4094,LOW);
  pinOutput(DATA4094,LOW);
  pinOutput(CLK4094,LOW);
  pinOutput(OE4094,LOW);
  //ADprevodnik a spol
  analogReference(DEFAULT); //reference 5V
  //analogReference(INTERNAL); //nastavime interni referenci, u 385 je to 1.1V
}

//----------------------------------------------------------------
// Dallas 1wire

/*
OneWire wire1(WIRE1); //jednodrat na urceny drat
DallasTemperature ds18b20(&wire1); // vytvoření instance senzoryDS z knihovny DallasTemperature
*/

//----------------------------------------------------------------
// Retez 4094

#define CHAIN4094_LENGTH 16 //kolik ma retizek vystupnich bitu

///Čtení vstupů.
///Fígl je v tom, že po řetězu 4094 proženeme jedničku a tím přes BIN_READ postupně načteme všechny vstupy.
uint16_t readin4094(void) {
  uint8_t b;
  uint16_t r; 
    
  DATA4094_1(); //data do jednicky
  CLK4094_PULSE(); //cukneme hodinama
  DATA4094_0(); //data a zase zpet do 0
  STR4094_1(); //a jedem s tim na vystupy
  b=CHAIN4094_LENGTH; //tolik ma retizek posuvaku bitu
  r=0; //k nulam pridavame orem umime pridat jednicku, kdyby tam byla nekde jednicka, tak ji nula neprebije
  for (;;) {    
    if (!IS_BIN_READ()) r|=1; //pokud tam neni jednicka, tak sup s tim na vystup
    if ((--b)==0) { //pokud uz mame vsechno      
      CLK4094_PULSE(); //naposled cukneme hodinama, aby byly na vystupech posuvaku vsude nuly
      STR4094_0(); //a tento stav uz tam ponechame
      return r; //a vracime vysledek
    }
    CLK4094_PULSE(); //cukneme hodinama  
    r<<=1; //udelam si misto na dalsi bitik
  };   
}

///Nahození jednoho výstupu.
///@param id číslo výstupu, víc jak CHAIN4094_LENGTH znamená všechno do nuly
void set4094(uint8_t id) {
  uint8_t b;

  id++; //protoze normalni lidi pocitaji od nuly, musime s tim o jednu cuknout
  b=CHAIN4094_LENGTH; //tolik ma retizek posuvaku bitu
  do {
    DATA4094_0(); //vetsinou tam je nula
    if (b==id) DATA4094_1(); //vyjimecne jednicka
    CLK4094_PULSE(); //cukneme hodinama    
  } while ((--b)!=0); //udelame vsechny bitky posuvaku
  DATA4094_0(); //pro formu dame na zaver data do nuly, ale je to fakt naprd
  STR4094_PULSE(); //a jeb s tim na vystupy  
}

//vynulovani retezu 4094 a schozeni buzeni relat
void clr4094(void) {  
  set4094(0xFF); //nahazujeme pin, kterej neexistuje, takze to tam posle samy nuly
}

///Inicializace řetězu 4094
void init4094(void) {
  clr4094(); //vsechno to pujde do nuly
  clr4094(); // tak jako pro jistotu 2x
  OE4094_1(); //a povolime vystupy, to zakazovat uz nikde nebudeme
}

//----------------------------------------------------------------
// Ovladac rele

uint16_t relays_change; //pozadavky na buzeni
uint8_t relays_timer; //casovadlo buzeni

#define RELAY_QUIET       0x00  //nic se nedeje
#define RELAY_DRIVE       0x01  //budime relata
#define RELAY_DRIVE_TIME  20 //jak dlouho budime [ms]

#define NO_RELAY          0
#define RELAY_GROUP_A     ((NO_RELAY)+1)
#define RELAY_GROUP_B     ((RELAY_GROUP_A)+1)

void relays_drive(uint8_t mode) {
  switch (mode) {
    case RELAY_GROUP_A:
      RELE_B_1(); //nejprve pro sichr schodime skupinu B
      RELE_A_0(); //a nahodime A          
      return;
    case RELAY_GROUP_B:
      RELE_A_1(); //nejprve pro sichr schodime skupinu A
      RELE_B_0(); //a nahodime B    
      return;    
    default:
      RELE_A_1(); //deaktivace buzeni
      RELE_B_1(); // vsech relat
      return;
  }
}

void relays_init() {
  relays_drive(NO_RELAY); //nebudime
  relays_timer=0; //a pripravime se do akce
  relays_change=-1;
}

uint8_t relays_tick(uint8_t ms) {
  uint16_t timer,mask,st;
  uint8_t i;
    
  timer=relays_timer; //zlokalnim si to
  if (timer==0) { //pokud se nic nedeje, tak je moznost pripadne neco zacit
    st=relays_change; //zlokalnim si pozadavky na zmenu
    if (st==0) return RELAY_QUIET; //pokud zadna pozadovana zmena, tak nic
    //TODO: zmerit napeti na relatech, pokud malo, tak slus s navratovym kodem RELAY_QUIET
    mask=1; //maska
    i=0; //idcko    
    while ((st&mask)==0) { //pokud maska nemaskuje
      mask<<=1; //soupneme masku
      i++; //a cukneme s idckem
    }
    relays_change=st&(~mask); //schodim bitika a ulozim to jako novy stav zmen
    set4094(i); //naposunujeme jednicku do prislusneho bitiku
    i=RELAY_GROUP_A; //zatim skupina A
    if ((relays_status&mask)!=0) i++; //pokud B, tak preci B
    relays_drive(i); //a budime
    relays_timer=1; //rozebehneme casovadelko
    return RELAY_DRIVE; //a koncime s hlasenim ze se kona buzeni
  }
  timer+=ms; //posrcime timeeera
  if (timer>=RELAY_DRIVE_TIME) { //pokud uz jsme dobudili
    relays_drive(NO_RELAY); //deaktivace buzeni vsech relat
    clr4094(); //koncim buzeni
    timer=0; //zastavuju casovadlo
  }
  relays_timer=timer; //nova hodnota casovadla
  return RELAY_DRIVE; //oznamujeme volajicimu, ze budime
}

//----------------------------------------------------------------
// Ovladac vstupu

#define INPUT_FLT_MASK    0xFF  //bitiky filtru co me zajimavy
#define INPUT_FLT_PRESS   0x7F  //nabezna hrana je 7 jednicek za sebou
#define INPUT_FLT_RELASE  0x80  //sestupna hrana je 7 nul za sebou
#define INPUT_PERIOD      30  //rychlost cteni vstupu

uint8_t inputs_timer;
uint8_t inputs_flt[CHAIN4094_LENGTH+2]; //vstupy pres retez a 2 extra cudly

void input_flt_proc(uint8_t id, uint8_t value) {
  uint8_t f;

  f=inputs_flt[id]; //zlokalnim si to
  f<<=1; //udelam misto
  if (value!=0) f|=1; //pokud aktiv, naperu jednicku
  inputs_flt[id]=f; //a novy stav filtru
}

void inputs_status_update(void) {
  int i;
  uint8_t f;  
  uint16_t mask,st;

  i=0; //index do filtru
  mask=1; //pojizdna maska
  st=inputs_status; //nabereme aktualni stavy
  do {
    f=inputs_flt[i]&INPUT_FLT_MASK; //nabereme si aktualni stav filtru a vymlaskneme jen hledane bitiky
    if (f==INPUT_FLT_PRESS) { //pokud je to nabezna hrana
      st|=mask; //nahod bitika
    } else if (f==INPUT_FLT_RELASE) { //pokud je to sestupna
      st&=~mask; //schodime bitika
    }
    if ((mask<<=1)==0) { //cukneme maskou a pokud je to nula, tak uz mame hotovo
      inputs_status=st; //novy stav
      return; //a koncime
    }
    i++;    
  } while (1);
}

void inputs_tick(uint8_t ms, uint8_t relaysst) {
  uint8_t i;
  uint16_t st;

  if ((inputs_timer+=ms)<INPUT_PERIOD) return; //pokud jeste nenazral cas, tak nic
  inputs_timer-=INPUT_PERIOD; //naplanujeme si dalsi cas  
#ifdef BUTTON1
  i=0;
  if (IS_BUTTON1()) i++;  
  input_flt_proc(CHAIN4094_LENGTH+0,i);
#endif //BUTTON1
#ifdef BUTTON2
  i=0;
  if (IS_BUTTON2()) i++;  
  input_flt_proc(CHAIN4094_LENGTH+1,i);
#endif //BUTTON2
  if (relaysst!=RELAY_QUIET) return; //pokud nejsou relata v naprostym klidku, tak spatno a konecno
  st=readin4094(); //nacteme vstupy    
  i=0; //index vstupu  
  do {
    input_flt_proc(hwid2logid(i),st&1); //proved proceduru    
    if (i==(CHAIN4094_LENGTH-1)) { //pokud uz to je vsecho
      //DEBUG_PLN_HEX("xxx=",inputs_flt[0]);            
      inputs_status_update(); //aktualizujeme bitovy stav vstupu
      return; //schluss
    }
    st>>=1; //a jdeme na dalsi bitik
    i++; //dalsi    
  } while (1);
}

//----------------------------------------------------------------
// MODBUS

/*
ModbusRTUSlave rtu(0x01, &Serial, RS485_DE); //inicilaizace objektu s modbusem

void modbus_init(void) {
  rtu.addBitArea(0x1000, (u8*)modbus_data, 32); //prvnich 32 bitiku jsou bitovy promenny
  rtu.addWordArea(0x2000, (u16*)modbus_data, sizeof(modbus_data)/sizeof(uint16_t)); //a jako word je pristupny komplet cely pole
  rtu.begin(9600); //spoustime modbus
}

void modbus_proc() {
  uint16_t relays;

  relays=relays_status; //schovam si posledni znamy stav relat
  rtu.process(); //procedura modbus
  relays^=relays_status; //zjistime si zmeny, jako ze neco prislo modbusem
  relays_change|=relays; //prihodime zmeny od modbusu
}
*/

//----------------------------------------------------------------
// Displej

U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE); //toto je nas displej, reset nevedeme

uint16_t display_last_inputs_status;  //naposled zobrazovany stav vstupu
uint16_t display_last_relays_status; //naposled zobrazovany stav relat
uint16_t display_ph; //faze pro rozlozeni zobrazovani

const uint8_t font_3x5_0_9[]={ //font pro pidicislice 0 az 9
/* 0 */ 0x3E,0x22,0x3E,
//  XXX
//  X.X
//  X.X
//  X.X
//  XXX
/* 1 */ 0x24,0x3E,0x20,
//  .X.
//  XX.
//  .X.
//  .X.
//  XXX
/* 2 */ 0x3A,0x2A,0x2E,
//  XXX
//  ..x
//  XXX
//  x..
//  xxx
/* 3 */ 0x2A,0x2A,0x3E,
//  XXX
//  ..X
//  XXX
//  ..X
//  XXX
/* 4 */ 0x0E,0x08,0x3E,
//  X.X
//  X.X
//  XXX
//  ..X
//  ..X
/* 5 */ 0x2E,0x2A,0x3A,
//  XXX
//  X..
//  XXX
//  ..X
//  XXX
/* 6 */ 0x3E,0x2A,0x3A,
//  XXX
//  X..
//  XXX
//  X.X
//  XXX
/* 7 */ 0x22,0x1A,0x06,
//  XXX
//  ..X
//  .X.
//  .X.
//  X..
/* 8 */ 0x3E,0x2A,0x3E,
//  XXX
//  X.X
//  XXX
//  X.X
//  XXX
/* 9 */ 0x2E,0x2A,0x3E,
//  XXX
//  X.X
//  XXX
//  ..X
//  XXX
};

void display_show_id(uint8_t x, uint8_t y, uint8_t num, uint16_t inv) {
  uint8_t buf[8];
  uint8_t ofs,i;

  memset(buf,0,sizeof(buf)); //pocatkem vseho je mezera
  num++; //cislujeme jako lidi od jednicky
  if (num<20) { //umim jen 0 az 19, ostatni je mezera
    num+=num<<1; //vynasobim trema, optimalizace na rychlost jak blazen
    if (num<30) { //cislice 0-9 delame skoro normane    
      ofs=2; //zacina to tadyk
    } else {
      buf[1]=0x3E;  //jednicka je jen carka
      ofs=3;  //za jednickou bue mezera
      num-=30;  //a na konec prijde 0-9 jako normalne    
    } 
    for (i=3;i!=0;i--) buf[ofs++]=font_3x5_0_9[num++]; //preneseme 3 bajty, nebylo by lepsi rozbalit? Odpovim si: nebylo, bo to asi rozbali kompilator
  }
  if (inv!=0) { //pokud mame inverzi
    for (i=7;i!=0;) buf[--i]^=0x7F; //tak vhodne invertujeme
  }  
  u8x8.drawTile(x,y,1,buf); //a na zaver to kydneme na displej
}

uint16_t display_status_proc(uint16_t act,uint8_t disp_line, uint16_t last) {
  uint8_t i;
  uint16_t change,mask;

  i=display_ph&0x0F;  //index je v dolnich bitikach
  mask=1<<i;  //podle dolnich bitiku faze displeje si udelam masku  
  if (((act^last)&mask)!=0) { //pokud doslo ke zmene na aktualnim bitiku, tak to zpracujeme
    last^=mask; //preklopime do noveho stavy    
    display_show_id(i,disp_line,i,last&mask); //provedeme zobrazeni
  }
  return last;
}

void display_proc(void) {
  uint8_t i;
  uint16_t change,mask,st;

  if (display_ph<16) { //zobrazujeme vstupy
    display_last_inputs_status=display_status_proc(inputs_status,0,display_last_inputs_status); //provedem proceduru zobrazeni vstupu
  } else if (display_ph<32) {
    display_last_relays_status=display_status_proc(relays_status,3,display_last_relays_status); //provedem proveduru zobrazeni vystupu
  } else {
    display_ph=0;
    return;
  }
  display_ph++;
}

void display_init(void) {
  u8x8.begin();    
  u8x8.setFont(u8x8_font_chroma48medium8_r);      
  u8x8.draw2x2String(0,1,"<S(i)OB>");
  display_last_inputs_status=~inputs_status;
  display_last_relays_status=~relays_status;
  display_ph=0; //zahajujeme prvni fazi
  do {
    display_proc(); //rozkladaci procedura displeje
  } while (display_ph==0); //dokud neni hotovej celej
}

//----------------------------------------------------------------
// Odvozene udalosti pro zakladni kooperativni multitasking.

void ev_init(void) {
  io_setup(); //zakladni inicializace IO nozek
  init4094(); //inicializace retezu posuvnych registru  
  relays_init(); //inicializace relat  
  display_init(); //inicializace displeje
  Serial.begin(9600); //ladici seriak
  
  //modbus_init(); //start modbus rozhrani
}

void ev_proc(void) { 
  //modbus_proc();
  display_proc();
}

void ev_tick(uint8_t ms) {
  //inputs_tick(ms, RELAY_QUIET);
  inputs_tick(ms,relays_tick(ms)); //na retezu 4094 je treba se stridat, takze vstupy musi vedet, co delaj relata
}

void ev_sec(void) {
  DEBUG_PLN_HEX("flt16=",inputs_flt[16]);
  DEBUG_PLN_HEX("inputs_status=",inputs_status);  
}

void ev_min(void) {  
}

//----------------------------------------------------------------
// Hlavni Arduino udalosti, na ktery se zavesime

#define BASE_DEBUG

#ifdef xBASE_DEBUG //hrajeme si

uint8_t dbgct;

void setup() {
  Serial.begin(9600); //ladici seriak
  Serial.println(F("\r\nSIOB - debug"
                   "\r\n============"
                   "\r\n\r\n"));  
  io_setup(); //zakladni inicializace IO nozek
  init4094(); //inicializace retezu posuvnych registru  
  relays_init(); //inicializace relat  
  display_init(); //inicializace displeje
}

void loop() {
  WDOG_PULSE();  
  //Ladeni AD vstupu
  /*
  Serial.print(F("osvetleni "));
  Serial.println(analogRead(AMBIENT_LIGHT),DEC);  
  Serial.print(F("napajeni relat "));
  Serial.println(analogRead(U_RELAY),DEC);  
  */
  
  //Ladeni watchdogu, TODO: nejakej potiz s HW, zatim deaktivovani HW je jako vstup
  /*
  Serial.println(F("Kick the dog"));
  //WDOG_PULSE();
  pinOutput(WDOG,HIGH);digitalWrite(WDOG,LOW);
  //WDOG_1(); delay(10); WDOG_0();
  delay(500);
  */
  
  //Ladeni retezu 4094      
  /*
  inputs_status=readin4094();  
  Serial.print(F("inputs_status="));
  Serial.println(inputs_status,HEX);  
  display_proc();
  //delay(100);
  */
  
  //Ladeni relatek 
  /*
  #define REL_DLY 20
  #define REL_ID ((dbgct&0xF))
  set4094(REL_ID);relays_drive(RELAY_GROUP_A);delay(REL_DLY);relays_drive(RELAY_QUIET);set4094(255);
  delay(30);
  set4094(REL_ID);relays_drive(RELAY_GROUP_B);delay(REL_DLY);relays_drive(RELAY_QUIET);set4094(255);  
  delay(30);   
  */
  //Ladeni ledek  

  /*
  #define LED_DELAY   250
  digitalWrite(LED1,LOW);delay(LED_DELAY);  
  digitalWrite(LED1,HIGH);delay(LED_DELAY);
  digitalWrite(LED2,LOW);delay(LED_DELAY);
  digitalWrite(LED2,HIGH);delay(LED_DELAY);   
  */
  
  //Ladeni cudliku
  /*
  dbgct=0;
  if (IS_BUTTON1()) dbgct+=1;
  if (IS_BUTTON2()) dbgct+=2;
  Serial.print(F("button status "));
  Serial.println(dbgct,HEX);  
  delay(100);
  */

  //konec hlavni samyce
  dbgct++;
}

#else BASE_DEBUG  //plna verze desky

uint16_t secct; //pocitadlo pro generator vterin
uint8_t minct; //pocitadlo pro generator minut
uint8_t flags; //priznaky
uint8_t last_millis;

#define FL_SEC 0x01 //priznak vteriny
#define FL_MIN 0x02 //priznak minuty

void setup() {  
  ev_init(); //udalost inicializace
  last_millis=(uint8_t)millis(); //inicializace casovani
  secct=0; //start generatoru vteriny
  minct=0; //start generatoru minuty
}

void loop() {
  uint8_t ms;
  
  ms=((uint8_t)millis())-last_millis;
  if (ms!=0) {    
    //DEBUG_PLN_DEC("ms=",ms);
    last_millis+=ms;
    ev_tick(ms); //volame udalost    
    secct+=ms; //prihodime k pocitadlu vteriny
    if (secct>=1000) { //pokud mame vterinu
      //DEBUG_PLN_DEC("secct=",secct);
      secct-=1000; //odzitou vterinu zahodime a jedeme dal
      flags|=FL_SEC; //nahodime priznak vteriny
      if ((++minct)>=60) { //pocitame minutu  
        flags|=FL_SEC; //nahodime priznak minuty
        minct=0; //a pocitadlo zase od zacatku
      }
    } 
  }
  if ((flags&FL_SEC)!=0) { //nebyla tuhle vterina
    flags&=~FL_SEC; //schodime priznaka
    ev_sec(); //volame funkce
    return; //a slus
  }
  if ((flags&FL_MIN)!=0) { //nebyla tuhle minuta
    flags&=~FL_MIN; //schodime priznaka
    ev_min(); //volame funkce
    return; //a slus
  }
  ev_proc(); //neni nic jinyho na praci, jedem procedury
  WDOG_PULSE(); //kopneme do cokla
}
#endif //BASE_DEBUG
