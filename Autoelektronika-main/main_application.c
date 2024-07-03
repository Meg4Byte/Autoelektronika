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
static float levi_napred, desni_napred, levi_nazad, desni_nazad;
static uint16_t v_trenutno = 0, v_trenutno1 = 0, vmax_tr = 0, vmax = 130, vmax_d = 0, vmin_d = 0;
static char vmax_string[7];
static float srednja_v = (float)0;
static  uint8_t ukljuceno_1 = 0, ukljuceno_2 = 0, ukljuceno_3 = 0, ukljuceno_4 = 0, ukljuceno_5 = 0;

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
	static char tmp_string[100], string_queue[100];
	static uint8_t i; // koristi se u for petlji
	static uint8_t j = 0; // promenljiva za indeks niza
	for (;;) // umesto while(1)
	{
		if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {   //predavanje ce biti na svakih 200ms
			printf("ERROR TAKE \n"); // ako semafor nije dobro uzet, javi gresku
		}
		if (get_serial_character(COM_CH_0, &cc) != 0) {   //karaktere smestamo u cc, ako je ispis neuspesan javi gresku
			printf("ERROR GET \n");
		}

		if (cc != (uint8_t)43) {       // ako znak koji dolazi nije + (ascii 43 == +)
			tmp_string[j] = (char)cc;  
			j++;
		}
		else {
			tmp_string[j] = '\0';    // ako je stigao +, zavrsi smestanje karaktera
			j = 0;
			//printf("String sa nultog kanala serijske %s, %c \n", tmp_string, tmp_string[0]);  
			duzina = (uint8_t)strlen(tmp_string) % (uint8_t)12;   //carriage return, sa 13 se detektuje kraj poruke
			for (i = 0; i < duzina; i++) {
				string_queue[i] = tmp_string[i]; // dokle god postoje clanovi u tmp nizu, prepisuj te clanove u red
				tmp_string[i] = "";  //praznjenje niza
			}
			string_queue[duzina] = '\0'; //zavrsi prepisivanje reda
			//printf("String : %s, \n", string_queue);
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

	for (;;) //automatski+, manuelno+, vmax 130+, prozor 2 1+
	{

		if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
			printf("ERROR TAKE 1 \n");
		}

		if (get_serial_character(COM_CH_1, &cc) != 0) {  // karakter ubaci na ch1
			printf("ERROR GET 1\n");
		}

		if (cc != (uint8_t)43) { 

			tmp_string[i] = (char)cc;   
			i++;
		}
		else { 
			tmp_string[i] = '\0';   //na mjestu plusa stavi terminator
			i = 0;
			printf("String sa prvog kanala serijske: %s \n", tmp_string);
			strcpy(string_queue, tmp_string);  //kopira string u red

			if (xQueueSend(serial_queue, &string_queue, 0) != pdTRUE) {     //smjestamo u red sve sa kanala1
				printf("ERROR GET\n");
			}
		}
	}
}

/*LEDOVKE*/
static void LED_bar_task(void* pvParameters) { 
	uint8_t led_tmp, led_temp = 0;

	static char tmp_string[20];

	for (;;) {
		printf("LED FUNKCIJA\n");
		if (xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY) != pdTRUE) { // take semafor ledovke 
			printf("ERROR TAKE \n");
		}
		//ovdje je dosao ako je neko kliknuo na led bar, ako se desio interrupt za ledovke
		get_LED_BAR((uint8_t)0, &led_tmp);//ocitavamo stanje nultog stupca led bara

		if((led_tmp & 0x01) != 0) {
			ukljuceno_1 = 1;
		}
		else
		{
			ukljuceno_1 = 0;
		}
		printf("Ukljuceno1 je: %d\n", ukljuceno_1);

		if ((led_tmp & 0x02) != 0) {
			ukljuceno_2 = 1;
		}
		else
		{
			ukljuceno_2 = 0;
		}
		printf("Ukljuceno2 je: %d\n", ukljuceno_2);

		if ((led_tmp & 0x04) != 0) {
			ukljuceno_3 = 1;
		}
		else
		{
			ukljuceno_3 = 0;
		}
		printf("Ukljuceno3 je: %d\n", ukljuceno_3);

		if ((led_tmp & 0x08) != 0) {
			ukljuceno_4 = 1;
		}
		else
		{
			ukljuceno_4 = 0;
		}
		printf("Ukljuceno4 je: %d\n", ukljuceno_4);

		if ((led_tmp & 0x16) != 0) {
			ukljuceno_5 = 1;
		}
		else
		{
			ukljuceno_5 = 0;
		}
		printf("Ukljuceno5 je: %d\n", ukljuceno_5);

	}
}

