#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>     // for EINTR
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <string.h>
#include "common_threads.h"
#include "common.h"


// Print out the usage of the program and exit
void Usage(char*);

//Declare Hash function
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , uint64_t );

//Declare tree thread function
void *tree(void *arg);

//Global var nblocks to hold the number of blocks
uint32_t nblocks;

//Global eachBlock to hold the mmaped version of the file
uint8_t *eachBlock;

// block size
#define BSIZE 4096


//struct threadVars for passing variables in the tree subroutine
struct threadVars {
  //int threads to hold the number of threads
  int threads;
  //int index to hold the node index
  int index;
  //char *hash
  char *hash;
};
int
main(int argc, char** argv)
{

  //int32_t fd to get file
  int32_t fd;

  // input checking 
  if (argc != 3)
    Usage(argv[0]);

  // open input file
  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
  }

  // use fstat to get file size
  struct stat buf;
  fstat(fd,&buf);
  size_t  size = buf.st_size;

  //int threadNum to hold the value of the number of wanted threads
  int threadNum = atoi(argv[2]);

  //Prints the number of threads wanted
  printf("num Threads = %d \n", threadNum);

  // calculate nblocks
  //nblocks for the number of blocks in total
  nblocks = size / BSIZE;

  //int nblocksThread to hold number of blocks in one thread
  int nblocksThread = nblocks/threadNum;

  //Prints the number of number of blocks per thread
  printf("Blocks per Thread= %u \n", nblocksThread);

  //Print for proper formatting
  printf("hash value = ");

  //eachblock to hold an mmaped array of the file
  eachBlock  =  mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  double start = GetTime();
  // calculate hash value of the input file

  //thread to create instance for tree class
  pthread_t thread;
  //Creates struct threadVars vars to format values to send to tree class
  struct threadVars vars;
  vars.threads = threadNum;
  vars.index = 0;
  vars.hash = malloc(sizeof(char) * 2000);

  //Creates thread to execute tree class
  pthread_create(&thread, NULL, tree, (void*)&vars);

  //Waits for thread to finish
  pthread_join(thread, (void**)&vars);

  //Prints hash value of input file
  printf("%s \n", vars.hash);
  printf("\n");

  double end = GetTime();

  printf("time taken = %f \n", (end - start));

  //Closes file and end programs
  close(fd);
  return EXIT_SUCCESS;
}

uint32_t
jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length)
{
  uint64_t i = 0;
  uint32_t hash = 0;

  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}
void
Usage(char* s)
{
  fprintf(stderr, "Usage: %s filename num_threads \n", s);
  exit(EXIT_FAILURE);
}

