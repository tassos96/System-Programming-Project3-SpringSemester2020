/**
 *  This file contains all worker operations. 
 **/
#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h> 
#include <fcntl.h>
#include <signal.h>

#include "include/namedPipes.h"
#include "include/workerOperations.h"
#include "include/fileOperations.h"
#include "include/recordsList.h"
#include "include/hashTables.h"
#include "include/interface.h"

#define DISEASE_HASH_TABLE_SIZE 5
#define HASH_TABLE_BUCKET_SIZE 50 
#define DATES_LEN 10

#define LISTEN_QUEUE_SIZE 10

void perror_exit(char *message);
void sockets_read_data (int fd, char *msg_buffer, int msgTotalBytes);
int sockets_write_data(int fd, char* msgToSend);

//Flag for signal handling
volatile sig_atomic_t printLogFile = 0;

//Function for signal handling
void workerCatchSignals(int signo)
{
    printLogFile = 1;
}

//==============================================    WORKERS    ==============================================//
//Function for worker to open his pipes that already has been created
int workerOpenPipes(int* readFD, int* writeFD, int curWorker)
{
    //Get pipes name to open
    char* readName = NULL;
    char* writeName = NULL;

    getNamedPipesName(curWorker, &readName, &writeName);

    if((*readFD = open(readName, O_RDWR)) < 0)
    {
        perror("Named pipe open error");
        return -1;
    }
    
    if((*writeFD = open(writeName, O_WRONLY)) < 0)
    {
        perror("Named pipe open error");
        return -1;
    }
    fcntl(*writeFD, F_SETFL, O_NONBLOCK); //make it Non-Block

    free(readName);
    free(writeName);
    return 1;
}

