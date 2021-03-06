/*
Inspirations and helps :
Pid control source : http://www.flashingleds.net/sousvader/sousvader.html
Triac bucketting : http://www.rotwang.co.uk/projects/triac.html
*/

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "lut.h"
#include <avr/eeprom.h>

#define TIMER_FIRED 1
#define BaudRate 9600
#define MYUBRR (F_CPU / 16 / BaudRate ) - 1 

volatile int main_flags = 0;
volatile int interrupt_count =0;
volatile long TCReadValue;

long ColdJunctionTintPart;
int ColdJunctionTfloatPart;
long HotJunctionTintPart;
int HotJunctionTfloatPart;

int temp_int;
long temp_long;

#define BUFF_LEN 10
char buffer[BUFF_LEN];

#define DEBUG

#define COUNTS(n) ((n) << 1)

struct phase_characteristics {
  int duration;
  int target_temp;
  int max_slope;
};

struct program {
  struct phase_characteristics pre_soak;
  struct phase_characteristics soak;
  struct phase_characteristics reflow;
  struct phase_characteristics cooling;
};

void delayLong()
{
        unsigned int delayvar;
        delayvar = 0; 
        while (delayvar <=  65500U)
        { 
                asm("nop");  
                delayvar++;
        } 
}


void serialWrite(unsigned char DataOut)
{
  while ((UCSR0A & _BV(UDRE0)) == 0);              // while NOT ready to transmit 
  UDR0 = DataOut;
}

void serialWriteStr(char * DataOut){
  char i = 0;
  while(*(DataOut+i) !=0){
     while ((UCSR0A & _BV(UDRE0)) == 0);              // while NOT ready to transmit 
     UDR0 = *(DataOut+i);
     i++;
  }
  
}

void serialWriteStrLn(char * DataOut){
  serialWriteStr(DataOut);
  while ((UCSR0A & _BV(UDRE0)) == 0);UDR0 ='\r';
 while ((UCSR0A & _BV(UDRE0)) == 0);UDR0 ='\n';
}

void serialWriteLn(){
  while ((UCSR0A & _BV(UDRE0)) == 0);UDR0 ='\r';
  while ((UCSR0A & _BV(UDRE0)) == 0);UDR0 ='\n';
}

void tempfromTCReadValue(){
  HotJunctionTintPart =  (TCReadValue>>20);  // see maxim datasheet p 10
  HotJunctionTfloatPart =  (TCReadValue>>18) & 0b11;  // see maxim datasheet p 10
  ColdJunctionTintPart = (TCReadValue>>8) & 0x7f;
  ColdJunctionTfloatPart = (TCReadValue>>4) & 0xf;
}

volatile unsigned int power_wait = 0xffff;


ISR(INT0_vect)
{
  // zero crossing

  if(power_wait){
    PORTB &= ~  (1<<PB1);
    OCR1A = power_wait;
    // shut down triac driver pin
    TCCR1B |= (1 << WGM12); // CTC mode : p 136
    TIMSK1 |= (1 << OCIE1A); // Enable CTC interrupt 
    TCCR1B |= (1 << CS11); // start timer prescal = 8;
  }
  interrupt_count++; 
}

ISR(TIMER1_COMPA_vect) 
{ 
  if(power_wait !=  COUNTS(LUT50hz[0] )  )
  PORTB |=  (1<<PB1);; //  triac driver pin -> on
  TCCR1B = 0; // stop timer
}

volatile timer_counter = 61;
ISR(TIMER0_COMPA_vect) 
{  

  if(timer_counter==1){
    OCR0A   = 70;
  }
 if(!timer_counter){
    main_flags |= TIMER_FIRED;
    PORTB ^= _BV(PORTB5);
    OCR0A   = 255;
    timer_counter = 61;
 }
    timer_counter--;
}


void banner(){
  char * mess = "Refloven 1.0\r\n";
  serialWriteStrLn(mess);
}

void ReadTC(){
  // CS(4) & CLCK(5)
  char i =0;
  TCReadValue = 0;
  PORTC &= ~(1<<PC4); // CS LOW -> active;
  for(i=0;i<32;i++){
    PORTC |= (1<<PC5); // CLOCK HI
    TCReadValue<<=1;
    if( PINC & (1<<PC3) )
      TCReadValue |= 1;
    PORTC &= ~(1<<PC5); // CLOCK lo
  }
  PORTC |= (1<<PC4); // CS HI -> inactive;
}

char longtobuffer(long orig,char * buffer){
  char neg=0;
  char i =0;
  char buffer_rank=BUFF_LEN-2;
  if(orig < 0){
    neg = 1;
    orig = (~(orig-1)) & 0b111111111111;
  }
  for(i=0;i<BUFF_LEN;i++) buffer[i] = 0;
  buffer[buffer_rank]='0';
  while(orig){
    buffer[buffer_rank]='0' + orig%10;
    orig/=10;
    buffer_rank--;
  }
  if(neg){
    buffer[buffer_rank]='-';
     buffer_rank--;
  }
  return buffer_rank+1;
}

