// Final Project EEL 4742C Embedded Systems
// Alexander Peacock, Paul Curtin
// Due 20 April 2026

//#define DEBUG

#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"          // Graphics library (grlib)
#include "LcdDriver/lcd_driver.h"       // LCD driver
#include <stdio.h>
#include <stdint.h>

#define redLED BIT0
#define greenLED BIT7
#define S1 BIT1
#define S2 BIT2

#define PWM_PIN BIT7 // PWM pin at P2.7

#define FLAGS UCA1IFG // Contains the transmit & receive flags
#define RXFLAG UCRXIFG // Receive flag
#define TXFLAG UCTXIFG // Transmit flag
#define TXBUFFER UCA1TXBUF // Transmit buffer
#define RXBUFFER UCA1RXBUF // Receive buffer

// refresh rate of the screen (Hz)
//#define REFRESH_RATE 4

// Attitude Constraints

/*
#define max_x 3500
#define max_y 3500
#define min_x 500
#define min_y 500

#define shift_high 2500
#define shift_low 1500
*/

#define max_x 800
#define max_y 800
#define min_x -800
#define min_y -800
/*
int max_x = 800;
int max_y = 800;
int min_x = -800;
int min_y = -800;
*/
#define shift_high 500
#define shift_low -500

// Attitude coordinates
static volatile int Xp = 0, Yp = 0;


// Analog Stick
#define poll_rate 50000
volatile uint16_t stick_X, stick_Y;
#define stick_Divisor 500      // scalar for how much stick affects the attitude (const int?)
                                    // This needs to be inv proportional to stick poll rate



    


// Images used for attitudes
extern tImage jester_cat;
extern tImage level;
extern tImage lb;
extern tImage rb;
extern tImage climbing_lb;
extern tImage climbing_rb;
extern tImage climbing;
extern tImage desc_lb;
extern tImage desc_rb;
extern tImage desc;

void Initialize_Clock_System();

void Attitude_Constraints(); // Checks Max Constraints
void Attitude_Images(); // Checks positioning for image changes

Graphics_Context g_sContext;        // Declare a graphic library context


//  other function prototypes

void Initialize_UART(void);
void uart_write_char(unsigned char ch);
void uart_write_uint16(unsigned int n);
void uart_write_string(char * string);
unsigned char uart_read_char(void);


void Poll_stick();
void Initialize_ADC();

// Accelerometer
uint16_t accel_X, accel_Y, accel_Z;
void Initialize_accel();
void Read_accel();



