#include "LPC17xx.h"                        /* LPC17xx definitions */
#include "type.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "usbaudio.h"
#include <stdio.h>
#include <RTL.h>
#include "GLCD.h"
#include "LED.h"
#include "string.h"
#include "KBD.h"
#include "stdio.h"
#include "apple.c"
#include "chrome.c"
#include "safari.c"
#include "google.c"
#include "android.c"
#include "ttt_o.c"
#include "ttt_x.c"
#include "black.c"
#include "stdlib.h"



#define __FI        1                      /* Font index 16x24               */
#define __USE_LCD   0										/* Uncomment to use the LCD */

extern  void SystemClockUpdate(void);
extern uint32_t SystemFrequency;  
uint8_t  Mute;                                 /* Mute State */
uint32_t Volume;                               /* Volume Level */

#if USB_DMA
uint32_t *InfoBuf = (uint32_t *)(DMA_BUF_ADR);
short *DataBuf = (short *)(DMA_BUF_ADR + 4*P_C);
#else
uint32_t InfoBuf[P_C];
short DataBuf[B_S];                         /* Data Buffer */
#endif

uint16_t  DataOut;                              /* Data Out Index */
uint16_t  DataIn;                               /* Data In Index */

uint8_t   DataRun;                              /* Data Stream Run State */
uint16_t  PotVal;                               /* Potenciometer Value */
uint32_t  VUM;                                  /* VU Meter */
uint32_t  Tick;                                 /* Time Tick */

int p1 = 0;
int p2 = 0;
char p1s[8];
char p2s[8];
int first_to_go = 2;

struct Coordinates{
	int row;
	int column;
	
	
};

void delay(int j){
	int i;
	for (i = 0; i<j; i++){
		
	}
}

/*
 * Get Potenciometer Value
 */

void get_potval (void) {
  uint32_t val;

  LPC_ADC->CR |= 0x01000000;              /* Start A/D Conversion */
  do {
    val = LPC_ADC->GDR;                   /* Read A/D Data Register */
  } while ((val & 0x80000000) == 0);      /* Wait for end of A/D Conversion */
  LPC_ADC->CR &= ~0x01000000;             /* Stop A/D Conversion */
  PotVal = ((val >> 8) & 0xF8) +          /* Extract Potenciometer Value */
           ((val >> 7) & 0x08);
}


/*
 * Timer Counter 0 Interrupt Service Routine
 *   executed each 31.25us (32kHz frequency)
 */

void TIMER0_IRQHandler(void) 
{
  long  val;
  uint32_t cnt;

  if (DataRun) {                            /* Data Stream is running */
    val = DataBuf[DataOut];                 /* Get Audio Sample */
    cnt = (DataIn - DataOut) & (B_S - 1);   /* Buffer Data Count */
    if (cnt == (B_S - P_C*P_S)) {           /* Too much Data in Buffer */
      DataOut++;                            /* Skip one Sample */
    }
    if (cnt > (P_C*P_S)) {                  /* Still enough Data in Buffer */
      DataOut++;                            /* Update Data Out Index */
    }
    DataOut &= B_S - 1;                     /* Adjust Buffer Out Index */
    if (val < 0) VUM -= val;                /* Accumulate Neg Value */
    else         VUM += val;                /* Accumulate Pos Value */
    val  *= Volume;                         /* Apply Volume Level */
    val >>= 16;                             /* Adjust Value */
    val  += 0x8000;                         /* Add Bias */
    val  &= 0xFFFF;                         /* Mask Value */
  } else {
    val = 0x8000;                           /* DAC Middle Point */
  }

  if (Mute) {
    val = 0x8000;                           /* DAC Middle Point */
  }

  LPC_DAC->CR = val & 0xFFC0;             /* Set Speaker Output */

  if ((Tick++ & 0x03FF) == 0) {             /* On every 1024th Tick */
    get_potval();                           /* Get Potenciometer Value */
    if (VolCur == 0x8000) {                 /* Check for Minimum Level */
      Volume = 0;                           /* No Sound */
    } else {
      Volume = VolCur * PotVal;             /* Chained Volume Level */
    }
    val = VUM >> 20;                        /* Scale Accumulated Value */
    VUM = 0;                                /* Clear VUM */
    if (val > 7) val = 7;                   /* Limit Value */
  }

  LPC_TIM0->IR = 1;                         /* Clear Interrupt Flag */
	
	if (get_button() == KBD_LEFT){
			GLCD_Clear(Black); 
	
			
			NVIC_DisableIRQ(TIMER0_IRQn);
			NVIC_DisableIRQ(USB_IRQn);
			USB_Connect(0);
			USB_Reset();
			USB_Connect(1);
				GLCD_Clear(Black);
		GLCD_SetBackColor(Black);
					
				}
}

