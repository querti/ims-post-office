/**
 * Subor:         model.cc
 * Verzia:        1.2
 * Datum:		  November 2016
 *
 * Predmet:       IMS (2016)
 * Projekt:       Model posty
 *
 * Authory:       Lubomir Gallovic, 3BIT, [team-leader]
 *                Michal Ormos, 3BIT
 * 
 * Faculta:       Fakulta Informacnych Technologii,
 *                Vysoke uceni technicke v Brne,
 *                Czech Republic
 *
 * E-mails:       xgallo03@stud.fit.vutbr.cz
 *                xormos00@stud.fit.vutbr.cz
 *
 * Popis:         Implementacia spracovania zakaznikov pocas spicky na 
 *				  poste. Implementacia vyuziva SimLib/C++ simulacnu
 *				  kniznicu.
 */


#include "simlib.h"
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

using namespace std;

#define LIST_DODANIE 8
#define BALIK_DODANIE 9
#define PENIAZ_DODANIE 10
#define LIST_PODANIE 11
#define BALIK_PODANIE 12
#define PENIAZ_PODANIE 13

//modifikacie
//***************************************************************//

#define DOBA_SIMULACIE 10800 //3h

#define POCET_PREPAZOK 10

#define PRICHOD_ZAKAZNIKOV 10 //kazdych tolko sekund

#define PRAVDEPODOBNOST_NEVYPISANEHO_DOKLADU 0.27 //ak je zakaznik pri prepazke ale zabudol si vypisat tlacivo,
//v skutocnosti 13% avsak 27% z ludi ktory podavaju a nevypisali si tlacivo pri prichode

#define DOBA_VYPISOVANIA 360
#define PRAVDEPODOBNOST_VYPISOVANIA 0.4 //ak si potrebuje vypisat tlacivo na poste pri vchode, v skutocnosti je to 32% avsak je to 40% z 80%

#define PRAVDEPODOBNOST_PODANIA 0.8

#define PRAVDEPODOBNOST_PODANIA_LISTU 0.67
#define PRAVDEPODOBNOST_PODANIA_BALIKU 0.15

#define PRAVDEPODOBNOST_VIACERYCH_BALIKOV 0.2
#define ROZMEDZIE_VIACERYCH_BALIKOV_LOW 2
#define ROZMEDZIE_VIACERYCH_BALIKOV_HIGH 4
#define DOBA_DALSIEHO_BALIKU 42

#define PRAVDEPODOBNOST_VIACERYCH_LISTOV 0.3
#define ROZMEDZIE_VIACERYCH_LISTOV_LOW 2
#define ROZMEDZIE_VIACERYCH_LISTOV_HIGH 5
#define DOBA_DALSIEHO_LISTU 31

#define PRAVDEPODOBNOST_VIACERYCH_PENAZI 0.15
#define ROZMEDZIE_VIACERYCH_PENAZI_LOW 2
#define ROZMEDZIE_VIACERYCH_PENAZI_HIGH 3
#define DOBA_DALSIEHO_PENIAZU 29

#define DOBA_PODANIA_LISTU 100
#define DOBA_PODANIA_BALIKU 158
#define DOBA_PODANIA_PENAZI 82

#define PRAVDEPODOBNOST_DODANIA_LISTU 0.43
#define PRAVDEPODOBNOST_DODANIA_BALIKU 0.32

#define DOBA_DODANIA_LISTU 50
#define DOBA_DODANIA_BALIKU 70
#define DOBA_DODANIA_PENAZI 70

//hladanie dodaneho baliku/listu moze zabrat ine doby
#define UNIF_DOBA_LISTU_MIN 60
#define UNIF_DOBA_LISTU_MAX 180

#define UNIF_DOBA_BALIKU_MIN 60
#define UNIF_DOBA_BALIKU_MAX 240

#define PRAVDEPODOBNOST_NETRPEZLIVOSTI 0.06
#define CAS_MOZNEHO_VYSTUPU 615 //po tomto case odide ak pred nim nie je malo ludi
#define MAXIMALNY_POCET_LUDI_LOW 5 //ak je pred zakaznikom viac ako tolko ludi, odide z fronty, 
#define MAXIMALNY_POCET_LUDI_HIGH 9 //kazdy zakaznik ma iny threshold

#define PRAVDEPODOBNOST_OKAMZITEHO_ODCHODU 0.11
#define THRESHOLD_OKAMZITEHO_ODCHODU_LOW 12
#define THRESHOLD_OKAMZITEHO_ODCHODU_HIGH 16


