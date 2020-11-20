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

#define LOOP_INC 100000000
#define PERMS 0664
#define PROC_LIMIT 18

#define TEMP_FORK_INT 200000000

/* Message Queue structure */
struct msgbuf {
  long mtype;
  char mtext[100];
};

void incrementclock(unsigned int *sec, unsigned int *nano, int amount);
int detachandremove(int shmid, void *shmaddr);
void displayhelpinfo();
void generaterandomtime(unsigned int *nano, unsigned int *sec, unsigned int maxnano, unsigned int maxsec);
void sendmessage(struct msgbuf buf, int msgid, int len);

int main(int argc, char **argv)
{
  int childpid; // Holds PID after fork
  int proc_cnt; // Holds number of children processes executing
  int status;   // Holds child status returned in wait()

  FILE *logptr; // File pointer for logfile

  int len;            // Holds length of mtext to use in msgsnd invocation
  struct msgbuf buf;  // Struct used for message queue
  int msgid;          // ID for allocated message queue
  key_t msgkey;       // Key to use in creation of message queue

  unsigned int delaynano = 0; // Nanoseconds to delay before spawning new child
  unsigned int delaysec = 0;  // Seconds to delay before spawning new child

  unsigned int forkclocksec;  // Holds seconds when to fork next
  unsigned int forkclocknano; // Holds nanoseconds when to fork next

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

  // Initialize Shared Clock and clock tracking when to fork
  *clocknano = 0;
  *clocksec = 0;
  incrementclock(&forkclocksec, &forkclocknano, TEMP_FORK_INT);



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


  // TESTING: example of how to write to file
  //fprintf(logptr, "nano delay: %u || sec delay: %u\n", delaynano, delaysec);


  
  // Create strings from IDs for exec-ing children
  char strclocksecid[100+1] = {'\0'}; // Create string from shared memory clock seconds id
  sprintf(strclocksecid, "%d", clocksecid); 

  char strclocknanoid[100+1] = {'\0'}; // Create string from shared memory clock nanoseconds id
  sprintf(strclocknanoid, "%d", clocknanoid); 


  /* * * TODO: SETUP RESOURCE DESCRIPTORS * * */ // Like the max claims, allocation, resource, and available tables




  /* * * * * * * * * * * * */
  /* * *   MAIN LOOP   * * */
  /* * * * * * * * * * * * */


  // TODO: Implement the Main Loop Logic ! ! !
  // 1. Non-blocking receive on message queue (processes requesting resources, terminating, releasing a resource)
  // 2. Increment Shared Clock

  int i;
  for (i = 1; i <= 50; i++) {

    // IF clock >= prevclock + wait_to_fork then fork...

    if (*clocksec > forkclocksec || (*clocksec == forkclocksec && *clocknano >= forkclocknano) ) {

      if ((childpid = fork()) == -1) {
        perror("Fork failed!");
        exit(1);
      }
    
      // Child Code - - - - -
      if (childpid == 0) {
        char strindex[100+1] = {'\0'}; // Create string from shared memory clock nanoseconds id
        sprintf(strindex, "%d", i); 
        execl("./user_proc", strclocksecid, strclocknanoid, strindex, '\0');
        perror("Child failed to exec!");
        exit(1);
      }
 
      proc_cnt++;    

      // TESTING: sending messages right away-------
      // TODO: Send a message indicating the upper bound B of time the child will wa
      buf.mtype = i;
      sprintf(buf.mtext, "%i:: String here\n", i);
      len = strlen(buf.mtext);

      sendmessage(buf, msgid, len);
      // --------------------------------------------


      // set next time to fork another process
      incrementclock(&forkclocksec, &forkclocknano, TEMP_FORK_INT);
    }


    printf("TEST: right before msgrcv in mainloop...\n"); 

    // Non-blocking receive on message queue
    // TODO: Alter message to bring in the PID(0,1,2), Resource Needed(R1,R2,...), # of Resource Needed(0,1,2...)
    if(msgrcv(msgid, &buf, sizeof(buf.mtext), 99, IPC_NOWAIT) == -1)
    {
      if (errno != 42) { // 42 is just no message, perror all other errors
        perror("oss.c - msgrcv");
        exit(1);
      }
    } else {
      printf("OSS::msg rcv: \"%s\"\n", buf.mtext);

      // If message received write appropriate msg to logfile
      // TODO: - replace 'Rx' with the actual resource requested from process
      fprintf(logptr, "OSS has detected Process %s requesting resource Rx at time %u:%u\n", buf.mtext, *clocksec, *clocknano);

    }



    // TODO: Message is a REQUEST => run D.A. algorithm to determine safe/unsafe state


    // TESTING: using smaller number for test, have constant PROC_LIMIT defined at 18
    if (proc_cnt == 3) {
      if(wait(&status) == -1) {
        perror("wait() ");
      }
      proc_cnt--;
      printf("...waited...\n");
    }

    // Increment shared clock
    incrementclock(clocksec, clocknano, LOOP_INC);
  }

  // TODO: TESTING
  printf("Passed initial loop of OSS --- before 2nd for to msgsnd\n");


  while (proc_cnt != 0) {
    wait(&status);
    proc_cnt--;
  }

  printf("Waited for last children...\n");



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

void incrementclock(unsigned int *sec, unsigned int *nano, int amount)
{
  int sum = *nano + amount;
  if (sum >= 1000000000) {
    *nano = (sum - 1000000000);
    *sec += 1;
  } else {
    *nano = sum;
  }
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

  if (randomtime >= 1000000000)
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

void sendmessage(struct msgbuf buf, int msgid, int len)
{
  if (msgsnd(msgid, &buf, len + 1, 0) == -1)
    perror("msgsnd:");
}
