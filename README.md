# Autoelektronika

Sistem za kontrolu prozora u automobilu

## Projekat izradili:
Jelena Jović, EE18/2018
Ljubica Potrebić, EE26/2018

# PERIFERIJE
Periferije koje koristimo su LED_bars_plus, Seg7_Mux displej i AdvUniCom softver simulacije serijske komunikacije.
Pri pokretanju LED_bars_plus.exe navodimo rGBYO kao argument da bi se dobio led bar sa 1 ulaznim i 4 izlazna stupca. Ulazni stubac je crvene boje, izlazni stupci su redom zelene, plave, žute i narandžaste. 
Prilikom pokretanja softvera za serijsku komunikaciju AdvUniCom.exe dodajemo 0 za pokretanje nultog kanala, a 1 se dodaje na istu komandu kako bi se pokrenuo prvi kanal. 
Prilikom pokretanja displeja Seg7_Mux.exe dodajemo broj 9 kako bi se dobio displej sa 9 cifara.

# OPIS TASKOVA

## Rec_sens_CH0_task 
Ovaj task ima zadatak da primi podatke sa senzora brzine i prozora. Informacije koje pristižu treba da budu u formatu npr. 1 0 1 0 130+ .Ukoliko je podatak o brzini dvocifren broj, poruka treba da bude u formatu npr. 1 0 1 0  80+ (između informacije o prozorima i brzine treba da bude razmak više u odnosu na onaj sa trocifrenim brojem). Dobijeni karakteri se smještaju u niz, a zatim u red da bi bili na raspolaganju ostalim taskovima. Kad god stigne neki karakter sa kanala 0, desi se interrupt koji šalje semafor.

## Rec_PC_CH1_task 
Naredbe koje pritižu u ovom tasku sa kanala 0 su formata manuelno+, automatski+, vmax 150+, prozor 3 0+.Ovde dakle govorimo o režimu rada, maksimalnoj brzini posle koje se zatvaraju prozori i komandom npr. prozor 3 0+ zadaje se stanje svakog prozora pojedinačno. U okviru komande prozor, prvi parametar koji se prosleđuje govori o kom se prozoru radi (1 – levi prednji prozor, 2 desni prednji prozor, 3 – levi zadnji prozor, 4 – desni zadnji prozor), a drugi parametar govori o tome da li je prozor otvoren ili zatvoren (0 – prozor je potpuno otvoren, 1 – prozor je potpuno zatvoren). I ovaj task čeka semafor da bi se pokrenuo i izvršio. Takođe, daje interrupt svaki put kad pristigne karakter na kanal 1.

## LED_bar_task
Task pomoću kojeg zadajemo trenutno stanje prozora i pomoću kog na sedmosegmentnom displeju ispisujemo podatak o maksimalnoj izmerenoj brzini, i to pritiskom na odgovarajući taster ulaznog stupca  LED_bar_task-a.

## Disp_task
Služi za ispisivanje na sedmosegmentnom displeju. Na nultoj poziciji ispisujemo režim rada, na poziciji udaljenoj jedan razmak ispisujemo podatak o trenutnoj brzini, i razmak nakon toga ispisujemo maksimalnu brzinu (ovo se dešava samo ukoliko je pritisnut odgovarajuci taster na led baru). 

## Send_PC_to_CH1_task
Ovaj task služi da podatke poslate sa kanala 0 prikaže na big box-u kanala 1.

## Data_proc_task
Obrađujemo podatke u zavisnosti od toga iz kog taska su stigli. Podaci koji se obrađuju su režim rada, ispis na 7-segmentni displej, podaci sa senzora prozora i brzine, maksimalna brzina, kao i led bar.

## TimerCallBack
Aktivira funkciju za brojač svakih 200ms i pritom šalje 'T' na kanal 0. Takođe, svakih 1s osvježava displej i svakih 5s šalje potrebne podatke na kanal 1.

# PREDLOG SIMULACIJE SISTEMA
U padajućem meniju potrebno je izabrati Tools -> Command Prompt. U okviru Command Prompt-a uneti AdvUniCom.exe 0, a zatim i AdvUniCom.exe 1 kako bi se otvorila dva kanala serijske komunikacije. Nakon toga, takođe iz Command Prompt-a, otvoriti Seg7_Mux.exe 9 da bi se otvorio sedmosegmentni displej sa 9 cifara. Neophodan je i led bar koji se otvara komandom Led_bars_plus.exe rGBYO, gde se podrazumeva da je ulazni stubac crvene boje, a izlazni stupci su redom zelene, plave, žute i narandžaste boje. 
Posle kompajliranja sistema, sa kanala 0 šaljemo poruku u formatu npr. prozor 3 1+. Nakon toga, šalje se poruka formata npr. vmax 144+. Sve to prikazuje se u okviru big box-a na kanalu 1.
Zatim se podešavaju prekidači LED bar-a. Izbor prekidača svodi se na donjih 5 prekidača ulaznog stupca crvene boje – prva 4 prekidača se koriste da bi se kontrolisao nivo prozora (da li je prozor potpuno otvoren ili zatvoren), a peti prekidač sluzi za ispis maksimalne brzine pri kojoj prozori smeju da budu otvoreni na sedmosegmentnom displeju.
Komande koje se mogu slati nakon prethodno navedenih su manuelno+ ili automatski+. Ako se preko kanala 0 pošalje komanda manuelno+, očitava se stanje prekidača simuliranih preko LED bar-a, odnosno ako je pritisnut odgovarajuci prekidač, prozor se zatvara, što se na LED bar-u vidi kao paljenje čitavog njemu odgovarajuceg stupca. Ako se promene uključeni ili isključeni prekidači, potrebno je ponovo poslati komandu manuelno+. 
Pre nego što se pošalje komanda automatski+, potrebno je uneti komandu formata npr. 1 0 1 0 111+, što označava trenutno stanje prozora, kao I trenutnu brzinu kretanja automobila. Za unos 10 navedenih komandi, usrednjava se vrednost trenutne brzine i prikazuje se na terminalu. 
Zatim se može preći na komandu automatski+. Nakon njenog unosa, prozori se kontrolišu prema senzoru brzine, odnosno, ako je srednja vrednost brzine kretanja vozila veća nego maksimalna brzina pri kojoj prozori smeju da budu otvoreni, svi prozori se zatvaraju sve dok se srednja brzina ne spusti. Kad srednja brzina opadne ispod maksimalne dozvoljene, prozori se vraćaju u prethodno stanje.
Unos komandi manuelno+ ili automatski+ takodje se očitavaju na BigBox-u kanala 1.
Na sedmosegmentnom displeju prikazuje se sledeće: na nultoj poziciji govori se o režimu rada. Ako je prikazana nula, radi se o manuelnom režimu rada, a ako je prikazana jedinica, radi se o automatskom režimu. Nakon ove cifre sledi razmak, pa prikaz trenutne brzine kretanja vozila. Ako se radi o dvocifrenoj brzini, na mestu stotine biće upisana nula. Zatim sledi razmak i na poslednje 3 cifre prikazuje se maksimalna izmerena brzina vozila. Ova vrednost biće ispisana samo ukoliko je pritisnut peti prekidač na LED baru.


