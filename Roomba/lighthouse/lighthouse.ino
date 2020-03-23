#include <EEPROM.h>

extern "C" {
  void rlh_send(void);    //function prototype for asm code
  uint8_t get_mask(void); //function prototype for asm code
}

/*

Some electrical engeneering ;-)
===========================

Ucc o------+           
           |    _  _      |
          ---   /| /|     |
 |        \ /  /  /       | Uf  
 |         V             \|/
 |         |              '
 | I       |           
 |        .-.             |       
_|_       | |             |
\ /       | |  R          | Ur
 '        '-'             |
           |             \|/
           |              '
pin o------+                 
                          |
                          | Ul
                         \|/
GND o------               '

Ur=Ucc-Uf-Ul  voltage on resistor
Ucc=5V  Power supply voltage (USB)
Ul=0,9V Arduino logical low level voltage (see documentation)
R=Ur/I

                     Uf[V]   Imax[mA]  I[mA] Ur[V] R[ohm]  R[ohm] shop
narrow beam IR LED   1.35    100       40    2.75  68.75   68     https://www.gme.cz/infra-led-5mm-tsal6100
wide meam IR LED     1.2     20        20    2.9   145     150    https://www.gme.cz/infra-led-3mm-l-934f3c

 */

volatile uint8_t  tick_data[8];    //port setup for 8 bits
/*

Roomba frame encoding
=====================

The frame consists  of 8 bits, the first is msb (big endian).
The Bit is consists of four quotters. In first quarter (q1) is the 
transmitter allways active. In quartes q2 and q3 transmitter activity 
depends of bit value. In last quarter (q4) is the transmitter allways 
inactive.
 ________ _________________          ________ _________________
/        X_________________\________/        X_________________\________ ...

|        |                 |        |        |                 |
|<--q1-->|<--q2-->|<--q3-->|<--q4-->|<--q1-->|<--q2-->|<--q3-->|<--q4--> ...
|<------------- bit 7 ------------->|<------------- bit 6 -------------> ...
 */

asm("                                                                ");
asm("        .equ    outport,0x05   ;port for IR transnit PORTB=0x05 ");
asm("        .equ    out_mask,0x1F  ;we will use bits 0 to 4         ");
asm("        .equ    halfpulse,65   ;length of half IR pulse         ");
asm("        .equ    numofpulses,38 ;number of pulses in a quarter   ");
asm("                                                                ");
asm("rlh_send_1q:                                                    ");
asm("        ;>r16 out port content                                  ");
asm("        ; r17 xor mask                                          ");
asm("        ;~r18,r19                                               ");
asm("                                                                ");
asm("        ldi   r19,numofpulses*2-1                               ");
asm("rlh_send_1q1:                                                   ");
asm("        eor  r16,r17                                            ");
asm("        out  outport,r16                                        ");
asm("        ldi  r18,halfpulse                                      ");
asm("        call rlh_send_1q2                                       ");
asm("        dec  r19                                                ");
asm("        brne rlh_send_1q1                                       ");
asm("        eor  r16,r17                                            ");
asm("        out  outport,r16                                        ");
asm("        ldi  r18,halfpulse-1                                    ");
asm("rlh_send_1q2:                                                   ");
asm("        dec r18                                                 ");
asm("        brne rlh_send_1q2                                       ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        ret                                                     ");
asm("                                                                ");
asm("rlh_send:                                                       ");
asm("        cli                        ;disable interupts           ");
asm("        in    r16,outport          ;take state of out port      ");
asm("        ldi   r30,lo8(tick_data)   ;address of data, low byte   ");
asm("        ldi   r31,hi8(tick_data)   ;address of data, high byte  ");
asm("        ldi   r20,8                ;we will send 8 bits         ");
asm("rlh_send_loop:                                                  ");
asm("        ldi   r17,out_mask         ;in q1 allways 1             ");
asm("        call  rlh_send_1q          ;send q1                     ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        ld    r17,z                ;take a bit                  ");
asm("        call  rlh_send_1q          ;send as q2                  ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        ld    r17,z+               ;take a bit                  ");
asm("        call  rlh_send_1q          ;send as q3                  ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        nop                                                     ");
asm("        clr   r17                  ;in q4 allwais 0             ");
asm("        nop                                                     ");
asm("        call  rlh_send_1q          ;send q4                     ");
asm("        dec   r20                  ;decrement bit counter       ");
asm("        brne  rlh_send_loop        ;again if remain             ");
asm("        sei                        ;enable interrupts           ");
asm("        ret                        ;das ist alles               ");
asm("                                                                ");
asm("get_mask:                                                       ");
asm("        ldi   r24,out_mask         ;take out_mask to return reg.");
asm("        ret                        ;return                      ");
asm("                                                                ");

//===========================================================================

uint8_t flags; //application setup
unsigned long disable_communication; //timestamp for disable communication

