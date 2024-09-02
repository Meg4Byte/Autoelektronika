/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/* Hardware simulator utility functions */
#include "HW_access.h"

/* SERIAL SIMULATOR CHANNEL TO USE */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/* TASK PRIORITIES */
#define	receive_ch0 ( tskIDLE_PRIORITY + (UBaseType_t)6) //Rec_sens_CH0_task
#define ledovke (tskIDLE_PRIORITY + (UBaseType_t)5) //LED_bar_task
#define	receive_ch1 (tskIDLE_PRIORITY + (UBaseType_t)4 ) //Rec_PC_CH1_task
#define data_processing (tskIDLE_PRIORITY + (UBaseType_t)3 ) //Data_proc_task
#define	send_ch1 (tskIDLE_PRIORITY + (UBaseType_t)2 ) //Send_PC_to_CH1_task
#define	display	( tskIDLE_PRIORITY + (UBaseType_t)1 ) //Disp_task

#define LED_ON 0xFF
#define LED_OFF 0x00

#define LED_MASK_1 0x01
#define LED_MASK_2 0x02
#define LED_MASK_3 0x04
#define LED_MASK_4 0x08
#define LED_MASK_5 0x10
#define LED_MASK_6 0x20
#define LED_MASK_7 0x40
 
#define FIRST_EIGHT   0x01     // (0 to 31)
#define SECOND_EIGHT  0x03     // (32 to 63)
#define THIRD_EIGHT   0x07     // (64 to 95)
#define FOURTH_EIGHT  0x0F     // (96 to 127)
#define FIFTH_EIGHT   0x1F     // (128 to 159)
#define SIXTH_EIGHT   0x3F     // (160 to 191)
#define SEVENTH_EIGHT 0x7F     // (192 to 223)
#define EIGHT_EIGHT   0xFF     // (224 to 255)

void main_demo(void);

typedef float my_float;

/* TASKS: FORWARD DECLARATIONS */
static void Send_PC_to_CH1_task(void* pvParameters);
static void Rec_PC_CH1_task(void* pvParameters);
static void Rec_sens_CH0_task(void* pvParameters);
static void LED_bar_task(void* pvParameters);
static void Data_proc_task(void* pvParameters);
static void Disp_task(void* pvParameters);
static void TimerCallBack(TimerHandle_t timer);

/* TRASNMISSION DATA - CONSTANT IN THIS APPLICATION */
const char trigger[] = "Pozdrav svima\n";
unsigned volatile t_point;

/* RECEPTION DATA BUFFER */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
static uint8_t prozor = 0, duzina, duzina1, flag_info = 0, flag_rezim = 0, rezim_rada = 0, taster_display = 0;
static uint8_t r_point, up_down, manuelno_automatski;
static uint8_t levi_napred, desni_napred, levi_nazad, desni_nazad;
static uint16_t v_trenutno = 0, v_trenutno1 = 0, vmax_tr = 0, vmax = 130, vmax_d = 0, vmin_d = 0;
static char vmax_string[7];
static float srednja_v = 0;
static uint8_t ukljuceno_1 = 0, ukljuceno_2 = 0, ukljuceno_3 = 0, ukljuceno_4 = 0, ukljuceno_5 = 0 , ukljuceno_6 = 0 , ukljuceno_7 = 0;

/* 7-SEG NUMBER DATABASE - ALL HEX DIGITS */
static const unsigned char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
								0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

/* GLOBAL OS-HANDLES */
static SemaphoreHandle_t Display_BinarySemaphore;
static SemaphoreHandle_t Send_BinarySemaphore;
static SemaphoreHandle_t RXC_BinarySemaphore0;
static SemaphoreHandle_t RXC_BinarySemaphore1;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t TXC_BinarySemaphore;

static TimerHandle_t tH;
static QueueHandle_t serial_queue;

/* INTERRUPT*/
static uint32_t prvProcessRXCInterrupt(void)
{
	BaseType_t xHigherPTW = pdFALSE;

	if (get_RXC_status(0) != 0) {   //interrupt sa kanala 0
		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &xHigherPTW) != pdTRUE) {
			printf("ERROR0 \n");
		}
	}
	if (get_RXC_status(1) != 0) {   //interrupt sa kanala 1

		if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &xHigherPTW) != pdTRUE) {
			printf("ERROR1 \n");
		}
	}
	portYIELD_FROM_ISR((uint32_t)xHigherPTW);  //povratak u task pre interrupt-a
}

/*INTERRUPT ZA LEDOVKE*/
static uint32_t OnLED_ChangeInterrupt()  //svaki klik na ledovku dovodi do interrupt-a
{
	BaseType_t higherPriorityTaskWoken = pdFALSE;
	printf("LED Interrupt\n");
	if (xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &higherPriorityTaskWoken) != pdTRUE) {
		printf("ERROR \n");
	}
	portYIELD_FROM_ISR((uint32_t)higherPriorityTaskWoken);  //povratak u task pre interrupt-a
}

