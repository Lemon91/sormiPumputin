/*
 * sormiPumputin.c
 *
 * Created: 17.9.2015 21:19:41
 *  Author: Lemon
 */

//775 = 147.8v
//740 = 129v
//720 = 121v
//700 = 114v
//650 = 101v
//600 = 91v

//80  = 12.0v
//90  = 13.5v
//100 = 14.9v
//120 = 17.8v
//140 = 20.7v
//160 = 23.6v
//180 = 26.5v
//200 = 29.4v
//220 = 32.4v
//240 = 35.3v
//260 = 38.2v
//271 = 39.8v	MAX!!!
//y = 0,1473x

 

//D:n 4 vikaa varataan lsd n�yt�lle tavun l�hett�miseen
//PD2 napille 3. D2
//PB2 D10 booster hakkaus OC1B. v�hint��n 10khz taajuudella pulssisuhteella 85:15?
//A1, eli PC1 napille 4
//PB0 on kytketty Register Selectiin ja PB1 Enableen
//PB2 ekalle napeille. D10
//PB3 napille 2. D11
//PC2 kuorman tyhjennys. PC2 eli A2

#define F_CPU 16000000UL //16 MHz kellotaajuus

#include <stdio.h>
#include <avr/io.h>			//t�st� taisi tulla k�ytt��n i/o?
#include <util/delay.h>		//delayt saa t�st�
#include <inttypes.h>		//eri bittisi� inttej� saa t�st�. muunmuassa int16_t
#include <stdbool.h>		//boolin saa k�ytt��n t�st�
#include <avr/interrupt.h>
#include <stdlib.h>

void laheta_komento( const uint8_t komento );
void laheta_data( const uint8_t komento );
void alusta(); //alustetaan lcd n�ytt� sopivaksi
void tyhjenna(); //tyhjennet��n lcd n�ytt� ja palautetaan kursori alkuun
void m( char tavu ); //kirjoitetaan lcd n�yt�n kursorin osoittamaan kohtaan haluttu merkki ja liikutetaan kursoria askel eteenp�in
bool mb( char tavu ); //sama kuin edell�, mutta palautetaan false, jos mentiin lcd n�yt�n loppuun
void mjono( const char* mjono ); //kirjoitetaan merkkijono lcd n�yt�lle k�ytt�en hy�dyksi edellist� funktiota

uint8_t Cursor; //kertoo kursorin sijainnin lcd n�yt�ll�. eka on kohassa 0, ekan rivin vika kohdassa 7. tokan rivin eka kohdassa 40 ja vika 47
void merkinAlustus(void);
uint8_t i_merkki = 0;

void purku(const uint16_t ms);


volatile uint16_t adc;
volatile uint8_t  latausKaynnissa = 0; //kun j�nnite ylitt�� halutun arvon, lopetetaan pulssin antaminen, mutta timer k�y edelleen

char buffer[7];

uint8_t modePurkuaika(void);
uint8_t modeLatausarvo(void);
uint8_t modeLataushz(void);
uint8_t modeLataussuhde(void);
uint8_t modePumppausmaara(void);
uint8_t modePumppausnopeus(void);
uint8_t modeNapkerroin(void);
uint8_t modeKasikaytto(void);
uint8_t modeAutolataus(void);
uint8_t modeLaukaisutesti(void);

void piezo(void);
volatile uint8_t autopurku = 0;

uint16_t purkuaika = 40; //purkamiseen k�ytetty aika millisekunteina
uint16_t latausarvo = 775; //j�nniteyksikk�� x
const uint16_t MAXlataus = 775; //y = 0,1473x.... noin 145v
uint16_t lataushz = 100; //khz. ei k�yt�ss� eik� tulossa k�ytt��n
uint16_t lataussuhde = 5; //kautta 32
uint16_t pumppausmaara = 2; //ei k�yt�ss�
uint16_t pumppausnopeus = 120; //purkuk�skyst� purkamisen aloittamiseen k�ytetty aika millisekunteina
uint16_t napkerroin = 5; //yhden n�pp�inpainalluksen kerroin