// ****************************************************************************
void main(void) {
    char mystring[20];

    const uint16_t REFRESH_RATE = 4;
    // Configure WDT & GPIO
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;


    __delay_cycles(100000); // startup delay fixes RGB LED issue?

    // turn off RGB LED

    P3SEL0 &= ~(BIT3 | BIT6);
    P3SEL1 &= ~(BIT3 | BIT6);

    P2SEL0 &= ~BIT6;
    P2SEL1 &= ~BIT6;

    P3DIR |= (BIT3 | BIT6);
    P3OUT &= ~(BIT3 | BIT6);
    P2DIR |= BIT6;
    P2OUT &= ~BIT6;


    // click stick pin config
    P3DIR &= ~BIT2;
    P3REN |= BIT2;
    P3OUT |= BIT2;

    // Configure LEDs
    P1DIR |= redLED;                P9DIR |= greenLED;
    P1OUT &= ~redLED;               P9OUT &= ~greenLED;
    
    //**********************************
    // BUZZER CONFIGURATION

    // Direct Buzzer Pin to TB0.6 functionality
    P2DIR |= PWM_PIN; // Direct pin as output
    P2SEL1 &= ~PWM_PIN;
    P2SEL0 |= PWM_PIN;

    // Starting the timer in the up mode; period = 0.001 seconds
    // (ACLK @ 32 KHz) (divide by 1) (Up mode)
    TB0CCR0 = 33-1; // 32 KHz --> 0.001 seconds (1000 HZ)
    TB0CTL = TASSEL_1 | ID_0 | MC_1 | TACLR;

    // Configuring Channel 6 for PWM
    TB0CCTL6 |= OUTMOD_7; // Output pattern: Reset/set
    TB0CCR6 = 0; // buzz sounds between 0-32
                 // 0 for off, 1-32 for other sounds

    //**********************************

    // Configure ACLK to the 32 KHz crystal
    config_ACLK_to_32KHz_crystal();

    // Configure timer A, this is for screen refresh
    TA0CTL = TASSEL_1 | ID_0 | MC_1 | TACLR;
    //TA0CCR0 = (REFRESH_RATE / 2) - 1; // set refresh rate (twice per second)
    TA0CCR0 = 16384 - 1; // explicit definition (macro is bugged?)
    TA0CCTL0 &= ~CCIFG;
    TA0CCTL0 |= CCIE;


    // Enable global interrupt bit
    _enable_interrupts();


    // Configure buttons
    P1DIR &= ~(S1|S2);
    P1REN |= (S1|S2);
    P1OUT |= (S1|S2);
    P1IFG &= ~(S1|S2);          // Flags are used for latched polling

    // Set the LCD backlight to highest level
    P2DIR |= BIT6;
    P2OUT |= BIT6;

    // Configure clock system
    Initialize_Clock_System();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Graphics functions

    Crystalfontz128x128_Init();         // Initialize the display

    // Set the screen orientation
    Crystalfontz128x128_SetOrientation(0);

    // Initialize the context
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    // Set background and foreground colors
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);

    // Set the default font for strings
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);

    //Clear the screen
    Graphics_clearDisplay(&g_sContext);
    ////////////////////////////////////////////////////////////////////////////////////////////


    // ******* MAIN LOOPS ***********
    
    if (P3IN & BIT2) { // if joystick button is NOT pressed, do accelerometer mode
        
        Initialize_accel(); // initialize the ADC for the accelerometer
        
        while (1) {              // infinite loop
            Read_accel();       // read ADC values from accelerometer

            // Adjust range to -1000,+1000
            int32_t temp_X = (int32_t)accel_X - 2050;
            int32_t temp_Y = (int32_t)accel_Y - 2050;

            temp_X = (temp_X * 1250) / 1000;
            temp_Y = (temp_Y * 1250) / -1000; //divide by negative for inverted controls

            // clamping constraints
            if (temp_X > 1000) temp_X = 1000;
            else if (temp_X < -1000) temp_X = -1000;
            if (temp_Y > 1000) temp_Y = 1000;
            else if (temp_Y < -1000) temp_Y = -1000;

            Xp = (int16_t)temp_X;
            Yp = (int16_t)temp_Y;



            Attitude_Constraints(); // check thresholds for warning sound

            
            _delay_cycles(5000);
        }
        
    }
    else {             // if joystick button IS pressed, do joystick mode
        
        Initialize_ADC(); // initialize the ADC for the joystick
        while(1) {          // infinite poll loop
            
            Poll_stick();
            Attitude_Constraints(); // check thresholds for warning sound
            
            //Attitude_Images(); // ISR takes care of this
            //_delay_cycles(50000);
            _delay_cycles(poll_rate);
        }
    }

}

// Screen refresh ISR 
#pragma vector = TIMER0_A0_VECTOR //link the ISR to the vector
__interrupt void T0A0_ISR()
{
    Attitude_Images();
    //hardware clears IFG
}



void Read_accel() {
    while (ADC12CTL1 & ADC12BUSY);
    // Start conversion
    ADC12CTL0 |= ADC12SC;

    // Wait for end of sequence (MEM10 → IFG10)
    while (!(ADC12IFGR0 & ADC12IFG10));

    // Read results
    accel_X = ADC12MEM8;   // P8.4 (A4)
    accel_Y = ADC12MEM9;   // P8.5 (A5)
    accel_Z = ADC12MEM10;   // P8.6 (A6)

    // Optional: clear flag (not strictly required, auto-clears on next start)
    ADC12IFGR0 &= ~ADC12IFG10;
}

