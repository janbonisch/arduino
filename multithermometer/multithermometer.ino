#include <Wire.h>
#include <OneWire.h>              //http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
#include <DallasTemperature.h>    //https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <LiquidCrystal_I2C.h>    //https://www.makerguides.com/character-i2c-lcd-arduino-tutorial/
#include <EEPROM.h>

const char fw_ver[]="v1.0";

//----------------------------------------------------------------
// Zadratovani a dalsi konstanty

#define PIN_RS485     2 //rizeni smeru rs485
#define PIN_1WIRE     3 //1wire teplomery
#define PIN_KEY_UP    4 //cudl nahoru
#define PIN_KEY_DOWN  5 //cudl dolu

#define TEMP_REC_SIZE   32  //delka zaznamu
#define TEMP_POS_ADDR   0   //pozice adresy
#define TEMP_LEN_ADDR   8   //delka adresy
#define TEMP_POS_ORDER  8   //poradi na displeji
#define TEMP_POS_NAME   12  //pocatek jmena, max do konce zanznamu

#define rs485_tx()  digitalWrite(PIN_RS485,HIGH)  //zahajeni vysilani
#define rs485_rx()  digitalWrite(PIN_RS485,LOW)   //zahajeni prijmu

//----------------------------------------------------------------
// Jednoduchy radkovy terminal na streamu

 class SimpleTerminal {
  private:
    
    char line[81]; //vyrovnavaci pamet, posledni znak bude vzdy 0
    unsigned char lpos; //pozice v bufiku
    unsigned char flags;  //priznaky    
      
  public: 
    static const int SKIP_CR   =0x01;
    static const int SKIP_LF   =0x02;
    static const int CHAR_ECHO =0x04;
    static const int LINE_ECHO =0x08;
    static const int PRE_CR    =0x10;
    static const int PRE_LF    =0x20;
    static const int POST_CR   =0x40;
    static const int POST_LF   =0x80;

    Stream *s;  //proud, odkud bereme a kam posilame znaky
    void pre_crlf(void);
    void post_crlf(void);
  
    void begin(Stream *s, int flags); //zahajeni prace, neni to konstruktor, ale delaj to tak
    void clrbuf(void); //inicializace bufiku
    void showbuf(void); //ukaz bufik
    void println(char* str, int len); //tisk jednoho radku
    void println(const __FlashStringHelper* str); //tisk konstantiho retezce
    void println(char* str);
    void println(const char* str);
    void println(const __FlashStringHelper* s1, String* s2);
    void print(char* str);
    void print(const __FlashStringHelper* str);
    void printspace(void);
    void printuint(unsigned int d);
    void printuinthex(unsigned int d);
    void println(void);
    int proc(void); //pracovni procedure. Pokud vraci 0, tak se nic nedeje, nenula ze mame celej radek v bufiku
    String getstring(void); //vraci prijaty radek jako retezec
    char* getchars(void); //vraci prijaty radek jako obyc cckovy retezec
    int parseInt(int pos);
};

void SimpleTerminal::begin(Stream *s, int flags) {
  this->s=s; //nastavime si odkaz na proud, odkud bereme znaky
  this->flags=flags&(CHAR_ECHO|LINE_ECHO|PRE_CR|PRE_LF|POST_CR|POST_LF); //priznaky, jen ty co nas opravdu zajimaji
  clrbuf(); //a provedeme inicializaci
}

void SimpleTerminal::clrbuf(void) {
  this->lpos=0; //ukazovatko na zacatek
  memset(line,0,sizeof(line)); //a pro sichr to snulujeme kompleto
}

void SimpleTerminal::showbuf(void) {
  println(line,lpos); //pokud je radkove echo, tak ho provedeme
}

String SimpleTerminal::getstring(void) {
  String s=line; //z pole znaku udelame retezec
  return s; //a vracime vysledek snazeni
}

