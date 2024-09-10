/*  Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/*  Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

/*  Hardware simulator utility functions  */
#include "HW_access.h"

/*  SERIAL SIMULATOR CHANNEL TO USE  */
#define COM_CH_0 (0)
#define COM_CH_1 (1)

/*  TASK PRIORITIES  */
#define receive_ch0       (tskIDLE_PRIORITY + (UBaseType_t)6)  /*  Rec_sens_CH0_task  */
#define ledovke           (tskIDLE_PRIORITY + (UBaseType_t)5)  /*  LED_bar_task  */
#define receive_ch1       (tskIDLE_PRIORITY + (UBaseType_t)4)  /*  Rec_PC_CH1_task  */
#define data_processing   (tskIDLE_PRIORITY + (UBaseType_t)3)  /*  Data_proc_task  */
#define send_ch1          (tskIDLE_PRIORITY + (UBaseType_t)2)  /*  Send_PC_to_CH1_task  */
#define display           (tskIDLE_PRIORITY + (UBaseType_t)1)  /*  Disp_task  */

void main_demo(void);

typedef float my_float;

/*  TASKS: FORWARD DECLARATIONS  */
static void Send_PC_to_CH1_task(void* pvParameters);
static void Rec_PC_CH1_task(void* pvParameters);
static void Rec_sens_CH0_task(void* pvParameters);
static void LED_bar_task(void* pvParameters);
static void Data_proc_task(void* pvParameters);
static void Disp_task(void* pvParameters);
static void TimerCallBack(TimerHandle_t timer);

static void Parse_velocity_to_digits(uint8_t *global_max, uint8_t* stotine, uint8_t* desetice, uint8_t* jedinice);


/*  TRASNMISSION DATA - CONSTANT IN THIS APPLICATION  */
const char trigger[] = "Pozdrav svima\n";
unsigned volatile t_point;

/*  RECEPTION DATA BUFFER  */
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];

static uint8_t prozor = 0, duzina, duzina1, flag_info = 0, flag_rezim = 0, rezim_rada = 0, taster_display = 0;
static uint8_t r_point, up_down, manuelno_automatski = 0;
static uint8_t levi_napred = 0, desni_napred = 0, levi_nazad = 0, desni_nazad = 0;

static uint8_t v_trenutno = 0, v_trenutno1 = 0, vmax = 130, vmax_d = 0, vmin_d = 0;
static char vmax_string[4];
static float srednja_v = 0;  /*  dont use it   */
static uint8_t ukljuceno_1 = 0, ukljuceno_2 = 0, ukljuceno_3 = 0, ukljuceno_4 = 0;
static uint8_t ukljuceno_5 = 0, ukljuceno_6 = 0, ukljuceno_7 = 0;

static uint8_t global_min_brzina = 0;
static uint8_t global_max_brzina = 0;
static uint8_t average_vel[10] = {0};
uint8_t *average_vel_ptr = average_vel;

/*  7-SEG NUMBER DATABASE - ALL HEX DIGITS  */
static const unsigned char hexnum[] = {
    0x3F, 0x06, 0x5B, 0x4F,  /*  0, 1, 2, 3  */
    0x66, 0x6D, 0x7D, 0x07,  /*  4, 5, 6, 7  */
    0x7F, 0x6F, 0x77, 0x7C,  /*  8, 9, A, B  */
    0x39, 0x5E, 0x79, 0x71   /*  C, D, E, F  */
};

/* GLOBAL OS-HANDLES */
static SemaphoreHandle_t Display_BinarySemaphore;
static SemaphoreHandle_t Send_BinarySemaphore;
static SemaphoreHandle_t RXC_BinarySemaphore0;
static SemaphoreHandle_t RXC_BinarySemaphore1;
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t TXC_BinarySemaphore;

static TimerHandle_t tH;
static QueueHandle_t serial_queue;