// Poll the control stick and adjust heading accordingly
void Poll_stick() {
    ADC12CTL0 |= ADC12SC; // trigger conversion

    while(ADC12CTL1 & ADC12BUSY == ADC12BUSY){} // wait until not busy

    stick_X = ADC12MEM0;
    stick_Y = ADC12MEM1;
    
    //center is roughly 2000,2000

    // 501-1500, 2500-3499 -> intermediate change in attitude
    // 0-500, 3500-4096 -> sharp change in attitude

    const uint16_t int_thresh_sub = 1500;
    const uint16_t int_thresh_add = 2500;
    const uint16_t sharp_thresh_sub = 500;
    const uint16_t sharp_thresh_add = 3500;

    const int intChange = 100;
    const int sharpChange = 250;

    // Check X value from stick. apply attitude changes accordingly
    /*
    if (stick_X < sharp_thresh_sub) {
        Xp -= sharpChange;
    }
    else if (stick_X < int_thresh_sub) {
        Xp -= intChange;
    }
    else if (stick_X > sharp_thresh_add) {
        Xp += sharpChange;
    }
    else if (stick_X > int_thresh_add) {
        Xp += intChange;
    }
    
    
    // Do the same for Y
    if (stick_Y < sharp_thresh_sub) {
        Yp -= sharpChange;
    }
    else if (stick_Y < int_thresh_sub) {
        Yp -= intChange;
    }
    else if (stick_Y > sharp_thresh_add) {
        Yp += sharpChange;
    }
    else if (stick_Y > int_thresh_add) {
        Yp += intChange;
    }
    */

    // convert analog values to signed ints
    int signed_X = stick_X - 2000;
    int signed_Y = stick_Y - 2000;

    // adjust attitude based on current stick position
    if (signed_X > 300 || signed_X < -300) {
        Xp += (signed_X / stick_Divisor);
    }
    if (signed_Y > 300 || signed_Y < -300) {
        Yp -= (signed_Y / stick_Divisor);  //subtract instead of add for inverted controls.
    }

    // keep attitudes within min/max
    if (Xp > 1000) Xp = 1000;
    else if (Xp < -1000) Xp = -1000;

    if (Yp > 1000) Yp = 1000;
    else if (Yp < -1000) Yp = -1000;

    
}

// Turns buzzer on if beyond constraints set or turn it off
void Attitude_Constraints(){
    if(Xp >= max_x | Xp <= min_x){
        TB0CCR6 = 1; // buzz
    } else if(Yp >= max_y | Yp <= min_y){
        TB0CCR6 = 1; // buzz
    } else{
        TB0CCR6 = 0; // turn buzzer off
    }

    #ifdef DEBUG
    TB0CCR6 = 0;
    #endif
}

// Image that displays on LCD based on current attitude
void Attitude_Images(){
    char mystring[20];


    // Displays image
    if(Xp <= shift_low & Yp >= shift_high){
        // Climbing Left Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &climbing_lb, 32, 32);

    } else if(Xp >= shift_high & Yp >= shift_high){
        // Climbing Right Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &climbing_rb, 32, 32);

    } else if(Yp >= shift_high){
        // Climbing
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &climbing, 32, 32);

    } else if(Xp <= shift_low & Yp <= shift_low){
        // Descending Left Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &desc_lb, 32, 32);

    } else if(Xp >= shift_high & Yp <= shift_low){
        // Descending Right Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &desc_rb, 32, 32);

    } else if(Yp <= shift_low){
        // Descending
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &desc, 32, 32);

    } else if(Xp <= shift_low){
        // Left Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &lb, 32, 32);

    } else if(Xp >= shift_high){
        // Right Bank
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &rb, 32, 32);

    } else{
        // Level
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &level, 32, 32);
    }

    // Displays pitch and roll
    sprintf(mystring, "Roll: %d", Xp);
    //sprintf(mystring, "X- %d", stick_X);
    Graphics_drawStringCentered(&g_sContext, mystring, AUTO_STRING_LENGTH, 64, 110, OPAQUE_TEXT);
    sprintf(mystring, "Pitch: %d", Yp);
    //sprintf(mystring, "Y- %d", stick_Y);
    Graphics_drawStringCentered(&g_sContext, mystring, AUTO_STRING_LENGTH, 64, 120, OPAQUE_TEXT);

}