//void *tree to execute tree threading of file
void *tree(void* arg){

  //Stores argument in struct threadVars vars
  struct threadVars* vars = (struct threadVars*) arg;

  //Stores thread argument in threadNum
  int threadNum = vars->threads;

  //Stores index argument in currentIndex
  int currentIndex = vars->index;

  //Stores hash value in hash
  char* hash = vars->hash;

  //char tempHolder to temporarily hold the parent node hash value
  char tempHolder[100];

  //char catHolder to temporarily hold the combined parent/child's node hash values
  char catHolder[300];

  //blockOffset to hold the offsetted value of the pointer so pointer points to proper index in array
  uint8_t *blockOffset = eachBlock + (BSIZE * (currentIndex * (nblocks /threadNum)));

  //blockLength to hold length of the block for hashing
  uint64_t blockLength = BSIZE * (int) (nblocks / threadNum);

  //hasNum to hold the hash value of block by calling jenkins_one_at_a_time_hash function
  uint32_t hashNum = jenkins_one_at_a_time_hash(blockOffset, blockLength);

  //Converts hashNum into string and saves it in tempHolder
  sprintf(tempHolder, "%u", hashNum);

  //If statement to check how many child processes the parent node needs to make. If there is 2 child nodes then the following is executed.
  if ( (((2 * currentIndex) + 2) <= (threadNum-1)) && (((2 * currentIndex) + 1) < (threadNum-1))){
    //leftThread and rightThread to make two child threads
    pthread_t leftThread, rightThread;

    //char* to hold the hash values of the left child node and right child node
    char* hashSectLeft;
    char*  hashSectRight;

    //indexLeft and indexRight to to find index of left and right thread
    int indexLeft = (2 * currentIndex) + 1;
    int indexRight = (2 * currentIndex) + 2;

    //struct to hold arguments to pass arguments to left and right thread
    struct threadVars leftVar;
    struct threadVars rightVar;

    //stores the argument in the struct to pass to left child node
    leftVar.threads = threadNum;
    leftVar.index = indexLeft;
    leftVar.hash = malloc(sizeof(char) * 1000);

    //stores the argument in the struct to pass to right child node
    rightVar.threads = threadNum;
    rightVar.index = indexRight;
    rightVar.hash = malloc(sizeof(char) * 1000);

    int leftResult;
    int rightResult;
    leftResult = pthread_create(&leftThread, NULL, tree, (void*)&leftVar);
        if (leftResult != 0) {
                printf("Error creating thread. Error code: %d\n", leftResult);
                    }

    rightResult = pthread_create(&rightThread, NULL, tree, (void*)&rightVar);
        if (rightResult != 0) {
                printf("Error creating thread. Error code: %d\n", rightResult);
                    }
        leftResult = pthread_join(leftThread, (void**)&leftVar);
            if (leftResult != 0) {
                    printf("Error joining thread. Error code: %d\n", leftResult);
                        }
        rightResult = pthread_join(rightThread, (void**)&rightVar);
            if (rightResult != 0) {
                    printf("Error joining thread. Error code: %d\n", rightResult);
                        }

    //Stores the returned hash values within respective char holders
    hashSectLeft = leftVar.hash;
    hashSectRight = rightVar.hash;

    //temp to hold concatendated hash codes
    char temp[300];
    
    //Combines the different hash codes for child and parent and stores them in catHolder
    strcat(catHolder, tempHolder);
    strcat(catHolder, hashSectLeft);
    strcat(catHolder, hashSectRight);

    //catLength to find length of catHolder for hashing
    uint64_t catLength = (uint64_t)strlen(catHolder);

    //Finds combined hash code of child and parent threads through calling jenkins_one_at_a_time_hash functiom
    uint32_t catHashNum = jenkins_one_at_a_time_hash((uint8_t*)catHolder, catLength);
    sprintf(temp, "%u", catHashNum);
    strcat(hash, temp);

  //If there is only one child thread needed then the following is executed
  } else if ( (((2 * currentIndex) + 1) <=(threadNum-1)) && !(((2 * currentIndex) + 2) <=(threadNum-1)) ){

    //leftThread and rightThread to make two child threads
    pthread_t leftThread;

    //char* to hold the hash values of the left child node
    char* hashSectLeft;
    //indexLeft and indexRight to to find index of left thread
    int indexLeft = (2 * currentIndex) + 1;

    //struct to hold arguments to pass arguments to left thread
    struct threadVars leftVar;

    //stores the argument in the struct to pass to left child node
    leftVar.threads = threadNum;
    leftVar.index = indexLeft;
    leftVar.hash = malloc(sizeof(char) * 1000);

    //temp to hold concatendated hash codes
    char temp[300];

   int leftResult;

    leftResult = pthread_create(&leftThread, NULL, tree, (void*)&leftVar);
        if (leftResult != 0) {
                printf("Error creating thread. Error code: %d\n", leftResult);

                            }
         leftResult = pthread_join(leftThread, (void**)&leftVar);
             if (leftResult != 0) {
                     printf("Error joining thread. Error code: %d\n", leftResult);
                         }
                         
    //Stores the returned hash values within hashSectLeft
    hashSectLeft = leftVar.hash;

    //Combines the different hash codes for child and parent and stores them in catHolder
    strcat(catHolder, tempHolder);
    strcat(catHolder, hashSectLeft);

    //catLength to find length of catHolder for hashing
    uint64_t catLength = (uint64_t)strlen(catHolder);

    //Finds combined hash code of child and parent threads through calling jenkins_one_at_a_time_hash functiom
    uint32_t catHashNum = jenkins_one_at_a_time_hash((uint8_t*)catHolder, catLength);
    sprintf(temp, "%u", catHashNum);
    strcat(hash, temp);

  //If there is no child node, then current thread node is a leaf node, simply adds hash code on and returns 
  } else {
    strcat(hash, tempHolder);

  }

  //Stores the newly changed arg values back into original vars arg struct
  vars->threads = threadNum;
  vars->index = currentIndex;
  vars->hash = hash;

  //Exits current struct
  pthread_exit(vars);

  return NULL;
}