#define PIN_RS485           2 //rs485 control pin
#define rs485_tx()          digitalWrite(PIN_RS485,HIGH)  //tx enable
#define rs485_rx()          digitalWrite(PIN_RS485,LOW)   //tx disable
#define MASK_RED_BUOY       0x01
#define MASK_FENCE          0x02
#define MASK_GREEN_BUOY     0x04
#define MASK_FORCE_FIELD1   0x08
#define MASK_FORCE_FIELD2   0x10
#define MASK_ALL            (MASK_RED_BUOY|MASK_FENCE|MASK_GREEN_BUOY|MASK_FORCE_FIELD1|MASK_FORCE_FIELD2)
#define EEPROM_SETUP_ADDR   0
#define FL_ID_MASK          0x0F
#define FL_WFA              0x10
#define FL_MODE_SHIFT       6
#define FL_MODE_MASK        (3<<(FL_MODE_SHIFT))
#define FL_MODE_NOP         (0<<(FL_MODE_SHIFT))
#define FL_LIGHTHOUSE       (1<<(FL_MODE_SHIFT))
#define FL_LIGHTHOUSE_WALL  (2<<(FL_MODE_SHIFT))
#define FL_ALL_WALL         (3<<(FL_MODE_SHIFT))
// http://www.robotreviews.com/sites/default/files/IRobot_Roomba_500_Open_Interface_Spec.pdf
// IR Remote Control
#define ROOMBA_LEFT       129 //Left 
#define ROOMBA_FORWARD    130 //Forward 
#define ROOMBA_RIGHT      131 //Right 
#define ROOMBA_SPOT       132 //Spot 
#define ROOMBA_MAX        133 //Max 
#define ROOMBA_SMALL      134 //Small 
#define ROOMBA_MEDIUM     135 //Medium 
#define ROOMBA_LARGE      136 //Large / Clean 
#define ROOMBA_STOP       137 //Stop 
#define ROOMBA_POWER      138 //Power 
#define ROOMBA_ARC_LEFT   139 //Arc Left 
#define ROOMBA_ARC_RIGHT  140 //Arc Right 
#define ROOMBA_STOP2      141 //Stop  
// Scheduling Remote  
#define ROOMBA_DOWNLOAD   142 //Download 
#define ROOMBA_SEEK_DOCK  143 //Seek Dock 
// Roomba Discovery Driveon Charger 
#define ROOMBA_DDCH_RESERVED    240 //Reserved 
#define ROOMBA_DDCH_RED_BUOY    248 //Red Buoy 
#define ROOMBA_DDCH_GREEN_BUOY  244 //Green Buoy 
#define ROOMBA_DDCH_FORCE_FIELD 242 //Force Field 
// Roomba 500/600 Drive-on Charger
#define ROOMBA_DDCH_X00_RESERVED    160 //Reserved 
#define ROOMBA_DDCH_X00_FORCE_DIELD 161 //Force Field 
#define ROOMBA_DDCH_X00_GREEN_BUOY  164 //Green Buoy 
#define ROOMBA_DDCH_X00_RED_BUOY    168 //Red Buoy 
// Roomba 500 Virtual Wall
#define ROOMBA_VIRTUAL_WALL         162 //Virtual Wall 
/* 
 Roomba 500/600 Virtual Wall Lighthouse
 0LLLL0BB 
 LLLL = Virtual Wall Lighthouse ID
 (assigned automatically by Roomba 
 560 and 570 robots) 
 1-10: Valid ID 
 11: Unbound 
 12-15: Reserved 
 BB = Which Beam
 00 = Fence 
 01 = Force Field 
 10 = Green Buoy 
 11 = Red Buoy
*/
#define FENCE       0x00
#define FORCE_FIELD 0x01
#define GREEN_BUOY  0x02
#define RED_BUOY    0x03

//===========================================================================

void pfln(const __FlashStringHelper* text) {
  rs485_tx(); //enable rs485 transmitter
  Serial.println(text); //send data
}

void pfnln(const __FlashStringHelper* text, uint8_t num) {
  rs485_tx(); //enable rs485 transmitter
  Serial.print(text); //send prefix
  Serial.print(num); //send number
  Serial.println(); //send end of line  
}

//Proc one bit to tickdata
char proc_data(uint8_t c, uint8_t mask, volatile uint8_t* data) {
  uint8_t w;

  w=*data;                    //get the data from ram
  w&=~mask;                   //zero bit by mask
  if ((c&0x80)!=0) w|=mask;   //if is there 1, set bit by mask
  *data=w;                    //store new value to ram
  return c<<1;                //return input value with rotation  
}