char* SimpleTerminal::getchars(void) {
  return line;
}

void SimpleTerminal::pre_crlf(void) {
  if (flags&PRE_CR) s->write(13);
  if (flags&PRE_LF) s->write(10);
}

void SimpleTerminal::post_crlf(void) {
  if (flags&POST_CR) s->write(13);
  if (flags&POST_LF) s->write(10);
}

void SimpleTerminal::print(char* str) {
  s->print(str);
}

void SimpleTerminal::print(const __FlashStringHelper* str) {
  s->print(str);
}

void SimpleTerminal::println(const __FlashStringHelper* str) {
  pre_crlf();
  s->print(str);
  post_crlf();  
}

void SimpleTerminal::println(const __FlashStringHelper* s1, String* s2) {
  pre_crlf();
  s->print(s1);
  s->print(F(" "));
  s->print(*s2);
  post_crlf();  
}

void SimpleTerminal::println(char* str, int len) {
  pre_crlf();
  s->write(str,len);
  post_crlf();
}

void SimpleTerminal::println(char* str) {
  pre_crlf();
  s->print(str);
  post_crlf();
}

void SimpleTerminal::println(const char* str) {
  pre_crlf();
  s->print(str);
  post_crlf();
}

void SimpleTerminal::printspace(void) {
  s->print(F(" "));
}

void SimpleTerminal::println(void) {
  s->println();
}

void SimpleTerminal::printuint(unsigned int d) {
  if (d<10) printspace();  
  if (d<100) printspace();
  s->print(d,DEC);
}

void SimpleTerminal::printuinthex(unsigned int d) {
  s->print(d>>4,HEX);
  s->print(d&15,HEX);
}

int SimpleTerminal::parseInt(int pos) {
  int r=0;
  while (pos<lpos&&(!isDigit(line[pos]))) pos++;
  while (pos<lpos&&(isDigit(line[pos]))) {
    r*=10;
    r+=line[pos++]-0x30;
  }
  return r;
}

int SimpleTerminal::proc(void) {  
  unsigned char c;

  rs485_rx(); //prijem
  while (s->available()!=0) { //dokud mame neco ke cteni
    c=s->read(); //nacteme znak
    switch (c) {
      case 27: //ESC
        clrbuf();
        break;
      case 13: //CR
        if ((flags&SKIP_CR)==0) { //pokud neni ignorace, tak to zpracujeme
          c=SKIP_LF;  //pokud bude nasledovad lf, tak ho ignorujeme
          goto proc_new_line; //dokoncime prijem celeho radku
        }
        break;
      case 10: //LF
        if ((flags&SKIP_LF)==0) { //pokud neni ignorace, tak to zpracujeme
          c=SKIP_CR;  //pokud bude nasledovad cr, tak ho ignorujeme
          goto proc_new_line; //dokoncime prijem celeho radku
        }
      case 9: //delete
        if (lpos>0) line[--lpos]=0; //pokud je co, tak mazeme znak
        break; //a doufame, ze to terminal v pripade echo taky smaze
      case 8: //tab predelame na mezeru
        c=20; //a naschval pokracujeme bez break
      default:
        if ((isPrintable(c))&&(lpos<(sizeof(line)-1))) line[lpos++]=(char)c; //pokud je to nejakej rozumnej znak a vejde se do bufiku, tak trada s tim tam
        break;
    }
    flags&=~(SKIP_CR|SKIP_LF); //zahodime priznaky detektoru dvojznaku <CR><LF> resp. <LF><CR>
    if ((flags&CHAR_ECHO)!=0) {
      rs485_tx();
      s->write(c); //pokud mame echo, tak echujeme
    }
  }
  return 0;
proc_new_line:
  flags|=c; //modifikace priznaku
  if ((flags&LINE_ECHO)!=0) {
    rs485_tx();
    showbuf();
  }
  return 1; //oznamujeme, ze mame
}

//----------------------------------------------------------------
// Ruzne