/**********************************/

Stat doba_cakania_listy("Doba cakania v rade pre listove zasielky");
Stat doba_cakania_peniaze("Doba cakania v rade pre penazne zasielky");
Stat doba_cakania_baliky("Doba cakania v rade pre balikove zasielky");
Stat doba_vybavovania_listy("Doba stravena pri prepazke pri listovej sluzbe");
Stat doba_vybavovania_peniaze("Doba stravena pri prepazke pri penaznej sluzbe");
Stat doba_vybavovania_baliky("Doba stravena pri prepazke pri balikovej sluzbe");
Histogram cislo_prepazky("cislo prepazky",0,1,11);
Histogram doba_cakania_h("doba cakania v rade",0,60,39);

//pre kontrolu kolko ludi je pred nejakym clovekom ked premysla nad odchodom
int pocet_zaradenych_prepazka[POCET_PREPAZOK] = {0};
int pocet_vybavenych_prepazka[POCET_PREPAZOK] = {0};
vector<int> predcasne_odideni[POCET_PREPAZOK];

Facility Prepazky[POCET_PREPAZOK];

//ktore prepazky maju aku funkcionalitu
vector<int> peniaze;
vector<int> listy;
vector<int> baliky;

/*******************************************************/

//trieda pre obsluhu mozneho odchodu netrpezliveho zakaznika
class Odchod_z_rady : public Event {
    Process *ptr;   
    int prepazka;
    int poradie;    
  public:
    Odchod_z_rady(double t, Process *p, int cislo_prepazky,int poradove_cislo): ptr(p) {
      prepazka=cislo_prepazky;
      poradie=poradove_cislo;
      Activate(Time+Normal(t,15)); 
    }
    void Behavior() {

       int pocet_ludi_pred=poradie-pocet_vybavenych_prepazka[prepazka]-1;

       for (unsigned int i=0;i<predcasne_odideni[prepazka].size();i++) {
       		if (predcasne_odideni[prepazka].at(i) < poradie) {
       			pocet_ludi_pred--;
       		}
       }
       //kolko ludi dokaze zakaznik pred nim tolerovat
       int threshold_zakaznika=(int)round(Uniform(MAXIMALNY_POCET_LUDI_LOW,MAXIMALNY_POCET_LUDI_HIGH));

      if (pocet_ludi_pred>threshold_zakaznika) {

      	    ptr->Out();       
        	delete ptr; 
        	predcasne_odideni[prepazka].push_back(poradie);
      }
       
      Cancel();          
    }
};

//vrati, ci je dana prepazka spravneho typu pre zakaznika

class Zakaznik: public Process {
	void Behavior() {

		double typ_sluzby=Random();
		bool vypisal_tlacivo=false;
		if (Random()<PRAVDEPODOBNOST_PODANIA) { //80% podava

			if (Random()<PRAVDEPODOBNOST_VYPISOVANIA) { //32% si idu nieco vypisat - cakanie 6 minut
				Wait(Normal(DOBA_VYPISOVANIA,15));
				vypisal_tlacivo=true;
			}

			if (typ_sluzby<PRAVDEPODOBNOST_PODANIA_LISTU) { //podanie listu
				//vyberie sa prepazka s najkratsim radom
				cakanie_v_rade(DOBA_PODANIA_LISTU,LIST_PODANIE,vypisal_tlacivo);

			} else if (typ_sluzby<PRAVDEPODOBNOST_PODANIA_LISTU+PRAVDEPODOBNOST_PODANIA_BALIKU) { // podanie baliku
				cakanie_v_rade(DOBA_PODANIA_BALIKU,BALIK_PODANIE,vypisal_tlacivo);

			} else { // podanie peniazej transakcie
				cakanie_v_rade(DOBA_PODANIA_PENAZI,PENIAZ_PODANIE,vypisal_tlacivo);

			}
		} else { //20% dodava
			if (typ_sluzby<PRAVDEPODOBNOST_DODANIA_LISTU) { //dodanie listu
				cakanie_v_rade(DOBA_DODANIA_LISTU,LIST_DODANIE,vypisal_tlacivo);

			} else if (typ_sluzby<PRAVDEPODOBNOST_DODANIA_LISTU+PRAVDEPODOBNOST_DODANIA_BALIKU) { // dodanie baliku
				cakanie_v_rade(DOBA_DODANIA_BALIKU,BALIK_DODANIE,vypisal_tlacivo);

			} else { // dodanie peniazej transakcie
				cakanie_v_rade(DOBA_DODANIA_PENAZI,PENIAZ_DODANIE,vypisal_tlacivo);
			}
		}
	}

