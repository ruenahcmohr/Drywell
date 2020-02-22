/*

				       PRO mini+
				      +--------------------------------+ 
				      |          o o o o o o           |
				      | o D1(TxD)                 +V o | 
				      | o D0(RxD)                GND o | 
				      | o RST   +-----------+    RST o | 
	                       C      | o GND   |O          |    VCC o | 
			      RC      | o D2    |           |     C3 o | 
			       R      | o D3    |           |     C2 o | 
				      | o D4    | ATMEGA328 |     C1 o | 
				      | o D5    |           |     C0 o | 
				      | o D6    |           |     B5 o | 
				      | o D7    +-----------+     B4 o | 
				      | o B0           C6 o C7 o  B3 o | 
				      | o B1           C4 o C5 o  B2 o | 
				      |                                |
				      +--------------------------------+



*/
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "usart.h"


#define IsHigh(BIT, PORT)    ((PORT & (1<<BIT)) != 0)
#define IsLow(BIT, PORT)     ((PORT & (1<<BIT)) == 0)
#define SetBit(BIT, PORT)     PORT |= (1<<BIT)
#define ClearBit(BIT, PORT)   PORT &= ~(1<<BIT)


#define ABS(a)                ((a) < 0 ? -(a) : (a))
#define SIGN(x)               (x)==0?0:(x)>0?1:-1
#define NOP()                 asm volatile ("nop"::)

/*****************************| DEFINIATIONS |********************************/

#define OUTPUT             1
#define INPUT              0

#define LED_CATHODE        3
#define LED_ANODE          2
#define LED_PORT           PORTD
#define LED_DDR            DDRD
#define LED_INPUT          PIND

uint16_t  EEMEM EEHIGHLEVEL;
uint16_t  EEMEM EELOWLEVEL;

/*****************************| VARIABLES |********************************/
 
volatile int AdcValues[8];
unsigned int highLevel, lowLevel;
 
/************************| FUNCTION DECLARATIONS |*************************/

void Delay2(unsigned int delay) ;
void Delay(unsigned int delay);
unsigned int getCap();
void AnalogInit (void);
int  Analog (int n);
void pumpOn(void ) ;
void pumpOff(void) ;
void recieveChar(unsigned char c);

/****************************| CODE... |***********************************/

int main (void) {

  unsigned int sample;
  unsigned char i;    
  
  highLevel = eeprom_read_word(&EEHIGHLEVEL);
  lowLevel  = eeprom_read_word(&EELOWLEVEL);

  // set up directions 

  DDRB = (INPUT << PB0 | INPUT << PB1 |INPUT << PB2 |INPUT << PB3 |INPUT << PB4 |INPUT << PB5 |INPUT << PB6 |INPUT << PB7);
  DDRC = (INPUT << PC0 | INPUT << PC1 |INPUT << PC2 |OUTPUT << PC3 |INPUT << PC4 |INPUT << PC5 |INPUT << PC6 );
  DDRD = (INPUT << PD0 | INPUT << PD1 |INPUT << PD2 |INPUT << PD3 |INPUT << PD4 |INPUT << PD5 |INPUT << PD6 |INPUT << PD7);         
  
  USART_Init( 103 ); // 9600 @ 16Mhz
 // AnalogInit();
  sei();
 
  while(1){
    
    if (HAVEDATA) {
      recieveChar(USART_Receive());
    }
    
    sample = getCap();
    for (i = 0; i < 100; i++)
      sample = (sample + getCap())/2;
    
    if (sample > highLevel) 
      pumpOn();
    
    if (sample < lowLevel)
      pumpOff();
                      
    USART_printint(sample);        
    
    USART_Transmit( '\r');   
    USART_Transmit( '\n');             
  }     
}

//------------------------| FUNCTIONS |------------------------
void recieveChar(unsigned char c) {
  static char state = 0;
  static unsigned int value;
  
  if (0) {
  } else if(c == '?') {
    USART_printstring((unsigned char *)"H:");
    USART_printint(highLevel);
    USART_printstring((unsigned char *)"\r\nL:");
    USART_printint(lowLevel);
    USART_printstring((unsigned char *)"\r\n");
    
  } else if ((c == '\n')||(c == '\r')) {
    if (state != 0) {
      if (state < 20) {
        lowLevel = value;
	eeprom_update_word(&EELOWLEVEL, value);
	USART_printstring((unsigned char *)"Low value written\r\n");
	state = 0;
      } else {
        highLevel = value;
	eeprom_update_word(&EEHIGHLEVEL, value);
	USART_printstring((unsigned char *)"High value written\r\n");
	state = 0;
      }
    
    }
  } else {  
    switch(state) {
      case 0:
	value = 0;
	if (c == 'l') state = 10;
	if (c == 'h') state = 20;
      break;

      case 10:
      case 20:
	value *= 10;
	value += (c - '0');
      break;

      default:
	state = 0;
      break;
    }
  }

}


void pumpOn(void ) {
  USART_Transmit( '+' );
  SetBit(3, PORTC);
}

void pumpOff(void) {
  USART_Transmit( '-');
  ClearBit(3, PORTC);
}

void Delay(unsigned int delay) {
  unsigned int x;
  for (x = delay; x != 0; x--) {
    asm volatile ("nop"::); 
  }
}

void Delay2(unsigned int delay) {
  unsigned int x;
  for (x = delay; x != 0; x--) {
    Delay(65000);
  }
}


unsigned int getCap() {
  unsigned int j;

  // 

  // discharge the capacitor
  SetBit(LED_CATHODE, LED_DDR ); //,  OUTPUT
  SetBit(LED_ANODE,   LED_DDR ); //,  OUTPUT
  ClearBit(LED_CATHODE, LED_PORT); //, Low
  ClearBit(LED_ANODE, LED_PORT); //, LOW
  Delay(5);
  
  // Isolate the cathode 
  ClearBit(LED_ANODE, LED_DDR); //,  INPUT
  SetBit(LED_CATHODE, LED_PORT); //, HIGH  // turn On internal pull-up resistor

  // Count how long it takes the diode to charge to a logic one
  
  for (j = 0; ((j != 65535) && (IsLow(LED_ANODE, LED_INPUT))); j++) ; 
  
  return j; 
}


void AnalogInit (void) {

  int i;

  // Activate ADC with Prescaler 
  ADCSRA =  1 << ADEN  |
            1 << ADSC  | /* start a converstion, irq will take from here */
            0 << ADATE |
            0 << ADIF  |
            1 << ADIE  | /* enable interrupt */
            1 << ADPS2 |
            1 << ADPS1 |
            1 << ADPS0 ;
                        
            
  for (i = 0; i < 8; AdcValues[i++] = 0);
  
}


 
int Analog (int n) {
  return AdcValues[n & 7];
}


//SIGNAL(SIG_ADC) {
ISR(ADC_vect) {
  int i;

  i = ADMUX & 7;       // what channel we on?
  AdcValues[i] = ADC;  // save value
  i++;                 // next value
  ADMUX = (i & 7) | (1<<REFS0);       // select channel
  ADCSRA |= _BV(ADSC); // start next conversion

  return;
}