int main (void)
{
 char buffer[10];
 char buffer_rank;

 enum phases { PHASE_STOP, PHASE_PRE_SOAK, PHASE_SOAK, PHASE_REFLOW, PHASE_COOLING};
 enum phases phase = PHASE_STOP ;
	
/* set pin 5 & 1 of PORTB for output*/
 DDRB |= ((1<<PB1)|(1<<PB5));
 PORTB &= ~  (1<<PB1); // put it down

// Serial Initialization
/*Set baud rate */ 
 UBRR0H = (unsigned char)(MYUBRR>>8); 
 UBRR0L = (unsigned char) MYUBRR; 
 /* Enable transmitter   */
 UCSR0B = (1<<TXEN0); 
 /* Frame format: 8data, No parity, 1stop bit */ 
 UCSR0C = (3<<UCSZ00);  



// Timer Initialization

// timer 0 : 8 bit
TCCR0A |= (1 << WGM01); // Configure timer 0 for CTC mode
TIMSK0 |= (1 << OCIE0A); // Enable CTC interrupt 
 OCR0A   = 255;
// Set CTC compare value to 61Hz at 16MHz AVR clock, with a prescaler of 1024
// the interrupt code triggers temp read every 60 calls @ 255 + 1 @70
//OCR1A   = 7812; // Set CTC compare value to 2Hz at 16MHz AVR clock, with a prescaler of 1024


// timer 1 : 16 bit
TCCR1B |= (1 << WGM12); // CTC mode : p 136
TIMSK1 |= (1 << OCIE1A); // Enable CTC interrupt 
OCR1A   =  COUNTS(LUT50hz[0]); // start at 0%

// Zero Crossing Initialization
DDRD  &= ~(1<<PD2); // PD2 input
PORTD |=  (1<<PD2); // disable pull up
EICRA |= (1 << ISC01)|(1 << ISC00);//set interrupt in INT0 on raising edge
EIMSK |= (1 << INT0); // enable interrupt

// TCouple reader chip init

DDRC  |=   (1<<PC4);// PC4 output -> chip sel
PORTC |=   (1<<PC4); // CS HI -> inactive;
DDRC  |=   (1<<PC5);// PC5 output -> clck
PORTC &=  ~(1<<PC5); // clock active hi;

DDRC  &= ~(1<<PC3); // PC3 input for SO
PORTC |=  (1<<PC3); // disable pull up







sei(); // enable interrupts

//before starting timers
power_wait = COUNTS(LUT50hz[0]); // 0%
 // START Timers
TCCR0B |= (1 << CS00)|(1 << CS02); // Start up timer0, prescaler 1024 (DSheet 328p page 110) -> 15625 ticks / sec
TCCR1B |= (1 << CS11);             // Start up timer0, prescaler    8 (DSheet 328p page 116) 
// -> 2000000 ticks / sec
// 1 pulse (50hz)   -> 40000 pulses 
// -> zero crossing -> 20000 pulses
// bins are in usec

//#define COUNTS(n) ((n) << 1) // defined higher
 long int curr_secs = 0;
 char i = 0;
 char j = 0;
 char dir = 1;
 int temp;

 for(i=0;i<10;i++) buffer[i] = 0;

 //char * mark = "EEERCCCCCCCCCCCSFRHHHHHHHHHHHHHH";
 //             00001101010010000001110000111111
 banner();
 

 struct program curr_prog;
 
 curr_prog.pre_soak.duration = 40;
 curr_prog.pre_soak.target_temp = 30;
 curr_prog.pre_soak.max_slope = 3;
 
 curr_prog.soak.duration = 180;
 curr_prog.soak.target_temp = curr_prog.pre_soak.target_temp;
 curr_prog.soak.max_slope = 0;

 curr_prog.reflow.duration = 50;
 curr_prog.reflow.target_temp = 100;
 curr_prog.reflow.max_slope = (curr_prog.reflow.target_temp -  curr_prog.soak.target_temp )/ curr_prog.reflow.duration;

 curr_prog.cooling.duration = 100;
 curr_prog.cooling.target_temp = 20;
 curr_prog.cooling.max_slope = (curr_prog.cooling.target_temp -  curr_prog.reflow.target_temp )/ curr_prog.cooling.duration;

 float error_p;
 float error_i;
 float error_d;

 float term_p=5;
 float term_i;
 float term_d;

 int last_sec;

 while(1) {
   
   if(main_flags & TIMER_FIRED){
     curr_secs++;
     ReadTC();
     temp_long = TCReadValue;
     tempfromTCReadValue();



#ifdef DEBUG  
    
     // serialWriteStrLn(buffer+longtobuffer(interrupt_count,buffer));
     interrupt_count =0; 
     //main_flags &= ~TIMER_FIRED;
     //serialWrite('\r');serialWrite('\n');
     //     serialWriteStrLn(mark);
     //for(i=0;i<32;i++){
     //  serialWrite( ( (temp_long & 0b1) +'0') );
     //  temp_long>>=1;
     //}
     //serialWriteLn();
   serialWriteStr(buffer+longtobuffer(curr_secs,buffer));
   serialWriteStr(";");
   serialWriteStr(buffer+longtobuffer(HotJunctionTintPart,buffer));
   if(HotJunctionTfloatPart){
   serialWriteStr(".");
   serialWriteStr(buffer+longtobuffer(HotJunctionTfloatPart*25,buffer));
   }
   serialWriteStr(";");
   serialWriteStr(buffer+longtobuffer(power_wait,buffer));
   serialWriteLn();

#endif   
   
     
   switch(phase){

   case PHASE_STOP:
     power_wait=COUNTS(LUT50hz[0]);
     if(curr_secs > 5){
       phase = PHASE_PRE_SOAK;
       last_sec = curr_secs;
        serialWriteStrLn("/////PRESOAK");
     }
     break;

   case PHASE_PRE_SOAK:
     ///////////////////////////////////////////
     //  maintaining pid term for each phase  //
     ///////////////////////////////////////////
     term_p = 10;
     ///////////////////////
     error_p = ((curr_prog.pre_soak.target_temp) - (HotJunctionTintPart+HotJunctionTfloatPart*0.25));
     if(error_p > 0){
      error_p *= term_p;
      if(error_p>99){
	power_wait=COUNTS(LUT50hz[99]);
      }else{
	power_wait=COUNTS(LUT50hz[(int)error_p]);
      }
     }else{
       power_wait=COUNTS(LUT50hz[0]);
     }
     
     if(curr_secs> (curr_prog.pre_soak.duration + last_sec)){
       phase = PHASE_SOAK;
       last_sec = curr_secs;
        serialWriteStrLn("/////SOAK");
       }
     break;

     
     
   case PHASE_SOAK:
       
      ///////////////////////////////////////////
     //  maintaining pid term for each phase  //
     ///////////////////////////////////////////
     term_p = 10;
     ///////////////////////
      error_p = ((curr_prog.soak.target_temp) - (HotJunctionTintPart+HotJunctionTfloatPart*0.25));
     if(error_p > 0){
      error_p *= term_p;
      if(error_p>99){
	power_wait=COUNTS(LUT50hz[99]);
      }else{
	power_wait=COUNTS(LUT50hz[(int)error_p]);
      }
     }else{
       power_wait=COUNTS(LUT50hz[0]);
     }
     
     if(curr_secs> (curr_prog.soak.duration + last_sec)){
        phase = PHASE_REFLOW;
	last_sec = curr_secs;
	serialWriteStrLn("/////REFLOW");
     }

     break;
     
   case PHASE_REFLOW:
     ///////////////////////////////////////////
     //  maintaining pid term for each phase  //
     ///////////////////////////////////////////
     term_p = 10;
     ///////////////////////
      error_p = ((curr_prog.reflow.target_temp) - (HotJunctionTintPart+HotJunctionTfloatPart*0.25));
     if(error_p > 0){
      error_p *= term_p;
      if(error_p>99){
	power_wait=COUNTS(LUT50hz[99]);
      }else{
	power_wait=COUNTS(LUT50hz[(int)error_p]);
      }
     }else{
       power_wait=COUNTS(LUT50hz[0]);
     }
     if(curr_secs> (curr_prog.reflow.duration + last_sec)){
        phase = PHASE_COOLING;
	last_sec = curr_secs;
	serialWriteStrLn("/////COOLING");
      }
     break;


   case PHASE_COOLING:
      ///////////////////////////////////////////
     //  maintaining pid term for each phase  //
     ///////////////////////////////////////////
     term_p = 10;
     ///////////////////////
      error_p = ((curr_prog.cooling.target_temp) - (HotJunctionTintPart+HotJunctionTfloatPart*0.25));
     if(error_p > 0){
      error_p *= term_p;
      if(error_p>99){
	power_wait=COUNTS(LUT50hz[99]);
      }else{
	power_wait=COUNTS(LUT50hz[(int)error_p]);
      }
     }else{
       power_wait=COUNTS(LUT50hz[0]);
     }
     if(curr_secs> (curr_prog.cooling.duration + last_sec)){
        phase = PHASE_STOP;
	last_sec = curr_secs;
	serialWriteStrLn("/////DONE");
      }
     break;
   }
     
   
     //  power_wait = COUNTS(LUT50hz[j]);

     /*  
	 serialWriteStrLn(buffer+longtobuffer(j,buffer));
	 serialWriteStrLn(buffer+longtobuffer(power_wait,buffer));
     */
     //serialWriteLn();
     if(dir){
       j+=4;
     }else{
       j-=4;
     }
     if(j<0)       j=0;
     if(j>99)     j=99;

     if((j==0) || (j==99)){
       dir = !dir;
     }
     main_flags &= ~TIMER_FIRED;
   } 
 }
 
 return 0;
}