//Function for all the worker operations
int workerOperations(int readFD, int writeFD, int bufferSize, int input_dirLen)
{   
    //whoServer infos
    char* serverIP = NULL;
    int serverPort = -1;

    //SIGNALS
    static struct sigaction act;
    act.sa_handler = workerCatchSignals;
    sigfillset(&(act.sa_mask));
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGINT);
    sigaddset(&(act.sa_mask), SIGQUIT);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    ////////////////////////////////
    
    //Dynamic array to save paths
    int dirPathsArraySize = 1;
    char** dirPathsArray = malloc(dirPathsArraySize * sizeof(*dirPathsArray));
    
    //READ ALL PATHS PARENT SENDS
    while (1)
    {   
        //Read how many bytes parent will send
        int msgTotalBytes;
        read(readFD, &msgTotalBytes, sizeof(int));
        //printf("Bytes to read: %d\n", msgTotalBytes);

        //Read message from parent and store it
        char* message = calloc(msgTotalBytes+1, sizeof(char*));

        if(readFromParent(readFD, msgTotalBytes, message, bufferSize) == 2)
            break;
        //printf("%s\n", message);

        //Store path to string array
        if(dirPathsArraySize == 1)
        {
            dirPathsArray[dirPathsArraySize-1] = malloc(sizeof(char)*(strlen(message)+1));
            strcpy(dirPathsArray[dirPathsArraySize-1], message);
            dirPathsArraySize++;
        }
        else
        {
            dirPathsArray = realloc(dirPathsArray, (dirPathsArraySize)*sizeof(*dirPathsArray));
            dirPathsArray[dirPathsArraySize-1] = malloc(sizeof(char)*(strlen(message)+1));
            strcpy(dirPathsArray[dirPathsArraySize-1], message);
            dirPathsArraySize++;
        }
        free(message);    
    }
    //Worker send message to parent that all paths received
    char* test = "FATHER ALL PATHS RECEIVED";
    writeToParent(writeFD, test);

    //READ FROM PARENT whoServer INFOS

    //READ SERVER PORT FROM PIPE
    read(readFD, &serverPort, sizeof(int));
    //printf("%d\n", serverPort);
    
    while (1)
    {
        //READ SERVER IP FROM PIPE
        int msgTotalBytes;
        read(readFD, &msgTotalBytes, sizeof(int));

        char* message = calloc(msgTotalBytes+1, sizeof(char*));

        if(readFromParent(readFD,msgTotalBytes, message, bufferSize) == 2)
        {
            free(message);
            break;
        }
        serverIP = malloc(sizeof(char)*(strlen(message)+1));
        strcpy(serverIP, message);

        free(message);
    }
    
    printf("Worker: %d, serverPort: %d, serverIP: %s\n", getpid(), serverPort, serverIP);
    //sleep(3);
    //VARS FOR SOCKETS
    //FOR WORKER TO SEND LISTENING PORT AND STATS TO SERVER
    int worker_server_Socket;
    struct sockaddr_in serverAddr;
    struct sockaddr *serverPtr = (struct sockaddr*)&serverAddr;
    struct hostent *rem;
    /* Create socket */
    if ((worker_server_Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	perror_exit("socket");
    }
    setsockopt(worker_server_Socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

	/* Find server address */
    
    if ((rem = gethostbyname(serverIP)) == NULL) 
    {	
	   herror("gethostbyname");
       free(serverIP);
       exit(1);
    }
    serverAddr.sin_family = AF_INET; //Internet domain
    //memcpy(&serverAddr.sin_addr, rem->h_addr, rem->h_length);
    bcopy((char*) rem->h_addr, (char*)&serverAddr.sin_addr, rem->h_length);
    serverAddr.sin_port = htons(serverPort); //Server port
    //Initialize connection
    if(connect(worker_server_Socket, serverPtr, sizeof(serverAddr)) < 0)
    {
        perror_exit("Connect worker");
    }
    printf("Worker connecting to %s port %d\n", serverIP, serverPort);

    free(serverIP);
    //FOR REQUESTS FROM SERVER
    int server_worker_Socket;
    int new_server_worker_Socket;
    int workerPort;

    struct sockaddr_in server_workerAddr;
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    struct sockaddr* server_workerPtr = (struct sockaddr *) &server_workerAddr; 
    struct sockaddr* clientPtr = (struct sockaddr *) &clientAddr;

    if( (server_worker_Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("Socket client");
    }
    setsockopt(server_worker_Socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    bzero((char *) &server_workerAddr, sizeof(server_workerAddr));
    server_workerAddr.sin_family = AF_INET;
    server_workerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_workerAddr.sin_port = 0; //0 so it can take random free port
    if( bind(server_worker_Socket, server_workerPtr, sizeof(server_workerAddr)) < 0)
    {
        perror_exit("Bind worker");
    }
    //Get the choosen random port
    socklen_t len = sizeof(server_workerAddr);
    if(getsockname(server_worker_Socket, server_workerPtr, &len) == -1)
    {
        perror_exit("Get socket name");
    }
    workerPort = ntohs(server_workerAddr.sin_port);
    printf("Worker listens for requests to port: %d\n", workerPort);
    
    // LISTEN FOR CONNECTIONS
    if( listen(server_worker_Socket, LISTEN_QUEUE_SIZE) < 0)
    {
        perror_exit("Listen worker");
    }
    //send worker listening port to server
    char workerPortString[15];
    sprintf(workerPortString, "%d", workerPort);
    sockets_write_data(worker_server_Socket, workerPortString);
    ////////////////////
    //######### END WITH PARENT PROCESS AND PIPES ###########
    
    //Store how many dirs worker have to open
    int totalDirPathsToOpen = dirPathsArraySize-1;
    
    //Array to contain all countries to process
    char ** countries = malloc(totalDirPathsToOpen * sizeof(*countries));
    int offset = input_dirLen+1; //offset to take country name from path
     
    //Store countries that worker have to process
    for(int i = 0; i < totalDirPathsToOpen; i++)
    {
        //Get country name
        int bytesToCopy = strlen(dirPathsArray[i]) - input_dirLen;
        countries[i] = malloc(sizeof(char) * (bytesToCopy + 1));
        //Store country name
        strncpy(countries[i], dirPathsArray[i]+offset, bytesToCopy);
        countries[i][bytesToCopy] = '\0';
    }

    //Print countries worker have to process
    // for(int i = 0; i < totalDirPathsToOpen; i++)
    // {
    //     printf("Country_%d: %s\n", i, countries[i]);
    // }

    //###########################################################################################
    //Create structures to store records from files (Structures from 1st Project)
    FILE* filePointer;

    RecordsList recordsList = malloc(sizeof(List));
    recordsList->headNode = initializeRecordsList();
    recordsList->tailNode = recordsList->headNode;

    HashTable diseaseHashTable;
    diseaseHashTable = createHashTable(DISEASE_HASH_TABLE_SIZE, HASH_TABLE_BUCKET_SIZE);
    if(diseaseHashTable == NULL)
    {
        freeWorkerMemory(diseaseHashTable, NULL, recordsList, totalDirPathsToOpen, countries, dirPathsArray);
        return -1;
    }

    HashTable countryHashTable;
    countryHashTable = createHashTable(DISEASE_HASH_TABLE_SIZE, HASH_TABLE_BUCKET_SIZE);
    if(countryHashTable == NULL)
    {
        freeWorkerMemory(diseaseHashTable, NULL, recordsList, totalDirPathsToOpen, countries, dirPathsArray);
        return -1;
    }
    
    //Counters for records errors (UNUSED)
    int successCounter = 0;
    int failsCounter = 0;
    //##########################################################################################3
    
    //String array to store dates (File names)
    char** dateFileNamesPath = NULL;
    int totalFileNames = 0;
    
    //FOR EVERY COUNTRY DIR PATH INTO ARRAY
    for(int curCountryPath = 0; curCountryPath < totalDirPathsToOpen; curCountryPath++)
    {
        //GET DD-MM-YYYY FILENAMES INTO COUNTRIES DIRS
        dateFileNamesPath = getDateFileNames(dirPathsArray[curCountryPath], &totalFileNames);

        char* countryDirName = countries[curCountryPath];     

        //Print file names to open for test
        //Must be dd-mm-yyyy sorted
        for(int i = 0; i < totalFileNames; i++)
        {
        
            char dateName[DATES_LEN + 1];
            int offset = strlen(dateFileNamesPath[i]) - DATES_LEN;
            strncpy(dateName, dateFileNamesPath[i] + offset, DATES_LEN);
            dateName[DATES_LEN] = '\0';
            
            //Read and store records from each file
            //#################################################################################
            if(readRecordsFromFile(&filePointer, diseaseHashTable, countryHashTable, recordsList, dateFileNamesPath[i], countryDirName, dateName, &successCounter, &failsCounter) == -1)
            {
                freeHashTable(diseaseHashTable);
                free(diseaseHashTable);
                freeHashTable(countryHashTable);
                free(countryHashTable);
                freeRecordsList(recordsList);
                free(recordsList);
                for(int i = 0; i < totalDirPathsToOpen; i++)
                {
                    free(countries[i]);
                    free(dirPathsArray[i]);
                    
                }
                for(int j = 0; j < totalFileNames; j++)
                {
                    free(dateFileNamesPath[j]);
                }
                free(countries);
                free(dirPathsArray);
                free(dateFileNamesPath);
                return -1;
            }
            
            //here after file reading we have to send file statistics to father
            //final stats message format to be -> Country|Date|Disease1_statistics|Disease2_statistics.....
            //Date -> dateName  
            //Country -> countryDirName
            
            //start construction of msg to send
            char* summaryStatsMsgToSend = malloc( (strlen(countryDirName) + strlen("|") + strlen(dateName) +1) * sizeof(char) );
            sprintf(summaryStatsMsgToSend, "%s|%s", countryDirName, dateName);
            
            ////////////////////////////////
            //GET ALL DISEASES INTO AN ARRAY
            char** diseases = NULL;
            int diseasesCounter = 0;
            Bucket temporaryDisease;
            for(int diseaseHashIndex = 0; diseaseHashIndex < diseaseHashTable->hashTableSize; diseaseHashIndex++)
            {
                temporaryDisease = diseaseHashTable->bucketList[diseaseHashIndex];
                while (temporaryDisease != NULL)
                {
                    for(int j = 0; j < temporaryDisease->maxBucketEntries; j++)
                    {
                        if(temporaryDisease->entries[j] != NULL && temporaryDisease->entries[j]->treeRoot != NULL )
                        {
                            diseasesCounter++;
                            diseases = realloc(diseases, diseasesCounter * sizeof(*diseases));
                            diseases[diseasesCounter-1] = malloc(sizeof(char)*(strlen(temporaryDisease->entries[j]->entryString)+1));
                            strcpy(diseases[diseasesCounter-1], temporaryDisease->entries[j]->entryString);
                        }
                    }
                    temporaryDisease = temporaryDisease->nextBucket;
                }
            }
            
            ///////////////////////////////
            //Calculate all diseases statistics
            //Create disease statistics message
            //Format for each disease -> Disease1.range1.range2.range3.range4 ( . as delimiter)
            Bucket temporary;
            for(int k = 0; k < countryHashTable->hashTableSize; k++)
            {
                temporary = countryHashTable->bucketList[k];
                while (temporary != NULL)
                {
                    for(int j = 0; j < temporary->maxBucketEntries; j++)
                    {
                        if(temporary->entries[j] != NULL && temporary->entries[j]->treeRoot != NULL )
                        {
                            if(strcmp(temporary->entries[j]->entryString, countryDirName) == 0)
                            {
                                for(int di = 0; di < diseasesCounter; di++)
                                {
                                    int counterRange_0_20 = 0;
                                    int counterRange_21_40 = 0;
                                    int counterRange_41_60 = 0;
                                    int counterRange_61_max = 0;
                                    summaryStatisticsCountNodes(temporary->entries[j]->treeRoot, dateName, diseases[di], &counterRange_0_20, &counterRange_21_40, &counterRange_41_60, &counterRange_61_max);
                                    if(counterRange_0_20 == 0 && counterRange_21_40 == 0 && counterRange_41_60 == 0 && counterRange_61_max == 0)
                                    {
                                        continue;
                                    }
                                    
                                    size_t plusSize = strlen("|") + strlen(diseases[di]) + strlen(".") + howManyDigits(counterRange_0_20) + strlen(".") + howManyDigits(counterRange_21_40) + strlen(".") + howManyDigits(counterRange_41_60) + strlen(".") + howManyDigits(counterRange_61_max) + 1;

                                    char* plusMsg = malloc(plusSize * sizeof(char));
                                    sprintf(plusMsg, "|%s.%d.%d.%d.%d", diseases[di], counterRange_0_20, counterRange_21_40, counterRange_41_60, counterRange_61_max);
                                    summaryStatsMsgToSend = realloc(summaryStatsMsgToSend, (strlen(summaryStatsMsgToSend) + plusSize) * sizeof(char));
                                    strcat(summaryStatsMsgToSend, plusMsg);

                                    free(plusMsg);
                                }
                            }
                        }
                    }
                    temporary = temporary->nextBucket;
                }    
            }
            
            //##############################################################################################
            for(int di = 0; di < diseasesCounter; di++)
            {
                free(diseases[di]);
            }
            free(diseases);

            //Send summary statistics to parent
            sockets_write_data(worker_server_Socket,summaryStatsMsgToSend);
            free(summaryStatsMsgToSend);
        }
        
        for(int j = 0; j < totalFileNames; j++)
        {
            free(dateFileNamesPath[j]);
        }
        free(dateFileNamesPath);
    }

    char* end_msg = "END";
    sockets_write_data(worker_server_Socket, end_msg);

    sleep(1);
    close(worker_server_Socket);
    //WORKER READY TO ACCEPT QUERIES FROM FROM SOCKET !!!
    if( (new_server_worker_Socket = accept(server_worker_Socket, clientPtr, &clientLen)) < 0)
    {
        perror_exit("Accept worker: ");
    }
    /* Find client's address */
    printf("Accepted worker connection\n");

    //Counters for logFiles
    int totalReceived = 0;
    int totalSuccess = 0;
    int totalFails = 0;
    while (1)
    {
        if(waitAndExecuteRequests(recordsList, new_server_worker_Socket, countries, totalDirPathsToOpen, &totalReceived, &totalSuccess, &totalFails) == 0)
            break;
    }
    close(new_server_worker_Socket);
    close(server_worker_Socket);
    
    //FREEEE ALLOCATED MEMORY
    freeWorkerMemory(diseaseHashTable, countryHashTable, recordsList, totalDirPathsToOpen, countries, dirPathsArray);
    return 1;
}

//Function for message reading from parent
int readFromParent(int readFD, int msgTotalBytes, char *message, int bufferSize)
{
    int nread;
    int totalReaden = 0;
    char buf[bufferSize];

    for(;;)
    {
        if(printLogFile == 1)   //If signal arrived
        {
            free(message);
            return 3;
        }
        if((totalReaden + bufferSize) > msgTotalBytes)
        {
            int toRead = msgTotalBytes - totalReaden;
            nread = read(readFD, buf, toRead);
        }
        else
        {
            nread = read(readFD, buf, bufferSize);
        }
        
        if(nread > 0)
        {
            for(int i = 0; i < nread; i++)
            {
                (message)[totalReaden+i] = buf[i];
            }
            totalReaden += nread;
            if(totalReaden == msgTotalBytes)
                break;
        }
    }
    (message)[msgTotalBytes] = '\0';
    
    if(strcmp((message), "PATHS_END") == 0)
    {
        return 2;
    }
    if(strcmp((message), "INFOS_END") == 0)
    {
        return 2;
    }
    if(strcmp((message), "END") == 0)
    {
        return 3;
    }

    return 1;
}

//Function for writting message to parent
int writeToParent(int writeFD, char* msgToSend)
{
    
    int nwrite;
    int bytesToSend = strlen(msgToSend)+1;
    
    write(writeFD, &bytesToSend, sizeof(int));

    if((nwrite = write(writeFD, msgToSend, strlen(msgToSend)+1)) == -1)
    {
        perror("Error in worker writting");
        return -1;
    }

    return 1;
}

//Function to free some memory allocated for records storage structures, paths and countries
void freeWorkerMemory(HashTable diseaseHashTable, HashTable countryHashTable, RecordsList recordsList, int totalDirPathsToOpen, char **countries, char **dirPathsArray)
{
    freeHashTable(diseaseHashTable);
    free(diseaseHashTable);
    if(countryHashTable != NULL)
    {
        freeHashTable(countryHashTable);
        free(countryHashTable);
    }
    freeRecordsList(recordsList);
    free(recordsList);
    //free pathsArray and countries array
    for(int i = 0; i < totalDirPathsToOpen; i++)
    {
        free(countries[i]);
        free(dirPathsArray[i]);
        
    }
    free(countries);
    free(dirPathsArray);
}

//Function that executes requests from parent
int waitAndExecuteRequests(RecordsList recordsList, int socketFD, char** countries, int totalDirPathsToOpen, int* totalReceived, int* totalSuccess, int* totalFails)
{
    while(1)
    {   
        if(printLogFile == 1)   //If signal arrived
        {
            writeWorkerLogFile(countries, totalDirPathsToOpen, (*totalReceived), (*totalSuccess), (*totalFails));
            printLogFile = 0;
            break;
        }

        //Total bytes to read
        int msgTotalBytes;
        read(socketFD, &msgTotalBytes, sizeof(int));
        //printf("Bytes to read: %d\n", msgTotalBytes);

        //Read command from parent
        char* message = NULL;
        message = calloc(msgTotalBytes+1, sizeof(char));
        // if(readFromParent(readFD, msgTotalBytes, message, bufferSize) == 3)
        //     break;
        sockets_read_data(socketFD, message, msgTotalBytes);
        if(strcmp(message, "END") == 0)
            break;
        //printf("Received command: %s\n", message);

        //Store command and its arguments
        char* command = NULL;
        char* copyOfCommand = NULL;
        char** arguments = NULL;
        int argumentsNum = 0;
        
        copyOfCommand = malloc(strlen(message) * sizeof(char) + 1);
        strcpy(copyOfCommand, message);

        command = strtok(message, " ");
        argumentsNum = countCommandArguments(&command);
        arguments = getCommandArguments(&arguments, &copyOfCommand, argumentsNum);

        free(copyOfCommand);
        free(message);

        //################################################
        if(strcmp(arguments[0], "/searchPatientRecord") == 0)
        {
            (*totalReceived)++;
            RecordListNode recordNode = checkId(recordsList, arguments[1]);
            if(recordNode != NULL)
            {
                char* msg = NULL;
                int digits = howManyDigits(recordNode->patientAge);
                char date1[11];
                char date2[11];
                sprintf(date1, "%02d-%02d-%04d", recordNode->entryDate->day, recordNode->entryDate->month, recordNode->entryDate->year);
                if(recordNode->exitDate->day == 0)
                {
                    strcpy(date2, "--");
                }
                else
                {
                    sprintf(date2, "%02d-%02d-%04d", recordNode->entryDate->day, recordNode->entryDate->month, recordNode->entryDate->year);
                }
                msg = malloc(sizeof(char*) * ( strlen(recordNode->recordID) + 1 + 
                      strlen(recordNode->patientFirstName) + 1 + strlen(recordNode->patientLastName) + 1 + 
                      strlen(recordNode->diseaseID) + 1 + digits + 1 + strlen(date1) + 1 + strlen(date2)) + 1);
                sprintf(msg, "%s %s %s %s %d %s %s", recordNode->recordID, recordNode->patientFirstName, recordNode->patientLastName, recordNode->diseaseID, recordNode->patientAge, date1, date2);
                
                //writeToParent(writeFD, msg);
                sockets_write_data(socketFD, msg);
                printf("msg sended\n");
                (*totalSuccess)++;
                free(msg);
                
            }
            else
            {
                //sockets_write_data(socketFD, "NOT FOUND");
                (*totalFails)++;
            }
            
            sockets_write_data(socketFD, "END");
            for(int i = 0; i < argumentsNum; i++)
            {
                free(arguments[i]);
            }
            free(arguments);
            break;
        }
        else if(strcmp(arguments[0], "/numPatientDischarges") == 0)
        {
            (*totalReceived)++;
            if(argumentsNum == 4)
            {
                char* virusName = malloc(sizeof(char*) * strlen(arguments[1]) +1);
                strcpy(virusName, arguments[1]);

                char* date1_string = malloc(sizeof(char*) * strlen(arguments[2]) + 1);
                strcpy(date1_string, arguments[2]);

                char* date2_string = malloc(sizeof(char*) * strlen(arguments[3]) + 1);
                strcpy(date2_string, arguments[3]);

                DateStructure date1 = setDate(date1_string);
                if (date1->day == -1 || date1->month == -1 || date1->year == -1)
                {
                    destroyDate(date1);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                DateStructure date2 = setDate(date2_string);
                if (date2->day == -1 || date2->month == -1 || date2->year == -1)
                {
                    destroyDate(date1);
                    destroyDate(date2);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                if(datesOrderValidation(date1, date2) == -1)
                {
                    destroyDate(date1);
                    destroyDate(date2);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                for(int i = 0; i < totalDirPathsToOpen; i++)
                {
                    int totalDischarges = 0;
                    countDischarges(recordsList, virusName, date1, date2, countries[i], &totalDischarges);

                    char* msg = NULL;
                    int digits = howManyDigits(totalDischarges);
                    msg = malloc(sizeof(char*) * (strlen(countries[i]) + 1 + digits) + 1);
                    sprintf(msg, "%s %d", countries[i], totalDischarges);

                    //writeToParent(writeFD, msg);
                    sockets_write_data(socketFD, msg);

                    free(msg);
                }

                //writeToParent(writeFD, "END");
                sockets_write_data(socketFD, "END");
                (*totalSuccess)++;
                destroyDate(date1);
                destroyDate(date2);
                free(virusName);
                free(date1_string);
                free(date2_string);
                for(int i = 0; i < argumentsNum; i++)
                {
                    free(arguments[i]);
                }
                free(arguments);
                break;
            }
            if(argumentsNum == 5)
            {
                char* virusName = malloc(sizeof(char*) * strlen(arguments[1]) +1);
                strcpy(virusName, arguments[1]);

                char* date1_string = malloc(sizeof(char*) * strlen(arguments[2]) + 1);
                strcpy(date1_string, arguments[2]);

                char* date2_string = malloc(sizeof(char*) * strlen(arguments[3]) + 1);
                strcpy(date2_string, arguments[3]);

                char* country = malloc(sizeof(char*) * strlen(arguments[4]) + 1);
                strcpy(country, arguments[4]);

                DateStructure date1 = setDate(date1_string);
                if (date1->day == -1 || date1->month == -1 || date1->year == -1)
                {
                    destroyDate(date1);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    free(country);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                DateStructure date2 = setDate(date2_string);
                if (date2->day == -1 || date2->month == -1 || date2->year == -1)
                {
                    destroyDate(date1);
                    destroyDate(date2);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    free(country);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                if(datesOrderValidation(date1, date2) == -1)
                {
                    destroyDate(date1);
                    destroyDate(date2);
                    free(virusName);
                    free(date1_string);
                    free(date2_string);
                    free(country);
                    for(int i = 0; i < argumentsNum; i++)
                    {
                        free(arguments[i]);
                    }
                    free(arguments);
                    (*totalFails)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                    continue;
                }

                int countryInArray = 0;
                for(int i = 0; i < totalDirPathsToOpen; i++)
                {
                    if(strcmp(countries[i], country) == 0)
                    countryInArray = 1;
                }
                if(countryInArray == 1)
                {
                    int totalDischarges = 0;
                    countDischarges(recordsList, virusName, date1, date2, country, &totalDischarges);

                    char* msg = NULL;
                    int digits = howManyDigits(totalDischarges);
                    msg = malloc(sizeof(char*) * (strlen(country) + 1 + digits) + 1);
                    sprintf(msg, "%s %d", country, totalDischarges);

                    //writeToParent(writeFD, msg);
                    sockets_write_data(socketFD, msg);
                    (*totalSuccess)++;

                    free(msg);
                
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                }
                else
                {
                    (*totalSuccess)++;
                    //writeToParent(writeFD, "END");
                    sockets_write_data(socketFD, "END");
                }
                destroyDate(date1);
                destroyDate(date2);
                free(virusName);
                free(date1_string);
                free(date2_string);
                free(country);
                for(int i = 0; i < argumentsNum; i++)
                {
                    free(arguments[i]);
                }
                free(arguments);
                break;
            }
        }
        else if(strcmp(arguments[0], "/exit") == 0)
        {
            for(int i = 0; i < argumentsNum; i++)
            {
                free(arguments[i]);
            }
            free(arguments);
            return 0;
        }
    }
    return 1;
}

//Function used for worker log file creation
void writeWorkerLogFile(char** countries, int totalCountries, int total, int success, int fail)
{
    pid_t workerID = getpid();
    int idDigits = howManyDigits(workerID);

    char* logFileName = malloc(sizeof(char*) * (strlen("./log_files/log_file.") + idDigits) + 1);
    sprintf(logFileName, "./log_files/log_file.%d", workerID);

    char* msgToLog = NULL;
    int totalBytes = 0;
    for(int i = 0; i < totalCountries; i++)
    {
        totalBytes += strlen(countries[i]) + strlen("\n");
    }
    int totalDigits = howManyDigits(total);
    int successDigits = howManyDigits(success);
    int failDigits = howManyDigits(fail);

    totalBytes += strlen("TOTAL") + totalDigits + strlen("\n");
    totalBytes += strlen("SUCCESS") + successDigits + strlen("\n");
    totalBytes += strlen("FAIL") + failDigits + strlen("\n");
    
    char* totalMsg = malloc(sizeof(char*) * (strlen("TOTAL ") + totalDigits + strlen("\n")) + 1);
    sprintf(totalMsg, "TOTAL %d\n", total);

    char* successMsg = malloc(sizeof(char*) * (strlen("SUCCESS ") + successDigits + strlen("\n")) + 1);
    sprintf(successMsg, "SUCCESS %d\n", success);

    char* failMsg = malloc(sizeof(char*) * (strlen("FAIL ") + failDigits + strlen("\n")) + 1);
    sprintf(failMsg, "FAIL %d\n", fail);

    msgToLog = calloc(totalBytes + 1, sizeof(char*));
    for(int i = 0; i < totalCountries; i++)
    {
        strcat(msgToLog, countries[i]);
        strcat(msgToLog, "\n");
    }
    strcat(msgToLog, totalMsg);
    strcat(msgToLog, successMsg);
    strcat(msgToLog, failMsg);

    FILE* file;
    file = fopen(logFileName, "w");
    if(file == NULL)
    {
        perror("Worker LogFile");
        free(logFileName);
        free(msgToLog);
        free(totalMsg);
        free(successMsg);
        free(failMsg);
        return;
    }

    fputs(msgToLog, file);  //write msg to file

    free(logFileName);
    free(msgToLog);
    free(totalMsg);
    free(successMsg);
    free(failMsg);
    fclose(file);
    return;
}

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

//sockets
void sockets_read_data (int fd, char *msg_buffer, int msgTotalBytes)
{
    //Read formated data
    read(fd, msg_buffer, msgTotalBytes);
    (msg_buffer)[msgTotalBytes] = '\0';
}

int sockets_write_data(int fd, char* msgToSend)
{
    //Write formated data
    int nwrite;
    size_t bytesToSend = strlen(msgToSend)+1;
    write(fd, &bytesToSend, sizeof(int));
    
    if((nwrite = write(fd, msgToSend, strlen(msgToSend)+1)) == -1)
    {
        perror("Error in writing");
        return -1;
    }

    return 1;
}