/* INTERRUPT  , OVO JE DOBRO  */
static uint32_t prvProcessRXCInterrupt(void) {
  BaseType_t xHigherPTW = pdFALSE;

  //  interrupt sa kanala 0
  if (get_RXC_status(0)) {
    if (xSemaphoreGiveFromISR(RXC_BinarySemaphore0, &xHigherPTW) != pdTRUE) {
      printf("ERROR0 \n");
    }
  }

  //  interrupt sa kanala 1
  if (get_RXC_status(1)) {
    if (xSemaphoreGiveFromISR(RXC_BinarySemaphore1, &xHigherPTW) != pdTRUE) {
      printf("ERROR1 \n");
    }
  }

  // povratak u task pre interrupt-a
  portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

/*  INTERRUPT ZA LEDOVKE OVO JE DOBRO  */
static uint32_t OnLED_ChangeInterrupt() {
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  printf("LED Interrupt\n");
  if (xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &higherPriorityTaskWoken) != pdTRUE) {
    printf("ERROR\n");
  }

  portYIELD_FROM_ISR((uint32_t)higherPriorityTaskWoken);
}

/*  TIMER CALLBACK FUNCTION  */
static void TimerCallBack(TimerHandle_t timer) {
  uint32_t cnt_1 = 0, cnt_2 = 0;
  const uint32_t count_to_5 = (uint32_t)5;
  const uint32_t count_to_25 = (uint32_t)25;

  /* slanje info na svakih 200ms */
  if (send_serial_character((uint8_t)COM_CH_0, (uint8_t)'T') != 0) {
    printf("ERROR TRANSMIT \n");
  }

  cnt_1++;
  cnt_2++;

  /* 25 * 200ms = 5000ms */
  if (cnt_1 == count_to_25) {
    cnt_1 = (uint32_t)0;

    if (xSemaphoreGive(Send_BinarySemaphore, 0) != pdTRUE) {
      printf("ERROR GIVE");
    }
  }
  
  if (cnt_2 == count_to_5) {
    cnt_2 = (uint32_t)0;

    if (xSemaphoreGive(Display_BinarySemaphore, 0) != pdTRUE) {
    printf("DISPLAY ERROR\n");
    }
  }
}