/* TIMER CALLBACK FUNCTION*/
static void TimerCallBack(TimerHandle_t timer)
{
	static uint32_t cnt1 = 0, cnt2 = 0;

	if (send_serial_character((uint8_t)COM_CH_0, (uint8_t)'T') != 0) { // slanje info na svakih 200ms
		printf("ERROR TRANSMIT \n");
	}
	cnt1++;
	cnt2++;

	if (cnt1 == (uint32_t)25) {      // 25*200ms = 5000ms 
		cnt1 = (uint32_t)0;
		if (xSemaphoreGive(Send_BinarySemaphore, 0) != pdTRUE) {
			printf("ERROR GIVE");
		}
	}
	if (cnt2 == (uint32_t)5) {       // broji 5s
		cnt2 = (uint32_t)0;
		if (xSemaphoreGive(Display_BinarySemaphore, 0) != pdTRUE) {
			printf("DISPLAY ERROR\n");
		}
	}
}

/* RECEIVE0*/
static void Rec_sens_CH0_task(void* pvParameters) {
	uint8_t cc;
	static char tmp_string[20], string_queue[20];
	static uint8_t j = 0; // promenljiva za indeks niza

	while(1){

		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {   //predavanje ce biti na svakih 200ms
			printf("ERROR TAKE \n"); 
		}
		if (get_serial_character(COM_CH_0, &cc) != 0) {   
			printf("ERROR GET \n");
		}

		if (cc != (uint8_t)43) {       // + oznacava kraj poruke  , 13 je non printable 23 i 33 ne rade (# !)
			tmp_string[j] = (char)cc;  
			j++;
		}

		else {
			tmp_string[j] = '\0';    
			j = 0;

			duzina = (uint8_t)strlen(tmp_string) % (uint8_t)12;   //carriage return, sa 13 se detektuje kraj poruke
			for (uint8_t i = 0; i < duzina; i++) {
				string_queue[i] = tmp_string[i]; 
				tmp_string[i] = "";  
			}
			string_queue[duzina] = '\0';
	
			if (xQueueSend(serial_queue, &string_queue, 0) != pdTRUE) {    //smestamo u red sve sa kanala 0
				printf("ERROR QUEUE\n");
			}
		}
	}
}

/*RECEIVE1*/
static void Rec_PC_CH1_task(void* pvParameters) {
	uint8_t cc = 0;
	static char tmp_string[100], string_queue[100];
	static uint8_t i = 0;

	while(1){ 

		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
			printf("ERROR TAKE 1 \n");
		}

		if (get_serial_character(COM_CH_1, &cc) != 0) {  // karakter ubaci na ch1
			printf("ERROR GET 1\n");
		}

		if (cc != (uint8_t)43) { //13 je CR

			tmp_string[i] = (char)cc;   
			i++;
		}
		else { 
			tmp_string[i] = '\0';   
			i = 0;

			printf("--> Unicom kanal 0 : %s \n", tmp_string);
			strcpy(string_queue, tmp_string);  

			if (xQueueSend(serial_queue, &string_queue, 0) != pdTRUE) {     
				printf("ERROR GET\n");
			}
		}
	}
}

/*LEDOVKE*/
static void LED_bar_task(void* pvParameters) { 

	uint8_t led_stanje;

	while(1) {

		printf("-->Ocitavanje LEDovki \n");

		if (xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY) != pdTRUE) { 
			printf("ERROR TAKE \n");
		}

		//Interupt se desio --> pogledaj sta je na prvom LED baru (crevni 0-ti bar)
		get_LED_BAR(0, &led_stanje);

		ukljuceno_1 = (led_stanje & LED_MASK_1) ? 1 : 0;
		if (ukljuceno_1) {printf("LED 1 -- > ON \n");}
		else{printf("LED 1 -- > OFF \n");}
		
		ukljuceno_2 = (led_stanje & LED_MASK_2) ? 1 : 0;
		if (ukljuceno_2) {printf("LED 2 -- > ON \n");}
		else {printf("LED 2 -- > OFF \n");}

		ukljuceno_3 = (led_stanje & LED_MASK_3) ? 1 : 0;
		if (ukljuceno_3) {printf("LED 3 -- > ON \n"); }
		else { printf("LED 3 -- > OFF \n");}

		ukljuceno_4 = (led_stanje & LED_MASK_4) ? 1 : 0;
		if (ukljuceno_4) { printf("LED 4 -- > ON \n");}
		else { printf("LED 4 -- > OFF \n"); }

		ukljuceno_5 = (led_stanje & LED_MASK_5) ? 1 : 0;
		if (ukljuceno_5) { printf("LED 5 -- > ON \n");}
		else { printf("LED 5 -- > OFF \n"); }

		ukljuceno_6 = (led_stanje & LED_MASK_6) ? 1 : 0;
		if (ukljuceno_6) { printf("LED 6 -- > ON \n"); }
		else { printf("LED 6 -- > OFF \n"); }

		ukljuceno_7 = (led_stanje & LED_MASK_7) ? 1 : 0;
		if (ukljuceno_6) { printf("LED 7 -- > ON \n"); }
		else { printf("LED 7 -- > OFF \n"); }
	}
}