void Initialize_Clock_System() {
  // DCO frequency = 16 MHz
  // MCLK = fDCO/1 = 16 MHz
  // SMCLK = fDCO/1 = 16 MHz

  // Activate memory wait state
  FRCTL0 = FRCTLPW | NWAITS_1;    // Wait state=1
  CSCTL0 = CSKEY;
  // Set DCOFSEL to 4 (3-bit field)
  CSCTL1 &= ~DCOFSEL_7;
  CSCTL1 |= DCOFSEL_4;
  // Set DCORSEL to 1 (1-bit field)
  CSCTL1 |= DCORSEL;
  // Change the dividers to 0 (div by 1)
  CSCTL3 &= ~(DIVS2|DIVS1|DIVS0);    // DIVS=0 (3-bit)
  CSCTL3 &= ~(DIVM2|DIVM1|DIVM0);    // DIVM=0 (3-bit)
  CSCTL0_H = 0;

  return;
}

// This intializes the ADC for using teh accelerometer
void Initialize_accel() {
   
    // Configure the pins to analog functionality
    // P8.4 → A4 (X)
    // P8.5 → A5 (Y)
    // P8.6 → A6 (Z)
    P8SEL1 |= BIT4 | BIT5 | BIT6;
    P8SEL0 |= BIT4 | BIT5 | BIT6;

    // Turn on ADC
    ADC12CTL0 |= ADC12ON;

    // Disable ENC before config
    ADC12CTL0 &= ~ADC12ENC;

    //*************** ADC12CTL0 ***************
    ADC12CTL0 |= ADC12SHT0_2 | ADC12MSC;

    //*************** ADC12CTL1 ***************
    ADC12CTL1 = ADC12SHS_0 | ADC12SHP | ADC12DIV_0 | ADC12SSEL_0 | ADC12CONSEQ_1;

    //*************** ADC12CTL2 ***************
    ADC12CTL2 = ADC12RES_2;
    ADC12CTL2 &= ~ADC12DF;

    //*************** ADC12CTL3 ***************
    // Start conversions at MEM8 instead of MEM0
    ADC12CTL3 = ADC12CSTARTADD_8;

    //*************** ADC12MCTLx ***************
    // Sequence: A4 → A5 → A6 using MEM8,9,10

    ADC12MCTL8  = ADC12VRSEL_0 | ADC12INCH_7;              // P8.4 (A7)
    ADC12MCTL9  = ADC12VRSEL_0 | ADC12INCH_6;              // P8.5 (A6)
    ADC12MCTL10 = ADC12VRSEL_0 | ADC12INCH_5 | ADC12EOS;   // P8.6 (A5, end)

    // Enable conversions
    ADC12CTL0 |= ADC12ENC;
}

void Initialize_ADC() {
    // Configure the pins to analog functionality
    // X-axis: A10/P9.2, for A10 (P9DIR=x, P9SEL1=1, P9SEL0=1)
    P9SEL1 |= BIT2;
    P9SEL0 |= BIT2;


    // Y-axis: A11/P9.3, for A11 (P9DIR=x, P9SEL1=1, P9SEL0=1)
    P9SEL1 |= BIT3;
    P9SEL0 |= BIT3;

    // Turn on the ADC module
    ADC12CTL0 |= ADC12ON;

    // Turn off ENC (Enable Conversion) bit while modifying the configuration
    ADC12CTL0 &= ~ADC12ENC;

    //*************** ADC12CTL0 ***************
    // Set ADC12SHT0 (select the number of cycles that you computed) // 16 cycles
    // Set the bit ADC12MSC (Multiple Sample and Conversion)
    ADC12CTL0 |= ADC12SHT0_2 | ADC12MSC;

    //*************** ADC12CTL1 ***************
    // Set ADC12SHS (select ADC12SC bit as the trigger)
    // Set ADC12SHP bit
    // Set ADC12DIV (select the divider you determined)
    // Set ADC12SSEL (select MODOSC)
    // Set ADC12CONSEQ (select sequence-of-channels)
    ADC12CTL1 = ADC12SHS_0 | ADC12SHP | ADC12DIV_0 | ADC12SSEL_0 | ADC12CONSEQ_1;


    //*************** ADC12CTL2 ***************
    // Set ADC12RES (select 12-bit resolution)
    // Set ADC12DF (select unsigned binary format)
    ADC12CTL2 = ADC12RES_2;
    ADC12CTL2 &= ~ADC12DF;


    //*************** ADC12CTL3 ***************
    // Leave all fields at default values
    // Set ADC12CSTARTADD to 0 (first conversion in ADC12MEM0)
    ADC12CTL3 &= ~ADC12CSTARTADD_31;


    //*************** ADC12MCTL0 ***************
    // Set ADC12VRSEL (select VR+=AVCC, VR-=AVSS)
    // Set ADC12INCH (select channel A10)
    ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_10;

    //*************** ADC12MCTL1 ***************
    // Set ADC12VRSEL (select VR+=AVCC, VR-=AVSS)
    // Set ADC12INCH (select the analog channel that you found)
    // Set ADC12EOS (last conversion in ADC12MEM1)
    ADC12MCTL1 = ADC12VRSEL_0 |  ADC12INCH_4 | ADC12EOS;

    // Turn on ENC (Enable Conversion) bit at the end of the configuration
    ADC12CTL0 |= ADC12ENC;
    return;
}

