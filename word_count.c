/*
[IDEA GENERALE INIZIALE]
1)Partizionare il numero di parole da contare per ciascun processore in modo equo,
ogni processore deve leggere lo stesso numero di parole (gestire caso in cui il numero totale di parole
non è perfettamente divisibile tra i processi)
2)Ogni processo (MASTER + SLAVE) crea il proprio istogramma locale
3)Ogni processo slave comunica al master il proprio istogramma locale e il master fa il merge con il proprio
*/

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

/*Struct:
1. Array di char (Word)
2. Frequenza con cui si presenta la Word
3. Pointer alla next Word della lista
*/
struct Word
{
    char *word;
    int word_frequency;
    struct Word *pNext;
};

//this is a test

//Variabile globale
struct Word *pStart = NULL; 


//Crea un nuovo nodo all'interno della lista di Word
struct Word* createWord(char *word, int number)
{
    struct Word *pStruct = NULL;
    pStruct = (struct Word*) malloc(sizeof(struct Word));
    pStruct -> word = (char*) malloc(strlen(word)+1);
    strcpy(pStruct -> word,word);
    pStruct -> word_frequency = number;
    pStruct -> pNext = NULL;
    return pStruct;
}

//Stampa i campi [WORD,FREQUENCY] di un determinato nodo 
void showStructInfo(struct Word *pWord,int rank)
{
    printf("\n (RANK %d): %s   %d", rank, pWord -> word, pWord -> word_frequency);
}

//Aggiungi una word alla lista o aggiornala se già esiste all'interno della lista
void addWordToList(char *word)
{

    struct Word *pStruct = NULL;
    struct Word *pLast = NULL;

    //Caso in cui la lista è vuota
    if(pStart == NULL)
    {
        pStart = createWord(word,1);
        return;
    }

    //Caso in cui la word è già presente nella lista [INCREMENTO IL SUO COUNTER FREQUENCY]
    pStruct = pStart;//ora pStruct punterà al primo nodo della lista
    //Scorro la lista finchè non arrivo all'ultimo elemento della lista, che punterà a NULL
    while(pStruct != NULL)
    {
        if(strcmp(word,pStruct -> word) == 0)
        {
            ++pStruct -> word_frequency;
            return;
        }
        pLast = pStruct;//mi salvo l'ultima posizione in cui si trova pStruct
        pStruct = pStruct -> pNext;
    }

    //Caso in cui la word non è presente nella lista [AGGIUNGO IL NODO WORD ALLA FINE DELLA LISTA]
    pLast -> pNext = createWord(word,1);
}

//Counter delle word all'interno di una lista
int counter_non_duplicate_words()
{
    struct Word *pStruct = NULL;
    pStruct = pStart;//pStruct punterà all'inizio della lista 
    int counter = 0;
    while(pStruct != NULL)
    {
        counter++;
        pStruct = pStruct-> pNext;
    }
    return counter;
}

//Ritorna la lunghezza di una word
int lengthOfCurrentWord(struct Word *pWord)
{
    int ind = 0;
    int len = 0;
    char word[100];
    strcat(word, pWord -> word);
    return strlen(pWord->word) + 1;
}

//Ritorna la frequenza associata ad una singola word
int returnWordFrequency(struct Word *pStruct){
    int frequency = pStruct -> word_frequency;
    return frequency;
}

//Ritorna la word
char* returnWord(struct Word *pWord){
    return pWord -> word;
}

//Metodo di supporto per il master [MERGE degli istogrammi degli slave con quello del master]
void addOrIncrWordInMaster(char *word, int count){

    struct Word *pStruct = NULL;
    struct Word *pLast = NULL;

    pStruct = pStart; //punta all'inizio della lista
    while(pStruct != NULL){
        if(strcmp(word,pStruct -> word) == 0){
            int old_count = pStruct -> word_frequency;
            int new_count = old_count + count;
            pStruct -> word_frequency = new_count;
            return;
        }

        pLast = pStruct;//Mi salvo l'ultima occorrenza visitata
        pStruct = pStruct -> pNext;
    }

    //In questo modo se la parola non è presente nell'istogramma del MASTER la aggiungo in coda alla lista
    pLast -> pNext = createWord(word,count);

}

