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

//Proc one bit to tickdata
char proc_data(uint8_t c, uint8_t mask, uint8_t* data) {
  uint8_t w;

  w=*data;                    //get the data from ram
  w&=~mask;                   //zero bit by mask
  if ((c&0x80)!=0) w|=mask;   //if is there 1, set bit by mask
  *data=w;                    //store new value to ram
  return c<<1;                //return input value with rotation  
}

//Prepare tickdata
void prepare_tick_data(uint8_t tr1, uint8_t tr2, uint8_t tr3, uint8_t tr4, uint8_t tr5) {  
  for (uint8_t* data=tick_data;data<&tick_data[8];data++) { //we have to send 8 bits for each transmitter
    tr1=proc_data(tr1,0x01,data);       //transmitter #1
    tr2=proc_data(tr2,0x02,data);       //transmitter #2
    tr3=proc_data(tr3,0x04,data);       //transmitter #3
    tr4=proc_data(tr4,0x08,data);       //transmitter #4
    tr5=proc_data(tr5,0x10,data);       //transmitter #5
    *data&=get_mask();                  //mask only valid bits
  }  
}

//Prepare tickdata, the same for all transmitters
void prepare_tick_data1(uint8_t tr) {
  prepare_tick_data(tr,tr,tr,tr,tr);
}

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
#define ROOMBA_ 142 Download 
#define ROOMBA_ 143 Seek Dock 
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

void ir_send_proc(uint8_t count, uint8_t sleep) {
  digitalWrite(LED_BUILTIN, HIGH);
  do {   
    rlh_send(); //send it out    
    delay(sleep); //wait a while    
  } while ((--count)!=0);
  digitalWrite(LED_BUILTIN, LOW);      
}

void prepare_tick_light_house(uint8_t id) {
  id&=0x0F; //only 0 to 15
  id<<=3; //shift to wall id possition
  prepare_tick_data(id+GREEN_BUOY,id+FENCE,id+RED_BUOY,id+FORCE_FIELD,id+FORCE_FIELD); //prepare data for transmitters
}

//===========================================================================
// User interface

void rc_help(void) {
Serial.println(F(
    "Roomba lighthouse debug\r\n"\
    "=======================\r\n"\
    "o ... left\r\n"\
    "p ... rigth\r\n"\
    "q ... fofward\r\n"\
    "a ... stop\r\n"\
    "w ... wake up roomba with power command\r\n"\
    "l ... start lighthouse id 1\r\n"\    
    "X ... invert transmitters logic\r\n"\
    "\r\n"\
    "\r\n"));  
}

void rc(void) {
  uint8_t ch,code;
  
  if (Serial.available()>0) {
    ch=Serial.read(); //read byte from serial    
    switch (ch) { //create remote code for roomba
      case 'o': 
        Serial.println(F("left"));
        code=ROOMBA_LEFT;
        break;
      case 'p':
        Serial.println(F("right"));
        code=ROOMBA_RIGHT;
        break;
      case 'q':
        Serial.println(F("forward"));
        code=ROOMBA_FORWARD;
        break;
      case 'a':
        Serial.println(F("stop"));
        code=ROOMBA_STOP;
        break;
      case 'w':
        Serial.println(F("Wake up roomba"));
        prepare_tick_data1(ROOMBA_POWER); //prepare data
        ir_send_proc(30,50);
        return;
      case '?':
        rc_help();
        return;        
      case 'X':
        PORTB ^=get_mask();
        return;
      case 'l':
        Serial.println(F("Light house start"));      
        prepare_tick_light_house(1);
        while (Serial.available()==0) {
          ir_send_proc(1,1);
          delay(50);
        }
        Serial.println(F("Light house end"));      
        return;
      default: //unknown command
        return; //do nothink
    }
    prepare_tick_data1(code); //prepare data
    ir_send_proc(4,50);    
  }
}

//===========================================================================
// Arduino start and loop.

void setup() {
  PORTB |= get_mask(); //IR led not active
  DDRB |= get_mask(); // portB5 outputs
  memset(tick_data,0,sizeof(tick_data));  //Make sure the array is clear
  Serial.begin(9600);
  rc_help();
}

void loop() {    
  rc();  
}
