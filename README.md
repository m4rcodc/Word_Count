# MPI WORD_COUNT PCPC PROJECT 2021/2002
Reallizzato da: Delle Cave Marco

# Problem Statement
Il problema Word_Count consiste nel leggere un numero casuale di parole all'interno di un numero variabile di file, con lo scopo di contare quante volta una singola parola si ripete all'interno dello stesso file e tra più file. Nello specifico, il tutto si realizza attraverso i principi della programmazione parallela, quindi ogni processo coinvolto ha una porzione di file da analizzare, per poi comunicare il proprio risultato ad un singolo processo MASTER.
Il problema è stato scomposto in 3 sottoproblemi:
- Partizionamento in modo equo del numero totale di parole presenti all'interno dei file, facendo in modo che ogni processore legga lo stesso numero di parole, o al massimo nel caso di divisione con resto, una parola in più.
- Ogni processo crea un proprio istogramma locale, eseguendo il conteggio delle parole all'interno della propria porzione di file. Nel caso di parole che si ripetono più volte, si procede con l'aggiornamento del campo frequency relativo a quest'ultime.
- Ogni processo comunica il proprio istogramma locale al processo MASTER, il quale effettua un merge col proprio istogramma al fine di crearne uno globale.

# Scelte progettuali fatte

Per la creazione degli istogrammi si è fatto uso di una linked list, in grado di gestire le parole rilevate in ogni file con le rispettive frequenze associate. La struct è molto semplice, ed ognuna di esse è collegata alla successiva all'interno della lista. Sono stati realizzati metodi per scorrere la lista, inserire un nuovo elemento all'interno della lista, aggiornare le frequenze relative alle parole ed altri metodi di supporto.

La struct è cosi definita:

```
struct Word
{
  char *word;
  int word_frequency;
  struct Word *pNext;
};
```

```
/*Aggiungi una nuova parola all'istogramma, o aggiorna la relativa frequenza */
void addWordToList(char *word);

/*Crea un nuovo item all'interno dell'istogramma */
struct Word* createWord(char *word, int number);

/*Stampa il contenuto di un item specifico */
void showStructInfo(struct Word *pWord,int rank);

/*Restituisci il numero di parole non duplicate presenti all'interno di un istogramma */
int counter_non_duplicate_words();

/*Restituisci la lunghezza della parola corrente */
int lengthOfCurrentWord(struct Word *pWord);

/*Restituisce la frequenza relativa ad una parola*/
int returnWordFrequency(struct Word *pStruct);

/*Restituisce la parola correntemente puntata */
char* returnWord(struct Word *pWord);

/*Metodo esclusivo per il processo MASTER, utilizzato per fare il merge degli istogrammi locali dei vari processi con il proprio */
void addOrIncrWordInMaster(char *word, int count);

```

**La sezione seguente illustrerà le soluzioni ai tre sottoproblemi descritti sopra:**

#Partizionamento
La prima cosa di cui abbiamo bisogno è sapere il numero totale di parole nell'insieme di file. Per fare ciò è stata fatta una lettura all'interno di ogni singolo file, aggiornando un counter per ogni parola trovata. Ogni parola termina alla presenza di un \n, \t ecc.

**Partizionamento [MASTER]**
Una volta ottenuto il numero totale di parole, la partizione viene calcolata in base al numero di processori coinvolti ad eseguire il lavoro. La partizione è espressa come una divisione, senza considerare la parte decimale. Una volta calcolata, si è gestito l'eventuale resto, per capire quali processi devono gestire una parola in più rispetto alla loro partizione (il resto è sempre compreso tra 0 e il numero di processori n -> 0 <= r <= n)
Dopo aver calcolato la partizione per ciascun processo, il master invia agli slave la partizione, il resto ed altre informazioni, come l'elenco dei nomi dei file e il numero di parole contenute in ciascuno di essi.
A questo punto il master inizia il lavoro sulla propria partizione. D'ora in poi possono verificarsi due casi:
1)Il primo file ha meno parole della partizione, quindi è necessario leggerlo tutto e continuare con il file successivo.
2)Il primo file ha più parole della sua partizione, quindi è necessario leggere al massimo un numero di parole pari alla partizione.