int main (int argc, char *argv[])
{


int world_size, rank;
int ch, index_of_tmpword=0;
int lw_bound = 0, local_partition = 0, partition = 0, resto = 0, readed_non_duplicate_words = 0;
int numberOfFile = 0, word_counter=0, single_file_word_counter=0, readed_num_char=0;
int *counters, *total_counters;

char path_file[2100];
char temporary_word[500];
char *histogram_word;
char *result_word;

struct Word *pStruct = NULL;

double start_time,finish_time;

MPI_Status status;
MPI_Init(&argc,&argv);
MPI_Comm_size(MPI_COMM_WORLD, &world_size);
MPI_Comm_rank(MPI_COMM_WORLD,&rank);

start_time = MPI_Wtime();

//Array [All_Gather e Ghaterv]
int recv_all_num_char[world_size];
int result_word_disp[world_size];
int total_counters_disp[world_size];
int recvs_allFreq_ndWord[world_size];

/*
Cosa fa il [MASTER]:
1) Conteggio del numero di parole presenti in ogni file all'interno della directory
2) Calcolo delle partizione da assegnare a se stesso ed a ciascun processo
3) Lavoro sulla propria partizione
*/

    if(rank==0)
    {

        /*
            Inizio del conteggio del numero di file nella directory
        */
        DIR *directory;
        FILE *fp;
        struct dirent *dir;

        directory = opendir("file_test");
        if(directory == NULL){
            printf("No directory found\n");
        }
        else {
            //Conto quanti file ci sono nella directory
            while((dir = readdir(directory)) != NULL){
            if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0){
                numberOfFile++;
                }
            }
        }

        closedir(directory);
        
        /*
        Fine del conteggio del numero di file nella directory
        */

        int number_of_word[numberOfFile];//Countero il numero di parole per ogni file
        char file_name[numberOfFile][100];//Ogni riga dell'array bidimensionale è un array di char di dimensione 100
        int while_counter = 0, flag_in_word = 0;

        /*
        Inizio del conteggio del numero di parole in ciascun file
        */

        directory = opendir("file_test");
        if(directory == NULL){
              printf("No directory found\n");  
        }
        else {
            while((dir = readdir(directory)) != NULL){ 
                if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0){
                    strcpy(path_file,"file_test/");
                    strcat(path_file,dir->d_name);
                    //Salvo man mano all'interno dell'array i nomi dei file
                    strcpy(file_name[while_counter],dir->d_name);
                    
                    //Apro ogni file in lettura
                    fp = fopen(path_file,"r");

                    if(fp == NULL){
                        perror("Unable to open file!\n");
                    }
                    
                    while((ch = fgetc(fp)) != EOF){
                        //Controllo se il carattere letto è alfanumerico
                        if(isalnum(ch)!=0){
                            flag_in_word = 1;
                            
                        }
                        else{
                            if((ch == ' ' || ch == '\t' || ch == '\n') && (flag_in_word==1)){
                                single_file_word_counter++;//Tengo traccia del numero totale di parole in un singolo file
                                word_counter++;//Tengo traccia del numero totale di parole in tutti i file
                                flag_in_word=0;
                            }
                        } 
                    }
                    //printf("\n");
                    number_of_word[while_counter] = single_file_word_counter;
                    while_counter++;
                    single_file_word_counter=0;
                  }
               }
                //printf("Numero totale di parole nei file : %d\n",word_counter);
            }
            fclose(fp);
            closedir(directory);

                /*
                Fine del conteggio del numero di parole in ciascun file all'interno della directory
                */

    

    //Suddivisione del numero totale di word in partizioni

    partition = word_counter / world_size;

    resto = word_counter % world_size; //Calcolo dell'eventuale resto
    //Send a tutti gli altri processi (non 0)
    for(int i = 1; i < world_size; i++){

        MPI_Send(&numberOfFile,1,MPI_INT,i,99,MPI_COMM_WORLD);
        MPI_Send(file_name,sizeof(file_name)/sizeof(file_name[0][0]),MPI_CHAR,i,0,MPI_COMM_WORLD);
        MPI_Send(number_of_word,sizeof(number_of_word)/sizeof(number_of_word[0]),MPI_INT,i,1,MPI_COMM_WORLD);
        MPI_Send(&partition,1,MPI_INT,i,2,MPI_COMM_WORLD);
        MPI_Send(&resto,1,MPI_INT,i,3,MPI_COMM_WORLD);

    }


    //Gestione dell'eventuale resto, per capire quanto deve essere grande la partizione su cui deve lavorare
    if(resto != 0){
         lw_bound = 0; //Setto il lower_bound per il master (sempre 0)
         local_partition = partition + 1;
    } else { 
        lw_bound = 0;
        local_partition = partition;
    }


    int index_file = 0;//variabile che mi serve per passare da un file ad un altro, se la partizione copre due file
    flag_in_word=0;
    word_counter=0;
    char f_path[800];


    /*
              -----------------------------------------INIZIO DEL LAVORO DEL [MASTER] SULLA SUA PARTIZIONE---------------------------------------
    */
    while(local_partition > 0){
        strcpy(f_path,"file_test/");

        strcat(f_path,file_name[index_file]);//concat ogni file ad ogni iterazione

        fp = fopen(f_path,"r");

        if(fp == NULL){
            perror("[MASTER] unable to open file");
        }

        //Lettura di una singola parola
        while((ch = fgetc(fp)) != EOF){
            if(isalnum(ch)!=0){
                flag_in_word=1;
                ch = tolower(ch);
                temporary_word[index_of_tmpword] = ch;//Aggiungo in temporary_word la word corrente
                index_of_tmpword++;
            } else {
                if((ch == ' ' || ch == '\t' || ch == '\n') && (flag_in_word==1)){
                    word_counter++;
                    flag_in_word=0;
                    local_partition--;//ho trovato la parola, decremento il counter della partition
                    temporary_word[index_of_tmpword] ='\0';//Aggiungo il carattere di fine stringa
                    index_of_tmpword++;
                    addWordToList(temporary_word);//Trovata una word l'aggiungo alla lista (se non è già presente viene aggiunta, altrimenti viene incrementato il suo counter)
                    memset(temporary_word,0,500);//reset di temporary_word per la prox iterazione
                    index_of_tmpword = 0;//reset indice
                    //Se ho trovato tutte le parole della mia partizione allora posso uscire dal while
                    if(local_partition <= 0){
                        break;
                    }
                    
                }
            }
        }

        fclose(fp);
        memset(f_path,0,800);
        index_file++;//Se sono arrivato ad EOF ed ancora non ho terminato la mia partizione, inizio a leggere nel file successivo.
    }

   /*

    [DEBUG MASTER] -> Stampa di tutte le parole della partizione del master

    pStruct = pStart;
    while(pStruct != NULL){
        showStructInfo(pStruct,rank);
        pStruct = pStruct -> pNext;
    }
    printf("\n \n \n");
    */
    } 

    /*
     ------------------------------------------------ FINE DEL LAVORO DEL [MASTER] SULLA SUA PARTIZIONE -------------------------------------
    */

   
    /*
     ------------------------------------------------ INIZIO DEL LAVORO DEGLI [SLAVE] ------------------------------------------------------
    */

    else {

        FILE *file;
        int cum_sum = 0,start_to_read = 0,flag_in_word = 0;
        int while_counter = 0;

        MPI_Recv(&numberOfFile,1,MPI_INT,0,99,MPI_COMM_WORLD,&status);//Ricevo il numero di file
        char file_name[numberOfFile][100];
        int number_of_word[numberOfFile];
        MPI_Recv(file_name,sizeof(file_name)/sizeof(file_name[0][0]),MPI_CHAR,0,0,MPI_COMM_WORLD,&status);
        MPI_Recv(number_of_word,sizeof(number_of_word)/sizeof(number_of_word[0]),MPI_INT,0,1,MPI_COMM_WORLD,&status);
        MPI_Recv(&partition,1,MPI_INT,0,2,MPI_COMM_WORLD,&status);
        MPI_Recv(&resto,1,MPI_INT,0,3,MPI_COMM_WORLD,&status);
        
        //Gestione del resto per i processi slave, aggiungo il resto in base all'ordine dei rank, ad esempio se ho resto=2 -> il processo di rank 0 aggiunge 1 alla sua partition, il processo di rank 1 aggiunge 1 alla sua partition, il processo di rank 2 non aggiungerà niente e cosi via
        if(resto != 0){ //Caso in cui c'è resto
            if(rank < resto){//Regolo le partizioni in base al rank dei processi
                lw_bound = (partition + 1) * rank;
                partition++;
            }
            else {
                lw_bound = (partition*rank) + resto;
            }
        } else { //Caso in cui non c'è resto
            lw_bound = partition * rank;
        }

        /*
        In number of word avrò: 29 e 25, ovvero le taglie dei due file
        size = 2
        */
        int size = sizeof(number_of_word)/sizeof(number_of_word[0]);

        for(int i=0; i < size; i++){

                cum_sum += number_of_word[i];//Somma delle parole in ogni file

                if((cum_sum > lw_bound) && (partition > 0)){
                    strcpy(path_file,"file_test/");
                    strcat(path_file,file_name[i]);
                    file = fopen(path_file,"r");
                    if(file == NULL){
                        perror("Unable to open file");
                    }

                    start_to_read = number_of_word[i] - (cum_sum-lw_bound);

                    /*
                      [Se start_to_read è < 0] -> devo leggere il file dall'inizio (start_to_read = 0)
                      [Se start_to_read è >= 0] -> start_to_read è il punto da cui devo iniziare a leggere
                    */
                    if(start_to_read < 0){
                        start_to_read = 0;
                    }
                    /*
                        -Da un lato mi costruisco la lista con l'array temporary_word
                        -Dall'altro lato mi metto tutte le parole all'interno di un'unico array (creo il mio istogramma locale);
                    */
                    while((ch = fgetc(file)) != EOF){
                        if(isalnum(ch)!=0){
                            flag_in_word = 1;
                            /*
                            Quando arrivo alla 27-esima parola di test1.txt inizio ad inserire nell'array temporaneo, in questo modo rank 1 inizierà a leggere le parole a partire dalla 27 esima
                            */
                            if(word_counter >= start_to_read){//Questo if mi fa arrivare al punto preciso da cui devo iniziare a leggere
                                ch = tolower(ch);
                                temporary_word[index_of_tmpword] = ch;
                                index_of_tmpword++;
                               
                            }
                        }
                        else {
                            if((ch == ' ' || ch == '\t' || ch == '\n') && (flag_in_word==1)){
                                word_counter++;
                                flag_in_word=0;
                                //Dal momento in cui lo slave arriva al punto da cui deve iniziare a leggere, per ogni parola letta decrementa la sua partizione
                                if(word_counter > start_to_read){
                                    partition--;
                                    temporary_word[index_of_tmpword] = '\0';
                                    index_of_tmpword++;
                                    addWordToList(temporary_word);
                                    memset(temporary_word,0,100);
                                    index_of_tmpword = 0;
                                   
                                }
                                //Quando ho terminato di leggere la mia partizione esco
                                if(partition <= 0){
                                    break;
                                }
                            }//end if
                        }// end else
                    }//end while
                }
                memset(path_file,0,2100);
        }

        fclose(file);

        //Conto il numero di parole (che già sono non duplicate) all'interno di ogni istogramma di ogni processo
        readed_non_duplicate_words = counter_non_duplicate_words();
        //printf("rank %d,number of nun duplicate words : %d\n",rank,readed_non_duplicate_words);
        counters = malloc(sizeof(int)*readed_non_duplicate_words);


        //Mi salvo la lunghezza di tutte le parole nella struct list(In readed_num_char ho il numero di caratteri totali di tutte le parole non duplicate)
        pStruct = pStart;
        while(pStruct != NULL){
            readed_num_char += lengthOfCurrentWord(pStruct);
            pStruct = pStruct -> pNext;
            fflush(stdout);
        }

        //Alloco spazio pari alla lunghezza di tutte le parole non duplicate
        histogram_word = calloc(readed_num_char,sizeof(char));
        
        /*
            Inserisco nell'array histogram_word l'insieme di tutte le word rilevate (non duplicate)
            Inserisco nell'array nell'array counters l'insieme di tutti i counters delle frequenze delle singole word

        */
        
        pStruct = pStart;
        int index_word_tmp = 0;
        int index_histogram_word = 0;
        char word_tmp[100];
        //Per ogni parola non duplicata
        for(int i=0; i < readed_non_duplicate_words;i++){
            counters[i] = returnWordFrequency(pStruct);//inserisco in counters la frequenza di ogni parola
            strcpy(word_tmp,returnWord(pStruct));//copio nell'array word_tmp la parola corrente della lista
            //Copio ogni parola carattere per carattere nell'array histogram_word, finchè non arrivo al carattere null        
            while(word_tmp[index_word_tmp]!=0){
                histogram_word[index_histogram_word] = word_tmp[index_word_tmp];
                index_histogram_word++;
                index_word_tmp++;
            }

            //Passo alla prossima parola della lista
            pStruct = pStruct -> pNext;
            //Reset degli indici
            index_word_tmp = 0;
            histogram_word[index_histogram_word] = 0;
            index_histogram_word++;
            
            //printf("[RANK %d] Counters di %d è uguale a %d\n",rank,x,counters[x]);
        }

/*
        [DEBUG SLAVE] -> Stampa di tutte le parole della partizione degli slave
        pStruct = pStart;

        while(pStruct != NULL){
            showStructInfo(pStruct,rank);
            pStruct = pStruct -> pNext;
        }
        printf("\n \n \n");
*/
        //Dealloco gli istogrammi di ogni slave
        pStruct = pStart;
        while(pStruct != NULL)
        {
            free(pStruct->word);
            pStart = pStruct;
            pStruct = pStruct->pNext;
            free(pStart);
        }
       
    } 

    /*
    --------------------------------------------------------  FINE CODICE SLAVE  ------------------------------------------------------------------
    */

    //Tutti inviano a tutti il proprio numero di caratteri letti ed il proprio numero di parole lette non duplicate a tutti gli altri (il processo 0 non partecipa alla allgather, infatti inserisce 0 in entrambi gli array)
    //Inserisco in due unici array comuni a tutti i processi tutte le parole non duplicate lette e le corrispettive frequenze.
    MPI_Allgather(&readed_num_char,1,MPI_INT,&recv_all_num_char,1,MPI_INT,MPI_COMM_WORLD);//numero di caratteri letti
    MPI_Allgather(&readed_non_duplicate_words,1,MPI_INT,&recvs_allFreq_ndWord,1,MPI_INT,MPI_COMM_WORLD);//numero di conteggi delle parole (non duplicate)

    
    /*
                                 [PER DEBUG]
    printf("num car after allgather %d\n",recv_all_num_char[1]);
    printf("readed non duplicate word after allgather %d\n",recvs_allFreq_ndWord[1]);
    */


    //[COMUNICAZIONE DEL RISULTATO AL MASTER] -> Preparazione parametri per la Gatherv
    int rcv_count_rword [world_size];
    int rcv_count_tcounters [world_size];
    int num_count = 0;
    int num = 0;


    /*
    Preparazione parametri per l'array total_counters
    N.B: il master non partecipa al calcolo del displacement e della size quindi si pongono i relativi valori a 0
    */
    for (int i = 0; i < world_size; i++){
        
        rcv_count_tcounters[i] = recvs_allFreq_ndWord[i];
        
        //Il processo 0 setta i suoi parametri a 0 poichè non partecipa al calcolo del displacement e di size
        if(i == 0){
            total_counters_disp[i] = 0;
        }
        else {
            total_counters_disp[i] = total_counters_disp[i-1] + rcv_count_tcounters[i-1];
        }
        num_count += rcv_count_tcounters[i];
        }
    
     /*
    Preparazione parametri per l'array result_word, calcolo dei displecement e delle size
    N.B: il master non partecipa al calcolo del displacement e della size quindi si pongono i relativi valori a 0
    */ 
    for (int i = 0; i < world_size; i++){
        
        rcv_count_rword[i] = recv_all_num_char[i];
        //Il processo 0 setta i suoi parametri a 0 poichè non partecipa al calcolo del displacement e di size
        if(i == 0){
            result_word_disp[i] = 0;
        }
        else {
            result_word_disp[i] = result_word_disp[i-1] + rcv_count_rword[i-1];
        }
        num += rcv_count_rword[i];
        }

        //[MASTER] -> Allocazione spazio per gli array in ricezione (dato che solo questi ultimi sono valorizzati lato MASTER)
        if(rank == 0){

            result_word = malloc(sizeof(char)* num);
            total_counters = malloc(sizeof(int)*num_count);                
    }


    /*
                                                                PARAMETRI GATHERV
    histogram_word/counters: indirizzo iniziale del buffer di invio
    readed_num_char: numero di elementi nel buffer di invio
    MPI_CHAR: tipo di dati degli elementi del buffer di invio
    result_word: indirizzo iniziale del buffer di ricezione
    sec_size: array contenente il numero di elementi da ricevere da ciascun processo
    ..._disp: array che specifica lo spostamento relativo a result_word/total_counters in cui posizionare i dati in ingresso
    MPI_CHAR:tipo di dati del buffer di ricezione
    0: rank del processo di ricezione
    MPI_COMM_WORLD: comunicatore di gruppo
    */

    /*
        Unisco gli istrogrammi locali(word + frequency) di ogni slave in due grandi array e li invio al master
    */
    MPI_Gatherv(histogram_word,readed_num_char,MPI_CHAR,result_word,rcv_count_rword,result_word_disp,MPI_CHAR,0,MPI_COMM_WORLD);//in result_word ho tutte le parole non duplicate in fila
    MPI_Gatherv(counters,readed_non_duplicate_words,MPI_INT,total_counters,rcv_count_tcounters,total_counters_disp,MPI_INT,0,MPI_COMM_WORLD);//in total_counters ho il counter relativo ad ogni parola

    
    //Libero la memoria allocata dagli sleve per histogram_word e per counters
    if(rank != 0) {
        free(histogram_word);
        free(counters);    
    }


    /*
    -------------------------------------------------[MASTER] Merge finale degli istogrammi--------------------------------------------
    */
    if(rank == 0){

        pStruct = pStart;
        char tmp_word[100];
        int index_of_word_count = 0;
        int count_parole = 0;
        /*
         Per ogni parola che incontra verifica se è presente all'interno del proprio istogramma.
         Si -> incrementa la frequency di tale word
         No -> aggiunge la word all'istogramma, utilizzando il counter locale agli slave per quella parola
        */
        for(int n = 0; n < num; n++){
            if(result_word[n] == 0){
                
                addOrIncrWordInMaster(tmp_word,total_counters[count_parole]);//addOrIncrementWordInMaster è un metodo esclusivo del master, se il master ha già quella parola somma la sua occorrenza con quella dello slave, se non ce l'ha usa come conteggio quello rilevato dagli slave che hanno quella parola
                /*
                [DEBUG]: per vedere il counter associato ad ogni word
                printf("%d\n",total_counters[count_parole]);
                */
                memset(tmp_word,0,100);//Una volta verificata la word resetto l'array tmp_word, e l'indice index_of_word_count
                index_of_word_count = 0;
                count_parole++;
            }
            else 
            {
                tmp_word[index_of_word_count] = result_word[n];
                index_of_word_count++;
            }
        }
        /*
        Inserimento dei risultati all'interno del file csv
        */
        pStruct = pStart;
        FILE *file;
        file = fopen("result_word_count.csv","w+");

        while(pStruct != NULL){
            fprintf(file,"WORD: %s --- FREQ: %d\n", returnWord(pStruct),returnWordFrequency(pStruct));
            pStruct = pStruct -> pNext;
        }

        fclose(file);

        //Libero la memoria allocata per result_word e per total_counters
        free(result_word);
        free(total_counters);
        
    }

    MPI_Barrier(MPI_COMM_WORLD);
    finish_time = MPI_Wtime();

    if(rank == 0){
        printf("%f \n",finish_time - start_time);
        fflush(stdout);
    }


    MPI_Finalize();
    return 0;

}