#include <Arduino.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

const uint16_t PixelCount=30; //pocet ledek
const uint16_t PixelPin= 2;   //kde jsou pripojeny
const uint16_t AnimCount=1;   //pocet animatoru

const uint16_t PixelFadeDuration = 300; // third of a second
// one second divide by the number of pixels = loop once a second
const uint16_t NextPixelMoveDuration = 700 / PixelCount; // how fast we move through the pixels

NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin); //mame jeden pasek
NeoPixelAnimator animations(AnimCount); // NeoPixel animation management object

//rozklad stromecku na radky
const unsigned char line0[]={3,  0,1,2,};            //podstavec
const unsigned char line1[]={1,  3,};                //kmen
const unsigned char line2[]={7,  4,5,6,7,8,9,29,};   //dolni vetev
const unsigned char line3[]={2,  10,28,};            //dolni veteve
const unsigned char line4[]={2,  11,27,};            //dolni veteve
const unsigned char line5[]={4,  12,13,25,26,};      //prostredni vetev
const unsigned char line6[]={2,  14,24,};            //prostredni vetev
const unsigned char line7[]={2,  15,23,};            //prostredni vetev
const unsigned char line8[]={2,  16,22,};            //prostredni vetev
const unsigned char line9[]={2,  17,21,};            //horni vetev
const unsigned char line10[]={2, 18,20,};            //horni vetev
const unsigned char line11[]={1, 19,};               //spicka
const unsigned char* line[]={line0,line1,line2,line3,line4,line5,line6,line7,line8,line9,line10,line11};
#define lines 12

const unsigned char candle[]={7,  10,28,14,24,17,21,19};  //svicky

void set_pixels_color(const unsigned char* defs, RgbColor color) {
  int ct;

  ct=*defs++;                                         //pocet pixlu
  while (ct-->0) strip.SetPixelColor(*defs++,color);  //a naplnime je barvou    
}

void set_line_color(int i, RgbColor color) {
  if ((i>=0)&&(i<lines)) {            //pokud neni parametr mimo misu
    set_pixels_color(line[i],color);  //nastavime skupinu
  }
}

static unsigned int pos;
static RgbColor target_color;
static RgbColor start_color;

//Inicializace nahody, entropii bereme z analogoveho vstupu
//Opsano z neopixelbus prikladu
void SetRandomSeed() {
  uint32_t seed;
  int i;
  
  seed=analogRead(0);
  delay(1);
  for (i=3;i<31;i+=3) {
    seed^=analogRead(0)<<i;
    delay(1);
  }
  randomSeed(seed);
}

void wait4done(int index) {
  while (animations.IsAnimationActive(index)) { //dokud neni hotovo
    animations.UpdateAnimations();          //aktualizace animaci
    strip.Show();                           //animace naty musime narvat do ledpasku  
  }  
}

void anim_line(const AnimationParam& param) {    
  set_line_color(pos,RgbColor::LinearBlend(start_color,target_color,param.progress));  
}

void line_down(RgbColor target, int speed) {    
  start_color=target_color; //z koncove pocatecni
  target_color=target; //sem jedem  
  pos=lines;
  do {
    pos--;
    animations.StartAnimation(0, speed, anim_line); //na nule startujeme prebarveni stromecku
    wait4done(0); //cekej az nula dojede    
  } while (pos!=0);
}

void line_up(RgbColor target, int speed) {    
  start_color=target_color; //z koncove pocatecni
  target_color=target; //sem jedem  
  pos=0;
  do {    
    animations.StartAnimation(0, speed, anim_line); //na nule startujeme prebarveni stromecku
    wait4done(0); //cekej az nula dojede    
    pos++;
  } while (pos<lines);
}

void anim_candle(const AnimationParam& param) {
  RgbColor color=RgbColor::LinearBlend(start_color,target_color,param.progress);
  if (pos==0) {  //pokud vsechny svice
    set_pixels_color(candle,color);    
  } else { //pokud jednotlivci
    strip.SetPixelColor(candle[pos],color);  //a naplnime je barvou        
  }
}

void show_candle(int index, RgbColor target, int speed) {  
  start_color=strip.GetPixelColor(candle[(index==0)?1:index]);
  target_color=target; //sem jedem
  pos=index;
  animations.StartAnimation(0, speed, anim_candle); //na nule startujeme prebarveni stromecku
  wait4done(0); //cekej az nula dojede  
}

const RgbColor tree_base=RgbColor(0,64,0);
const RgbColor tree_start=RgbColor(0,255,0);
const RgbColor candle_from=RgbColor(255,0,0);
const RgbColor candle_to=RgbColor(255,255,0);
const RgbColor candle_special=RgbColor(0,0,255);
const RgbColor flash_color=RgbColor(255,255,255);
const RgbColor flash_background=RgbColor(5,5,5);
const RgbColor black=RgbColor(0,0,0);

void flash(int index) {
  RgbColor backup=strip.GetPixelColor(index);
  strip.SetPixelColor(index,flash_color);  //a naplnime je barvou        
  strip.Show();
  delay(10);
  strip.SetPixelColor(index,backup);  //a naplnime je barvou        
  strip.Show();  
}

void flash_random() {  
  flash(random(PixelCount));
}

void flash_blazinec() {  
  for (int i=201;i>2;i-=2) {
    flash_random();
    delay(i);
  }               
  for (int c=0;c<15;c++) {
    for (int i=0;i<201;i++) flash_random();
    strip.ClearTo(RgbColor(0,c,0));
  }  
  for (int i=1;i<101;i++) {
    flash_random();
    delay(i);
  }
  delay(1000);
  line_up(black,100);                                        //pomalinku z cerna na zakladni stromek  
}

void stromecek1() {
  line_down(tree_base,1019);                                        //pomalinku z cerna na zakladni stromek
  for (int i=1;i<=candle[0];i++) show_candle(i,candle_special,991); //postupne rozsvitime svicky       
  for (int i=1;i<=candle[0];i++) show_candle(i,candle_from,1013);   //postupne rozsvitime svicky       
  for (int i=1;i<101;i++) {                                         //blikame svickama a do toho blesky
    show_candle(0,RgbColor::LinearBlend(candle_from,candle_to,10000.0/((float)random(10000))),257); //prejdem na nejakou svickoovu baru
    if ((i&3)==0) flash_random();                                   //blejskneme si
    if ((i&15)==0) flash_random();                                  //blejskneme si
    if ((i&31)==0) flash_random();                                  //blejskneme si
  }
  show_candle(0,candle_special,997);                               //svicky do podivne barvy
  target_color=flash_color;                                         //takhle ftipne zmenim vychozi barvu, takze prechod nebuze z puvodni
  line_up(tree_base,100);                                           //prejdeme na zakladni stromek
  delay(1009);                                                      //posekcame
  line_up(black,200);                                               //vypneme  
}

void setup() {
  strip.Begin(); //startujeme praci s paskem
  strip.ClearTo(black);
  strip.Show();
  SetRandomSeed();    
}

void loop() {    
  flash_blazinec();
  delay(7873);  
  stromecek1();
  delay(7879);  
}

