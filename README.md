# Autoelektronika
Upravljanje polozajem prozora u zavisnosti od brzine vozila

## TO DO 

  Coming when it's ready 

## Added minor changes

  Still not  100% ready# Autoelektronika
Upravljanje polozajem prozora u zavisnosti od brzine vozila


## Pregled sistema
- [Autoelektronika](#autoelektronika)
  - [Pregled sistema](#pregled-sistema)
  - [Projekat izradili:](#projekat-izradili)
- [PERIFERIJE](#periferije)
- [OPIS TASKOVA](#opis-taskova)
  - [Rec\_sens\_CH0\_task](#rec_sens_ch0_task)
  - [Rec\_PC\_CH1\_task](#rec_pc_ch1_task)
  - [LED\_bar\_task](#led_bar_task)
  - [Data\_proc\_task](#data_proc_task)
  - [Send\_PC\_to\_CH1\_task](#send_pc_to_ch1_task)
  - [Disp\_task](#disp_task)
  - [TimerCallBack](#timercallback)
  - [Predlog simulacije sistema](#predlog-simulacije-sistema)

## Projekat izradili:

Nenad Petrovic EE69/2018

# PERIFERIJE
Simulacija senzora i displeja se vrsi pomocu periferija:
- AdvUniCom 
- Seg7_Mux
- LED_Bars

LED_bars_plus.exe prima 5 argumenata , malim slovima su oznaceni ulazni barovi , velikim slovima izlazni.Slova su R , G , B , Y , O.

Primer :
```
  rBGYO , bRRRO , oBYY , bR
```

Serijska komunikacija se vrsi pomocu AdvUniCom.exe, otvaranje kanala se vrsi dodavanjem broja sa imenom kanala.

Primer:
```
AdvUnicom 0 , AdvUnicom 1 
```
Seg7_Mux.exe otvara 7segmetni displej duzine 4-9 , ako se izostave argumenti otvara se 4 7segmenta displeja.

Primer:
```
Seg7_Mux 4 , Seg7_Mux 6 , Seg7_Mux 9 
```

# OPIS TASKOVA

## Rec_sens_CH0_task 
Ovaj task prima podatke sa senzora brzine i prozora. Karakteri se stavljaju u privremeni bafer (red) za ostale taskove. Kad stigne karakter sa kanala 0, desi se interrupt koji šalje semafor.

## Rec_PC_CH1_task 
Naredbe koje pristižu su :
- 'manuelno'
- 'automatski'  
- 'brzina X' 
- 'nivo X Y' 
- 'tr X'

Manuelno oznacava da se upravljanje vrsi preko LED barova.


Automatski znaci da se ignorisu LED barovi (osim ako nisu klknuti dugmici za max/min brzinu i 'Child Lock Sistem').


Brina je parametar koji zadaje max mogucu brzinu na koju se prozori zatvraju kada je srednja brzina veca od nje u automatskom rezimu rada.Brzina se zadaje u sledecem formatu : 

```
  brzina 134+ , brzina  66+
```


Nivo oznacava koji je zeljeni prozor i do kog nivoa se podize/spusta (0-100).Zadaje se u sledecem formatu:

```
  nivo 1 020+ , nivo 3 100+ , nivo 4 059+ , nivo 2 031+
```

Tr oznacava trenutnu brzinu kojom se vozilo krece.
```
tr 139+ , tr 150+ , tr  20+
```

## LED_bar_task
Ocitava stanje nultog LED bara i namesta odgovarajuce registre na ON/OFF u zavisnoti da li je dugme pritisnuto ili nije.
Validni dugmici su 1-7 pocevsi od dole.
Mapa dugmica je :
- 1-4 , prozori (napred levi/desni i nazad levi/desni)
- 5 , prikaz max izmerene brzine na LCD-u 
- 6 , prikaz min izmerene brzine na LCD-u
- 7 , 'Child Lock Sistem' zakljucava poslednja 2 prozora nezavisno od rezima
  

## Data_proc_task
Ovo je glavni task u sistemu koji prikuplja informacije sa senzora koje se simulira preko AdvUniCom sistema.Sa kanala 0 na kanal 1 dolaze podaci i bivaju obrađivani zavisno od izvora.

## Send_PC_to_CH1_task
Ovaj task šalje podatke sa kanala 0 na big box kanala 1.

## Disp_task
Prikazuje informacije na sedmosegmentnom displeju:

- Režim rada na nultoj poziciji (0 za automatski 1 za manuelno) 

- Max izmerenu brzinu na poziciji 3-5 
  
- Min izmerenu brzinu na poziciji 7-9

## TimerCallBack
Aktivira brojač svakih 200ms i šalje 'T' na kanal 0.  Displej se osvezi na svakih 1000ms i šalje podatke na kanal 1 svakih 5000ms.

## Predlog simulacije sistema

Ukljuciti sve periferije iz terminala.
AdvUnicom postaviti na 0 i na 1, LCD treba unesti sa 9 cifara , LED bar boje nisu bitne samo postaviti jedan ulazni i 4 izlazna bara.

Preko serijske komunikacije moguce je unositi max dozvoljenu brzinu u vec odredjenom obliku.

```
brzina 112+
```

Postavljati trenutnu brzinu komandom tr X.

Svaku komadnu zavrsiti sa '+' znakom (43 u ASCII).

Postaviti odredjeni prozor u zeljeno stanje , npr :

```
  nivo 2 040+
```

,potom u zavisnosti od rezima rada uneti manuelno/automatski.Na LCD-u prva cifra pokazuje rezim rada.Ako je uzeto manuelno potrebno je kliknuti na drugo dugme od dole na ulaznom LED baru i uneti komadnu 'manuelno+'.
Ako se ponovo kilikne na dugme i unese 'manuelno+' , prozor se zatvara.

Ako je rezim automatski nema potrebe za kliktanjem , samo se unosi 'nivo 3 040+' i komanda 'automatski+'.

Klikom na dugme 5 od dole na ulaznom LED baru prikazuje se max izmerena brzina koja je do sada uneta.
Ako je ovo dugme kliknutno svakim unosom nove max brzine LCD ce ispisivati novu brzinu , u kasnjenju od 5000ms.

Klikom na dugme 6 od dole na ulaznom LED baru prikazuje se min izmerena brzina koja je do sada uneta.
Ako je ovo dugme kliknutno svakim unosom nove min brzine LCD ce ispisivati novu brzinu , u kasnjenju od 5000ms.

U pocetku su min i max brzine jednake.

Klikom na dugme 7 od dole na ulaznom LED baru aktivira se "Child Lock Sistem".
To znaci da se zadnja dva prozora zatvaraju nebitno od njihovog dotadasnjeg stanja i rezima rada.
Kada se ovo dugme iskljuci prozori se vracaju u prethodno stanje.

Slanjem komande 'automatski+' i namestanjem trenutne brzine tako da srednja vrednost svih prethodno unetih 10 brzina bude veca od max dozvoljenje brzine aktivira se senzor koji zatvara sve prozore , nebitno od njihovog prethodnog stanja. 