/*  RECEIVE0  */
static void Rec_sens_CH0_task(void* pvParameters) {
  uint8_t cc;
  static char tmp_string[100], string_queue[100];
  static uint8_t j = 0; // promenljiva za indeks niza

  while ((uint8_t)1) {

    if (xSemaphoreTake(RXC_BinarySemaphore0, portMAX_DELAY) != pdTRUE) {
      printf("ERROR TAKE \n"); 
    }

    if (get_serial_character(COM_CH_0, &cc) != 0) {   
      printf("ERROR GET \n");
    }

    if (cc != (uint8_t)43) {      
      tmp_string[j] = (char)cc;  
      j++;
    }

    else {
      tmp_string[j] = '\0';    
      j = 0;

      duzina = (uint8_t)strlen(tmp_string) % (uint8_t)12;  
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

/*  RECEIVE1  TREBA MENJATI  */
static void Rec_PC_CH1_task(void* pvParameters) {
  uint8_t cc = 0;
  static char tmp_string[100], string_queue[100];
  uint8_t i = 0;

  while ((uint8_t)1) {
    if (xSemaphoreTake(RXC_BinarySemaphore1, portMAX_DELAY) != pdTRUE) {
      printf("ERROR TAKE 1 \n");
    }

    /*  karakter ubaci na ch1  */
    if (get_serial_character(COM_CH_1, &cc) != 0) {
      printf("ERROR GET 1\n");
    }

    if (cc != (uint8_t)43) {
      tmp_string[i] = (char)cc;
      i++;
    } else {
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

static void led_bar_check(uint8_t led_stanje, uint8_t *led_bar, const uint8_t led_mask,
const uint8_t bar_number) {
  *led_bar = (led_stanje & led_mask) ? (uint8_t)1 : (uint8_t)0;
  if (*led_bar != (uint8_t)0) {
    printf("LED %d -- > ON \n", bar_number);
  } else {
    printf("LED %d -- > OFF \n", bar_number);
  }
}


/*  LEDOVKE  ODLICNO  */
static void LED_bar_task(void* pvParameters) {
    uint8_t led_stanje;

    uint8_t max_vel_jedinice = 0;
    uint8_t max_vel_desetice = 0;
    uint8_t max_vel_stotine = 0;

    const uint8_t LED_MASK_1 = (uint8_t)0x01;
    const uint8_t LED_MASK_2 = (uint8_t)0x02;
    const uint8_t LED_MASK_3 = (uint8_t)0x04;
    const uint8_t LED_MASK_4 = (uint8_t)0x08;
    const uint8_t LED_MASK_5 = (uint8_t)0x10;
    const uint8_t LED_MASK_6 = (uint8_t)0x20;
    const uint8_t LED_MASK_7 = (uint8_t)0x40;

    while ((uint8_t)1) {
        printf("-->Ocitavanje LEDovki \n");

        if (xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
            printf("ERROR TAKE \n");
        }
        /*  Interupt se desio --> pogledaj sta je na prvom LED baru */
        get_LED_BAR((uint8_t)0, &led_stanje);

        led_bar_check(led_stanje, &ukljuceno_1, LED_MASK_1, 1);
        led_bar_check(led_stanje, &ukljuceno_2, LED_MASK_2, 2);
        led_bar_check(led_stanje, &ukljuceno_3, LED_MASK_3, 3);
        led_bar_check(led_stanje, &ukljuceno_4, LED_MASK_4, 4);
        led_bar_check(led_stanje, &ukljuceno_5, LED_MASK_5, 5);
        led_bar_check(led_stanje, &ukljuceno_6, LED_MASK_6, 6);
        led_bar_check(led_stanje, &ukljuceno_7, LED_MASK_7, 7);
        /* Consider adding a function for this ... */
        if (manuelno_automatski == (uint8_t)1) {
            if (select_7seg_digit(0)) {
                printf("ERROR SELECT \n");
            }

            if (set_7seg_digit(hexnum[manuelno_automatski])) {
                printf("ERROR SET \n");
            }

            if (ukljuceno_1 == (uint8_t)1) {
            
                set_LED_BAR(1, 0xFF);
            }
            else {
                set_LED_BAR(1, 0x00);
            }
            
            if (ukljuceno_2 == (uint8_t)1) {
            
                set_LED_BAR(2, 0xFF);
            }
            else {
                set_LED_BAR(2, 0x00);
            }
            
            if (ukljuceno_3 == (uint8_t)1) {
                //set_prozor_on(1);
                set_LED_BAR(3, 0xFF);
            }
            else {
                set_LED_BAR(3, 0x00);
            }
            
            if (ukljuceno_4 == (uint8_t)1) {
                //set_prozor_on(1);
                set_LED_BAR(4, 0xFF);
            }
            else {
                set_LED_BAR(4, 0x00);
            }
            
            if (ukljuceno_7 == (uint8_t)1) {
                //set_prozor_on(1);
                set_LED_BAR(3, 0xFF);
                set_LED_BAR(4, 0xFF);
            }
        } else {
            
            if (ukljuceno_7 == (uint8_t)1) {
                //set_prozor_on(1);
                set_LED_BAR(3, 0xFF);
                set_LED_BAR(4, 0xFF);
            }
        }
    }
}

/* Validate input manuelno  */
static uint8_t is_valid_manuelno(char* input) {

    if (strcmp(input, "manuelno") != 0) {
        return (uint8_t)0;
    }

    return (uint8_t)1;
}

/* Validate input automatikski */
static uint8_t is_valid_automatski(char* input) {

    if (strcmp(input, "automatski") != 0) {
        return (uint8_t)0;
    }

    return (uint8_t)1;
}

/* Validate input brzina */
static uint8_t is_valid_brzina_input(char *input) {

    
    if (strlen(input) != (uint8_t)10) {
        return (uint8_t)0;
    }

    if (strncmp(input, "brzina ", 7) != 0) {
        return (uint8_t)0;
    }
    
    for (uint8_t i = 7; i < (uint8_t)10; i++) {
        if (!isdigit((unsigned char)input[i])) {
            return (uint8_t)0;
        }
    }
    
    return (uint8_t)1;
}

/* Validate input 'nivo X Y' */
static uint8_t is_valid_nivo_input(const char *input) {
    if (strlen(input) != (uint8_t)10) {
        return (uint8_t)0;
    }

    if (strncmp(input, "nivo ", 5) != 0) {
        return (uint8_t)0;
    }

    if (!isdigit((unsigned char)input[5])) {
      return (uint8_t)0;
    }

    if (input[6] != ' ') {
      return (uint8_t)0;
    }
   
    if ((input[7] != '0') && (input[7] != '1')) {
        return (uint8_t)0;
    }

    for (uint8_t i = 8 ; i < (uint8_t)10 ; i++) {
        if (!isdigit((unsigned char)input[i])) {
            return (uint8_t)0;
        }
    }

    return (uint8_t)1;
}

/*  Function to determine LED state based on window position  */
static void update_LED_bar(uint8_t prozor_pos, uint8_t bar_pos) {
    const uint8_t LED_OFF = (uint8_t)0x00;
    const uint8_t FIRST_EIGHT = (uint8_t)0x01;     /*  0 to 31  */
    const uint8_t SECOND_EIGHT = (uint8_t)0x03;    /*  32 to 63  */
    const uint8_t THIRD_EIGHT = (uint8_t)0x07;     /*  64 to 95  */
    const uint8_t FOURTH_EIGHT = (uint8_t)0x0F;    /*  96 to 127  */
    const uint8_t FIFTH_EIGHT = (uint8_t)0x1F;     /*  128 to 159  */
    const uint8_t SIXTH_EIGHT = (uint8_t)0x3F;     /*  160 to 191  */
    const uint8_t SEVENTH_EIGHT = (uint8_t)0x7F;   /*  192 to 223  */
    const uint8_t EIGHT_EIGHT = (uint8_t)0xFF;     /*  224 to 255  */

    if (prozor_pos == (uint8_t)0) {
        set_LED_BAR(bar_pos, LED_OFF);
    } else if (prozor_pos < (uint8_t)13) {
        set_LED_BAR(bar_pos, FIRST_EIGHT);
    } else if (prozor_pos < (uint8_t)25) {
        set_LED_BAR(bar_pos, SECOND_EIGHT);
    } else if (prozor_pos < (uint8_t)38) {
        set_LED_BAR(bar_pos, THIRD_EIGHT);
    } else if (prozor_pos < (uint8_t)50) {
        set_LED_BAR(bar_pos, FOURTH_EIGHT);
    } else if (prozor_pos < (uint8_t)63) {
        set_LED_BAR(bar_pos, FIFTH_EIGHT);
    } else if (prozor_pos < (uint8_t)76) {
        set_LED_BAR(bar_pos, SIXTH_EIGHT);
    } else if (prozor_pos < (uint8_t)89) {
        set_LED_BAR(bar_pos, SEVENTH_EIGHT);
    } else if (prozor_pos <= (uint8_t)100) {
        set_LED_BAR(bar_pos, EIGHT_EIGHT);
    }
}

static void set_prozor_on(uint8_t prozor_pos) {
  const uint8_t LED_ON = (uint8_t)0xFF;
  set_LED_BAR(prozor_pos, LED_ON);
}

static void set_prozor_off(uint8_t prozor_pos) {
  const uint8_t LED_OFF = (uint8_t)0x00;
  set_LED_BAR(prozor_pos, LED_OFF);
}

static void print_stanje_prozora() {
  printf("-->Levi napred: %d%% \n" , levi_napred);
  printf("-->Desni napred: %d%% \n" , desni_napred);
  printf("-->Levi nazad: %d%% \n" , levi_nazad);
  printf("-->Desni nazad: %d%% \n" , desni_nazad);
}

/*  SAKULPLJANE SA ADV TERMINALA  */
static void Data_proc_task(void* pvParameters) {
  char string_red[20];
  uint8_t vmax_tr = 0;
  uint8_t index_v = 0, index_v1 = 0;
  uint8_t stotine = 0 , desetice = 0, jedinice = 0;
  float v_suma = (float)0, v_suma1 = (float)0;

  while ((uint8_t)1) {
    printf("-->Max brzina je : %d\n", vmax);
    printf("-->Tr. brzina je : %d\n", v_trenutno);
    printf("-->Sr. brinza je : %.2f\n", srednja_v);

    if (xQueueReceive(serial_queue, &string_red, portMAX_DELAY) != pdTRUE) {
      printf("ERROR\n");
    }

    string_red[duzina] = '\0';


    if (is_valid_automatski(string_red) == (uint8_t)1) {
      flag_info = 1;
      flag_rezim = 1;
      manuelno_automatski = 0;

      printf("-->Rezim rada je automatski\n");

      update_LED_bar(levi_napred, 1);
      update_LED_bar(desni_napred, 2);
      update_LED_bar(levi_nazad, 3);
      update_LED_bar(desni_nazad, 4);

      print_stanje_prozora();

    } else if (is_valid_manuelno(string_red) == (uint8_t)1) {
      flag_info = 1;
      flag_rezim = 1;
      manuelno_automatski = 1;

      printf("-->Rezim rada je manuelni\n");
    }

    if (is_valid_brzina_input(string_red) == (uint8_t)1) {
        /*  format je brzina X , gde je X u obliku broja od 3 cifre*/
        vmax_string[0] = string_red[7];
        vmax_string[1] = string_red[8];
        vmax_string[2] = string_red[9];
        vmax_string[3] = '\0';

        printf("BRZINA JE PRIMLJENA!");

        stotine = (uint16_t)string_red[7] - (uint16_t)48;
        desetice = (uint16_t)string_red[8] - (uint16_t)48;
        jedinice = (uint16_t)string_red[9] - (uint16_t)48;

        vmax_tr = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
        vmax = vmax_tr;
        vmax_tr = 0;

        flag_info = (uint8_t)0;

        /*  ovo treba slati na serijsku  */
        printf("-->Nova max brzina je : %d\n", vmax);
    } else if (string_red[0] == 't' && string_red[1] == 'r' && string_red[2] == ' ') {
      if (string_red[3] == ' ') { string_red[3] = '0'; }

      flag_info = 0;

      stotine = (uint16_t)string_red[3] - (uint16_t)'0';
      desetice = (uint16_t)string_red[4] - (uint16_t)'0';
      jedinice = (uint16_t)string_red[5] - (uint16_t)'0';

      v_trenutno1 = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;
      v_trenutno = v_trenutno1;

      printf("Trenutna brzina %d", v_trenutno);

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
    if ((manuelno_automatski == (uint8_t)0) && (srednja_v >= vmax)) {
        set_prozor_on(levi_napred);
        set_prozor_on(desni_napred);
        set_prozor_on(levi_nazad);
        set_prozor_on(desni_nazad);

        printf("-->Prevelika brzina zatvori prozore! \n");

        v_trenutno1 = 0;
      }
    }

    if ((is_valid_nivo_input(&string_red) == (uint8_t)1) && (manuelno_automatski == (uint8_t)0)) {
      stotine  = (uint16_t)string_red[7] - (uint16_t)48;
      desetice = (uint16_t)string_red[8] - (uint16_t)48;
      jedinice = (uint16_t)string_red[9] - (uint16_t)48;

      uint8_t procenat = (stotine * (uint16_t)100) + (desetice * (uint16_t)10) + jedinice;

      if (string_red[5] == '1') {
        levi_napred = procenat;
      } else if (string_red[5] == '2') {
        desni_napred = procenat;
      } else if (string_red[5] == '3') {
        levi_nazad = procenat;
      } else if (string_red[5] == '4') {
        desni_nazad = procenat;
      }

      update_LED_bar(levi_napred, 1);
      update_LED_bar(desni_napred, 2);
      update_LED_bar(levi_nazad, 3);
      update_LED_bar(desni_nazad, 4);

      flag_info = 1;

      print_stanje_prozora();
    } else {
      printf("...\n");
    }
  }
}

/*  SLANJE PODATAKA NA PC  DORADI  */

static void Send_PC_to_CH1_task(void* pvParameters) {
    static tmp_string[50], tmp_string1[10], tmp_string2[10];
    static uint8_t i = 0, a = 0, b = 1, c = 0;
    static uint8_t tmp_cifra = 0;
    static uint16_t tmp_broj = 0;
    static int brojac = 0;

    char string_ok[2] = "ok";

    while (1) {

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

/*
static void Update_display(uint8_t digit_pos, uint8_t *value) {
    if (select_7seg_digit(digit_pos) != (uint8_t)0) {
      printf("ERROR SELECT \n");
    }

    if (set_7seg_digit(hexnum[*value]) != (uint8_t)0) {
      printf("ERROR SET \n");
    }
}
*/

static void Parse_velocity_to_digits(uint8_t *vel, uint8_t* jedinice,
uint8_t* desetice, uint8_t* stotine) {
  *jedinice = *vel % 10;
  *desetice = (*vel / 10) % 10;
  *stotine = *vel / 100;
}

/*  DISP TASK  KAO DA GA NEPREPOZNAJE???  */
static void Disp_task(void* pvParameters) {
  uint8_t max_vel_jedinice = 0 , max_vel_desetice = 0 , max_vel_stotine = 0;
  uint8_t min_vel_jedinice = 0 , min_vel_desetice = 0 , min_vel_stotine = 0;

  while ((uint8_t)1) {
    if (xSemaphoreTake(Display_BinarySemaphore, portMAX_DELAY) != pdTRUE) {
          printf("ERROR TAKE\n");
    }

    printf("INSIDE DISPLAY TASK!!");

    //Update_display(1, &manuelno_automatski);
    if (select_7seg_digit(0)) {
        printf("ERROR SELECT \n");
    }

    if (set_7seg_digit(hexnum[manuelno_automatski])) {
        printf("ERROR SET \n");
    }


    if (global_max_brzina < v_trenutno) {
      global_max_brzina = v_trenutno;
    }

    if ((global_min_brzina == (uint8_t)0) || (global_min_brzina > v_trenutno)) {
      global_min_brzina = v_trenutno;
    }
      /*  Display max speed  */
    if (ukljuceno_5 == (uint8_t)1) {
      //Parse_velocity_to_digits(global_max_brzina, &max_vel_jedinice, &max_vel_desetice, &max_vel_stotine);
      max_vel_jedinice = global_max_brzina % 10;
      max_vel_desetice = (global_max_brzina / 10) % 10;
      max_vel_stotine = global_max_brzina / 100;

      printf("max_vel_jedinice --> %d", max_vel_jedinice);
      printf("max_vel_desetice --> %d", max_vel_desetice);
      printf("max_vel_stotine --> %d", max_vel_stotine);

      if (select_7seg_digit(4)) { printf("ERROR SELECT \n"); }

      if (set_7seg_digit(hexnum[max_vel_jedinice])) { printf("ERROR SET \n"); }

      if (select_7seg_digit(3)) { printf("ERROR SELECT \n"); }

      if (set_7seg_digit(hexnum[max_vel_desetice])) { printf("ERROR SET \n"); }

      if (select_7seg_digit(2)) { printf("ERROR SELECT \n"); }

      if (set_7seg_digit(hexnum[max_vel_stotine])) { printf("ERROR SET \n"); }
    }

    if (ukljuceno_6) {

        min_vel_jedinice = global_min_brzina % 10;
        min_vel_desetice = (global_min_brzina / 10) % 10;
        min_vel_stotine = global_min_brzina / 100;

        if (select_7seg_digit(8)) { printf("ERROR SELECT \n"); }

        if (set_7seg_digit(hexnum[min_vel_jedinice])) { printf("ERROR SET \n"); }

        if (select_7seg_digit(7)) { printf("ERROR SELECT \n"); }

        if (set_7seg_digit(hexnum[min_vel_desetice])) { printf("ERROR SET \n"); }

        if (select_7seg_digit(6)) { printf("ERROR SELECT \n"); }

        if (set_7seg_digit(hexnum[min_vel_stotine])) { printf("ERROR SET \n"); }
    }





    /*  Display min speed  */
    
  }
}

/*  MAIN - SYSTEM STARTUP POINT  */
void main_demo(void) {
  if (init_LED_comm() != 0) {
    printf("Neuspesna inicijalizacija \n");
  }

  if (init_7seg_comm() != 0) {
    printf("Neuspesna inicijalizacija \n");
  }
  /*  samo primaj podatke sa serijske  */
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

  /*  TASKS  */
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

  /*  idleHookCounter++;  */
}