	void cakanie_v_rade(int cas_cakania,int typ, bool vypisal_tlacivo) {

		int i;
		bool first_time=true; //ak sa zaradil prvy krat, aby sa nemohlo stat ze pojde viac krat vypisat doklad
		int poradove_cislo;

		if (false) { //ak zabudol vypisat doklad vrati sa spat a vypise ho
			zabudol_vypisat_doklad:
			Wait(Normal(DOBA_VYPISOVANIA,15));
		}

		if (typ==LIST_PODANIE || typ==LIST_DODANIE)
			i=listy[0];
		else if (typ==BALIK_PODANIE || typ==BALIK_DODANIE)
			i=baliky[0];
		else if (typ==PENIAZ_PODANIE || typ==PENIAZ_DODANIE)
			i=peniaze[0];

		for (int a = 0; a<POCET_PREPAZOK ; a++) {
			if (je_prepazka_spravna(typ, a)) {//ak ide o relevantnu prepazku
				if (Prepazky[a].QueueLen() <Prepazky[i].QueueLen()) {
					i=a;
				}
			}
		}

		//ak su prilis dlhe rady, je sanca ze okamzite odchadza
		unsigned int threshold_odchodu=(int)round(Uniform(THRESHOLD_OKAMZITEHO_ODCHODU_LOW,THRESHOLD_OKAMZITEHO_ODCHODU_HIGH));

		if (Random()<PRAVDEPODOBNOST_OKAMZITEHO_ODCHODU && Prepazky[i].QueueLen()>=threshold_odchodu) {
			//zakaznik odchadza
			//ked som dal terminate mi niekedy na merlinovi dalo segfault, preto taky sposob 

		} else { //bezne spravanie zakaznika

			poradove_cislo=pocet_zaradenych_prepazka[i];
			pocet_zaradenych_prepazka[i]++;
			double t_vstup=Time;
			double t_zaciatok;
			if (Random() < PRAVDEPODOBNOST_NETRPEZLIVOSTI) { //ak ide o netrpezliveho zakaznika
				Event *odchod = new Odchod_z_rady(CAS_MOZNEHO_VYSTUPU,this,i,poradove_cislo);
				Seize(Prepazky[i]);
				t_zaciatok=Time;
				doba_cakania_h(Time-t_vstup);
				cislo_prepazky(i);

				if (typ==LIST_PODANIE || typ==LIST_DODANIE){
					doba_cakania_listy(Time-t_vstup);
				}
				else if (typ==BALIK_PODANIE || typ==BALIK_DODANIE) {
					doba_cakania_baliky(Time-t_vstup);
				}
				else if (typ==PENIAZ_PODANIE || typ==PENIAZ_DODANIE) {
					doba_cakania_peniaze(Time-t_vstup);
				}

				delete odchod;//odchod nebol vyuzity
		
			} else {
				Seize(Prepazky[i]);
				t_zaciatok=Time;

				doba_cakania_h(Time-t_vstup);
				cislo_prepazky(i);

				if (typ==LIST_PODANIE || typ==LIST_DODANIE){
					doba_cakania_listy(Time-t_vstup);
				}
				else if (typ==BALIK_PODANIE || typ==BALIK_DODANIE) {
					doba_cakania_baliky(Time-t_vstup);
				}
				else if (typ==PENIAZ_PODANIE || typ==PENIAZ_DODANIE) {
					doba_cakania_peniaze(Time-t_vstup);
				}

			}

			if (typ==LIST_PODANIE || typ==BALIK_PODANIE || typ==PENIAZ_PODANIE) { //v pripade podania sa moze stat ze nebude mat vypisane doklady
				if (vypisal_tlacivo==false && first_time==true) { // ak je zo skupiny ludi ktori este nemaju doklad vypisani
					if (Random() < PRAVDEPODOBNOST_NEVYPISANEHO_DOKLADU) { //ak zabudol
						first_time=false;
						Release(Prepazky[i]);
						goto zabudol_vypisat_doklad;
					}
				}
			}

			if (typ==LIST_DODANIE) {
				Wait(Normal(cas_cakania,15)+Uniform(UNIF_DOBA_LISTU_MIN,UNIF_DOBA_LISTU_MAX));

			} else if (typ==BALIK_DODANIE) {
				Wait(Normal(cas_cakania,15)+Uniform(UNIF_DOBA_BALIKU_MIN,UNIF_DOBA_BALIKU_MAX));

			} else if (typ==BALIK_PODANIE && Random()<PRAVDEPODOBNOST_VIACERYCH_BALIKOV) {
				int pocet_balikov=(int)round(Uniform(ROZMEDZIE_VIACERYCH_BALIKOV_LOW,ROZMEDZIE_VIACERYCH_BALIKOV_HIGH));
				Wait(Normal(cas_cakania,15)+Normal(DOBA_DALSIEHO_BALIKU,10)*(pocet_balikov-1));	

			} else if (typ==LIST_PODANIE && Random()<PRAVDEPODOBNOST_VIACERYCH_LISTOV){
				int pocet_listov=(int)round(Uniform(ROZMEDZIE_VIACERYCH_LISTOV_LOW,ROZMEDZIE_VIACERYCH_LISTOV_HIGH));
				Wait(Normal(cas_cakania,15)+Normal(DOBA_DALSIEHO_LISTU,10)*(pocet_listov-1));

			} else if (typ==PENIAZ_PODANIE && Random()<PRAVDEPODOBNOST_VIACERYCH_PENAZI){
				int pocet_penazi=(int)round(Uniform(ROZMEDZIE_VIACERYCH_PENAZI_LOW,ROZMEDZIE_VIACERYCH_PENAZI_HIGH));
				Wait(Normal(cas_cakania,15)+Normal(DOBA_DALSIEHO_PENIAZU,10)*(pocet_penazi-1));

			} else {
				Wait(Normal(cas_cakania,15));
			}

			if (typ==LIST_PODANIE || typ==LIST_DODANIE)
					doba_vybavovania_listy(Time-t_zaciatok);
			else if (typ==BALIK_PODANIE || typ==BALIK_DODANIE)
					doba_vybavovania_baliky(Time-t_zaciatok);
			else if (typ==PENIAZ_PODANIE || typ==PENIAZ_DODANIE)
					doba_vybavovania_peniaze(Time-t_zaciatok);

			Release(Prepazky[i]);

			pocet_vybavenych_prepazka[i]=poradove_cislo;
		}
	}
	