///Čtení eepromky do paměti
///@param mem kam to čteme
///@param eeprom_addr adresa v eepromce
///@param ct počet
void readFromEEprom(uint8_t* mem, int eeprom_addr, int ct) {
  do {
    *mem++=(uint8_t)EEPROM[eeprom_addr++];
  } while ((--ct)!=0);
}

//----------------------------------------------------------------
// RS485

void rs485_init(void) {
  pinMode(PIN_RS485,OUTPUT); //je to vystup
  rs485_rx(); //a pochystame prijem
}

//----------------------------------------------------------------
// Dallas 1wire

OneWire wire1(PIN_1WIRE); //jednodrat na urceny drat
DallasTemperature ds18b20(&wire1); // vytvoření instance senzoryDS z knihovny DallasTemperature

//----------------------------------------------------------------
// Display

#define MAX_LINES 32
int display_line[MAX_LINES]; //pole se serazenejma radkama
#define DISPLAY_LINES 4

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x3F, 20, 4); // Change to

void ScanI2C(SimpleTerminal* st) {
  byte error, address;
  int nDevices;
  
  st->println(F("Scanning I2C bus ..."));
  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      st->print(F("I2C device found at address 0x"));
      st->printuinthex(address);
      st->println();
      nDevices++;
    }
    else if (error == 4) {
      st->print(F("Unknown error at address 0x"));
      st->printuinthex(address);
      st->println();
    }
  }
  if (nDevices == 0) {
    st->println(F("No I2C devices found\n"));
  } else {
    st->println(F("done\n"));
  }    
}

void display_init(void) {
  lcd.init();
  lcd.backlight();  
  lcd.setCursor(0, 0);
  //           01234567890123456789  
  lcd.print(F("********************"));
  lcd.setCursor(0, 1);
  //           01234567890123456789
  lcd.print(F("* Multithermometer *"));
  lcd.setCursor(0, 2);
  //         01234567890123456789
  lcd.print(F("********************"));
  lcd.setCursor(21-sizeof(fw_ver), 3);
  //         01234567890123456789
  lcd.print(fw_ver);  
}

void display_show(int pos) {
  unsigned int i,addr,p;
  char bufik[32];
  
    
  for (i=0;i<DISPLAY_LINES;i++) {
    lcd.setCursor(0, i);
    addr=display_line[pos++];        
    maketext(bufik,addr,1); //vyrob popis mericiho mistecka
    p=strlen(bufik);
    while (p<20) bufik[p++]=' ';
    bufik[20]=0;    
    lcd.print(bufik);
    addr+=TEMP_REC_SIZE;
  }
}

//----------------------------------------------------------------
// cudliky

int display_pos; //od jake pozice zobrazujeme
uint8_t flt_ku;
uint8_t flt_kd;

void display_show(void) { //ukaz to na displeji
  display_show(display_pos); //ukaz to na displeji
}

int key_proc1(int pin, uint8_t* flt) {
  uint8_t f;

  f=*flt; //vezmeme filtr
  if (digitalRead(pin)==LOW) { //pokud je vstup log.0
    f<<=1; //narotujeme 0        
  } else { //pokud je v 1
    f=(f<<1)|1; //pereme tam jednicku
  }
  *flt=f; //novy stav filtru
  if ((f&0x0F)==0x08) {
    return 1;
  }
  return 0;  
}

void key_proc(void) {
  if (key_proc1(PIN_KEY_UP,&flt_ku)) { //provedeme filtr cudlu nahoru a pokud se fakt neco deje
    if (display_pos>0) display_pos--; //pokud je kam coufat, tak uber
    display_show(); //ukaz to na displeji
  }
  if (key_proc1(PIN_KEY_DOWN,&flt_kd)) { //provedeme filtr cudlu dolu a pokud se fakt neco deje    
    if (display_pos<(MAX_LINES-DISPLAY_LINES-1)) display_pos++; //pridame pokud jsme alespon teoreticky v rozsahu   
    while ((display_pos>0)&&(display_line[display_pos+DISPLAY_LINES-1]<0)) display_pos--; //upravime s ohledem na polozky na displeji
    display_show(); //ukaz to na displeji
  }  
}