void lataa(uint8_t mode); //1=k�sik�ytt�, 0=jatkuva
volatile uint8_t latauspois = 1; //pys�ytet��n koko timer, joka antaa pulssia

//-1 ei j�� looppaamaan. 0-4 j�� looppaamaan, kunnes nappulan nostaa yl�s
//1-4 return 1, jos loopissa painetaan pohjaan 1-4
uint8_t nap1(int8_t mod);
uint8_t nap2(int8_t mod);
uint8_t nap3(int8_t mod);
uint8_t nap4(int8_t mod);




int main(void){	
	DDRD =   0b11111000; //K�ytet��n koko D lsd n�yt�lle tavun l�hett�miseen. Ei en��, vaan vain 4 viimeist�.
	DDRB =   0b00000111; //B:st� 0 ja 1 k�ytet��n lcd n�yt�n enableen ja komennon ja charin erottamiseen. 2 on hakkuri eli D10 eli OC1B
	DDRC =   0b00000100; //purkupinni A2
	ADMUX =  0b01000000; //k�ytt�j�nite referenssin�. adc0
	ADCSRA = 0b11101111;
	ADCSRB = 0b00000000;
	//DIDR0 =  0b00111110; //disabloidaan adc muista paitsi adc0:sta
	PCICR =  0b00000010;
	
	
	
	alusta();
	mjono("Laite k�ynnistyi");
	sei();
	
	uint8_t (*nayttoMode[10])(void) = { //funktio pointtereita
		modeNapkerroin,
		modeLatausarvo,
		modePurkuaika,
		modePumppausnopeus,
		modeLaukaisutesti,
		0
	};
	
	uint8_t nayttoja = 0;
	for(; !(nayttoMode[nayttoja] == 0); ++nayttoja);//lasketaan n�ytt�modejen m��r�
	
	uint8_t i = 0;
    while(1)
    {
		if((*nayttoMode[i])()==1) ++i;
		else if( i == 0){
			i = nayttoja-1;//takaa ymp�ri
		} else --i;
		if(i == nayttoja) i = 0;//edest� ymp�ri
    }
}

uint8_t modePurkuaika(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("PurkuT:");
		mjono(utoa(purkuaika, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) purkuaika-=napkerroin;
		if(nap4(0)) purkuaika+=napkerroin;
	}
}
uint8_t modeLatausarvo(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("LatausV:");
		mjono(utoa(latausarvo, buffer, 10));
		mjono(" ");
		mjono(utoa(adc*0.1473, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)){
			latausarvo-=napkerroin;
			if(latausarvo > MAXlataus) latausarvo = 0;
		}
		if(nap4(0)){
			latausarvo+=napkerroin;
			if(latausarvo > MAXlataus) latausarvo = MAXlataus;
		}
	}
}
uint8_t modeLataushz(void){ //funktio tulevaisuuden varalle. lataushz:taan ei ole j�rke� koskea.
	while(1){
		_delay_ms(100);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("LatausKHz:");
		mjono(utoa(lataushz, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) lataushz-=napkerroin;
		if(nap4(0)) lataushz+=napkerroin;
	}
}
uint8_t modeLataussuhde(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("LatausSuhde:");
		mjono(utoa(lataussuhde, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) lataussuhde-=napkerroin;
		if(nap4(0)) lataussuhde+=napkerroin;
	}
}
uint8_t modePumppausmaara(void){ //ei k�yt�ss�, koska pumppausm��r� on 1, koska j�rjestelm� ei kykene nopeampaan
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("PumppausKPL:");
		mjono(utoa(pumppausmaara, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) pumppausmaara-=napkerroin;
		if(nap4(0)) pumppausmaara+=napkerroin;
	}
}
uint8_t modePumppausnopeus(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("PumpNopeus:");
		mjono(utoa(pumppausnopeus, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) pumppausnopeus-=napkerroin;
		if(nap4(0)) pumppausnopeus+=napkerroin;
	}
}
uint8_t modeNapkerroin(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("NapKerroin:");
		mjono(utoa(napkerroin, buffer, 10));
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(0)) napkerroin--;
		if(nap4(0)) napkerroin++;
	}
}
uint8_t modeKasikaytto(void){
	while(1){
		_delay_ms(100);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("3La:4Pu:");
		cli();
		uint16_t tmpAdc = adc;
		sei();
		mjono(utoa(tmpAdc, buffer, 10));
		m('E');
		switch(nap1(2)){
			case 1:
				return 0;
			case 2:
				alusta();
				_delay_ms(500);
		}
		if(nap2(0)) return 1;
		if(nap3(-1)) lataa(1);
		if(nap4(0)) purku(purkuaika);
	}
}
uint8_t modeAutolataus(void){
	while(1){
		_delay_ms(50);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("A3La:4Pu:");
		if(!latauspois){
			mjono("!!!");
			if(latausKaynnissa){
				mjono("???");
			}
		}
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(-1)){
			if(latauspois) lataa(0);
			else lataa(1);//lopettaa latauksen
		}
		if(nap4(0)) purku(purkuaika);
	}
}