void mp3player(){

	  volatile uint32_t pclkdiv, pclk;
	
		
		



  /* SystemClockUpdate() updates the SystemFrequency variable */
  SystemClockUpdate();

  LPC_PINCON->PINSEL1 &=~((0x03<<18)|(0x03<<20));  
  /* P0.25, A0.0, function 01, P0.26 AOUT, function 10 */
  LPC_PINCON->PINSEL1 |= ((0x01<<18)|(0x02<<20));

  /* Enable CLOCK into ADC controller */
  LPC_SC->PCONP |= (1 << 12);

  LPC_ADC->CR = 0x00200E04;		/* ADC: 10-bit AIN2 @ 4MHz */
  LPC_DAC->CR = 0x00008000;		/* DAC Output set to Middle Point */

  /* By default, the PCLKSELx value is zero, thus, the PCLK for
  all the peripherals is 1/4 of the SystemFrequency. */
  /* Bit 2~3 is for TIMER0 */
  pclkdiv = (LPC_SC->PCLKSEL0 >> 2) & 0x03;
  switch ( pclkdiv )
  {
	case 0x00:
	default:
	  pclk = SystemFrequency/4;
	break;
	case 0x01:
	  pclk = SystemFrequency;
	break; 
	case 0x02:
	  pclk = SystemFrequency/2;
	break; 
	case 0x03:
	  pclk = SystemFrequency/8;
	break;
  }

  LPC_TIM0->MR0 = pclk/DATA_FREQ - 1;	/* TC0 Match Value 0 */
  LPC_TIM0->MCR = 3;					/* TCO Interrupt and Reset on MR0 */
  LPC_TIM0->TCR = 1;					/* TC0 Enable */
  NVIC_EnableIRQ(TIMER0_IRQn);

  USB_Init();				/* USB Initialization */
  USB_Connect(TRUE);		/* USB Connect */

}
//gallery function
void gallery(){
	int option = 1;
	
	GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);


	//get selection of image to display
	while(1){
		if (get_button() == KBD_DOWN){
			option++;
		
		
		}else if(get_button() == KBD_UP){
			option--;
			
		}
		
		if (option < 1){
				option = 1;
			}else if (option > 5){
				option = 5;
			}
		
		if (option == 1){
			
	
GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Gallery");
	
	GLCD_SetTextColor(Red);
	 GLCD_DisplayString(2, 0, __FI, "android.bmp");
		GLCD_SetTextColor(White);	
	GLCD_DisplayString(3, 0, __FI, "apple.bmp");
	GLCD_DisplayString(4, 0, __FI, "chrome.bmp");
	GLCD_DisplayString(5, 0, __FI, "google.bmp");
	GLCD_DisplayString(6, 0, __FI, "safari.bmp");
			
		}else if(option == 2){
						 
GLCD_SetTextColor(White);
GLCD_DisplayString(0, 0, __FI, "Gallery");

	GLCD_SetTextColor(White);
	 GLCD_DisplayString(2, 0, __FI, "android.bmp");
		GLCD_SetTextColor(Red);	
	GLCD_DisplayString(3, 0, __FI, "apple.bmp");
		GLCD_SetTextColor(White);	
	GLCD_DisplayString(4, 0, __FI, "chrome.bmp");
	GLCD_DisplayString(5, 0, __FI, "google.bmp");
	GLCD_DisplayString(6, 0, __FI, "safari.bmp");
			
		}else if (option == 3){
			 

GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Gallery");

	GLCD_SetTextColor(White);
	 GLCD_DisplayString(2, 0, __FI, "android.bmp");
	GLCD_DisplayString(3, 0, __FI, "apple.bmp");
		GLCD_SetTextColor(Red);	
	GLCD_DisplayString(4, 0, __FI, "chrome.bmp");
				GLCD_SetTextColor(White);
	GLCD_DisplayString(5, 0, __FI, "google.bmp");
	GLCD_DisplayString(6, 0, __FI, "safari.bmp");
			
		}else if (option == 4){
			GLCD_SetTextColor(White);
GLCD_DisplayString(0, 0, __FI, "Gallery");
	
	GLCD_SetTextColor(White);
	 GLCD_DisplayString(2, 0, __FI, "android.bmp");
	GLCD_DisplayString(3, 0, __FI, "apple.bmp");
			
	GLCD_DisplayString(4, 0, __FI, "chrome.bmp");
				GLCD_SetTextColor(Red);
	GLCD_DisplayString(5, 0, __FI, "google.bmp");
			GLCD_SetTextColor(White);
	GLCD_DisplayString(6, 0, __FI, "safari.bmp");
			
		}else if(option == 5){
						GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Gallery");
	
	GLCD_SetTextColor(White);
	 GLCD_DisplayString(2, 0, __FI, "android.bmp");
	GLCD_DisplayString(3, 0, __FI, "apple.bmp");
			
	GLCD_DisplayString(4, 0, __FI, "chrome.bmp");
			
	GLCD_DisplayString(5, 0, __FI, "google.bmp");
			GLCD_SetTextColor(Red);
	GLCD_DisplayString(6, 0, __FI, "safari.bmp");
			
		}
		
		if (get_button() == KBD_SELECT){
			if (option == 1){
					GLCD_Clear(Black);
					while(1){
		GLCD_Bitmap(0,0, 150, 150, ANDROID_PIXEL_DATA); //display the android logo
				if (get_button() == KBD_LEFT){//go back to the gallery menu
						GLCD_Clear(Black);
					break;
				}
	}
				
			}else if (option == 2){
					GLCD_Clear(Black);
				while(1){
		GLCD_Bitmap(0,0, 150, 150, APPLE_PIXEL_DATA);//display the apple logo
					if (get_button() == KBD_LEFT){
							GLCD_Clear(Black);
					break;
				}
		
	}
		
				
				
			}else if(option == 3){
					GLCD_Clear(Black);
				while(1){
		GLCD_Bitmap(0,0, 150, 150, CHROME_PIXEL_DATA);//display the chrome logo
					if (get_button() == KBD_LEFT){
							GLCD_Clear(Black);
					break;
				}
		
	}
				
			}else if(option == 4){
					GLCD_Clear(Black);
				while(1){
		GLCD_Bitmap(0,0, 150, 150, GOOGLE_PIXEL_DATA);//display the google logo
					if (get_button() == KBD_LEFT){
							GLCD_Clear(Black);
					break;
				}
		
	}
				
			}else if(option == 5){
					GLCD_Clear(Black);
				while(1){
		GLCD_Bitmap(0,0, 150, 150, SAFARI_PIXEL_DATA);//display the safari logo
					if (get_button() == KBD_LEFT){
							GLCD_Clear(Black);
					break;
				}
		
	}
				
			}
		}
		
		if (get_button() == KBD_LEFT){//break out of the infinite while loop and go back to the main menu
				GLCD_Clear(Black);
					break;
				}
		
}
	
			
	
	
}