	bool je_prepazka_spravna(int typ, int cislo_prepazky) {

	if (typ==LIST_PODANIE || typ==LIST_DODANIE) {
		for (unsigned int i=0;i<listy.size();i++) {
			if (cislo_prepazky==listy[i])
				return true;
		}
	} else if (typ==BALIK_PODANIE || typ==BALIK_DODANIE) {
		for (unsigned int i=0;i<baliky.size();i++) {
			if (cislo_prepazky==baliky[i])
				return true;
		}
	} else if (typ==PENIAZ_PODANIE || typ==PENIAZ_DODANIE) {
		for (unsigned int i=0;i<peniaze.size();i++) {
			if (cislo_prepazky==peniaze[i])
				return true;
		}
	}
	return false;
}
};

class Generator: public Event {
	void Behavior() {
		(new Zakaznik)->Activate();
		Activate(Time+Exponential(PRICHOD_ZAKAZNIKOV)); //novy zakaznik kazdych 10 sekund

	}
};

int main(int argc, char *argv[]) {

		peniaze.clear();
		listy.clear();
		baliky.clear();

		//defaultne hodnoty
		peniaze = {0,1,2,3,4,5,8,9};
		listy = {0,1,2,3,4,5,8,9};
		baliky = {6,7,8,9};


//*********************************
		//experimenty:
		//          listy      peniaze    baliky
		//default 1111110011 1111110010 0000001111
		//EXP 1   1111110011 1111110000 0000001111
		//EXP 2   1111110001 1111110010 0000001111
		//EXP 3   1111110010 1111110010 0000001111 -- good
		//EXP 4   1111110010 1111110011 0000001111
		//EXP 5   1111110010 1101110011 0000001111 -- good
		//EXP 6	  1111110010 1101110001 0000001111
		//EXP 7	  1111110010 1101110010 0000001111
		//EXP 8   1111110010 1100110011 0000001111
//***********************************


		SetOutput("model.out");
		Init(0,DOBA_SIMULACIE);
		RandomSeed(time(NULL));
		(new Generator)->Activate();
		Run();

		doba_cakania_listy.Output();
		doba_cakania_baliky.Output();
		doba_cakania_peniaze.Output();
		doba_vybavovania_listy.Output();
		doba_vybavovania_peniaze.Output();
		doba_vybavovania_baliky.Output();
		doba_cakania_h.Output();
		cislo_prepazky.Output();

}
