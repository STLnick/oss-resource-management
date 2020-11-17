#define _XOPEN_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PERMS 0664

/* Message Queue structure */
struct msgbuf {
  long mtype;
  char mtext[100];
};

int detachandremove(int shmid, void *shmaddr);
void displayhelpinfo();
void generaterandomtime(unsigned int *nano, unsigned int *sec, unsigned int maxnano, unsigned int maxsec);

int main(int argc, char **argv)
{
  int childpid; // Holds PID after fork

  FILE *logptr; // File pointer for logfile

  int len;            // Holds length of mtext to use in msgsnd invocation
  struct msgbuf buf;  // Struct used for message queue
  int msgid;          // ID for allocated message queue
  key_t msgkey;       // Key to use in creation of message queue

  unsigned int delaynano = 0; // Nanoseconds to delay before spawning new child
  unsigned int delaysec = 0;  // Seconds to delay before spawning new child

  unsigned int *clocknano; // Shared memory clock nanoseconds
  int clocknanoid;         // ID for shared memory clock nanoseconds
  key_t clocknanokey;      // Key to use in creation of nanoseconds shared memory
  unsigned int *clocksec;  // Shared memory clock seconds
  int clocksecid;          // ID for shared memory clock seconds
  key_t clockseckey;       // Key to use in creation of seconds shared memory

  int opt; // Used to parse command line options

  // TODO: Add '-v' option to make log outpute VERBOSE

  while ((opt = getopt(argc, argv, "h")) != -1)
  {
    switch (opt)
    {
      case 'h': // Help - describe how to run program and default values
        displayhelpinfo();
        exit(EXIT_SUCCESS);
        break;
      default:
        printf("OSS - Scheduler does not take any options to use from command line other than -h.\n");
        printf("Please use -h for help to see information about program.\n");
        exit(EXIT_FAILURE);
    }
  }

  // Open logfile for writing
  logptr = fopen("output.log", "a");

  // Create dummy txt file to create a key with ftok
  system("touch keys.txt");




  /* * * SETUP SHARED MEMORY CLOCK * * */

  // Keys
  if ((clocknanokey = ftok("keys.txt", 'A')) == -1)
  {
    perror("ftok");
    exit(1);
  }
  
  if ((clockseckey = ftok("keys.txt", 'B')) == -1)
  {
    perror("ftok");
    exit(1);
  }


  // IDs
  if ((clocknanoid = shmget(clocknanokey, sizeof(unsigned int *), IPC_CREAT | 0660)) == -1)
  {
    perror("Failed to create shared memory segment.");
    return 1;
  }

  if ((clocksecid = shmget(clockseckey, sizeof(unsigned int *), IPC_CREAT | 0660)) == -1)
  {
    perror("Failed to create shared memory segment.");
    return 1;
  }

  // Attach to shared memory segments
  if ((clocknano = (unsigned int *) shmat(clocknanoid, NULL, 0)) == (void *) - 1)
  {
    perror("Failed to attach shared memory segment.");
    if (shmctl(clocknanoid, IPC_RMID, NULL) == -1)
      perror("Failed to remove memory segment.");
    return 1;
  }
  
  if ((clocksec = (unsigned int *) shmat(clocksecid, NULL, 0)) == (void *) - 1)
  {
    perror("Failed to attach shared memory segment.");
    if (shmctl(clocksecid, IPC_RMID, NULL) == -1)
      perror("Failed to remove memory segment.");
    return 1;
  }

  // Initialize Clock
  *clocknano = 0;
  *clocksec = 0;



  /* * * SETUP MESSAGE QUEUE * * */
  // Create key to allocate message queue
  if ((msgkey = ftok("keys.txt", 'C')) == -1)
  {
    perror("ftok");
    exit(1);
  }

  // Allocate message queue and store returned ID
  if ((msgid = msgget(msgkey, PERMS | IPC_CREAT)) == -1)
  {
    perror("msgget: ");
    exit(1);
  }


  // TESTING GENERATION OF RANDOM DELAY
  generaterandomtime(&delaynano, &delaysec, MAX_TIME_BETWEEN_PROCS_NS, MAX_TIME_BETWEEN_PROCS_SEC);

  // TESTING: example of how to write to file
  //fprintf(logptr, "nano delay: %u || sec delay: %u\n", delaynano, delaysec);


  
  // Create strings from IDs for exec-ing children
  char strclocksecid[100+1] = {'\0'}; // Create string from shared memory clock seconds id
  sprintf(strclocksecid, "%d", clocksecid); 

  char strclocknanoid[100+1] = {'\0'}; // Create string from shared memory clock nanoseconds id
  sprintf(strclocknanoid, "%d", clocknanoid); 

  char strshmpcbsid[100+1] = {'\0'}; // Create string from shared memory PCB array id
  sprintf(strshmpcbsid, "%d", shmpcbsid); 



  /* * * * * * * * * * * * */
  /* * *   MAIN LOOP   * * */
  /* * * * * * * * * * * * */


  // TODO: Implement the Main Loop Logic ! ! !





  /* * * CLEAN UP * * */

  // Close logfile
  fclose(logptr);

  // Remove dummy txt file used to create keys
  system("rm keys.txt");

  // Remove message queue
  if (msgctl(msgid, IPC_RMID, NULL) == -1)
  {
    perror("msgctl: ");
    exit(1);
  }

  // Detach and remove shared memory segments
  detachandremove(clocknanoid, clocknano);
  detachandremove(clocksecid, clocksec);

  return 0;
}