void lataa(uint8_t mode){//0 on lataus itsest��n, 1 on k�sik�ytt� napista 3
	latauspois = 0;
	TCCR1A = 0b00100010;// pwm ocr1b. ylin ICR1:ss�. t�m� osittain m��ritelty TCCR1B:ss�
	ICR1 = 80; //16bit kello aloittaa alusta arvossa 160/2=100khz.... /2 koska phase correct laskee kumpaankin suuntaan
	TCNT1 = 1;
	TCCR1B = 0b00010001;//prescale 1
	TIMSK1 = 0b00000001;//interrupt alusta
	if(mode == 0){
		//lataa, kunnes j�nniteraja tulee vastaan ja jatkaa heti sen j�lkeen uudelleen
	} else {
		//lataa kun nappi3 pohjassa
		while(nap3(-1)){
			_delay_ms(30);
			merkinAlustus();
			tyhjenna();
			m(0x00);
			mjono("3La:4Pu:");
			cli();
			uint16_t tmpAdc = adc;
			sei();
			mjono(utoa(tmpAdc, buffer, 10));
			m('E');
		}
		TCCR1A = 0;
		TCCR1B = 0;
		TIMSK1 = 0;
		latauspois = 1;
	}
}

ISR(TIMER1_OVF_vect){//hakkuri
	if(latausKaynnissa==0){
		OCR1B = 0;
	}
	if(latauspois == 1){
		OCR1B  = 0;
		TCCR1A = 0;
		TCCR1B = 0;
		TIMSK1 = 0;
	}	

}

volatile uint8_t latausSaavutettu = 0;
ISR(ADC_vect){
	adc = ADC;
	if(adc > latausarvo){
		latausKaynnissa = 0; //pakotetaan lataus loppumaan, kun j�nniteraja tulee vastaan
		latausSaavutettu = 1;
	}
	else {
		latausKaynnissa = 1;
		//uint8_t tmp = lataussuhde;
		//if(tmp > 120) tmp = 120;
		uint16_t tmp = (uint16_t)(80*(1-80/adc));
		if(tmp<5) tmp = 5;
		if(tmp>65) tmp = 65;
		if(latausSaavutettu && tmp>60) tmp = 5;//j�nnite saavutettua ei tarvitse pit�� niin suurta pulssia en��
		OCR1B = tmp;
	}
}

uint8_t modeLaukaisutesti(void){
	while(1){
		_delay_ms(100);
		merkinAlustus();
		tyhjenna();
		m(0x00);
		mjono("LK");
		if(autopurku) mjono("��");
		if(!latauspois){
			mjono("!!!");
			if(latausKaynnissa){
				mjono("???");
			}
		}
		if(nap1(0)) return 0;
		if(nap2(0)) return 1;
		if(nap3(-1)){
			if(latauspois) lataa(0);
			else lataa(1);
		}
		if(nap4(0)) piezo();
	}
}
void piezo(void){
	if(autopurku){
		PCMSK1 = 0;
		autopurku = 0;
	}else{
		PCMSK1 = 0b00001000;
		autopurku = 1;
	}
}
ISR(PCINT1_vect){
	if(!latausSaavutettu) return;
	purku(purkuaika);
	PCMSK1 = 0;
}