//Prepare tickdata
void prepare_tick_data(uint8_t red_buoy, uint8_t fence, uint8_t green_buoy, uint8_t force_field1, uint8_t force_field2) {  
  for (volatile uint8_t* data=tick_data;data<&tick_data[8];data++) { //we have to send 8 bits for each transmitter
    red_buoy=proc_data(red_buoy,MASK_RED_BUOY,data);              //transmitter #1
    fence=proc_data(fence,MASK_FENCE,data);                       //transmitter #2
    green_buoy=proc_data(green_buoy,MASK_GREEN_BUOY,data);        //transmitter #3
    force_field1=proc_data(force_field1,MASK_FORCE_FIELD1,data);  //transmitter #4
    force_field2=proc_data(force_field2,MASK_FORCE_FIELD2,data);  //transmitter #5
    *data&=get_mask();                                            //mask only HW valid bits
  }  
}

//Prepare tickdata, the same for all transmitters
void prepare_tick_data1(uint8_t tr) {
  prepare_tick_data(tr,tr,tr,tr,tr); //all transmitters will send the same code
}


//Ir send procedure
void ir_send_proc(uint8_t count, uint8_t sleep) {
  digitalWrite(LED_BUILTIN, HIGH);
  do {   
    rlh_send(); //send it out    
    delay(sleep); //wait a while    
  } while ((--count)!=0);
  digitalWrite(LED_BUILTIN, LOW);      
}

//Prepare data for lighthouse with specified ID
void prepare_tick_light_house(uint8_t id) {
  id&=0x0F; //only 0 to 15
  id<<=3; //shift to wall id possition
  prepare_tick_data(id+GREEN_BUOY,id+FENCE,id+RED_BUOY,id+FORCE_FIELD,id+FORCE_FIELD); //prepare data for transmitters
}

//Enabe HW transmitter
void enable_tr(uint8_t mask) {
  PORTB |= get_mask(); //IR led not active
  DDRB=(DDRB&~(MASK_ALL&get_mask()))|(mask&MASK_ALL&get_mask()); //output mode for selected transmitters
}

void header(void) {
  pfln(F("  ___                _            _    _      _   _   _  _\r\n"\
         " | _ \\___  ___ _ __ | |__  __ _  | |  (_)__ _| |_| |_| || |___ _  _ ___ ___\r\n"\
         " |   / _ \\/ _ \\ '  \\| '_ \\/ _` | | |__| / _` | ' \\  _| __ / _ \\ || (_-</ -_)\r\n"\
         " |_|_\\___/\\___/_|_|_|_.__/\\__,_| |____|_\\__, |_||_\\__|_||_\\___/\\_,_/__/\\___|\r\n"\
         "                                         |___/                          v1.0\r\n"));
}

void help(void) {
  header();
  pfln(F(
    "?,h ... this help\r\n"\
    "/ ..... reset communication timeout\r\n"\
    "- ..... logout\r\n"\
    "\r\n"\
    "mode:\r\n"\
    "0 ..... quiet mode / remote control\r\n"\
    "1 ..... lighthouse\r\n"\
    "2 ..... lighthouse virtual wall\r\n"\
    "3 ..... wirtual wall\r\n"\
    "\r\n"\
    "remote control:\r\n"\
    "a ..... left\r\n"\
    "d ..... rigth\r\n"\
    "w ..... fofward\r\n"\
    "s ..... stop\r\n"\
    "z ..... home\r\n"\
    "x ..... wake up roomba with power command\r\n"\
    "\r\n"\
    "Setup device ID\r\n"\
    "A to I  set device id 1 to 9\r\n"\
    "\r\n"\
    "\r\n"));
}

void rc(uint8_t ch) {
  uint8_t code;
  
  switch (ch) { //create remote code for roomba
    case 'a': 
      pfln(F("left"));
      code=ROOMBA_LEFT;
      break;
    case 'd':
      pfln(F("right"));
      code=ROOMBA_RIGHT;
      break;
    case 'w':
      pfln(F("forward"));
      code=ROOMBA_FORWARD;
      break;
    case 's':
      pfln(F("stop"));
      code=ROOMBA_STOP;
      break;
    case 'z':
      pfln(F("home"));
      code=ROOMBA_SEEK_DOCK;
      break;      
    case 'x':
      pfln(F("Wake up roomba"));
      prepare_tick_data1(ROOMBA_POWER); //prepare data
      ir_send_proc(30,50);
      return;
    case 'X':
      PORTB ^=get_mask();
      return;
    default: //unknown command
      return; //do nothink
    }  
  enable_tr(MASK_ALL);  //enable all transmitters
  prepare_tick_data1(code); //prepare data  
  ir_send_proc(4,50); //send it out
}

//Show actual device ID
void show_id(void) {
  pfnln(F("Device id="),flags&FL_ID_MASK);
}