void key_init(void) {
  pinMode(PIN_KEY_UP,INPUT);
  pinMode(PIN_KEY_DOWN,INPUT);
  digitalWrite(PIN_KEY_UP,HIGH);
  digitalWrite(PIN_KEY_DOWN,HIGH);
}

//----------------------------------------------------------------
// Uzivatelske rozhrani

SimpleTerminal st; //vsichi to tak v arduinu delaji, tak to taky tak budeme pachat

///Čtení adresy do paměti
///@param mem kam načíst
///@param eeprompos adresa v eeprom
void readAddrFromEEprom(uint8_t* mem, int eeprompos) {
  readFromEEprom(mem,eeprompos,TEMP_LEN_ADDR);
}

///Zobrazení 1wire adresy
void ui_show_1wire(uint8_t* addr) {
  for (unsigned int j=0;j<8;j++) st.printuinthex(*addr++);
}

void ui_show_1wire_eeprom(int eeprom_addr) {
  uint8_t addr[TEMP_LEN_ADDR];

  readAddrFromEEprom(addr,eeprom_addr);
  ui_show_1wire(addr);  
}

///Vyrobí popis měřícího místa, tj. aktuální teplotu a text
///@param buffer kam to budeme tisknout, alespon 21 znaku
///@param eeprompos adresa zaznamu v eeprom (bacha ne index)
void maketext(char* buffer, int eeprompos, int mode) {
  uint8_t addr[TEMP_LEN_ADDR];
  float tempC;
  int t;
  char c;

  if (eeprompos<0) {
    *buffer=0;
    return;
  }
  readAddrFromEEprom(addr,eeprompos);  //vytahneme adresu teplomeru  
  //ds18b20.
  tempC=ds18b20.getTempC(addr); //cteme aktualni teplotu, je dost vhodny pred zobrazenim vyslat celkovy povel
  t=(int)tempC;
  if ((t>-85)&&(t<85)) { // umime teploty jen od -99 do +99
    c=' '; //kladna teplota ma na zacatku mezeru
    if (t<0) { //pokud je pod nulou
      c='-'; //tak tam bude minus
      t=-t; //a preklopime znaminko
    }
    *buffer++=c; //znaminko
    *buffer++='0'+t/10; //desitky
    *buffer++='0'+t%10; //jednotky
    if (mode>0) {
      int t=((int)(tempC*100))%100;
      *buffer++='.'; //oddelovac
      *buffer++='0'+t/10; //desetiny
      if (mode>1) *buffer++='0'+t%10; //setiny    
    }
  } else { //teplota mimo misu, tak to je asi v cudu teplomer
    mode+=3; //aspon 3 znaky budou vzdy
    if (mode>3) mode++; //nad tri znaky je tam jeste desetinna tecka, tak ji prihodim    
    memcpy(buffer,"error!  ",mode); //praskneme tam kus textu
    buffer+=mode; //a postrcime se za nej
  }
  *buffer++=' '; //oddelovac
  readFromEEprom((uint8_t*)buffer,eeprompos+TEMP_POS_NAME,TEMP_REC_SIZE-TEMP_POS_NAME);
  buffer[TEMP_REC_SIZE-TEMP_POS_NAME]='\0';  
}

///Hledá adresu v předvolbách
///@param addr hledaná adresa
///@param adresa záznamu v eepromce
int look4addr(uint8_t* addr) {
  int j;
  for (unsigned int i=0;i<EEPROM.length();i+=TEMP_REC_SIZE) {
    j=0; //projizdime od zacatku
    while (EEPROM[i+j]==addr[j]) { //pokud se dilci cast adresy shoduje
      if (++j==TEMP_LEN_ADDR) return i; //tak jdeme na dalsi a pokud je stejny vsechno, vracime nalezeny index
    }
  }
  return -1;  
}