/*OBRADA SENZORA*/
static void Data_proc_task(void* pvParameters) {
	static char string_red[100];
	static uint8_t index_v = 0, index_v1 = 0;
	static uint16_t stotine, desetice, jedinice;
	static float v_suma = (float)0, v_suma1 = (float)0;

	for (;;)
	{
		printf("Maksimalna brzina pri kojoj se prozori zatvaraju je: %d\n", vmax);
		printf("Trenutna brzina auta je: %d\n", v_trenutno);

		if (xQueueReceive(serial_queue, &string_red, portMAX_DELAY) != pdTRUE) {  // iz reda smjestamo u string
			printf("ERROR\n");
		}

		string_red[duzina] = '\0'; // zavrsi prepisivanje reda
		//("Primljen je red : %s \n", string_red); // ispisi taj red

		//STRCMP ako su jednaka dva stringa vraca 0

		if (strcmp(string_red, "automatski\0") == 0) {//automatski
			flag_info = 1;
			flag_rezim = 1;
			manuelno_automatski = 1;

			printf("rezim rada %d\n", manuelno_automatski);
		}

		else if (strcmp(string_red, "manuelno\0") == 0) {//manuelno
			flag_info = 1;
			flag_rezim = 1;
			manuelno_automatski = 0;

			if (manuelno_automatski == 0) {

				if (ukljuceno_1 != 0) {
					set_LED_BAR((uint8_t)1, 0xff);
					printf("Setovana prva ledovka\n");
				}
				else {
					set_LED_BAR((uint8_t)1, 0x00);
					printf("Prva ledovka nije setovana\n");
				}

				if (ukljuceno_2 != 0) {
					set_LED_BAR((uint8_t)2, 0xff);
					printf("Setovana druga ledovka\n");
				}

				else {
					set_LED_BAR((uint8_t)2, 0x00);
					printf("Druga ledovka nije setovana\n");
				}

				if (ukljuceno_3 != 0) {
					set_LED_BAR((uint8_t)3, 0xff);
					printf("Setovana treca ledovka\n");
				}

				else {
					set_LED_BAR((uint8_t)3, 0x00);
					printf("Treca ledovka nije setovana\n");
				}

				if (ukljuceno_4 != 0) {
					set_LED_BAR((uint8_t)4, 0xff);
					printf("Setovana cetvrta ledovka\n");
				}

				else {
					set_LED_BAR((uint8_t)4, 0x00);
					printf("Cetvrta ledovka nije setovana\n");
				}
			}
			printf("Rezim rada %d\n", manuelno_automatski);
		}

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

			//set_LED_BAR((uint8_t)0, 0x00);

			printf("Maksimalna zadata brzina je: %d\n", vmax);
		}

		/*else if ((string_red[0] == '0' || string_red[0] == '1')    //citanje nultog stupca
			&& (string_red[1] == '0' || string_red[1] == '1')
			&& (string_red[2] == '0' || string_red[2] == '1')
			&& (string_red[3] == '0' || string_red[3] == '1')) {

			taster_display = (uint8_t)string_red[4] - (uint8_t)48;

			flag_info = (uint8_t)0;


			if (manuelno_automatski == (uint8_t)0) { //manuelno

				levi_napred = (uint8_t)string_red[0] - (uint8_t)48;   //procitamo vrijednosti sa ledovki
				desni_napred = (uint8_t)string_red[1] - (uint8_t)48;
				levi_nazad = (uint8_t)string_red[2] - (uint8_t)48;
				desni_nazad = (uint8_t)string_red[3] - (uint8_t)48;

				printf("Levi prednji prozor %f\n", levi_napred);
				printf("Desni prednji prozor %f\n", desni_napred);
				printf("Levi zadnji prozor %f\n", levi_nazad);
				printf("Desni zadnji prozor %f\n", desni_nazad);
			}

		}*/
		//else if(strcmp(string_red, "reset"))
		
		else if (string_red[0] == 't' && string_red[1] == 'r' && string_red[2] == ' ') {

			flag_info = (uint8_t)0;

			if (manuelno_automatski == 0) {//manuelni rezim 

				if (string_red[3] == ' ') { // ako se unese dodatni razmak
					string_red[3] = '0'; // stampaj nulu
				}

				stotine = (uint16_t)string_red[3] - (uint16_t)'0';
				desetice = (uint16_t)string_red[4] - (uint16_t)'0';
				jedinice = (uint16_t)string_red[5] - (uint16_t)'0';

				v_trenutno1 = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
				v_trenutno = v_trenutno1;


				if (index_v < (uint8_t)10) { // treba da se izbroji 10 tr vrednosti za usrednjavanje
					v_suma = v_suma + (float)v_trenutno1;
					index_v++;
				}
				else {
					srednja_v = (float)v_suma / (float)10;
					v_suma = (float)0;
					index_v = 0;

					printf("Srednja brzina je: %f\n", srednja_v);
				}
				if (v_trenutno > vmax_d) {
					vmax_d = v_trenutno;
					printf("Nova maksimalna izmerena brzina auta je: %d\n", vmax_d);
				}

				//if (v_trenutno < vmax_d) {
				//set_LED_BAR((uint8_t)0, 0x00);
				//printf("Resetovan je bar 0\n");
				//}
				if (vmin_d == 0)
					vmin_d = v_trenutno;
				else if (v_trenutno < vmin_d) {
						vmin_d = v_trenutno;
						printf("Nova minimalna izmerena brzina auta je: %d\n", vmin_d);
				}
				v_trenutno1 = (uint16_t)0;
			}

			else if (manuelno_automatski == 1) { // automatski rezim

				if (string_red[8] == ' ') {
					string_red[8] = '0';
				}

				stotine = (uint16_t)string_red[8] - (uint16_t)48;
				desetice = (uint16_t)string_red[9] - (uint16_t)48;
				jedinice = (uint16_t)string_red[10] - (uint16_t)48;

				v_trenutno1 = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
				v_trenutno = v_trenutno1;

				if (index_v1 < (uint8_t)10) { // treba da se izbroji 10 tr vrednosti za usrednjavanje
					v_suma1 += (float)v_trenutno;
					index_v1++;
				}
				else {
					srednja_v = (float)v_suma1 / (float)10;
					v_suma1 = (float)0;
					index_v1 = 0;

					printf("Srednja brzina je: %.1f\n", srednja_v);
				}

				if (srednja_v < (float)vmax) {   //ako je manja, ostavi iste vrijednosti sa senzora
					//levi_napred = (uint8_t)string_red[0] - (uint8_t)48;   //izdvojimo sa tih pozicija, oduzmemo 48 da pretvorimo u cifre
					//desni_napred = (uint8_t)string_red[2] - (uint8_t)48;
					//levi_nazad = (uint8_t)string_red[4] - (uint8_t)48;
					//desni_nazad = (uint8_t)string_red[6] - (uint8_t)48;

					printf("Stanje prozora: %.1f, %.1f, %.1f, %.1f\n", levi_napred, desni_napred, levi_nazad, desni_nazad);
				}
				else {   //ako je Vsrednja > Vmax zatvaramo prozore
					levi_napred = 1;
					desni_napred = 1;
					levi_nazad = 1;
					desni_nazad = 1;

					printf("Prozori se zatvaraju zbog prevelike brzine\n");
					printf("Novo stanje prozora: %.1f, %.1f, %.1f, %.1f\n", levi_napred, desni_napred, levi_nazad, desni_nazad);
				}

				v_trenutno1 = (uint16_t)0;

			}

		}

		else if (string_red[0] == 'n' && string_red[1] == 'i' && string_red[2] == 'v'
			&& string_red[3] == 'o' && string_red[4] == ' '
			&& (string_red[5] == '1' || string_red[5] <= '2' || string_red[5] == '3' || string_red[5] <= '4')
			&& string_red[6] == ' ' && (string_red[7] == '0' || string_red[7 ] == '1') && string_red[8] == '.' 
			&& (string_red[9] == '0' || string_red[9] == '1' || string_red[9] == '2' || string_red[9] == '3' 
			|| string_red[9] == '4' || string_red[9] == '5' || string_red[9] == '6' || string_red[9] == '7' || string_red[9] == '8' || string_red[9] == '9')) {

			prozor = (uint8_t)string_red[5] - (uint8_t)48; // 48 je u ascii 0, broj prozora koji se kontrolise
			//up_down = (uint8_t)string_red[7] - (uint8_t)48; // ili je skroz spusten prozor ili je skroz podignut
			uint8_t jedinica = string_red[7] - (uint8_t)48;
			uint8_t decimala = string_red[9] - (uint8_t)48;
			switch (prozor) {
				case 1:
					levi_napred = (float)jedinica + (float)decimala * (float)0.1;
					break;
				case 2:
					desni_napred = (float)jedinica + (float)decimala * (float)0.1;
					break;
				case 3:
					levi_nazad = (float)jedinica + (float)decimala * (float)0.1;
					break;
				case 4:
					desni_nazad = (float)jedinica + (float)decimala * (float)0.1;
					break;
			}
				printf("prozor %d: nivo %d.%d\n", prozor, jedinica, decimala);
			flag_info = 1;
		}
		else {
			printf("...\n");
		}
	}
}