uint8_t nap1(int8_t mod){
	if(PINB & 0b00010000){
		_delay_ms(1);
		if(mod != -1){
			while(PINB & 0b00010000){
				if(mod == 1 && nap1(-1)) return 2;
				if(mod == 2 && nap2(-1)) return 2;
				if(mod == 3 && nap3(-1)) return 2;
				if(mod == 4 && nap4(-1)) return 2;
			}
		}
		return 1;
	}
	return 0;
}
uint8_t nap2(int8_t mod){
	if(PINB & 0b00001000){
		_delay_ms(1);
		if(mod != -1){
			while(PINB & 0b00001000){
				if(mod == 1 && nap1(-1)) return 2;
				if(mod == 2 && nap2(-1)) return 2;
				if(mod == 3 && nap3(-1)) return 2;
				if(mod == 4 && nap4(-1)) return 2;
			}
		}
		return 1;
	}
	return 0;
}
uint8_t nap3(int8_t mod){
	if(PIND & 0b00000100){
		_delay_ms(1);
		if(mod != -1){
			while(PIND & 0b00000100){
				if(mod == 1 && nap1(-1)) return 2;
				if(mod == 2 && nap2(-1)) return 2;
				if(mod == 3 && nap3(-1)) return 2;
				if(mod == 4 && nap4(-1)) return 2;
			}
		}
		return 1;
	}
	return 0;
}
uint8_t nap4(int8_t mod){
	if(PINC & 0b00000010){
		_delay_ms(1);
		if(mod != -1){
			while(PINC & 0b00000010){
				if(mod == 1 && nap1(-1)) return 2;
				if(mod == 2 && nap2(-1)) return 2;
				if(mod == 3 && nap3(-1)) return 2;
				if(mod == 4 && nap4(-1)) return 2;
			}
		}
		return 1;
	}
	return 0;
}

volatile uint16_t kelloKierros = 0;
volatile uint16_t purkuKierros = 0;
void purku(const uint16_t ms){ //millisekunteja
	kelloKierros = (uint16_t)((double)(ms)/(0.512*2));
	purkuKierros = (uint16_t)((double)pumppausnopeus/(0.512*2));
	TCNT0  = 0b00000000;
	TCCR0B = 0b00000011; //prescaler 32 8bit timerille
	TIMSK0 = 0b00000001; //8b kello heitt�� interruptin, kun ylivuoto. 0.000512 sekunnin v�lein
}

ISR(TIMER0_OVF_vect){//8bit kellon ylivuoto interrupt k�sittelij�
	if(purkuKierros > 0){//ennen solenoidin purkua odotetaan haluttu aika
		purkuKierros--;
		return;
	}
	PORTC = PORTC | 0b00000100;//purkaminen aloitetaan
	if(kelloKierros == 0){//purkaminen lopetetaan
		TCCR0B = 0;
		PORTC = PORTC & 0b11111011;
		if(autopurku) PCMSK1 = 0b00001000;
		latausSaavutettu = 0;
		return;
	}
	kelloKierros--;
}

void alusta(){
	PORTB = 0b00000010|(PORTB & 0b11111100);
	PORTD = 0b00100000 & 0b11110000;
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	_delay_ms(5);
	laheta_komento( 0b00101100 ); //4bit, -2lines-, -5x10-
	laheta_komento( 0b00001100 ); //Display ON
	_delay_us(1);
	tyhjenna(); //ei taida olla t�rke� suorittaa alussa, mutta Cursor pit�� kuitenkin alustaa 0:ksi
}

void tyhjenna(){
	laheta_komento( 0b00000001 );
	Cursor = 0;
}