//Set new device ID
void set_id(uint8_t new_id) {
  if ((new_id>=1)&&(new_id<=10)&&(new_id!=(flags&FL_ID_MASK))) { //id can be in interval 1 to 10
    flags=((~FL_ID_MASK)&flags)|(new_id&FL_ID_MASK); //set new ID
    EEPROM[EEPROM_SETUP_ADDR]=flags; //store new configuration into eeprom    
  }
  show_id(); //show new ID
}

//Show actual device mode
void show_mode(void) {  
  switch (flags&FL_MODE_MASK) {
    case FL_LIGHTHOUSE:
      pfnln(F("Lighthouse id="),flags&FL_ID_MASK);
      break;
    case FL_LIGHTHOUSE_WALL:
      pfln(F("Lighthouse virtual wall"));
      break;
    case FL_ALL_WALL: 
      pfln(F("Virtual wall on all transmitters"));
      break;
    default: //in NOP mode 
      pfln(F("Remote control"));
      return;      
  }
}

//Set new device mode
void set_mode(uint8_t new_mode) {
  uint8_t old_mode;

  new_mode&=FL_MODE_MASK; //mask only mode bits
  old_mode=flags&FL_MODE_MASK; //take old mode
  if (old_mode!=new_mode) { //if mode changes, then   
    flags=(~FL_MODE_MASK&flags)|new_mode; //set new mode  
    EEPROM[EEPROM_SETUP_ADDR]=flags; //store new configuration into eeprom   
  } 
  show_mode(); //show new mode 
}

//Connect procedure
int try_connect(uint8_t id) {  
  if (id==0xFF) { //code for disconnect
    disable_communication=0; //disable communication
    return; //end for now
  }
  if ((id==0)||(id==(flags&FL_ID_MASK))) { //select this device if id=0 or if id match device id
    disable_communication=millis()+30000L; //enable communication form 30s
    header(); //show header
    show_id(); //show actual ID
    show_mode(); //show actual device mode
    pfln(F("? for help")); //and help for help ;-)
  }
}

//===========================================================================
// Arduino start and loop.

void setup() {
  enable_tr(MASK_ALL);  //enable all transmitters
  memset((void*)tick_data,0,sizeof(tick_data));  //Make sure the array is clear
  pinMode(PIN_RS485,OUTPUT); //this will be output pin
  rs485_rx(); //ready for receive
  Serial.begin(9600);
  flags=EEPROM[EEPROM_SETUP_ADDR]&(~FL_WFA); //read configuration from eeprom  
  try_connect(0xFF); //disable communication
}

void loop() {
  uint8_t ch;
  
  rs485_rx(); //enable rs485 receiver
  ch=0; //at first there is no char in uart
  if (Serial.available()>0) { //if there is a byte in uart buffer
    ch=Serial.read(); //read byte from uart
    if (ch=='*') { //address mode
      flags|=FL_WFA; //set wait for adrress flag, address in next byte
      return; //end for now
    }
    if ((flags&FL_WFA)!=0) { //if we are waiting for address
      flags&=~FL_WFA; //clear wait for adrress flag
      try_connect(ch-'0'); //try to connect with specified address
      return; //end for now
    }
    if (ch>0xF0) { //one byte connect mode (0xF0,0xF1,...)
      try_connect(ch-0xF0); //try to connect with specified address
      return; //end for now
    }
    if (millis()>disable_communication) ch=0; //if communication insn't enabled, then there are no data from uart    
  }
  switch (flags&FL_MODE_MASK) { //in what mode we work
    case FL_LIGHTHOUSE: //full light house
      enable_tr(MASK_ALL);  //enable all transmitters
      prepare_tick_light_house(flags); //make lighthouse codes
      goto modc;
    case FL_LIGHTHOUSE_WALL: //lighoutse hardware, only fence will send a code
      enable_tr(MASK_FENCE);  //enable only fence transmitter
      prepare_tick_data1(ROOMBA_VIRTUAL_WALL); //and it will transmit this code
      goto modc;
    case FL_ALL_WALL: 
      enable_tr(MASK_ALL);  //enable all transmitters
      prepare_tick_data1(ROOMBA_VIRTUAL_WALL); //all transmitter send this code
modc: ir_send_proc(1,1); //send it out without delay (short red led flash)
      delay(50); //some dly        
      break;
    default: //in NOP mode 
      rc(ch); //we can be a remote control
      break;
  }
  if ((ch>='0')&&(ch<='3')) { //0-3 se tmode
    set_mode((ch-'0')<<FL_MODE_SHIFT);
  } else if ((ch>='A')&&(ch<='I')) { //A to I set ID
    set_id(ch-'A'+1);
  } else switch (ch) { //other characters
    case '?':
    case 'h':
      help(); //show help
      break;
    case '/':
      try_connect(0); //reset connection timeout
      break;      
    case '-':
      pfln(F("bye"));
      try_connect(0xFF); //disable communication
      break;
  }
}
