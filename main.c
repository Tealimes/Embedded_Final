/*
EEL 4742C - UCF

Code that prints a welcome message to the pixel display.
*/

#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"          // Graphics library (grlib)
#include "LcdDriver/lcd_driver.h"       // LCD driver
#include <stdio.h>

#define redLED BIT0
#define greenLED BIT7
#define S1 BIT1
#define S2 BIT2

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

#define PWM_PIN BIT7 // PWM pin at P2.7

// ****************************************************************************
void main(void) {
    char mystring[20];

    // Configure WDT & GPIO
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    // Direct Buzzer Pin to TB0.6 functionality
    P2DIR |= PWM_PIN; // Direct pin as output
    P2SEL1 &= ~PWM_PIN;
    P2SEL0 |= PWM_PIN;

    // Configure ACLK to the 32 KHz crystal
    config_ACLK_to_32KHz_crystal();

    // Enable global interrupt bit
    _enable_interrupts();

    // Starting the timer in the up mode; period = 0.001 seconds
    // (ACLK @ 32 KHz) (divide by 1) (Up mode)
    TB0CCR0 = 33-1; // 32 KHz --> 0.001 seconds (1000 HZ)
    TB0CTL = TASSEL_1 | ID_0 | MC_1 | TACLR;

    // Configuring Channel 6 for PWM
    TB0CCTL6 |= OUTMOD_7; // Output pattern: Reset/set
    TB0CCR6 = 0; // buzz sounds between 0-32
                 // 0 for off, 1-32 for other sounds

    // Configure LEDs
    P1DIR |= redLED;                P9DIR |= greenLED;
    P1OUT &= ~redLED;               P9OUT &= ~greenLED;

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
    Graphics_Context g_sContext;        // Declare a graphic library context
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

    // Set Level
    Graphics_drawImage(&g_sContext, &level, 32, 32);

    _delay_cycles(1000000);

    // Set Climbing Left Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &climbing_lb, 32, 32);

    _delay_cycles(1000000);

    // Set Climbing Right Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &climbing_rb, 32, 32);

    _delay_cycles(1000000);

    // Set Climbing
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &climbing, 32, 32);

    _delay_cycles(1000000);

    // Set Descending
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &desc, 32, 32);

    _delay_cycles(1000000);

    // Set Descending Left Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &desc_lb, 32, 32);

    _delay_cycles(1000000);

    // Set Descending Right Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &desc_rb, 32, 32);

    _delay_cycles(1000000);

    // Set Right Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &rb, 32, 32);

    _delay_cycles(1000000);

    // Set Left Bank
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawImage(&g_sContext, &lb, 32, 32);


    _delay_cycles(1000000);

    while(1){
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawImage(&g_sContext, &level, 32, 32);

        _delay_cycles(10000000);

    }

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