/*OBRADA SENZORA*/
static void Data_proc_task(void* pvParameters) {

	static char string_red[20];
	static uint8_t index_v = 0, index_v1 = 0;
	static uint16_t stotine, desetice, jedinice;
	static float v_suma = (float)0, v_suma1 = (float)0;

	while(1){

		printf("-->Max brzina je : %d\n", vmax);
		printf("-->Tr. brzina je : %d\n", v_trenutno);
		printf("-->Sr. brinza je : %.2f\n", srednja_v);

		if (xQueueReceive(serial_queue, &string_red, portMAX_DELAY) != pdTRUE) {  
			printf("ERROR\n");
		}

		string_red[duzina] = '\0'; 

		if (strcmp(string_red, "automatski\0") == 0) {
			flag_info = 1;
			flag_rezim = 1;
			manuelno_automatski = 0;

			if (levi_napred == 0) {
				set_LED_BAR(1, LED_OFF);
			}
			else {

				if (levi_napred > 0 && levi_napred < 13) {
					set_LED_BAR(1, FIRST_EIGHT);
				}
				else if (levi_napred >= 13 && levi_napred < 25) {
					set_LED_BAR(1, SECOND_EIGHT);
				}
				else if (levi_napred >= 25 && levi_napred < 38) {
					set_LED_BAR(1, THIRD_EIGHT);
				}
				else if (levi_napred >= 38 && levi_napred < 50) {
					set_LED_BAR(1, FOURTH_EIGHT);
				}
				else if (levi_napred >= 50 && levi_napred < 63) {
					set_LED_BAR(1, FIFTH_EIGHT);
				}
				else if (levi_napred >= 63 && levi_napred < 76) {
					set_LED_BAR(1, SIXTH_EIGHT);
				}
				else if (levi_napred >= 76 && levi_napred < 89) {
					set_LED_BAR(1, SEVENTH_EIGHT);
				}
				else if (levi_napred >= 89 && levi_napred <= 100) {
					set_LED_BAR(1, EIGHT_EIGHT);
				}

				printf("Prozor levi prednji --> %d%%\n", levi_napred);
			}

			if (desni_napred == 0) {
				set_LED_BAR(2, LED_OFF);
			}
			else {

				if (desni_napred > 0 && desni_napred < 13) {
					set_LED_BAR(2, FIRST_EIGHT);
				}
				else if (desni_napred >= 13 && desni_napred < 25) {
					set_LED_BAR(2, SECOND_EIGHT);
				}
				else if (desni_napred >= 25 && desni_napred < 38) {
					set_LED_BAR(2, THIRD_EIGHT);
				}
				else if (desni_napred >= 38 && desni_napred < 50) {
					set_LED_BAR(2, FOURTH_EIGHT);
				}
				else if (desni_napred >= 50 && desni_napred < 63) {
					set_LED_BAR(2, FIFTH_EIGHT);
				}
				else if (desni_napred >= 63 && desni_napred < 76) {
					set_LED_BAR(2, SIXTH_EIGHT);
				}
				else if (desni_napred >= 76 && desni_napred < 89) {
					set_LED_BAR(2, SEVENTH_EIGHT);
				}
				else if (desni_napred >= 89 && desni_napred <= 100) {
					set_LED_BAR(2, EIGHT_EIGHT);
				}

				printf("Prozor desni prednji --> %d%%\n", desni_napred);
			}

			if (levi_nazad == 0) {
				set_LED_BAR(3, LED_OFF);
			}
			else {

				if (levi_nazad > 0 && levi_nazad < 13) {
					set_LED_BAR(3, FIRST_EIGHT);
				}
				else if (levi_nazad >= 13 && levi_nazad < 25) {
					set_LED_BAR(3, SECOND_EIGHT);
				}
				else if (levi_nazad >= 25 && levi_nazad < 38) {
					set_LED_BAR(3, THIRD_EIGHT);
				}
				else if (levi_nazad >= 38 && levi_nazad < 50) {
					set_LED_BAR(3, FOURTH_EIGHT);
				}
				else if (levi_nazad >= 50 && levi_nazad < 63) {
					set_LED_BAR(3, FIFTH_EIGHT);
				}
				else if (levi_nazad >= 63 && levi_nazad < 76) {
					set_LED_BAR(3, SIXTH_EIGHT);
				}
				else if (levi_nazad >= 76 && levi_nazad < 89) {
					set_LED_BAR(3, SEVENTH_EIGHT);
				}
				else if (levi_nazad >= 89 && levi_nazad <= 100) {
					set_LED_BAR(3, EIGHT_EIGHT);
				}

				printf("Prozor desni zadnji --> %d%%\n", levi_nazad);
			}

			if (desni_nazad == 0) {
				set_LED_BAR(4, LED_OFF);
			}
			else {

				if (desni_nazad > 0 && desni_nazad < 13) {
					set_LED_BAR(4, FIRST_EIGHT);
				}
				else if (desni_nazad >= 13 && desni_nazad < 25) {
					set_LED_BAR(4, SECOND_EIGHT);
				}
				else if (desni_nazad >= 25 && desni_nazad < 38) {
					set_LED_BAR(4, THIRD_EIGHT);
				}
				else if (desni_nazad >= 38 && desni_nazad < 50) {
					set_LED_BAR(4, FOURTH_EIGHT);
				}
				else if (desni_nazad >= 50 && desni_nazad < 63) {
					set_LED_BAR(4, FIFTH_EIGHT);
				}
				else if (desni_nazad >= 63 && desni_nazad < 76) {
					set_LED_BAR(4, SIXTH_EIGHT);
				}
				else if (desni_nazad >= 76 && desni_nazad < 89) {
					set_LED_BAR(4, SEVENTH_EIGHT);
				}
				else if (desni_nazad >= 89 && desni_nazad <= 100) {
					set_LED_BAR(4, EIGHT_EIGHT);
				}

				printf("Prozor desni zadnji --> %d%%\n", desni_nazad);
			}

			if (ukljuceno_7) {

				set_LED_BAR(3, LED_ON);
				set_LED_BAR(4, LED_ON);
			}
		}

		else if (strcmp(string_red, "manuelno\0") == 0) {
			
			//remove this 
			flag_info = 1;
			flag_rezim = 1;
			//
			manuelno_automatski = 1;

			if (manuelno_automatski) {

				if (ukljuceno_1) {

					if (levi_napred == 0) {
						set_LED_BAR(1, LED_OFF);
					}

					if (levi_napred > 0 && levi_napred < 13) {
						set_LED_BAR(1, FIRST_EIGHT);
					}
					else if (levi_napred >= 13 && levi_napred < 25) {
						set_LED_BAR(1, SECOND_EIGHT);
					}
					else if (levi_napred >= 25 && levi_napred < 38) {
						set_LED_BAR(1, THIRD_EIGHT);
					}
					else if (levi_napred >= 38 && levi_napred < 50) {
						set_LED_BAR(1, FOURTH_EIGHT);
					}
					else if (levi_napred >= 50 && levi_napred < 63) {
						set_LED_BAR(1, FIFTH_EIGHT);
					}
					else if (levi_napred >= 63 && levi_napred < 76) {
						set_LED_BAR(1, SIXTH_EIGHT);
					}
					else if (levi_napred >= 76 && levi_napred < 89) {
						set_LED_BAR(1, SEVENTH_EIGHT);
					}
					else if (levi_napred >= 89 && levi_napred <= 100) {
						set_LED_BAR(1, EIGHT_EIGHT);
					}

					//set_LED_BAR(1, LED_ON);
					printf("Prozor levi prednji --> %d%%\n" , levi_napred);
				}
				else {

					set_LED_BAR(1, LED_OFF);
					//levi_napred = 0;
					printf("Prozor levi prednji --> OFF\n");
				}

				if (ukljuceno_2) {

					if (desni_napred == 0) {
						set_LED_BAR(2, LED_OFF);
					}

					if (desni_napred  > 0 && desni_napred  < 13) {
						set_LED_BAR(2, FIRST_EIGHT);
					}
					else if (desni_napred  >= 13 && desni_napred  < 25) {
						set_LED_BAR(2, SECOND_EIGHT);
					}
					else if (desni_napred  >= 25 && desni_napred  < 38) {
						set_LED_BAR(2, THIRD_EIGHT);
					}
					else if (desni_napred  >= 38 && desni_napred  < 50) {
						set_LED_BAR(2, FOURTH_EIGHT);
					}
					else if (desni_napred  >= 50 && desni_napred  < 63) {
						set_LED_BAR(2, FIFTH_EIGHT);
					}
					else if (desni_napred  >= 63 && desni_napred  < 76) {
						set_LED_BAR(2, SIXTH_EIGHT);
					}
					else if (desni_napred  >= 76 && desni_napred  < 89) {
						set_LED_BAR(2, SEVENTH_EIGHT);
					}
					else if (desni_napred  >= 89 && desni_napred  <= 100) {
						set_LED_BAR(2, EIGHT_EIGHT);
					}
					
					printf("Prozor desni prednji --> %d%%\n" , desni_napred);
				}
				else {

					set_LED_BAR(2, LED_OFF);
					//desni_napred = 0;
					printf("Prozor desni prednji --> OFF\n");
				}

				if (ukljuceno_3 && !ukljuceno_7) {

					if (levi_nazad == 0) {
						set_LED_BAR(3, LED_OFF);
					}

					if (levi_nazad > 0 && levi_nazad < 13) {
						set_LED_BAR(3, FIRST_EIGHT);
					}
					else if (levi_nazad >= 13 && levi_nazad < 25) {
						set_LED_BAR(3, SECOND_EIGHT);
					}
					else if (levi_nazad >= 25 && levi_nazad < 38) {
						set_LED_BAR(3, THIRD_EIGHT);
					}
					else if (levi_nazad >= 38 && levi_nazad < 50) {
						set_LED_BAR(3, FOURTH_EIGHT);
					}
					else if (levi_nazad >= 50 && levi_nazad < 63) {
						set_LED_BAR(3, FIFTH_EIGHT);
					}
					else if (levi_nazad >= 63 && levi_nazad < 76) {
						set_LED_BAR(3, SIXTH_EIGHT);
					}
					else if (levi_nazad >= 76 && levi_nazad < 89) {
						set_LED_BAR(3, SEVENTH_EIGHT);
					}
					else if (levi_nazad >= 89 && levi_nazad <= 100) {
						set_LED_BAR(3, EIGHT_EIGHT);
					}

					printf("Prozor desni zadnji --> %d%%\n" , levi_nazad);
					
				}
				else if (!ukljuceno_3 && !ukljuceno_7){ 

					set_LED_BAR(3, LED_OFF);
				}
				else if (!ukljuceno_3 && ukljuceno_7) {

					set_LED_BAR(3, LED_ON);
				}
				else if (ukljuceno_3 && ukljuceno_7) {
					
					set_LED_BAR(3, LED_ON);
				}

				if (ukljuceno_4 && !ukljuceno_7) {

					if (desni_nazad == 0) {
						set_LED_BAR(4, LED_OFF);
					}

					if (desni_nazad > 0 && desni_nazad < 13) {
						set_LED_BAR(4, FIRST_EIGHT);
					}
					else if (desni_nazad >= 13 && desni_nazad < 25) {
						set_LED_BAR(4, SECOND_EIGHT);
					}
					else if (desni_nazad >= 25 && desni_nazad < 38) {
						set_LED_BAR(4, THIRD_EIGHT);
					}
					else if (desni_nazad >= 38 && desni_nazad < 50) {
						set_LED_BAR(4, FOURTH_EIGHT);
					}
					else if (desni_nazad >= 50 && desni_nazad < 63) {
						set_LED_BAR(4, FIFTH_EIGHT);
					}
					else if (desni_nazad >= 63 && desni_nazad < 76) {
						set_LED_BAR(4, SIXTH_EIGHT);
					}
					else if (desni_nazad >= 76 && desni_nazad < 89) {
						set_LED_BAR(4, SEVENTH_EIGHT);
					}
					else if (desni_nazad >= 89 && desni_nazad <= 100) {
						set_LED_BAR(4, EIGHT_EIGHT);
					}

					printf("Prozor desni zadnji --> %d%%\n", desni_nazad);
				}			
				else if (!ukljuceno_4 && !ukljuceno_7) {

					set_LED_BAR(4, LED_OFF);
				}
				else if (!ukljuceno_4 && ukljuceno_7) {

					set_LED_BAR(4, LED_ON);
				}
				else if (ukljuceno_4 && ukljuceno_7) {

					set_LED_BAR(4, LED_ON);
				}
			}

			printf("-->Rezim rada je manuelni\n");
		}

			//max brzina se unosi u formatu brzina 140+ ako je brzina sa 3 cifre ili brzina  45+ ako je sa 2 cifre

		else if (string_red[0] == 'b' && string_red[1] == 'r' && string_red[2] == 'z' && string_red[3] == 'i' && string_red[4] == 'n' && string_red[5] == 'a' && string_red[6] == ' ') { //unos maksimalne brzine

			if (string_red[7] == ' ') {
				string_red[7] = '0';
			}

			vmax_string[0] = string_red[7];
			vmax_string[1] = string_red[8];
			vmax_string[2] = string_red[9];
			vmax_string[3] = '\0';


			stotine  = (uint16_t)string_red[7] - (uint16_t)48;
			desetice = (uint16_t)string_red[8] - (uint16_t)48;
			jedinice = (uint16_t)string_red[9] - (uint16_t)48;

			vmax_tr = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
			vmax = vmax_tr;
			vmax_tr = (uint16_t)0;

			flag_info = (uint8_t)0;

			printf("-->Nova max brzina je : %d\n", vmax);
		}

		//namestanje trenutne brzine tr X+ , npr tr 149+

		else if (string_red[0] == 't' && string_red[1] == 'r' && string_red[2] == ' ') {

			flag_info = 0;

			if (string_red[3] == ' ') { string_red[3] = '0'; }

			stotine = (uint16_t)string_red[3] - (uint16_t)'0';
			desetice = (uint16_t)string_red[4] - (uint16_t)'0';
			jedinice = (uint16_t)string_red[5] - (uint16_t)'0';

			v_trenutno1 = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
			v_trenutno = v_trenutno1;

			if (index_v < 10) {				// treba da se izbroji 10 tr vrednosti za usrednjavanje
				
				v_suma += v_trenutno1;
				index_v++;
			}
			else if (index_v == 10){

				srednja_v = v_suma / 10;
				v_suma = 0;
				index_v = 0;

			}

			if (v_trenutno > vmax_d) {
				vmax_d = v_trenutno;
				printf("Najveca max brzina auta je: %d\n", vmax_d);
			}

			if (vmin_d == 0) { vmin_d = v_trenutno; }

			else if (v_trenutno < vmin_d) {
				vmin_d = v_trenutno;
				printf("Najmanja min brzina auta je: %d\n", vmin_d);
			}

			v_trenutno1 = 0;

			//automatski
			if (!manuelno_automatski && srednja_v >= vmax) {

				set_LED_BAR(1, LED_ON);
				set_LED_BAR(2, LED_ON);
				set_LED_BAR(3, LED_ON);
				set_LED_BAR(4, LED_ON);

				printf("-->Prevelika brzina zatvori prozore! \n");

				v_trenutno1 = 0;
			}
		}

			//format je nivo X Y (x je 1 , 2 , 3 , 4) (Y je 0-100) **napomena uvek se unose 3 cifre za nivo prozora 
			//npr nivo 2 100 ili nivo 3 045 ili nivo 1 003

		else if (string_red[0] == 'n' && string_red[1] == 'i' && string_red[2] == 'v' && string_red[3] == 'o' && string_red[4] == ' '
			&& (string_red[5] == '1' || string_red[5] == '2' || string_red[5] == '3' || string_red[5] == '4') && string_red[6] == ' ') {

			uint8_t procenat = (uint8_t)(string_red[7] - '0') * 100 + (uint8_t)(string_red[8] - '0') * 10 + (uint8_t)(string_red[9] - '0');

			if (string_red[5] == '1') { levi_napred = procenat; }
			else if (string_red[5] == '2') { desni_napred = procenat; }
			else if (string_red[5] == '3') { levi_nazad = procenat; }
			else if (string_red[5] == '4') { desni_nazad = procenat; }

			printf("-->Zeljeno stanje prozora: %d%%, %d%% , %d%% , %d%% \n", levi_napred, desni_napred, levi_nazad, desni_nazad);
			
			flag_info = 1;
		}

		else {printf("...\n");}
	}
}