/*SLANJE PODATAKA NA PC*/
static void Send_PC_to_CH1_task(void* pvParameters) {
	static char string_man[10], string_auto[12], tmp_string[50], tmp_string1[10], tmp_string2[10];
	static uint8_t i = 0, a = 0, b = 1, c = 0;
	static uint8_t tmp_cifra = 0;
	static uint16_t tmp_broj = 0;
	static int brojac = 0;

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

	string_man[0] = 'm';
	string_man[1] = 'a';
	string_man[2] = 'n';
	string_man[3] = 'u';
	string_man[4] = 'e';
	string_man[5] = 'l';
	string_man[6] = 'n';
	string_man[7] = 'o';
	string_man[8] = '\0';

	for (;;)
	{

		if (xSemaphoreTake(Send_BinarySemaphore, portMAX_DELAY) != pdTRUE) {    //brojac     0-25   200ms*25=5s
			printf("TAKE ERROR\n");
		};

		if ((srednja_v > (float)vmax) && (rezim_rada == (uint8_t)1)) { // automatski rezim, zatvaranje prozora
			levi_napred = 1;
			desni_napred = 1;
			levi_nazad = 1;
			desni_nazad = 1;
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
			printf("Srednja brzina je: %d\n", tmp_broj);

			printf("Levi prednji prozor:  %.1f\n", levi_napred);
			printf("Desni prednji prozor: %.1f\n", desni_napred);
			printf("Levi zadnji prozor:   %.1f\n", levi_nazad);
			printf("Desni zadnji prozor:  %.1f\n", desni_nazad);

			printf("-----------------------------\n");

			while (tmp_broj != (uint16_t)0) {
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

		else if (flag_info == (uint8_t)1) {

			if (manuelno_automatski == (uint8_t)0) {     //manuelno
				for (i = 0; i <= (uint8_t)8; i++) {
					if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)string_man[i]) != 0) {
						printf("SEND ERROR \n");
					}
					vTaskDelay(pdMS_TO_TICKS(100)); // ubacujemo delay izmedju svaka dva karaktera
				}
			}
			else if (manuelno_automatski == (uint8_t)1) {                 //automatski
				for (i = 0; i <= (uint8_t)10; i++) {
					if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)string_auto[i]) != 0) {
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

				for (i = 0; i <= (uint8_t)8; i++) {  //for prolazi kroz cijeli string i ispisuje kako izgledaju senzori
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
	static uint16_t tmp_broj = 0, tmp_broj1 = 0;

	for (;;)
	{
		if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) {  //brojac  2-7  200ms*5=1s za osvjezavanje displeja
			printf("ERROR TAKE\n");
		}

		if (select_7seg_digit(0) != 0) {  //selektujemo nultu cifru
			printf("ERROR SELECT \n");
		}
		if (set_7seg_digit(hexnum[manuelno_automatski]) != 0) {    //postavimo rezim rada
			printf("ERROR SET \n");
		}

		tmp_broj = (uint8_t)v_trenutno; //ispisujemo trenutnu brzinu

		if (tmp_broj != (uint8_t)0) { // 123

			jedinice = (uint8_t)tmp_broj % (uint8_t)10;
			desetice = (uint8_t)(tmp_broj / (uint8_t)10) % (uint8_t)10;
			stotine = (uint8_t)tmp_broj / (uint8_t)100;

			if (select_7seg_digit(4) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[jedinice]) != 0) {
				printf("ERROR SET \n");
			}

			if (select_7seg_digit(3) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[desetice]) != 0) {
				printf("ERROR SET \n");
			}

			if (select_7seg_digit(2) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[stotine]) != 0) {
				printf("ERROR SET \n");
			}

		}

		if (ukljuceno_5 == (uint8_t)1)        // kada je pritisnut taster za displej na led baru
			tmp_broj1 = vmax_d;
		else
			tmp_broj1 = vmin_d;

		if (tmp_broj1 != (uint8_t)0) { //123

			jedinice1 = tmp_broj1 % 10;
			desetice1 = (tmp_broj1 / 10) % 10;
			stotine1 = tmp_broj1 / 100;

			if (select_7seg_digit(8) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[jedinice1]) != 0) {
				printf("ERROR SET \n");
			}

			if (select_7seg_digit(7) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[desetice1]) != 0) {
				printf("ERROR SET \n");
			}

			if (select_7seg_digit(6) != 0) {
				printf("ERROR SELECT \n");
			}

			if (set_7seg_digit(hexnum[stotine1]) != 0) {
				printf("ERROR SET \n");
			}

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
