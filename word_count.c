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
int lw_bound = 0, local_partition = 0, partition = 0, resto = 0, n_words = 0;
int numberOfFile = 0, word_counter=0, single_file_word_counter=0, n_char=0;

char path_file[2100];
char temporary_word[500];

//Istogrammi e counters
char *local_histogram;
char *global_histogram;
int *local_counters;
int *global_counters;

struct Word *pStruct = NULL;

double start_time,finish_time;

MPI_Status status;
MPI_Request request;
MPI_Init(&argc,&argv);
MPI_Comm_size(MPI_COMM_WORLD, &world_size);
MPI_Comm_rank(MPI_COMM_WORLD,&rank);

start_time = MPI_Wtime();

//Array [Gather e Ghaterv]
int recv_n_char[world_size];
int global_histogram_disp[world_size];
int global_counters_disp[world_size];
int recvs_counts[world_size];

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
        int while_counter = 0;

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
                            if(ch == ' ' || ch == '\t' || ch == '\n'){
                                single_file_word_counter++;//Tengo traccia del numero totale di parole in un singolo file
                                word_counter++;//Tengo traccia del numero totale di parole in tutti i files
                            }
                        } 
                    
                    number_of_word[while_counter] = single_file_word_counter;
                    while_counter++;
                    single_file_word_counter=0;
                  }
               }

            }
            fclose(fp);
            closedir(directory);

                /*
                Fine del conteggio del numero di parole in ciascun file all'interno della directory
                */
    

    //Partizione
    partition = word_counter / world_size;

    //Eventuale resto
    resto = word_counter % world_size;

    //Send
    for(int i = 1; i < world_size; i++){

        MPI_Isend(&numberOfFile,1,MPI_INT,i,99,MPI_COMM_WORLD,&request);
        MPI_Isend(file_name,sizeof(file_name)/sizeof(file_name[0][0]),MPI_CHAR,i,0,MPI_COMM_WORLD,&request);
        MPI_Isend(number_of_word,sizeof(number_of_word)/sizeof(number_of_word[0]),MPI_INT,i,1,MPI_COMM_WORLD,&request);
        MPI_Isend(&partition,1,MPI_INT,i,2,MPI_COMM_WORLD,&request);
        MPI_Isend(&resto,1,MPI_INT,i,3,MPI_COMM_WORLD,&request);

    }

    //Calcolo partizione del master
    if(resto != 0){
         lw_bound = 0;
         local_partition = partition + 1;
    } else { 
        lw_bound = 0;
        local_partition = partition;
    }


    int index_file = 0;//variabile che mi serve per passare da un file ad un altro, se la partizione copre due file
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

        //Inizio a leggere
        while((ch = fgetc(fp)) != EOF){
           if(isalnum(ch) != 0){
                temporary_word[index_of_tmpword] = ch;
                index_of_tmpword++;
            }
             else {
                if(ch == ' ' || ch == '\t' || ch == '\n'){ //Fine word
                    word_counter++;
                    local_partition--;
                    temporary_word[index_of_tmpword] ='\0';
                    index_of_tmpword++;
                    addWordToList(temporary_word);
                    memset(temporary_word,0,500);
                    index_of_tmpword = 0;
                    //Appena termino la mia partizione esco
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
    }


    /*
     ------------------------------------------------ FINE DEL LAVORO DEL [MASTER] SULLA SUA PARTIZIONE -------------------------------------
    */

   
    /*
     ------------------------------------------------ INIZIO DEL LAVORO DEGLI [SLAVE] ------------------------------------------------------
    */

    else {

        FILE *file;
        int cum_sum = 0,start_to_read = 0;
        int while_counter = 0;

        MPI_Recv(&numberOfFile,1,MPI_INT,0,99,MPI_COMM_WORLD,&status);//Ricevo il numero di file
        char file_name[numberOfFile][100];
        int number_of_word[numberOfFile];
        MPI_Recv(file_name,sizeof(file_name)/sizeof(file_name[0][0]),MPI_CHAR,0,0,MPI_COMM_WORLD,&status);
        MPI_Recv(number_of_word,sizeof(number_of_word)/sizeof(number_of_word[0]),MPI_INT,0,1,MPI_COMM_WORLD,&status);
        MPI_Recv(&partition,1,MPI_INT,0,2,MPI_COMM_WORLD,&status);
        MPI_Recv(&resto,1,MPI_INT,0,3,MPI_COMM_WORLD,&status);
        
        //Gestione del resto per i processi slave, aggiungo il resto in base all'ordine dei rank, ad esempio se ho resto=2 -> il processo di rank 0 aggiunge 1 alla sua partition, il processo di rank 1 aggiunge 1 alla sua partition, il processo di rank 2 non aggiungerà niente e cosi via
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
        
        //size -> numero di file
        int size = sizeof(number_of_word)/sizeof(number_of_word[0]);

        //Ciclo in ogni file, salvando all'interno di cum_sum as ogni iterazione il numero di word presenti in ognuno dei file
        for(int i=0; i < size; i++){

                cum_sum += number_of_word[i];//Somma delle parole in ogni file

                //Trovo il file da cui iniziare a leggere
                if((cum_sum > lw_bound) && (partition > 0)){
                    strcpy(path_file,"file_test/");
                    strcat(path_file,file_name[i]);

                    file = fopen(path_file,"r");
                    if(file == NULL){
                        perror("Unable to open file");
                    }

                    //Trovo il punto all'interno del file da cui iniziare a leggere
                    start_to_read = number_of_word[i] - (cum_sum-lw_bound);
                    /*
                      [Se start_to_read è < 0] -> devo leggere il file dall'inizio (start_to_read = 0)
                      [Se start_to_read è >= 0] -> start_to_read è il punto da cui devo iniziare a leggere
                    */
                    if(start_to_read < 0){
                        start_to_read = 0;
                    }
        
                    //Inizio lettura      
                    while((ch = fgetc(file)) != EOF){
                        if(isalnum(ch)!=0){
                            if(word_counter >= start_to_read){//Arrivo al punto da cui deo iniziare a leggere
                                temporary_word[index_of_tmpword] = ch;
                                index_of_tmpword++;      
                            }
                        }
                        else {
                            if(ch == ' ' || ch == '\t' || ch == '\n'){
                                word_counter++;
                                //Dal momento in cui lo slave arriva al punto da cui deve iniziare a leggere, per ogni parola letta decrementa la sua partizione
                                if(word_counter > start_to_read){
                                    partition--;
                                    temporary_word[index_of_tmpword] = '\0';
                                    index_of_tmpword++;
                                    addWordToList(temporary_word);
                                    memset(temporary_word,0,100);
                                    index_of_tmpword = 0;    
                                }
                                //Appena ho terminato di leggere la mia partizione esco
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

        //Conteggio del numero totale di word all'interno dell'istogramma locale
        n_words = counter_non_duplicate_words();
        //printf("rank %d,number of nun duplicate words : %d\n",rank, n_words);
        local_counters = malloc(sizeof(int)* n_words);


        //Calcolo della grandezza dell'istogramma in termini di caratteri presenti al suo interno.
        pStruct = pStart;
        while(pStruct != NULL){
            n_char += lengthOfCurrentWord(pStruct);
            pStruct = pStruct -> pNext;
            fflush(stdout);
        }

        local_histogram = malloc(sizeof(char) * n_char);
        
        /*
            Inserisco nell'array local_histogram l'insieme di tutte le word rilevate (non duplicate)
            Inserisco nell'array nell'array local_counters l'insieme di tutti i counters delle frequenze delle singole word

        */
        
        pStruct = pStart;
        int index_word_tmp = 0;
        int index_local_histogram = 0;
        char word_tmp[100];
        //Per ogni word
        for(int i=0; i < n_words;i++){
            local_counters[i] = returnWordFrequency(pStruct);
            strcpy(word_tmp,returnWord(pStruct));       
            while(word_tmp[index_word_tmp]!=0){
                local_histogram[index_local_histogram] = word_tmp[index_word_tmp];
                index_local_histogram++;
                index_word_tmp++;
            }

            pStruct = pStruct -> pNext;
            index_word_tmp = 0;
            local_histogram[index_local_histogram] = 0;
            index_local_histogram++;
            
        }

        //Dealloco lo spazio allocato in precedenza per l'istogramma [PER OGNI SLAVE]
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
    
    /*
    -------------------------------------------------------- COMUNICAZIONE RISULTATI AL MASTER ----------------------------------------------------
    */
    //Mi serve per allocare spazio per gli array finali contenenti tutti gli istogrammi degli slave con le relative freqenze associate alle parole.
    MPI_Gather(&n_char,1,MPI_INT,&recv_n_char,1,MPI_INT,0,MPI_COMM_WORLD);//numero di caratteri letti
    MPI_Gather(&n_words,1,MPI_INT,&recvs_counts,1,MPI_INT,0,MPI_COMM_WORLD);//numero di conteggi delle parole (non duplicate)

    int num_count = 0;
    int num = 0;
    
       if(rank == 0){

        for (int i = 0; i < world_size; i++){
        //Il processo 0 setta i suoi parametri a 0 poichè non partecipa al calcolo del displacement e di size
        if(i == 0){
            global_counters_disp[i] = 0;
        }
        else {
            global_counters_disp[i] = global_counters_disp[i-1] + recvs_counts[i-1];
        }
        num_count += recvs_counts[i];
    }

        for (int i = 0; i < world_size; i++){
        //Il processo 0 setta i suoi parametri a 0 poichè non partecipa al calcolo del displacement e di size
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

        //Comunicazione finale
        MPI_Gatherv(local_histogram,n_char,MPI_CHAR,global_histogram,recv_n_char,global_histogram_disp,MPI_CHAR,0,MPI_COMM_WORLD);
        MPI_Gatherv(local_counters,n_words,MPI_INT,global_counters,recvs_counts,global_counters_disp,MPI_INT,0,MPI_COMM_WORLD);
    

    if(rank != 0) {

            free(local_histogram);
            free(local_counters);
        
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
        /*
        Inserimento dei risultati all'interno del file csv
        */
        pStruct = pStart;
        FILE *file;
        file = fopen("result_frequency.csv","w+");
        fprintf(file,"Execution with %d processes\n", world_size);

        while(pStruct != NULL){
            fprintf(file,"WORD: %s --- FREQ: %d\n", returnWord(pStruct),returnWordFrequency(pStruct));
            pStruct = pStruct -> pNext;
        }

        fclose(file);

        //Libero la memoria allocata per global_histogram e per global_counters
        free(global_histogram);
        free(global_counters);
        
    }

    finish_time = MPI_Wtime();

    /*Tempo finale di esecuzione*/
    if(rank == 0){
        printf("Time %f with %d processors \n",finish_time - start_time,world_size);
        fflush(stdout);
    }

    MPI_Finalize();
    return 0;

}
