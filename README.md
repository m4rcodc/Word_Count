# MPI WORD_COUNT PCPC PROJECT 2021/2022

|Studente|Matricola|Numero Progetto|
|:---:|:---:|:---:|
|**Marco Delle Cave**|0522501162|01162 % 5 = 2|

# Indice
<!--ts-->
* [Problem Statement](#Problem-Statement)
* [Come lavora l'algoritmo [Ad alto livello]](#Come-lavora-l'algoritmo)
* [Scelte progettuali fatte](#Scelte-progettuali-fatte)
* [Partizionamento delle words](#Partizionamento-delle-words)
* [Conteggio locale delle parole](#Conteggio-locale-delle-parole)
* [Comunicazione degli istogrammi locali al MASTER](#Comunicazione-degli-istogrammi-locali-al-MASTER)
* [Correttezza](#Correttezza)
* [Benchmarking](#Benchmarking)
    * [Strong Scalability - 500k words](#Strong-Scalability--500k-words)
    * [Weak Scalability](#Weak-Scalability)
* [Commento dei risultati ottenuti](#Commento-dei-risultati-ottenuti)
* [Istruzioni per l'esecuzione](#Istruzioni-per-l'esecuzione)




<!--te-->

 
Problem Statement
============
Il problema Word_Count consiste nel leggere un numero casuale di parole all'interno di un numero variabile di file, con lo scopo di contare quante volte una singola parola si ripete all'interno dello stesso file e tra più file. Nello specifico, il tutto si realizza attraverso i principi della programmazione parallela, quindi ogni processo coinvolto ha una porzione di file da analizzare, per poi comunicare il proprio risultato ad un singolo processo MASTER.

Come lavora l'algoritmo [Ad alto livello]
============
Il problema è stato scomposto in 3 sottoproblemi:
- Partizionamento in modo equo del numero totale di parole presenti all'interno dei file, facendo in modo che ogni processore legga lo stesso numero di parole, o al massimo nel caso di divisione con resto, una parola in più.
- Ogni processo crea un proprio istogramma locale, eseguendo il conteggio delle parole all'interno della propria porzione di file. Nel caso di parole che si ripetono più volte, si procede con l'aggiornamento del campo frequency relativo a quest'ultime.
- Ogni processo comunica il proprio istogramma locale al processo MASTER, il quale effettua un merge col proprio istogramma al fine di crearne uno globale.

Scelte progettuali fatte
============
Per la creazione degli istogrammi si è fatto uso di una linked list, in grado di gestire le parole rilevate in ogni file con le rispettive frequenze associate. La struct è molto semplice, ed ognuna di esse è collegata alla successiva all'interno della lista. Sono stati realizzati metodi per: scorrere la lista, inserire un nuovo elemento all'interno di essa, aggiornare le frequenze relative alle parole ed altri metodi di supporto.

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
Metodi:

/*Aggiungi una nuova parola all'istogramma, o se già presente, aggiorna la relativa frequenza */
void addWordToList(char *word);

/*Metodo di supporto, crea una nuova word */
struct Word* createWord(char *word, int number);

/*Restituisci il numero di parole non duplicate presenti all'interno di un istogramma */
int counter_non_duplicate_words();

/*Restituisci la lunghezza della parola corrente */
int lengthOfCurrentWord(struct Word *pWord);

/*Restituisci la frequenza relativa ad una parola*/
int returnWordFrequency(struct Word *pStruct);

/*Restituisci la parola correntemente puntata */
char* returnWord(struct Word *pWord);

/*Metodo esclusivo per il processo MASTER, utilizzato per fare il merge degli istogrammi locali dei vari processi con il proprio */
void addOrIncrWordInMaster(char *word, int count);

```

**La sezione seguente illustrerà le soluzioni ai tre sottoproblemi descritti sopra:**

Partizionamento delle words
============
La prima cosa di cui abbiamo bisogno è sapere il numero totale di parole in tutti i file. Per fare ciò è stata fatta una lettura all'interno di ogni singolo file, aggiornando un counter per ogni parola trovata. Ogni parola termina alla presenza di un \n, \t ecc.

```
....

while((ch = fgetc(fp)) != EOF){
                if(ch == ' ' || ch == '\t' || ch == '\n'){
                    single_file_word_counter++;//Tengo traccia del numero totale di parole in un singolo file
                    word_counter++;//Tengo traccia del numero totale di parole in tutti i files
                            }
                        } 



```


**Partizionamento [MASTER]**
Una volta ottenuto il numero totale di parole, la partizione viene calcolata in base al numero di processori coinvolti ad eseguire il lavoro. La partizione è espressa come una divisione, senza considerare la parte decimale. Una volta calcolata, si è gestito l'eventuale resto, per capire quali processi debbano gestire una parola in più rispetto alla loro partizione (il resto è sempre compreso tra 0 e il numero di processori n -> 0 <= r <= n)
Dopo aver calcolato la partizione per ciascun processo, il master invia agli slave la partizione da leggere, il resto, ed altre informazioni, come l'elenco dei nomi dei file e il numero di parole contenute in ciascuno di essi.
A questo punto il master inizia il lavoro sulla propria partizione. 

```

partition = word_counter / world_size;

resto = word_counter % world_size;

//Send
for(int i = 1; i < world_size; i++){

    MPI_Send(&numberOfFile,1,MPI_INT,i,99,MPI_COMM_WORLD);
    MPI_Send(file_name,sizeof(file_name)/sizeof(file_name[0][0]),MPI_CHAR,i,0,MPI_COMM_WORLD);
    MPI_Send(number_of_word,sizeof(number_of_word)/sizeof(number_of_word[0]),MPI_INT,i,1,MPI_COMM_WORLD);
    MPI_Send(&partition,1,MPI_INT,i,2,MPI_COMM_WORLD);
    MPI_Send(&resto,1,MPI_INT,i,3,MPI_COMM_WORLD);

}

```

**Partizionamento [SLAVE]**
Per quanto riguarda gli slave è necessario indicargli anche la parola da cui devono iniziare ad eseguire il conteggio. Di conseguenza occorre capire da quale file leggere e da quale posizione all'interno di quest'ultimo partire con il conteggio, poichè il processo precedente potrebbe non aver letto tutto il file, ma solo una piccola parte.
Per questo motivo ogni slave calcola il proprio lowerbound, ovvero da quale posizione dovrebbe iniziare a leggere. Questa operazione è stata fatta attraverso semplici calcoli:

```
 if(resto != 0){ 
            if(rank < resto){
                lw_bound = (partition + 1) * rank;
                partition++;
            }
            else {
                lw_bound = (partition*rank) + resto;
            }
        } else { 
            lw_bound = partition * rank;
        }

```
Una volta calcolato il proprio lowerbound, passiamo ad identificare la posizione esatta da cui iniziare a leggere.
Ci sono diversi steps:
1) Occorre identificare esattamente quale file leggere. Per farlo è stata utilizzata una variabile che viene incrementata del numero di parole nell'i-esimo file all'interno di un ciclo for. Quando questa variabile supera il lowerbound calcolato, significa che abbiamo trovato il file da cui inziare a leggere.

```
int size = sizeof(number_of_word)/sizeof(number_of_word[0]);

for(int i=0; i < size; i++){

  cum_sum += number_of_word[i];

   if((cum_sum > lw_bound) && (partition > 0)){
     ...

    //Questo è il file che stiamo cercando     
     ...
  
   }
}

```
2) Una volta trovato il file da cui iniziare il conteggio, occorre capire se questo file è stato letto per intero dal processo precedente o contiene ancora parole da leggere. Per fare ciò è stata utilizzata una variabile "start_to_read", il cui valore si ottiene sottraendo al numero di parole contenute nel file corrente, la differenza tra la somma cumulativa di parole in tutti i file e il lowerbound. La variabile start_to_read può assumere due valori: se è minore di 0, significa che il file corrente deve essere letto dall'inizio, altrimenti il suo risultato è esattamente il punto da cui iniziare la lettura. Ad esempio, se start_to_read è pari a 100, il processo p inizierà a leggere dalla 101-esima parola.

```
start_to_read = number_of_word[i] - (cum_sum-lw_bound);
            
            if(start_to_read < 0){
                        start_to_read = 0;
                    }
            ...



```

Conteggio locale delle parole [MASTER/SLAVE]
============
La soluzione che si è deciso di utilizzare per il conteggio delle parole è abbastanza semplice, ovvero ogni volta che viene raggiunta la fine di una parola, quest'ultima viene copiata all'interno di un array temporaneo per poi essere aggiunta alla linked list (con il metodo addWordToList). Ad ogni parola trovata l'array viene ripristinato in modo tale da essere riutilizzato per la parola successiva.

```
else {
    if(ch == ' ' || ch == '\t' || ch == '\n'){
       
       //Una volta raggiunta la fine di una parola
       
       word_counter++;
       local_partition--;
       temporary_word[index_of_tmpword] ='\0';
       index_of_tmpword++;
       addWordToList(temporary_word);
       memset(temporary_word,0,500);
       index_of_tmpword = 0;

        if(local_partition <= 0){
            break;
        }
    }
}


```
A questo punto ogni processo ha generato il proprio istogramma locale ed è pronto per comunicarlo al master (comunicare le parole e le relative frequenze al master).

Comunicazione degli istogrammi locali al MASTER
============
Per consentire ad ogni processo slave di comunicare il proprio istogramma (linked list) al master, l'approccio utilizzato è stato quello di inserire all'interno di un array, 'local_histogram', l'insieme di tutte le parole rilevate da ciascun processo slave, ognuna di esse separata dal carattere \0. Lo stesso principio è stato applicato per le frequenze con l'array 'local_counters', al fine di sincronizzare i due array nel processo master e dunque riuscire a ricostruire la linked list.

```
//Array contenente le frequenze associate a ciascuna word

        n_words = counter_non_duplicate_words();
        local_counters = malloc(sizeof(int)* n_words);

        pStruct = pStart;
        while(pStruct != NULL){
            n_char += lengthOfCurrentWord(pStruct);
            pStruct = pStruct -> pNext;
        }

        local_histogram = malloc(sizeof(char) * n_char);

```
Ora è possibile iniziare la comunicazione con il master. Per fare ciò ho utilizzato la primitiva di comunicazione **MPI_Gatherv**, per la quale sono stati calcolati i due parametri necessari a questa funzione,displacement e size, sia per l'array counters delle frequenze, sia per l'array histogram_word per le parole. Prima di effettuare queste operazioni, è stato necessario comunicare al MASTER le dimensioni dei singoli array di ogni slave per capire quanto spazio allocare per i due array finali (global_histogram e global_counters). Ciò è stato fatto attraverso la primitiva **MPI_Gather**.
N.B. Per il processo MASTER displacement e size sono pari a 0.

```

 MPI_Gather(&n_char,1,MPI_INT,&recv_n_char,1,MPI_INT,0,MPI_COMM_WORLD);
 MPI_Gather(&n_words,1,MPI_INT,&recvs_counts,1,MPI_INT,0,MPI_COMM_WORLD);

if(rank == 0){

        for (int i = 0; i < world_size; i++){
        if(i == 0){
            global_counters_disp[i] = 0;
        }
        else {
            global_counters_disp[i] = global_counters_disp[i-1] + recvs_counts[i-1];
        }
        num_count += recvs_counts[i];
    }

        for (int i = 0; i < world_size; i++){
        if(i == 0){
            global_histogram_disp[i] = 0;
        }
        else {
            global_histogram_disp[i] = global_histogram_disp[i-1] + recv_n_char[i-1];
        }
        num += recv_n_char[i];
        }

        /*Alloco spazio per gli array finali*/
        global_histogram = malloc(sizeof(char) * num);
        global_counters = malloc(sizeof(int) * num_count);
        
    }     

```
Ora può avere inizio la comunicazione:

```
 MPI_Gatherv(local_histogram,n_char,MPI_CHAR,global_histogram,recv_n_char,global_histogram_disp,MPI_CHAR,0,MPI_COMM_WORLD);

 MPI_Gatherv(local_counters,n_words,MPI_INT,global_counters,recvs_counts,global_counters_disp,MPI_INT,0,MPI_COMM_WORLD);
    

```
Merge dei risultati all'interno dell'istogramma del MASTER
============
Avendo a disposizione gli istogrammi locali di ogni processo slave, a questo punto è possibile ricostruire la linked list e fare il merge con quella del master.

```
for(int n = 0; n < num; n++){

            if(global_histogram[n] == 0){                
                addOrIncrWordInMaster(tmp_word,global_counters[count_parole]);
                memset(tmp_word,0,100);
                index_of_word_count = 0;
                count_parole++;
            }
            else 
            {
                tmp_word[index_of_word_count] = global_histogram[n];
                index_of_word_count++;
            }
        }

```
Il metodo addOrIncrementInMaster prende in input una parola e verifica se il master la possiede già all'interno del suo istogramma, se è cosi allora somma la sua frequenza relativa a quella parola con quella dello slave, prelevando la frequenza dall'array dei conteggi. Viceversa, se il MASTER non possiede la parola, la deve aggiungere al proprio istogramma inserendo come frequenza quella rilevata localmente dagli slave nei loro istogrammi locali.

Correttezza
============
Per dimostrare la correttezza dell'algoritmo sono state effettuate tre esecuzioni, dove in ognuna di esse è stato cambiato il numero di processi coinvolti. Come si può osservare dalle immagini sottostanti, nonostante la variazione del numero di processi vengono prodotti sempre gli stessi risultati in output.

*File di input - Numero Processi = 1*          | *File di output - Numero Processi = 2*
:-------------------------:|:-------------------------:
![inFile](Correttezza/Input1Processo.png)      | ![outFile](Correttezza/Output1Processo.png)

*File di input - Numero Processi = 3*          | *File di output - Numero Processi = 3*
:-------------------------:|:-------------------------:
![inFile](Correttezza/Input3Processi.png)      | ![outFile](Correttezza/Output3Processi.png)

*File di input - Numero Processi = 5*          | *File di output - Numero Processi = 5*
:-------------------------:|:-------------------------:
![inFile](Correttezza/Input5Processi.png)      | ![outFile](Correttezza/Output5Processi.png)

Benchmarking
============
L'algoritmo è stato testato in termini di **strong scalability** e **weak scalability** su **Google Cloud Platform** su un cluster di 6 macchine **e2-standard-4**, ognuna dotata di 4 vCPUs, quindi per un totale di 24 vCPUs.

⚠️ **IMPORTANTE: il tempo rappresentato all'interno dei grafici sottostanti non considera la parte intera, che è pari a 0 per ogni risultato ottenuto**

Strong Scalability - 500k words
--------------------------
|vCPUs|Tempo(s)|Speed-up|
|:---:|:---:|:---:|
|1|0.438|1|
|2|0.321|1.36|
|3|0.292|1.5|
|4|0.283|1.54|
|5|0.286|1.53|
|6|0.265|1.65|
|7|0.243|1.80|
|8|0.247|1.77|
|9|0.230|1,90|
|10|0.219|2|
|11|0.206|2.12|
|12|0.198|2.21|
|13|0.192|2.28|
|14|0.189|2.31|
|15|0.194|2.25|
|16|0.172|2.54|
|17|0.168|2.60|
|18|0.188|2.46|
|19|0.204|2.14|
|20|0.200|2.19|
|21|0.226|1.93|
|22|0.234|1.87|
|23|0.231|1.89|
|24|0.254|1.72|


![Strong.png](Benchmark/Strong.png)


Il benchmark mostra che più processi vengono utilizzati, minore è il tempo necessario per completare il task. Da un certo punto in poi la riduzione del tempo di esecuzione inizia a diminuire, ed in particolare da 18 processi in su inizia a risalire, il chè significa che l'algoritmo inizia a perdere di efficienza, principalmente per l'overhead causato dalle comunicazioni tra i vari processi.

Weak Scalability
--------------------------
Le parole in input a ciascun processo hanno un rapporto di 15000:1.
|vCPUs|Tempo(s)|N-Words|
|:---:|:---:|:---:|
|1|0.011|15k|
|2|0.016|30k|
|3|0.029|45k|
|4|0.034|60k|
|5|0.054|75k|
|6|0.063|90k|
|7|0.065|105k|
|8|0.086|120k|
|9|0.091|135k|
|10|0.093|150k|
|11|0.106|165k|
|12|0.118|180k|
|13|0.120|195k|
|14|0.125|210k|
|15|0.131|225k|
|16|0.149|240k|
|17|0.151|255k|
|18|0.158|270k|
|19|0.159|285k|
|20|0.164|300k|
|21|0.176|315k|
|22|0.177|330k|
|23|0.180|345k|
|24|0.187|360k|


![Weak.png](Benchmark/Weak.png)

Come si può evincere dai risultati raccolti, il tempo di esecuzione aumenta (anche se di poco) costantemente all'aumentare del numero dei processori.

Commento dei risultati ottenuti
============
Come si può notare dalle tabelle riassuntive e dai grafici, lo speed-up con l'utilizzo di 2 processori è quello che più si avvicina allo speed-up ideale. Questo significa che in queste condizioni l'algoritmo parallelo è più veloce in rapporto alle risorse utilizzate (in termini di comunicazione) e quindi più efficiente.
All'aumentare del numero dei processori lo speed-up si allontana sempre più da quello ideale comportando, quindi, una perdita di efficienza. 
In entrambi i casi, per la **weak** e **strong scalability** non possiamo ottenere risultati pari a quelli ideali poichè bisogna sempre tener conto del costo della comunicazione. Nel caso specifico dell'algoritmo presentato, un peso in termini di comuncazione è rappresentato sicuramente dalle **send** iniziali con cui il MASTER comunica agli slave tutto ciò che è necessario affinchè quest'ultimi riescano a lavorare sulla propria partizione.

Istruzioni per l'esecuzione
============
⚠️ **Per eseguire il programma la directory file_test deve essere posizionata all'interno della stessa directory in cui si trova il file .c**


    📁 Word_Count    
        📝 word_count.c
            📁 file_test
             📝 file1.txt
             📝 file2.txt
                ....

Compilazione:
```
mpicc word_count.c -o word_count

```

Esecuzione locale:
```
mpirun --allow-run-as-root <np> word_count

```

Esecuzione sul cluster:
```
mpirun -np <np> --hostfile <host> word_count

```
Occorre sostituire **np** con il numero di processori da utilizzare e
**host** con il path dell'hostfile.
