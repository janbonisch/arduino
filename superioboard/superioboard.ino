#include <OneWire.h>              //http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
#include <DallasTemperature.h>    //https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <U8g2lib.h>              //https://github.com/olikraus/u8g2
#include <U8x8lib.h>              //https://github.com/olikraus/u8g2
#include <ModbusRTUSlave.h>       //!!!see https://forum.arduino.cc/index.php?topic=663329.0



// Ruzne poznamky:
// Radeji pouzivam stdin typy, jeden nikdy nevi, do ceho ten kod budu rvat.

//----------------------------------------------------------------
// Zadratovani a dalsi konstanty

#define TICK_MS       10  //kolik milisekund ma jeden tik

#define STR4094       A0  //4094 strobe         1=stale prenos
#define DATA4094      A1  //4094 data           normalne data, co jinyho
#define CLK4094       A2  //4094 clock          nabezna vsunuje
#define OE4094        A3  //4094 output enable  1=povoleni
#define MEA_RELAY     A6
#define RES1          A7
#define SENSOR        2
#define RS485_DE      3
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
#define BUTTON1       5
#define BUTTON2       A7
#define LED1          12
#define LED2          11

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
#define IS_BUTTON1()    (digitalRead(BUTTON1)!=LOW)
#endif //BUTTON1
#ifdef BUTTON2
#define IS_BUTTON2()    (digitalRead(BUTTON2)!=LOW)
#endif //BUTTON2

//modbus mapovani
uint16_t modbus_data[16]; //blok modbus dat
#define inputs_status  modbus_data[0] //bits  0-15 stav vstupu
#define relays_status  modbus_data[1] //birs 16-31 stav relat

#define pinOutput(pin_id,value) {pinMode(pin_id,OUTPUT);digitalWrite(pin_id,value);} //makro pro nastaveni pinu(pin_id) jako vystup a inicializace na pozadovanou hodnotu (value).

void io_setup(void) {
  //watchdog
  pinOutput(WDOG,LOW); //je to vystup, dame do nuly
  WDOG_PULSE(); //a hnedle si pusneme
  //rizeni relatek
  analogReference(INTERNAL); //nastavime interni referenci, u 385 je to 1.1V
  analogRead(MEA_RELAY); 
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
  pinOutput(STR4094,LOW);
  pinOutput(STR4094,LOW);
  pinOutput(DATA4094,LOW);
  pinOutput(CLK4094,LOW);
  pinOutput(OE4094,LOW);
}

//----------------------------------------------------------------
// Dallas 1wire

OneWire wire1(WIRE1); //jednodrat na urceny drat
DallasTemperature ds18b20(&wire1); // vytvoření instance senzoryDS z knihovny DallasTemperature

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
  r=0;
  do {    
    if (IS_BIN_READ()) r|=1; //pokud je tam jednicka, tak sup s tim na vystup
    if ((--b)==0) { //pokud uz mame vsechno
      STR4094_0(); //rusime buzeni      
      return r; //a vracime vysledek
    }
    CLK4094_PULSE(); //cukneme hodinama    
    r<<=1; //udelam si misto na dalsi bitik
  } while (1);   
}

///Nahození jednoho výstupu.
///@param id číslo výstupu, víc jak CHAIN4094_LENGTH znamená všechno do nuly
void set4094(uint8_t id) {
  uint8_t b;

  b=CHAIN4094_LENGTH; //tolik ma retizek posuvaku bitu
  do {
    DATA4094_0(); //vetsinou tam je nula
    if (b==id) DATA4094_1(); //vyjimecne jednicka
    CLK4094_PULSE(); //cukneme hodinama    
  } while ((--b)!=0); //udelame vsechny bitky posuvaku
  DATA4094_0(); //pro formu tam na zaver napereme nulu
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

uint16_t relays_change;
uint8_t relays_timer;

#define RELAY_QUIET       0x00  //nic se nedeje
#define RELAY_DRIVE       0x01  //budime relata
#define RELAY_DRIVE_TIME  20 //jak dlouho budime [ms]

#define NO_RELAY          0
#define RELAY_GROUP_A     (NO_RELAY)
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
  relays_drive(NO_RELAY);  //nebudime
  relays_timer=0; //a pripravime se do akce
}