//update the tic tac toe grid visually
void populate_states(int state[3][3]){
	int i;
	int j;

	
	for (i = 0; i<3; i++){
		
		for (j = 0; j<3; j++){
			if (state[i][j] == 1){
				GLCD_Bitmap(j*80,i*80, 80, 80, TTTO_PIXEL_DATA);
			}else if (state[i][j] == 2){
				GLCD_Bitmap(j*80,i*80, 80, 80, TTTX_PIXEL_DATA);
			}
		}
}
	

	
}


//check if there is a winner for tic tac toe
int check_winner(int state[3][3]){
int i, j;
	
	int zero_present = 0;
	
		if (state[0][0] == 1 && state[0][1] == 1 && state[0][2] == 1){
		return 1;
	}else if(state[0][0] == 2 && state[0][1] == 2 && state[0][2] == 2){
		return 2;
	}else if (state[0][0] == 1 && state[1][0] == 1 && state[2][0] == 1){
		return 1;
	}else if(state[0][0] == 2 && state[1][0] == 2 && state[2][0] == 2){
		return 2;
	}else if (state[0][0] == 1 && state[1][1] == 1 && state[2][2] == 1){
		return 1;
	}else if(state[0][0] == 2 && state[1][1] == 2 && state[2][2] == 2){
		return 2;
	}else if (state[1][1] == 1 && state[0][1] == 1 && state[2][1] == 1){
		return 1;
	}else if(state[1][1] == 2 && state[0][1] == 2 && state[2][1] == 2){
		return 2;
	}else if (state[1][1] == 1 && state[1][0] == 1 && state[1][2] == 1){
		return 1;
	}else if(state[1][1] == 2 && state[1][0] == 2 && state[1][2] == 2){
		return 2;
	}else if (state[1][1] == 1 && state[0][2] == 1 && state[2][0] == 1){
		return 1;
	}else if(state[1][1] == 2 && state[0][2] == 2 && state[2][0] == 2){
		return 2;
	}else if (state[2][2] == 1 && state[1][2] == 1 && state[0][2] == 1){
		return 1;
	}else if(state[2][2] == 2 && state[1][2] == 2 && state[0][2] == 2){
		return 2;
	}else if (state[2][2] == 1 && state[2][0] == 1 && state[2][1] == 1){
		return 1;
	}else if(state[2][2] == 2 && state[2][0] == 2 && state[2][1] == 2){
		return 2;
	}else{
		//checks if all of the spaces have been taken
		for(i = 0; i<3; i++){
			for(j = 0; j<3; j++){
				if(state[i][j] == 0){
					zero_present = 1;
					break;
				}
			}
	}
		
	
	if (zero_present == 0){
		return 0;
	}else{
		return 3;
	}
		
	
	
	
}
}
//cpu checks if the opponent is going to win and the method returns which box it should choose to prevent from losing
//if there is nothing to cause it to lose it will return 3, 3
struct Coordinates cpu_check_opponent(int state[3][3]){
	struct Coordinates opponent;
	if (state[0][0] == 1 && state[0][1] == 1 && state[0][2] == 0){
		opponent.row = 0;
		opponent.column = 2;
		
		
	}else if(state[0][1] == 1 && state[0][2] == 1 && state[0][0] == 0){
		opponent.row = 0;
		opponent.column = 0;
		
	}else if(state[0][2] == 1 && state[1][2] == 1 && state[2][2] == 0){
		opponent.row = 2;
		opponent.column = 2;
		
	}else if(state[1][2] == 1 && state[2][2] == 1 && state[0][2] == 0){
		opponent.row = 0;
		opponent.column = 2;
		
	}else if(state[2][2] == 1 && state[2][1] == 1 && state[2][0] == 0){
		opponent.row = 2;
		opponent.column = 0;
		
	}else if(state[2][1] == 1 && state[2][0] == 1 && state[2][2] == 0){
		opponent.row = 2;
		opponent.column = 2;
		
	}else if(state[2][0] == 1 && state[1][0] == 1 && state[0][0] == 0){
		opponent.row = 0;
		opponent.column = 0;
		
	}else if(state[1][0] == 1 && state[0][0] == 1 && state[2][0] == 0){
		opponent.row = 2;
		opponent.column = 0;
		
	}else if(state[0][1] == 1 && state[1][1] == 1 && state[2][1] == 0){
		opponent.row = 2;
		opponent.column = 1;
		
	}else if(state[1][1] == 1 && state[2][1] == 1 && state[0][1] == 0){
		opponent.row = 0;
		opponent.column = 1;
		
	}else if(state[1][0] == 1 && state[1][1] == 1 && state[1][2] == 0){
		opponent.row = 1;
		opponent.column = 2;
		
	}else if(state[1][1] == 1 && state[1][2] == 1 && state[1][0] == 0){
		opponent.row = 1;
		opponent.column = 0;
		
	}else if(state[0][0] == 1 && state[1][1] == 1 && state[2][2] == 0){
		opponent.row = 2;
		opponent.column = 2;
		
	}else if(state[1][1] == 1 && state[2][2] == 1 && state[0][0] == 0){
		opponent.row = 0;
		opponent.column = 0;
		
	}else if(state[0][2] == 1 && state[1][1] == 1 && state[2][0] == 0){
		opponent.row = 2;
		opponent.column = 0;
		
	}else if(state[1][1] == 1 && state[2][0] == 1 && state[0][2] == 0){
		opponent.row = 0;
		opponent.column = 2;
		
	}else if(state[0][0] == 1 && state[2][0] == 1 && state[1][0] == 0){
		opponent.row = 1;
		opponent.column = 0;
	
		}else if(state[2][0] == 1 && state[2][2] == 1 && state[2][1] == 0){
		opponent.row = 2;
		opponent.column = 1;
	
		}else if(state[0][0] == 1 && state[0][2] == 1 && state[0][1] == 0){
		opponent.row = 0;
		opponent.column = 1;
	
		}else if(state[0][2] == 1 && state[2][2] == 1 && state[1][2] == 0){
		opponent.row = 1;
		opponent.column = 2;
	
		}else if(state[0][2] == 1 && state[2][0] == 1 && state[1][1] == 0){
			opponent.row = 1;
		opponent.column = 1;
	
		}else if(state[0][0] == 1 && state[2][2] == 1 && state[1][1] == 0){
			opponent.row = 1;
		opponent.column = 1;
			
			}else if(state[0][1] == 1 && state[2][1] == 1 && state[1][1] == 0){
				
				opponent.row = 1;
		opponent.column = 1;
				
				
				}else if(state[1][0] == 1 && state[1][2] == 1 && state[1][1] == 0){
			opponent.row = 1;
		opponent.column = 1;
					
			
			
		}else{
		opponent.row = 3;
		opponent.column = 3;
	}
	