void displayhelpinfo()
{
  printf("\nOperating System Simulator - Scheduler\n");
  printf("-------------------------\n\n");
  printf("Example command to run:\n\n");
  printf("./oss\n\n");
  printf("-------------------------\n");
  printf("(( INSERT PROGRAM DESCRIPTION HERE ))\n");
  printf("-------------------------\n");
  printf("Program options information:\n");
  printf("NONE - progam does not use any command line options\n");
  printf("-------------------------\n\n");
}

int detachandremove(int shmid, void *shmaddr)
{
  int error = 0;

  if (shmdt(shmaddr) == -1)
    error = errno;
  if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
    error = errno;
  if (!error)
  {
    printf("Successfully detached and removed the shared memory segment - id: %d\n", shmid);
    return 0;
  }
  errno = error;
  perror("Error: ");
  return -1;
}

// Generates a random number using 'maxnano' and 'maxsec' as the upper limit of number generated
// which will store an appropriate number in 'nano' and 'sec' for time between processes, time
// it takes to 'schedule' a process, and the time quantum a process is given
void generaterandomtime(unsigned int *nano, unsigned int *sec, unsigned int maxnano, unsigned int maxsec)
{
  int maxpossibletimenano = maxnano + (maxsec * 1000000000);
  int randomtime = random() % maxpossibletimenano;

  if (randomtime > 1000000000)
  {
    *sec = 1;
    *nano = randomtime - 1000000000;
  }
  else
  {
    *sec = 0;
    *nano = randomtime;
  }
}

void scheduleprocess(struct msgbuf *buf, int *len, int msgid, unsigned int dispatchtime, int childpid, unsigned int *clocksec, unsigned int *clocknano, FILE *logptr)
{
  // TODO: Generate and store random time quantam

  // TODO: Create string from random time quantam to use in msg queue

  // Setup message
  buf->mtype = childpid;  // TODO: Make the mtype represent which PID/process to send msg to out of bitvector
  strcpy(buf->mtext, "50000");
  *len = strlen(buf->mtext);

  // SEND message into queue
  if (msgsnd(msgid, buf, (*len)+1, IPC_NOWAIT) == -1)
    perror("msgsnd:");

  // Increase clock by `dispatchtime`
  *clocknano += dispatchtime;

  // Write to logfile "Dispatching process...."
  fprintf(logptr, "OSS: Dispatching process with PID %d and from Queue 1 at time %u:%u\n", childpid, *clocksec, *clocknano);

  // Write to logifle "total time this dispatch...."
  fprintf(logptr, "OSS: -> total time spent in dispatch was %u nanoseconds\n", dispatchtime);
  

  // RECEIVE a message from the queue
  if(msgrcv(msgid, buf, sizeof(buf->mtext), 99, 0) == -1)
  {
    perror("ERROR::OSS - msgrcv");
    exit(1);
  }

}