uint8_t relays_tick() {
  uint16_t timer,mask,st;
  uint8_t i;
    
  timer=relays_timer; //zlokalnim si to
  if (timer==0) { //pokud se nic nedeje, tak je moznost pripadne neco zacit
    st=relays_change; //zlokalnim si pozadavky na zmenu
    if (st==0) return RELAY_QUIET; //pokud zadna pozadovana zmena, tak nic
    //TODO: zmerit napeti na relatech, pokud malo, tak lsus
    mask=1; //maska
    i=0; //idcko    
    while ((st&mask)==0) { //pokud maska nemaskuje
      mask<<=1; //soupneme masku
      i++; //a cukneme s idckem
    }
    relays_change=st&(~mask); //schodim bitika a slus
    set4094(i); //naposunujeme jednicku do prislusneho bitiku
    i=RELAY_GROUP_A; //zatim skupina A
    if ((relays_status&mask)!=0) i++; //pokud B, tak preci B
    relays_drive(i); //a budime
  }
  timer+=TICK_MS; //posrcime timeeera
  if (timer>=RELAY_DRIVE_TIME) { //pokud uz jsme dobudili
    relays_drive(NO_RELAY); //deaktivace buzeni vsech relat
    clr4094(); //koncim buzeni
    timer=0; //zastavuju casovadlo
  }
  relays_timer=timer; //nova hodnota casovadla
  return RELAY_DRIVE;
}

//----------------------------------------------------------------
// Ovladac vstupu

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
    f=inputs_flt[i]; //nabereme si aktualni stav filtru
    if (f==0x7F) { //pokud je to nabezna hrana
      st|=mask; //nahod bitika
    } else if (f==0x80) { //pokud je to sestupna
      st&=~mask; //schodime bitika
    }
    if ((mask<<=1)==0) { //cukneme maskou a pokud je to nula, tak uz mame hotovo
      inputs_status=st; //novy stav
      return; //a koncime
    }
    i++;    
  } while (1);
}

void inputs_tick(uint8_t relaysst) {
  uint8_t i;
  uint16_t st;
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
    input_flt_proc(i,st&1); //proved proceduru    
    if (i==(CHAIN4094_LENGTH-1)) { //pokud uz to je vsecho
      inputs_status_update(); //aktualizujeme bitovy stav vstupu
      return; //schluss
    }
    st>>=1; //a jdeme na dalsi bitik
    i++; //dalsi    
  } while (1);
}

//----------------------------------------------------------------
// MODBUS

ModbusRTUSlave rtu(0x01, &Serial, RS485_DE); //inicilaizace objektu s modbusem

void modbus_init(void) {
  rtu.addBitArea(0x1000, (u8*)modbus_data, 32); //prvnich 32 bitiku jsou bitovy promenny
  rtu.addWordArea(0x2000, (u16*)modbus_data, sizeof(modbus_data)/sizeof(uint16_t)); //a jako word je pristupny komplet cely pole
  rtu.begin(9600);
}

void modbus_proc() {
  uint16_t relays;

  relays=relays_status; //schovam si posledni znamy stav relat
  rtu.process(); //procedura modbus
  relays^=relays_status; //zjistime si zmeny, jako ze neco prislo modbusem
  relays_change|=relays; //prihodime zmeny  
}

//----------------------------------------------------------------
// Odvozene udalosti pro zakladni kooperativni multitasking.

void ev_init(void) {
  io_setup(); //zakladni inicializace IO nozek
  init4094(); //inicializace retezu posuvnych registru  
  relays_init(); //inicializace relat  
  //u8g2.begin(); //Inicializace dipleje
  //display_proc(0);  
  modbus_init(); //start modbus rozhrani
}

void ev_proc(void) { 
  modbus_proc();
}

void ev_tick(void) {
  inputs_tick(relays_tick()); //na retezu 4094 je treba se stridat, takze vstupy musi vedet, co delaj relata
}

void ev_sec(void) {
}

void ev_min(void) {  
}

//----------------------------------------------------------------
// Hlavni Arduino udalosti, na ktery se zavesime

uint8_t next_tick; //systemova milisekunda
uint8_t secct; //pocitadlo pro generator vterin
uint8_t minct; //pocitadlo pro generator minut
uint8_t flags; //priznaky

#define FL_SEC 0x01 //priznak vteriny
#define FL_MIN 0x02 //priznak minuty

void setup() {  
  ev_init(); //udalost inicializace
  next_tick=(uint8_t)millis(); //inicializace casovani
  secct=0; //start generatoru vteriny
  minct=0; //start generatoru minuty
}

void loop() {
  if ((((uint8_t)millis())-next_tick)>=TICK_MS) { //pokud nastal cas tick
    next_tick+=TICK_MS; //tak se pochystame na prichod dalsi tick
    ev_tick(); //volame udalost
    if ((++secct)>=(1000/TICK_MS)) { //pocitame vterinu      
      flags|=FL_SEC; //nahodime priznak vteriny
      secct-=1000/TICK_MS; //a zase znova      
      if ((++minct)>=60) { //pocitame minutu  
        flags|=FL_SEC; //nahodime priznak minuty
        minct=0;
      }
    }    
    return; //bylo tik, takze konec
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
  WDOG_PULSE(); //kopneme do cokla
  ev_proc(); //neni nic jinyho na praci, jedem procedury  
}
