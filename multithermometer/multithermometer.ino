#include <OneWire.h>              //http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
#include <DallasTemperature.h>    //https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <U8x8lib.h>              //https://github.com/olikraus/u8g2
#include <EEPROM.h>


//----------------------------------------------------------------
// Zadratovani

#define PIN_1WIRE   3 //1wire teplomery
 
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
    if ((flags&CHAR_ECHO)!=0) s->write(c); //pokud mame echo, tak echujeme
  }
  return 0;
proc_new_line:
  flags|=c; //modifikace priznaku
  if ((flags&LINE_ECHO)!=0) showbuf();
  return 1; //oznamujeme, ze mame
}



//----------------------------------------------------------------
// Dallas 1wire

OneWire wire1(PIN_1WIRE);
// vytvoření instance senzoryDS z knihovny DallasTemperature
DallasTemperature ds18b20(&wire1);

//----------------------------------------------------------------
// Uzivatelske rozhrani

SimpleTerminal st; //vsichi to tak v arduinu delaji, tak to taky tak budeme pachat
#define TEMP_REC_SIZE   32  //delka zaznamu
#define TEMP_POS_ADDR   0   //pozice adresy
#define TEMP_LEN_ADDR   8   //delka adresy
#define TEMP_POS_ORDER  8   //poradi na displeji
#define TEMP_POS_NAME   12  //pocatek jmena, max do konce zanznamu

int look4addr(unsigned char* addr) {
  int j;
  for (unsigned int i=0;i<EEPROM.length();i+=TEMP_REC_SIZE) {
    j=0; //projizdime od zacatku
    while (EEPROM[i+j]==addr[j]) { //pokud se dilci cast adresy shoduje
      if (++j==TEMP_LEN_ADDR) return j; //tak jdeme na dalsi a pokud je stejny vsechno, vracime nalezeny index
    }
  }
  return -1;  
}

void ui_init(void) {
  Serial.begin(9600); //startujeme seriak  
  st.begin(&Serial,SimpleTerminal::POST_CR|SimpleTerminal::POST_LF|SimpleTerminal::PRE_CR|SimpleTerminal::PRE_LF|SimpleTerminal::CHAR_ECHO|SimpleTerminal::LINE_ECHO); //startujeme lidske rozhrani na seriaku  
  st.println(F("Startujem"));
}

void ui_list(int flags) {
  int id=0;
  st.println(F("LISTECEK"));
  st.pre_crlf();
  st.s->println(F("id  1WIRE ADDR        DP name\r\n" \
                  "--- ---------------- --- --------------------"));    
  for (unsigned int i=0;i<EEPROM.length();i+=TEMP_REC_SIZE) {     
    if ((flags==0)&&(EEPROM[i+TEMP_POS_ORDER]==0)) break; //pokud neukazumeme vsechno, tak preskakujeme prazdny
    st.printuint(++id);
    st.printspace();
    for (unsigned int j=TEMP_POS_ADDR;j<(TEMP_POS_ADDR+TEMP_LEN_ADDR);j++) st.printuinthex(EEPROM[i+j]);
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

void ui_proc(void) {
  if (st.proc()==0) return; //pokud se nic nedeje, tak koncime
  String line=st.getstring(); //nabereme vstup v podobe retezce
  if (line.equalsIgnoreCase(F("help"))) {
    st.println(F("HELPICEK"\
               "\r\n--------"));    
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
  } else if (line.equalsIgnoreCase(F("format"))) {
    st.println(F("format in progress..."));
    for (unsigned int i=0;i<EEPROM.length();i++) EEPROM[i]=0; //projedem celou eeprom a vymazeme to    
    st.println(F("format done"));
  } else {
    st.println(F("Unknown command"),&line);
  } 
  st.clrbuf(); //mazeme bufik a tim zahajime novy prijem
}

//----------------------------------------------------------------
// Hlavni udalosti arduina

void setup() {  
  ui_init(); //startujeme uzivatelske rozhrani
}

void loop() {  
  ui_proc();  //procedurka uzivatelskeho rozhrani
}