//Hledani zaznamu podle pozice
//@param start od jake pozice hledame
//@param index na nasledujici polozku, -1 pokud uz nemame
int look4nextdp(int start) {
  int best,bestdist,pos,x;

  best=-1; //zatim nemame nejlepsi polozku
  bestdist=999; //a vzdalenost je taky uplne maximalni  
  for (unsigned int i=0;i<EEPROM.length();i+=TEMP_REC_SIZE) { //prosvistime pameti
    pos=EEPROM[i+TEMP_POS_ORDER]; //vezmeme pozici ulozenou v eeprom
    if (pos>start) { //pokud to ma cenu zkoumat, jdeme do nej (mensi nez uvodni nas vubec nezajimaji totiz vubec nezajimaji)
      x=pos-start; //spocteme vzdalenost od pocatku
      if (x<bestdist) { //novy kandidat na nejlepsi polozku
         bestdist=x; //poznamenam si vzdalenost nejlepsiho
         best=i; //a pozici polozky v eepromce
      }      
    }    
  }
  return best; //vracime nejlepsi vysledek
}

///Vyrobi pole se serazenejma polozkam
void make_display_lines(void) {
  int i,dp,addr;
    
  dp=0; //zaciname prvni pozici  
  for (i=0;i<MAX_LINES;i++) {    
    addr=look4nextdp(dp); //zkusime najit vhodnyho kandidata
    if (addr<0) break;
    display_line[i]=addr; //ulozim do policka
    dp=EEPROM[addr+TEMP_POS_ORDER]; //jeho displaypos je pristi nejlepsi
  }
  for (;i<MAX_LINES;i++) display_line[i]=-1; //zbytek pole vygumujeme
  //st.println(F("Make lines:"));for (i=0;i<MAX_LINES;i++) {st.printuint(i);st.printspace();st.printuint(display_line[i]);st.println();}
}

//Zobrazeni aktualnich dat
void ui_show(void) {
  int dp,addr;
  char bufik[32];
   
  dp=0; //zaciname od nejnizsiho
  st.pre_crlf();
  do { //vyskok uprostred
    addr=display_line[dp++]; //vezmu si adresu z tabulky
    if (addr<0) return; //pokud konec, tak konec
    maketext(bufik,addr,2); //vyrob popis mericiho mistecka
    st.print(bufik); //posleme to na displej    
    st.println();
  } while (1); //vyskok uprostred
  st.post_crlf();
}

///Inicializace uživatelského rozhraní
void ui_init(void) {
  Serial.begin(9600); //startujeme seriak    
  st.begin(&Serial,SimpleTerminal::POST_CR|SimpleTerminal::POST_LF|SimpleTerminal::PRE_CR|SimpleTerminal::PRE_LF|SimpleTerminal::LINE_ECHO); //startujeme lidske rozhrani na seriaku  
  // http://patorjk.com/software/taag/#p=display&f=Small&t=MultiThermometer
  st.print(F("\r\n  __  __      _ _   _ _____ _                                _"\
             "\r\n |  \\/  |_  _| | |_(_)_   _| |_  ___ _ _ _ __  ___ _ __  ___| |_ ___ _ _"\
             "\r\n | |\\/| | || | |  _| | | | | ' \\/ -_) '_| '  \\/ _ \\ '  \\/ -_)  _/ -_) '_|"\
             "\r\n |_|  |_|\\_,_|_|\\__|_| |_| |_||_\\___|_| |_|_|_\\___/_|_|_\\___|\\__\\___|_|     "));
  st.print((char*)fw_ver);
  st.println();  
  delay(100);
}


