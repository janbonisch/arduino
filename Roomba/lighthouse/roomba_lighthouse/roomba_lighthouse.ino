extern "C" {
  void rlh_send(void);    //function prototype for asm code
}

volatile unsigned char  tick_data[8];    //port setup for 8 bits
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
asm("        .equ    halfpulse,65   ;length of half IR pulse         ");
asm("        .equ    outport,0x05   ;port for IR transnit PORTB=0x05 ");
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
asm("        ldi   r17,0xFF             ;in q1 allways 1             ");
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

//Proc one bit to tickdata
char proc_data(unsigned char c, unsigned char mask, unsigned char* data) {
  unsigned char w;

  w=*data;                    //get the data from ram
  w&=~mask;                   //zero bit by mask
  if ((c&0x80)!=0) w|=mask;   //if is there 1, set bit by mask
  *data=w;                    //store new value to ram
  return c<<1;                //return input value with rotation  
}

//Prepare tickdata
void prepare_tick_data(unsigned char tr1, unsigned char tr2, unsigned char tr3, unsigned char tr4, unsigned char tr5, unsigned char tr6) {  
  for (unsigned char* data=tick_data;data<&tick_data[8];data++) { //we have to send 8 bits for each transmitter
    tr1=proc_data(tr1,0x01,data);       //transmitter #1
    tr2=proc_data(tr2,0x02,data);       //transmitter #2
    tr3=proc_data(tr3,0x04,data);       //transmitter #3
    tr4=proc_data(tr4,0x08,data);       //transmitter #4
    tr5=proc_data(tr5,0x10,data);       //transmitter #5
    tr6=proc_data(tr6,0x20,data);       //transmitter #6   
  }  
}

//===========================================================================

void setup() {
  DDRB |= B00111111; // portB5, bits 0-5 (pins: 12, 13, 14, 15, 16 and 17) is set as output
  memset(tick_data,0,sizeof(tick_data));  //Make sure the array is clear
}

void loop() {  
  prepare_tick_data(0x55,0xFF,0xFF,0xFF,0xFF,0xA0);  
  rlh_send(); //send it out
  asm("die: jmp die");   //chcipni
}