	return opponent;
	
}

//cpu checks if it can win and the methods returns which box it should choose to win
//if there is no winning coordinate it will return 3, 3
struct Coordinates cpu_check_myself(int state[3][3]){
	
	struct Coordinates myself;
	if (state[0][0] == 2 && state[0][1] == 2 && state[0][2] == 0){
		myself.row = 0;
		myself.column = 2;
		
		
	}else if(state[0][1] == 2 && state[0][2] == 2 && state[0][0] == 0){
		myself.row = 0;
		myself.column = 0;
		
	}else if(state[0][2] == 2 && state[1][2] == 2 && state[2][2] == 0){
		myself.row = 2;
		myself.column = 2;
		
	}else if(state[1][2] == 2 && state[2][2] == 2 && state[0][2] == 0){
		myself.row = 0;
		myself.column = 2;
		
	}else if(state[2][2] == 2 && state[2][1] == 2 && state[2][0] == 0){
		myself.row = 2;
		myself.column = 0;
		
	}else if(state[2][1] == 2 && state[2][0] == 2 && state[2][2] == 0){
		myself.row = 2;
		myself.column = 2;
		
	}else if(state[2][0] == 2 && state[1][0] == 2 && state[0][0] == 0){
		myself.row = 0;
		myself.column = 0;
		
	}else if(state[1][0] == 2 && state[0][0] == 2 && state[2][0] == 0){
		myself.row = 2;
		myself.column = 0;
		
	}else if(state[0][1] == 2 && state[1][1] == 2 && state[2][1] == 0){
		myself.row = 2;
		myself.column = 1;
		
	}else if(state[1][1] == 2 && state[2][1] == 2 && state[0][1] == 0){
		myself.row = 0;
		myself.column = 1;
	}else if(state[1][0] == 2 && state[1][1] == 2 && state[1][2] == 0){
		myself.row = 1;
		myself.column = 2;
		
	}else if(state[1][1] == 2 && state[1][2] == 2 && state[1][0] == 0){
		myself.row = 1;
		myself.column = 0;
		
	}else if(state[0][0] == 2 && state[1][1] == 2 && state[2][2] == 0){
		myself.row = 2;
		myself.column = 2;
		
	}else if(state[1][1] == 2 && state[2][2] == 2 && state[0][0] == 0){
		myself.row = 0;
		myself.column = 0;
		
	}else if(state[0][2] == 2 && state[1][1] == 2 && state[2][0] == 0){
		myself.row = 2;
		myself.column = 0;
		
	}else if(state[1][1] == 2 && state[2][0] == 2 && state[0][2] == 0){
		myself.row = 0;
		myself.column = 2;
		
	
		}else if(state[0][0] == 2 && state[2][0] == 2 && state[1][0] == 0){
		myself.row = 1;
		myself.column = 0;
	
		}else if(state[2][0] == 2 && state[2][2] == 2 && state[2][1] == 0){
		myself.row = 2;
		myself.column = 1;
	
		}else if(state[0][0] == 2 && state[0][2] == 2 && state[0][1] == 0){
		myself.row = 0;
		myself.column = 1;
	
		}else if(state[0][2] == 2 && state[2][2] == 2 && state[1][2] == 0){
		myself.row = 1;
		myself.column = 2;
	
		}else if(state[0][2] == 2 && state[2][0] == 2 && state[1][1] == 0){
			myself.row = 1;
		myself.column = 1;
	
		}else if(state[0][0] == 2 && state[2][2] == 2 && state[1][1] == 0){
			myself.row = 1;
		myself.column = 1;
			
		}else if(state[0][1] == 2 && state[2][1] == 2 && state[1][1] == 0){
				
				myself.row = 1;
		myself.column = 1;
				
				
				}else if(state[1][0] == 2 && state[1][2] == 2 && state[1][1] == 0){
			myself.row = 1;
		myself.column = 1;
					
				
	}else{
		myself.row = 3;
		myself.column = 3;
	}
	