void ui_list(int flags) {
  int id=0;
      
  st.pre_crlf();
  st.s->println(F("id  1WIRE ADDR        DP name\r\n" \
                  "--- ---------------- --- --------------------"));    
  for (unsigned int i=0;i<EEPROM.length();i+=TEMP_REC_SIZE) {     
    if ((flags==0)&&(EEPROM[i+TEMP_POS_ORDER]==0)) break; //pokud neukazumeme vsechno, tak preskakujeme prazdny
    st.printuint(++id);
    st.printspace();    
    ui_show_1wire_eeprom(i+TEMP_POS_ADDR);
    st.printspace();
    st.printuint(EEPROM[i+TEMP_POS_ORDER]);
    st.printspace();
    int j=TEMP_POS_NAME; //tady zacina jmeno      
    do { //smyce s vyskokem uprostred
      int c=EEPROM[i+j];
      if (c==0) break;
      if (isPrintable(c)) st.s->write(c);        
    } while ((++j)<TEMP_REC_SIZE);      
    st.println();
  }    
  st.post_crlf();
}

//Skenovani 1wire sbernice kombinovane s ulozenim vybraneho id.
//@param store_scanpos TODO
//@param store_id TODO
void ui_scan(int store_scanpos, unsigned int store_id) {
  uint8_t addr[8];
  int scanpos,id;
    
  wire1.reset_search(); //inicializace hledani
  scanpos=0;
  st.println(F("pos 1wireaddr        id\r\n" \
               "--- ---------------- ---"));    
  while (wire1.search(addr)) { //naval adresu
    scanpos++; //zvedneme ciselko
    if (store_scanpos>=0) { //pokud je to ukladaci rezim
      if (scanpos==store_scanpos) { //pokud je cislo skenu to co chceme ulozit) 
        id=store_id*TEMP_REC_SIZE; //z ID udelame offset
        if (id>=EEPROM.length()) { //pokud je to mimo misu
          st.println(F("Bad ID")); //drzkujeme
          return; //a koncime
        }        
        for (int i=0;i<TEMP_LEN_ADDR;i++) EEPROM[id++]=addr[i]; //naper adresu do zvolene predvolby
        st.println(F("Successfully stored"));
        return; //a slus
      }      
    } else { //ukazovaci rezim
      id=look4addr(addr); //mrkneme, zdali uz to mame v seznamu
      if ((store_id==0)||(id<0)) { //bud ukazujeme vsechno, nebo jen zatim neznamy
        st.printuint(scanpos); //ukaz poradi na 1wire
        st.printspace(); //oddelovac
        ui_show_1wire(addr); //1wire adresa    
        if (id>=0) st.printuint((id/TEMP_REC_SIZE)+1);        
        st.println();
      }
    }
  }    
  st.post_crlf();    
}

