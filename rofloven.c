
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>


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

ISR(INT0_vect)
{
    // user code here
  interrupt_count++;
 
}

ISR(TIMER1_COMPA_vect) 
{  
 main_flags |= TIMER_FIRED;
    PORTB ^= _BV(PORTB5);
}


void banner(){
  char * mess = "Refloven 1.0";
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

/* set pin 5 of PORTB for output*/
 DDRB |= _BV(DDB5);
  
// Serial Initialization
/*Set baud rate */ 
 UBRR0H = (unsigned char)(MYUBRR>>8); 
 UBRR0L = (unsigned char) MYUBRR; 
 /* Enable transmitter   */
 UCSR0B = (1<<TXEN0); 
 /* Frame format: 8data, No parity, 1stop bit */ 
 UCSR0C = (3<<UCSZ00);  



// Timer Initialization
TCCR1B |= (1 << WGM12); // Configure timer 1 for CTC mode
TIMSK1 |= (1 << OCIE1A); // Enable CTC interrupt 
//OCR1A   = 15624; // Set CTC compare value to 1Hz at 16MHz AVR clock, with a prescaler of 1024
OCR1A   = 7812; // Set CTC compare value to 1Hz at 16MHz AVR clock, with a prescaler of 1024



// Zero Crossing Initialization
DDRD  &= ~(1<<PD2); // PD2 input
PORTD |=  (1<<PD2); // disable pull up
EICRA |= (1 << ISC01)|(1 << ISC00);//set interrupt in INT0 on raising edge
EIMSK |= (1 << INT0); // enable interrupt

// TCouple reader chip init

DDRC  |=   (1<<PC4);
PORTC |=   (1<<PC4); // CS HI -> inactive;
DDRC  |=   (1<<PC5);
PORTC &=  ~(1<<PC5); // clock active hi;

DDRC  &= ~(1<<PC3); // PC3 input for SO
PORTC |=  (1<<PC3); // disable pull up

sei(); 

TCCR1B |= (1 << CS10)|(1 << CS12); // Start up timer, prescaler 1024 (DSheet 328p page 137)

 char i = 0;
 int temp;

 for(i=0;i<10;i++) buffer[i] = 0;

 char * mark = "EEERCCCCCCCCCCCSFRHHHHHHHHHHHHHH";
 //             00001101010010000001110000111111
 banner();
 
 while(1) {
 
   if(main_flags & TIMER_FIRED){
#ifdef DEBUG  
    
     serialWriteStrLn(buffer+longtobuffer(interrupt_count,buffer));
     interrupt_count =0; 
     main_flags &= ~TIMER_FIRED;
     serialWrite('\r');serialWrite('\n');
#endif
   
      
     ReadTC();
     temp_long = TCReadValue;
     serialWriteStrLn(mark);
     for(i=0;i<32;i++){
       serialWrite( ( (temp_long & 0b1) +'0') );
       temp_long>>=1;
     }
     serialWriteLn();
    

     
     tempfromTCReadValue();
     serialWriteStrLn(buffer+longtobuffer(HotJunctionTintPart,buffer));
     serialWriteStrLn(buffer+longtobuffer(ColdJunctionTintPart,buffer));
     
     serialWriteLn();
     main_flags &= ~TIMER_FIRED;
     
     
   } 
 


}
 
 
 return 0;
}