	return myself;
	
	
}





//multiplayer tic tac toe
void game(){
	int option = 1;
	int active_player=1;
	int status;
int k;
		
	char active_str[5];
    //array for keeping track of moves: 0 noone, 1 player one, 2 player two
	int grid[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

	
		GLCD_Clear(White);
GLCD_SetBackColor(White);	
  GLCD_SetTextColor(Black); 
	//checks who went first last time and lets the other user go first this time
	if (first_to_go == 2){
	active_player = 1;
	first_to_go = 1;
}else if(first_to_go == 1){
	active_player = 2;
	first_to_go = 2;
	

}
	while(1){
		
	
		if (get_button() == KBD_DOWN){
			if(option == 7 || option == 8){
				GLCD_Clear(White); 
				option = option;
				populate_states(grid);//clears the previous black box to update new black box position and repopulate grid
			}else{
			GLCD_Clear(White); 
		option = option + 3;
			populate_states(grid);
			}
		}else if(get_button() == KBD_UP){
			if (option == 2 || option == 3){
				GLCD_Clear(White);
				option = option;
				populate_states(grid);
			}else{
				
			GLCD_Clear(White); 
			option = option - 3;
			populate_states(grid);
			}
		}else if(get_button() == KBD_LEFT){
			GLCD_Clear(White); 
			option--;
			populate_states(grid);
			
		}else if(get_button() == KBD_RIGHT){
			GLCD_Clear(White); 
			option++;
			populate_states(grid);
		}
		
		if (option < 1){
				option = 1;
			}else if (option > 9){
				option = 9;
			}
		//grid selection
		if (option == 1){
				
	GLCD_Bitmap(0,0, 80, 80, BLACK_PIXEL_DATA);
			
			
			



			
		}else if(option == 2){
						 

	GLCD_Bitmap(80,0, 80, 80, BLACK_PIXEL_DATA);

			
		}else if (option == 3){
			 
			 GLCD_Bitmap(160,0, 80, 80, BLACK_PIXEL_DATA);


		}else if (option == 4){
			 
GLCD_Bitmap(0,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 5){
			 
GLCD_Bitmap(80,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 6){
			 
GLCD_Bitmap(160,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 7){
			 
GLCD_Bitmap(0,160, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 8){
			 GLCD_Bitmap(80,160, 80, 80, BLACK_PIXEL_DATA);


		}else if (option == 9){
			 
GLCD_Bitmap(160,160, 80, 80, BLACK_PIXEL_DATA);

		}
		
		if (get_button() == KBD_SELECT){
		
		
			
		
			// if it's the first grid
			if (option == 1){
				if (grid[0][0] == 0){//check if that gried is empty
				if (active_player == 1){//check who's turn it is
					GLCD_Bitmap(0,0, 80, 80, TTTO_PIXEL_DATA);//output correct image based on player
					grid[0][0] = 1;//update the grid array appropriately
					active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(0,0, 80, 80, TTTX_PIXEL_DATA);
					grid[0][0] = 2;
					active_player = 1;
				}
			}
				
			
			}else if (option == 2){
				if (grid[0][1] == 0){
				if (active_player == 1){
					GLCD_Bitmap(80,0, 80, 80, TTTO_PIXEL_DATA);
					grid[0][1] = 1;
					active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(80,0, 80, 80, TTTX_PIXEL_DATA);
					grid[0][1] = 2;
					active_player = 1;
				}
			}
				
			}else if(option == 3){
				if (grid[0][2] == 0){
					if (active_player == 1){
					GLCD_Bitmap(160,0, 80, 80, TTTO_PIXEL_DATA);
					grid[0][2] = 1;
						active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(160,0, 80, 80, TTTX_PIXEL_DATA);
					grid[0][2] = 2;
					active_player = 1;
				}
			}
				
			}else if(option == 4){
				if (grid[1][0] == 0){
						if (active_player == 1){
					GLCD_Bitmap(0,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][0] = 1;
							active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(0,80, 80, 80, TTTX_PIXEL_DATA);
					grid[1][0] = 2;
					active_player = 1;
				}
			}
			}else if(option == 5){
				if (grid[1][1] == 0){
						if (active_player == 1){
					GLCD_Bitmap(80,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][1] = 1;
							active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(80,80, 80, 80, TTTX_PIXEL_DATA);
					grid[1][1] = 2;
					active_player = 1;
				}
			}
			}else if(option == 6){
				if (grid[1][2] == 0){
				if (active_player == 1){
					GLCD_Bitmap(160,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][2] = 1;
					active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(160,80, 80, 80, TTTX_PIXEL_DATA);
					grid[1][2] = 2;
					active_player = 1;
				}
			}
			}else if(option == 7){
				if (grid[2][0] == 0){
				if (active_player == 1){
					GLCD_Bitmap(0,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][0] = 1;
					active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(0,160, 80, 80, TTTX_PIXEL_DATA);
					grid[2][0] = 2;
					active_player = 1;
				}
			}
			}else if(option == 8){
				if (grid[2][1] == 0){
					if (active_player == 1){
					GLCD_Bitmap(80,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][1] = 1;
						active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(80,160, 80, 80, TTTX_PIXEL_DATA);
					grid[2][1] = 2;
					active_player = 1;
				}
			}
			}else if(option == 9){
				if (grid[2][2] == 0){
					if (active_player == 1){
					GLCD_Bitmap(160,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][2] = 1;
						active_player = 2;
				}else if (active_player == 2){
					GLCD_Bitmap(160,160, 80, 80, TTTX_PIXEL_DATA);
					grid[2][2] = 2;
					active_player = 1;
				}
			}
		}
			
	
		}
		
	
		
sprintf(active_str, "%d", active_player);
		GLCD_DisplayString(0, 48, __FI, "P ");
			GLCD_DisplayString(0, 49, __FI, active_str);//show who's turn it is
			GLCD_DisplayString(0, 51, __FI, "T");
		GLCD_DisplayString(1, 51, __FI, "I");
		GLCD_DisplayString(2, 51, __FI, "C");
		GLCD_DisplayString(3, 51, __FI, "T");
		GLCD_DisplayString(4, 51, __FI, "A");
		GLCD_DisplayString(5, 51, __FI, "C");
		GLCD_DisplayString(6, 51, __FI, "T");
		GLCD_DisplayString(7, 51, __FI, "O");
		GLCD_DisplayString(8, 51, __FI, "E");
		
		
		status = check_winner(grid);//check if there is a winner
		//update the score from each round
		if (status == 1){
			delay(10000000);
	GLCD_Clear(White);
			p1++;
			
			break;
		}else if(status == 2){
			delay(10000000);
		GLCD_Clear(White);
			p2++;
		
			break;
		}else if(status == 0){
			delay(10000000);
			GLCD_Clear(White);
			
			break;
		}
}

	while(1){//end of round screen showing who won and the scores
if (status == 1){
		
			GLCD_DisplayString(0, 0, __FI, "P1 won the round.");
			
		}else if(status == 2){
		
			GLCD_DisplayString(0, 0, __FI, "P2 won the round.");
			
		}else if(status == 0){
			  
			GLCD_DisplayString(0, 0, __FI, "It's a tie.");
			
		}
		GLCD_DisplayString(2, 0, __FI, "Left->menu");
		GLCD_DisplayString(3, 0, __FI, "Select->play again");
		GLCD_DisplayString(5, 0, __FI, "Scores: ");
		sprintf(p1s, "%d", p1);
		GLCD_DisplayString(6, 0, __FI, "P1: ");
		GLCD_DisplayString(6, 4, __FI, p1s);
		sprintf(p2s, "%d", p2);
		GLCD_DisplayString(7, 0, __FI, "P2: ");
		GLCD_DisplayString(7, 4, __FI, p2s);
		
		if (get_button() == KBD_LEFT){
		break;
		}else if(get_button() == KBD_SELECT){
			game();
			break;
		}
	
	}
	
}
//single player tic tac toe builds on top of multiplayer tic tac toe
void game2(){
	
	int option = 1;
	int active_player=1;//active player at each turn/instance
	int status;
int k;
	
	int rand_r;
	int rand_c;
		
	char active_str[5];

	int grid[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};//tic tac toe grid

	
		GLCD_Clear(White);
GLCD_SetBackColor(White);	
  GLCD_SetTextColor(Black); 
	
	if (first_to_go == 2){
	active_player = 1;
	first_to_go = 1;
}else if(first_to_go == 1){
	active_player = 2;
	first_to_go = 2;
	

}
	while(1){
		if (active_player == 1){
		
		if (get_button() == KBD_DOWN){
			if(option == 7 || option == 8){
				GLCD_Clear(White); 
				option = option;
				populate_states(grid);
			}else{
			GLCD_Clear(White); 
		option = option + 3;
			populate_states(grid);
			}
		}else if(get_button() == KBD_UP){
			if (option == 2 || option == 3){
				GLCD_Clear(White);
				option = option;
				populate_states(grid);
			}else{
				
			GLCD_Clear(White); 
			option = option - 3;
			populate_states(grid);
			}
		}else if(get_button() == KBD_LEFT){
			GLCD_Clear(White); 
			option--;
			populate_states(grid);
			
		}else if(get_button() == KBD_RIGHT){
			GLCD_Clear(White); 
			option++;
			populate_states(grid);
		}
		
		if (option < 1){
				option = 1;
			}else if (option > 9){
				option = 9;
			}
		
		if (option == 1){
				
	GLCD_Bitmap(0,0, 80, 80, BLACK_PIXEL_DATA);
			
			
			



			
		}else if(option == 2){
						 

	GLCD_Bitmap(80,0, 80, 80, BLACK_PIXEL_DATA);

			
		}else if (option == 3){
			 
			 GLCD_Bitmap(160,0, 80, 80, BLACK_PIXEL_DATA);


		}else if (option == 4){
			 
GLCD_Bitmap(0,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 5){
			 
GLCD_Bitmap(80,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 6){
			 
GLCD_Bitmap(160,80, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 7){
			 
GLCD_Bitmap(0,160, 80, 80, BLACK_PIXEL_DATA);

		}else if (option == 8){
			 GLCD_Bitmap(80,160, 80, 80, BLACK_PIXEL_DATA);


		}else if (option == 9){
			 
GLCD_Bitmap(160,160, 80, 80, BLACK_PIXEL_DATA);

		}
		
		
		
		if (get_button() == KBD_SELECT){
		
		
			
		
			
			if (option == 1){
				if (grid[0][0] == 0){
				
					GLCD_Bitmap(0,0, 80, 80, TTTO_PIXEL_DATA);
					grid[0][0] = 1;
					active_player = 2;
				
			}
				
			
			}else if (option == 2){
				if (grid[0][1] == 0){
				
					GLCD_Bitmap(80,0, 80, 80, TTTO_PIXEL_DATA);
					grid[0][1] = 1;
					active_player = 2;
				
			}
				
			}else if(option == 3){
				if (grid[0][2] == 0){
					
					GLCD_Bitmap(160,0, 80, 80, TTTO_PIXEL_DATA);
					grid[0][2] = 1;
						active_player = 2;
				
			}
				
			}else if(option == 4){
				if (grid[1][0] == 0){
						
					GLCD_Bitmap(0,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][0] = 1;
							active_player = 2;
				
			}
			}else if(option == 5){
				if (grid[1][1] == 0){
						
					GLCD_Bitmap(80,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][1] = 1;
							active_player = 2;
				
			}
			}else if(option == 6){
				if (grid[1][2] == 0){
				
					GLCD_Bitmap(160,80, 80, 80, TTTO_PIXEL_DATA);
					grid[1][2] = 1;
					active_player = 2;
				
			}
			}else if(option == 7){
				if (grid[2][0] == 0){
				
					GLCD_Bitmap(0,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][0] = 1;
					active_player = 2;
				
			}
			}else if(option == 8){
				if (grid[2][1] == 0){
					
					GLCD_Bitmap(80,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][1] = 1;
						active_player = 2;
				
			}
			}else if(option == 9){
				if (grid[2][2] == 0){
					
					GLCD_Bitmap(160,160, 80, 80, TTTO_PIXEL_DATA);
					grid[2][2] = 1;
						active_player = 2;
				
			}
		}
			
	
		}
		
	}else if (active_player == 2){
		struct Coordinates opponent_winning;
		struct Coordinates myself_winning;
		myself_winning = cpu_check_myself(grid);
		opponent_winning = cpu_check_opponent(grid);
		
		
		//check if it has winning move
		if (myself_winning.row != 3 && myself_winning.column != 3){
			GLCD_Bitmap((myself_winning.column * 80),(myself_winning.row * 80), 80, 80, TTTX_PIXEL_DATA);
					grid[myself_winning.row][myself_winning.column] = 2;
		//check if it can prevent opponent from winning
		}else if (opponent_winning.row != 3 && opponent_winning.column != 3){
			GLCD_Bitmap((opponent_winning.column * 80),(opponent_winning.row * 80), 80, 80, TTTX_PIXEL_DATA);
					grid[opponent_winning.row][opponent_winning.column] = 2;
			
				
			

		//if the above two don't apply it randomly selects a spot
		}else{
			
			while(1){
				rand_r = rand()% 3;
				rand_c = rand() % 3;
				if (grid[rand_r][rand_c] == 0){
					GLCD_Bitmap((rand_c * 80),(rand_r * 80), 80, 80, TTTX_PIXEL_DATA);
					grid[rand_r][rand_c] = 2;
					break;
				}
				
			}
			
		}
		
		
	active_player = 1;
		
	}

			GLCD_DisplayString(0, 51, __FI, "T");
		GLCD_DisplayString(1, 51, __FI, "I");
		GLCD_DisplayString(2, 51, __FI, "C");
		GLCD_DisplayString(3, 51, __FI, "T");
		GLCD_DisplayString(4, 51, __FI, "A");
		GLCD_DisplayString(5, 51, __FI, "C");
		GLCD_DisplayString(6, 51, __FI, "T");
		GLCD_DisplayString(7, 51, __FI, "O");
		GLCD_DisplayString(8, 51, __FI, "E");
		
		
		status = check_winner(grid);
		
		if (status == 1){
				delay(10000000);
	GLCD_Clear(White);
			p1++;
			
			break;
		}else if(status == 2){
			delay(10000000);
		GLCD_Clear(White);
			
			p2++;
			
			break;
		}else if(status == 0){
			delay(10000000);
			GLCD_Clear(White);
			
			break;
		}
}

	while(1){
if (status == 1){
		
			GLCD_DisplayString(0, 0, __FI, "You won the round.");
			
		}else if(status == 2){
		
			GLCD_DisplayString(0, 0, __FI, "Comp won the round.");
			
		}else if(status == 0){
			  
			GLCD_DisplayString(0, 0, __FI, "It's a tie.");
			
		}
		GLCD_DisplayString(2, 0, __FI, "Left->menu");
		GLCD_DisplayString(3, 0, __FI, "Select->play again");
		GLCD_DisplayString(5, 0, __FI, "Scores: ");
		sprintf(p1s, "%d", p1);
		GLCD_DisplayString(6, 0, __FI, "P1: ");
		GLCD_DisplayString(6, 4, __FI, p1s);
		sprintf(p2s, "%d", p2);
		GLCD_DisplayString(7, 0, __FI, "C: ");
		GLCD_DisplayString(7, 4, __FI, p2s);
		
		if (get_button() == KBD_LEFT){
		break;
		}else if(get_button() == KBD_SELECT){
			game2();
			break;
		}
	
	}
	
}


void gamemenu(){
	int option = 1;
	GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
	
	while(1){
		if (get_button() == KBD_DOWN){
			option++;
		
		
		}else if(get_button() == KBD_UP){
			option--;
			
		}
		
		if (option < 1){
				option = 1;
			}else if (option > 2){
				option = 2;
			}
		
		if (option == 1){
			
	
  GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Tic Tac Toe");
			GLCD_SetTextColor(Red);
	GLCD_DisplayString(2, 0, __FI, "Single Player");
	
	GLCD_SetTextColor(White);
	GLCD_DisplayString(3, 0, __FI, "Multiplayer");
			
			
		}else if(option == 2){
						 
	
GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Tic Tac Toe");
		
	GLCD_DisplayString(2, 0, __FI, "Single Player");
	
	GLCD_SetTextColor(Red);
	GLCD_DisplayString(3, 0, __FI, "Multiplayer");
			
		}
		
	
		
		
		if (get_button() == KBD_SELECT){
			if (option == 1){
game2();
				p1 = 0;
				p2 = 0;
				strcpy(p1s, "");
				strcpy(p2s, "");
				GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
			
			}else if (option == 2){
	game();
			p1 = 0;
				p2 = 0;
				strcpy(p1s, "");
				strcpy(p2s, "");
				GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
			}
				
			}
		
			if (get_button() == KBD_LEFT){
			break;
		}

	
}
	
}

void menu(){
	
	int option = 1;
	GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
	
	while(1){//infinitely ask for user for option to select
		if (get_button() == KBD_DOWN){
			option++;
		
		
		}else if(get_button() == KBD_UP){
			option--;
			
		}
		
		if (option < 1){
				option = 1;
			}else if (option > 3){
				option = 3;
			}
		
		if (option == 1){
			
	//based on the option it highlights the selected option
  GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Kabilan Veerasingam");
	GLCD_DisplayString(1, 0, __FI, "Media Center");
	
	GLCD_SetTextColor(Red);
	GLCD_DisplayString(4, 0, __FI, "MP3 Player");
			GLCD_SetTextColor(White);
	GLCD_DisplayString(5, 0, __FI, "Gallery");
			GLCD_SetTextColor(White);
	GLCD_DisplayString(6, 0, __FI, "Game");
			
		}else if(option == 2){
						 
	
  GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Kabilan Veerasingam");
	GLCD_DisplayString(1, 0, __FI, "Media Center");

	GLCD_SetTextColor(White);
	GLCD_DisplayString(4, 0, __FI, "MP3 Player");
			GLCD_SetTextColor(Red);
	GLCD_DisplayString(5, 0, __FI, "Gallery");
			GLCD_SetTextColor(White);
	GLCD_DisplayString(6, 0, __FI, "Game");
			
		}else if (option == 3){
			 

  GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "Kabilan Veerasingam");
	GLCD_DisplayString(1, 0, __FI, "Media Center");
	
	GLCD_SetTextColor(White);
	GLCD_DisplayString(4, 0, __FI, "MP3 Player");
				GLCD_SetTextColor(White);
	GLCD_DisplayString(5, 0, __FI, "Gallery");
			GLCD_SetTextColor(Red);
	GLCD_DisplayString(6, 0, __FI, "Game");
		}
		//waits for user to select the current option
		if (get_button() == KBD_SELECT){
			if (option == 1){
				GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
  GLCD_SetTextColor(White);
	
	GLCD_DisplayString(0, 0, __FI, "MP3 Player");
	GLCD_DisplayString(4, 0, __FI, "Streaming");
				mp3player();
				GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
			
			}else if (option == 2){
				gallery();
			GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
				
			}else if(option == 3){
				gamemenu();
				GLCD_Clear(Black); 
	GLCD_SetBackColor(Black);
				
				
			}
		}
		

	
}


}

/*****************************************************************************
**   Main Function  main()
******************************************************************************/
int main (void){
	SystemInit();
	LED_Init();                                /* LED Initialization            */
	
	#ifdef __USE_LCD
  GLCD_Init();                               /* Initialize graphical LCD (if enabled */
	GLCD_Clear(Black);                         /* Clear graphical LCD display   */
  GLCD_SetTextColor(White);
  #endif
	
		menu();//goes into the menu

		
}