/*SLANJE PODATAKA NA PC*/
static void Send_PC_to_CH1_task(void* pvParameters) {
	static tmp_string[50], tmp_string1[10], tmp_string2[10];
	static uint8_t i = 0, a = 0, b = 1, c = 0;
	static uint8_t tmp_cifra = 0;
	static uint16_t tmp_broj = 0;
	static int brojac = 0;

	char string_ok[] = "ok";

	/*
	string_ok[0] = 'o';
	string_ok[1] = 'k';
	string_ok[2] = '\0';
	*/
	/*
	string_auto[0] = 'a';
	string_auto[1] = 'u';
	string_auto[2] = 't';
	string_auto[3] = 'o';
	string_auto[4] = 'm';
	string_auto[5] = 'a';
	string_auto[6] = 't';
	string_auto[7] = 's';
	string_auto[8] = 'k';
	string_auto[9] = 'i';
	string_auto[10] = '\0';
	*/
	/*
	string_man[0] = 'm';
	string_man[1] = 'a';
	string_man[2] = 'n';
	string_man[3] = 'u';
	string_man[4] = 'e';
	string_man[5] = 'l';
	string_man[6] = 'n';
	string_man[7] = 'o';
	string_man[8] = '\0';
	*/

	while(1)
	{

		if (xSemaphoreTake(Send_BinarySemaphore, portMAX_DELAY) != pdTRUE) {    //brojac     0-25   200ms*25=5s
			printf("TAKE ERROR\n");
		};

		if ((srednja_v > (float)vmax) && (rezim_rada == (uint8_t)1)) { // automatski rezim, zatvaranje prozora
			levi_napred = 100;
			desni_napred = 100;
			levi_nazad = 100;
			desni_nazad = 100;
		}

		if (flag_info == (uint8_t)0) {  //stanje senzora

			for (c = 0; c <= (strlen(vmax_string)); c++) {
				if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)vmax_string[c]) != 0) { //SLANJE PROZORA
					printf("SEND ERROR \n");
				}
				vTaskDelay(pdMS_TO_TICKS(100));
			}
			/*//salje samo stanje senzora prozora
			tmp_string[0] = levi_napred + (char)48;
			tmp_string[1] = (char)32;
			tmp_string[2] = desni_napred + (char)48;
			tmp_string[3] = (char)32;
			tmp_string[4] = levi_nazad + (char)48;
			tmp_string[5] = (char)32;
			tmp_string[6] = desni_nazad + (char)48;
			tmp_string[7] = (char)32; // u ascii je ovo ' '

			if (i > (sizeof(tmp_string) - 1)) {
				i = 0;
				if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)tmp_string[i++]) != 0) { //SLANJE PROZORA
					printf("SEND ERROR \n");
				}
				vTaskDelay(pdMS_TO_TICKS(100));
			}*/

			tmp_broj = (uint16_t)srednja_v; //148  char

			printf("\nProslo je 5s\n");
			//change this 
			printf("-->Levi prednji prozor:  %d\n", levi_napred);
			printf("-->Desni prednji prozor: %d\n", desni_napred);
			printf("-->Levi zadnji prozor:   %d\n", levi_nazad);
			printf("-->Desni zadnji prozor:  %d\n", desni_nazad);

			printf("-----------------------------\n");

			while (tmp_broj) {
				tmp_cifra = (uint8_t)tmp_broj % (uint8_t)10; //8, 4
				tmp_broj = tmp_broj / (uint16_t)10; //14
				tmp_string1[a] = tmp_cifra + (char)48; // 8 4 1  int
				a++;
			}

			//b = 1;
			while (a != (uint8_t)0) { // obrne ga kad ga salje
				brojac++;
				if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)tmp_string1[a - b]) != 0) { //srednja brzina
					printf("SEND ERROR \n");
				}
				a--;

				vTaskDelay(pdMS_TO_TICKS(100));
			}

			if (send_serial_character(COM_CH_1, 13) != 0) { //novi red
				printf("SEND ERROR \n");
			}
		}

		else if (flag_info) {

			if (!manuelno_automatski) {     //manuelno
				for (i = 0; i < strlen(string_ok); i++) {
					if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)string_ok[i]) != 0) {
						printf("SEND ERROR \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100)); // ubacujemo delay izmedju svaka dva karaktera
				}
			}
			else {                 //automatski

				for (i = 0; i < strlen(string_ok); i++) {
					if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)string_ok[i]) != 0) {
						printf("SEND ERROR \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}
			}

			if (flag_rezim != (uint8_t)1) { // ako se bavimo senzorom brzine i senzorom prozora

				//salje samo stanje senzora i brzinu
				tmp_string2[0] = levi_napred + (char)48;
				tmp_string2[1] = (char)32;
				tmp_string2[2] = desni_napred + (char)48;
				tmp_string2[3] = (char)32;
				tmp_string2[4] = levi_nazad + (char)48;
				tmp_string2[5] = (char)32;
				tmp_string2[6] = desni_nazad + (char)48;
				tmp_string2[7] = (char)32; // u ascii je ovo ' '
				tmp_string2[8] = (char)13; // predji u naredni red

				//change this

				if (prozor == (uint8_t)1) {             //levi prednji prozor
					if (up_down == (uint8_t)1) {         //nivo predstavlja stanje tog prozora
						tmp_string2[0] = (char)49;       //nivo = 1
						printf("Levi prednji prozor skroz podignut\n");
					}

					else if (up_down == (uint8_t)0) {
						tmp_string2[0] = (char)48;       //nivo = 0
						printf("Levi prednji prozor skroz spusten\n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}

				else if (prozor == (uint8_t)2) { // desni prednji prozor
					if (up_down == (uint8_t)1) {
						tmp_string2[2] = (char)49;
						printf("Desni prednji prozor skroz podignut\n");
					}

					else if (up_down == (uint8_t)0) {
						tmp_string2[2] = (char)48;
						printf("Desni prednji prozor skroz spusten\n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}

				else if (prozor == (uint8_t)3) { // levi nazad prozor
					if (up_down == (uint8_t)1) {
						tmp_string2[4] = (char)49;
						printf("Levi zadnji prozor skroz podignut\n");
					}

					else if (up_down == (uint8_t)0) {
						tmp_string2[4] = (char)48;
						printf("Levi zadnji prozor skroz spusten\n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}

				else if (prozor == (uint8_t)4) { // desni nazad prozor
					if (up_down == (uint8_t)1) {
						tmp_string2[6] = (char)49;
						printf("Desni zadnji prozor skroz podignut\n");
					}

					else if (up_down == (uint8_t)0) {
						tmp_string2[6] = (char)48;
						printf("Desni zadnji prozor skroz spusten\n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}

				for (i = 0; i <= (uint8_t)8; i++) {  
					if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)tmp_string2[i]) != 0) {
						printf("SEND ERROR \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}
			}
			flag_info = 0;
			flag_rezim = 0;
		}
	}
}

/*DISPLAY*/
static void Disp_task(void* pvParameters) {

	static uint8_t stotine, stotine1, desetice, desetice1, jedinice, jedinice1;
	static uint16_t global_min_brzina = 0 , global_max_brzina = 0;
	static uint16_t temp_max_brzina = 0;

	while(1){

		if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) {  //brojac  2-7  200ms*5=1s je refresh rate LCD-a
			printf("ERROR TAKE\n");
		}

		if (select_7seg_digit(0)) {printf("ERROR SELECT \n");}

		if (set_7seg_digit(hexnum[manuelno_automatski])) {printf("ERROR SET \n");}

		if (global_max_brzina <= v_trenutno) {
		
			global_max_brzina = v_trenutno;
			if (!global_min_brzina) { global_min_brzina = v_trenutno; }
		}

		if (global_min_brzina >= v_trenutno) {

			global_min_brzina = v_trenutno;
		}

		temp_max_brzina = v_trenutno; //ispis trenutne brzine

		if (ukljuceno_5) {

			jedinice = global_max_brzina % 10;
			desetice = (global_max_brzina / 10) % 10;
			stotine = global_max_brzina / 100;

			if (select_7seg_digit(4)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[jedinice])) {printf("ERROR SET \n");}

			if (select_7seg_digit(3)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[desetice])) {printf("ERROR SET \n");}

			if (select_7seg_digit(2)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[stotine])) {printf("ERROR SET \n");}
			
		}
		/*
		if (ukljuceno_5) {temp_max_brzina = vmax_d;}

		else {temp_min_brzina = vmin_d;}
		*/
		
		if (ukljuceno_6) {

			jedinice1 = global_min_brzina % 10;
			desetice1 = (global_min_brzina / 10) % 10;
			stotine1 = global_min_brzina / 100;

			if (select_7seg_digit(8)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[jedinice1])) {printf("ERROR SET \n");}

			if (select_7seg_digit(7)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[desetice1])) {printf("ERROR SET \n");}

			if (select_7seg_digit(6)) {printf("ERROR SELECT \n");}

			if (set_7seg_digit(hexnum[stotine1])) {printf("ERROR SET \n");}	
		}
	}
}

/* MAIN - SYSTEM STARTUP POINT */
void main_demo(void)
{

	if (init_LED_comm() != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_7seg_comm() != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	//samo primamo podatke sa serijske
	if (init_serial_downlink(COM_CH_0) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_uplink(COM_CH_0) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_downlink(COM_CH_1) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}
	if (init_serial_uplink(COM_CH_1) != 0) {
		printf("Neuspesna inicijalizacija \n");
	}

	BaseType_t status;

	// Tasks
	status = xTaskCreate(
		Rec_PC_CH1_task,
		"receive pc task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)receive_ch1,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}

	r_point = (uint8_t)0;

	status = xTaskCreate(
		Send_PC_to_CH1_task,
		"send pc task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)send_ch1,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}

	status = xTaskCreate(
		Disp_task,
		"display task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)display,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}

	status = xTaskCreate(
		Rec_sens_CH0_task,
		"receive sensor task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)receive_ch0,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}
	r_point = (uint8_t)0;

	status = xTaskCreate(
		Data_proc_task,
		"data processing task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)data_processing,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}

	status = xTaskCreate(
		LED_bar_task,
		"led bar task",
		configMINIMAL_STACK_SIZE,
		NULL,
		(UBaseType_t)ledovke,
		NULL
	);
	if (status != pdPASS) {
		for (;;) {}
	}

	serial_queue = xQueueCreate(1, 12u * sizeof(char));
	if (serial_queue == NULL) {
		printf("ERROR1\n");
	}

	tH = xTimerCreate(
		"timer",
		pdMS_TO_TICKS(200),
		pdTRUE,
		NULL,
		TimerCallBack);
	if (tH == NULL) {
		printf("Greska prilikom kreiranja\n");
	}
	if (xTimerStart(tH, 0) != pdPASS) {
		printf("Greska prilikom kreiranja\n");
	}

	/* Create TBE semaphore - serial transmit comm */
	RXC_BinarySemaphore0 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore0 == NULL) {
		printf("ERROR SEMAPHORE\n");
	}
	RXC_BinarySemaphore1 = xSemaphoreCreateBinary();
	if (RXC_BinarySemaphore1 == NULL) {
		printf("ERROR1\n");
	}
	TXC_BinarySemaphore = xSemaphoreCreateBinary();
	if (TXC_BinarySemaphore == NULL) {
		printf("ERROR1\n");
	}

	/* Create LED interrapt semaphore */
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();
	if (LED_INT_BinarySemaphore == NULL) {
		printf("ERROR1\n");
	}

	// semaphore init
	Display_BinarySemaphore = xSemaphoreCreateBinary();
	Send_BinarySemaphore = xSemaphoreCreateBinary();
	if (Display_BinarySemaphore == NULL) {
		printf("ERROR1\n");
	}

	/* SERIAL RECEPTION INTERRUPT HANDLER */
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);


	vTaskStartScheduler();

	for (;;) {}
}


void vApplicationIdleHook(void) {

	//idleHookCounter++;
}