void laheta_komento( const uint8_t komento ){ //4bit
	PORTB = 0b00000010|(PORTB & 0b11111100);
	PORTD = (komento & 0b11110000)|(PORTD & 0b00001111);
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	_delay_ms(3);
	PORTB = 0b00000010|(PORTB & 0b11111100);
	PORTD = (komento << 4)|(PORTD & 0b00001111);//bit shift nelj� vasempaan
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	_delay_ms(3);//pisimp��n kest�v�t komennot vie 1.52ms
}

void laheta_data( const uint8_t komento ){ //4bit
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (komento & 0b11110000)|(PORTD & 0b00001111);
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (komento << 4)|(PORTD & 0b00001111);//bit shift nelj� vasempaan
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	_delay_us(50);//pisimp��n kest�v�t komennot vie 1.52ms
}

void m( char tavu ){
	if( Cursor == 7+1 ){
		Cursor = 40;
		laheta_komento( 0b10101000 );
	}
	if( Cursor == 47+1 ){
		Cursor = 0;
		laheta_komento( 0b00000010 );
	}
	if( tavu == '�' ) tavu = 0b11100001; //korjataan �
	if( tavu == '�' ) tavu = 0b11101111; //korjataan �
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (tavu & 0b11110000)|(PORTD & 0b00001111);
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (tavu << 4)|(PORTD & 0b00001111);//bit shift nelj� vasempaan
	_delay_us(100);
	PORTB = PORTB & 0b11111100;
	_delay_us(50); //vie aikaa 37�s
	Cursor++;
}

bool mb( char tavu ){
	if( Cursor == 7+1 ){
		Cursor = 40;
		laheta_komento( 0b10101000 );
	}
	if( Cursor == 47+1 ){
		Cursor = 0;
		laheta_komento( 0b00000010 );
		return 0;
	}
	if( tavu == '�' ) tavu = 0b11100001; //korjataan �
	if( tavu == '�' ) tavu = 0b11101111; //korjataan �
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (tavu & 0b11110000)|(PORTD & 0b00001111);
	_delay_us(40);
	PORTB = PORTB & 0b11111100;
	PORTB = 0b00000011|(PORTB & 0b11111100);
	PORTD = (tavu << 4)|(PORTD & 0b00001111);//bit shift nelj� vasempaan
	_delay_us(40);
	PORTB = PORTB & 0b11111100;
	_delay_us(40); //vie aikaa 37�s
	Cursor++;
	return 1;
}

void mjono( const char* mjono ){
	int8_t i = 0;
	while( i<16 && mjono[0] != 0 ){
		if ( !mb( mjono[0] ) ){
			return;
		}
		mjono++;
		i++;
	}
}

void merkinAlustus(void){
	if( i_merkki == 0 ){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b10000);
		laheta_data(0b01000);
		laheta_data(0b00100);
		laheta_data(0b00010);
		laheta_data(0b00001);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 1){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b11000);
		laheta_data(0b00100);
		laheta_data(0b00011);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 2){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b11111);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 3){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00001);
		laheta_data(0b01110);
		laheta_data(0b10000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 4){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00001);
		laheta_data(0b00010);
		laheta_data(0b00100);
		laheta_data(0b01000);
		laheta_data(0b10000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 5){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00010);
		laheta_data(0b00010);
		laheta_data(0b00100);
		laheta_data(0b01000);
		laheta_data(0b01000);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 6){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b00100);
		laheta_data(0b00100);
		laheta_data(0b00100);
		laheta_data(0b00100);
		laheta_data(0b00100);
		laheta_data(0b00000);
		laheta_data(0b00000);
		} else if( i_merkki == 7){
		laheta_komento(0x40);
		laheta_data(0b00000);
		laheta_data(0b01000);
		laheta_data(0b01000);
		laheta_data(0b00100);
		laheta_data(0b00010);
		laheta_data(0b00010);
		laheta_data(0b00000);
		laheta_data(0b00000);
	}
	i_merkki++;
	if(i_merkki == 8) i_merkki = 0;
}