void ui_proc(void) {
  if (st.proc()==0) return; //pokud se nic nedeje, tak koncime
  String line=st.getstring(); //nabereme vstup v podobe retezce  
  rs485_tx();
  if (line.equalsIgnoreCase(F("help"))) {
    st.println(F("HELP"\
               "\r\n----"\
               "\r\nhelp            this help"\
               "\r\nshow            show actual data"\
               "\r\nlist            show used memory possition"\
               "\r\nlistall         show all memory possition"\
               "\r\nset id dp name  setup memory: id=memory id, dp=display position, name=name of thermometter"\
               "\r\nscan            look for a new thermometter(s) on 1wire bus (not used thermometter)"\
               "\r\nscanall         show all thermometter(s) connected on 1wire bus"\
               "\r\nstore pos id    store thermometter 1wire address to memory: pos=scan possition, id=memory id"\
               "\r\nformat          erase all memory possitions"\
               ));
  } else if (line.equalsIgnoreCase(F("list"))) {
    ui_list(0);
  } else if (line.equalsIgnoreCase(F("listall"))) {  
    ui_list(1);    
  } else if (line. startsWith(F("set"))) {    
    int s1=line.indexOf(F(" ")); //odelovac mezi set a id
    int s2=line.indexOf(F(" "),s1+1); //oddelovac mezi id a dp
    int s3=line.indexOf(F(" "),s2+1); //oddelovat mezi dp a name
    int id=st.parseInt(s1)-1; //vyprasime idcko
    int dp=st.parseInt(s2); //vyprasime dp
    if ((id<0)||(id>=(EEPROM.length()/TEMP_REC_SIZE))) { //pokud neni IDcko v poradku
      st.println(F("Bad ID")); //drzkujeme
    } else if (s2<0) { //pokud neni pozice na displeji
      st.println(F("DP is mandatory")); //drzkujeme
    } else { //vsechno ok, jdeme to zpracovat
      int i=id*TEMP_REC_SIZE; //z indexu udelam pozici v eeprom
      EEPROM[i+TEMP_POS_ORDER]=dp; //ulozim pozici na displeji
      if (s3>s2) { //pokud tam mame jmeno, tak ho preneseme
        for (int j=TEMP_POS_NAME;j<TEMP_REC_SIZE;j++) { //postupne prenesu jmeno
          EEPROM[i+j]=line[++s3]; //retezec je ukoncen nulou, za ni mohou bejt i kraviny, to mi neva
        }
      }
    }
    make_display_lines(); //prepocitame pole pro zobrazeni
  } else if (line.equalsIgnoreCase(F("format"))) {
    st.println(F("format in progress..."));
    for (unsigned int i=0;i<EEPROM.length();i++) EEPROM[i]=0; //projedem celou eeprom a vymazeme to    
    st.println(F("format done"));
  } else if (line.equalsIgnoreCase(F("scanall"))) {
    ui_scan(-1,0); //jenom to ukaz neznamy teplomery
  } else if (line.equalsIgnoreCase(F("scan"))) {
    ui_scan(-1,1); //ukaz uplne vsechno
  } else if (line.startsWith(F("store "))) {
    int s1=line.indexOf(F(" ")); //odelovac mezi store a scanpos
    int s2=line.indexOf(F(" "),s1+1); //oddelovac scanpos a id
    int scanpos=st.parseInt(s1); //vyprasime scanpos
    int id=st.parseInt(s2)-1; //vyprasime idcko
    ui_scan(scanpos,id); //a zkusime to ulozit
    make_display_lines(); //prepocitame pole pro zobrazeni
  } else if (line.equalsIgnoreCase(F("scani2c"))) {
    ScanI2C(&st);
  } else if (line.equalsIgnoreCase(F("show"))) {
    ui_show();
  } else {
    st.println(F("Unknown command"),&line);
  } 
  st.clrbuf(); //mazeme bufik a tim zahajime novy prijem
  delay(50);  
}

//----------------------------------------------------------------
// Hlavni udalosti arduina

unsigned long next_sec;

void setup() {  
  ds18b20.begin(); //inicializace dallas teplomeru  
  key_init(); //cudliky
  rs485_init(); //dalkova komunikace
  rs485_tx();
  display_init();   //displej  
  ui_init(); //startujeme uzivatelske rozhrani
  make_display_lines(); //vyrobime pole pro zobrazovani  
  next_sec=millis(); 
  /* //ladici dalsi jakoteplomery
  display_line[3]=display_line[0];
  display_line[4]=display_line[1];
  display_line[5]=display_line[2];
  display_line[6]=display_line[3];
  */
}

void loop() {  
  if ((millis()>next_sec)) { //pokud nastala vterina
    next_sec+=1000; //tak se pochystame na prichod dalsi vteriny
    ds18b20.requestTemperatures(); //chceme merit vsechno
    display_show(); //ukaz to na displeji
  }
  ui_proc();  //procedurka serioveho uzivatelskeho rozhrani
  key_proc(); //procedura cudliku  
}