// Configure UART to the popular configuration
// 9600 baud, 8-bit data, LSB first, no parity bits, 1 stop bit
// no flow control, oversampling reception
// Clock: SMCLK @ 1 MHz (1,000,000 Hz)
void Initialize_UART(void)
{
    //Configure pins to UART functionality
    P3SEL1 &= ~(BIT4|BIT5);
    P3SEL0 |= (BIT4|BIT5);

    //Main config register
    UCA1CTLW0 = UCSWRST; //engage reset, change all the fields to zero
    // Most fields in this register, when set to zero,
    // correspond to the popular configuration
    UCA1CTLW0 |= UCSSEL_2; //set clock to SMCLK

    // Configure the clock dividers and modulators (and enable oversampling)
    UCA1BRW = 6;
    // Modulators: UCBRF = 8 = 1000 --> UCBRF3 (Bit #3)
    // UCBRS = 0x20 = 0010 0000 = UCBRS5 (bit #5)
    UCA1MCTLW = UCBRF3 | UCBRS5 | UCOS16;

    //Exit the reset state
    UCA1CTLW0 &= ~UCSWRST;
}


void uart_write_char(unsigned char ch){
    // Wait for any ongoing transmission to complete
    while ( (FLAGS & TXFLAG)==0 ) {}

    // Copy the byte to the transmit buffer
    TXBUFFER = ch; // Tx flag goes to 0 and Tx begins!
    return;
}

void uart_write_uint16(uint16_t n)
{
    unsigned char digits[5]; //MSD first
    digits[0] = (n / 10000) % 10; //10^4
    digits[1] = (n / 1000) % 10;  //10^3
    digits[2] = (n / 100) % 10;   //10^2
    digits[3] = (n / 10) % 10;    //10^1
    digits[4] = n % 10;           //10^0

    volatile uint8_t i = 0;
    //move digit pointer past leading zeros. Stop at 4.
    while ((digits[i] == 0) && (i<4)) i++;

    while (i<=4) 
    {
        uart_write_char(digits[i]+48);
        i++;
    }
}

void uart_write_string(char * str)
{
    //max length precaution (500 character max)
    uint16_t maxLength = str + 500;

    while (*str) //iterate until null terminator
    {
        uart_write_char(*str);
        str++;

        //precautionary break condition
        if (str > maxLength) break;
    }
}

//**********************************
// Configures ACLK to 32 KHz crystal
void config_ACLK_to_32KHz_crystal() {
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz

    // Reroute pins to LFXIN/LFXOUT functionality
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;

    // Wait until the oscillator fault flags remain cleared
    CSCTL0 = CSKEY; // Unlock CS registers
    do {
        CSCTL5 &= ~LFXTOFFG; // Local fault flag
        SFRIFG1 &= ~OFIFG; // Global fault flag
    } while((CSCTL5 & LFXTOFFG) != 0);

    CSCTL0_H = 0; // Lock CS registers
    return;
}




