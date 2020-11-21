#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

/* Message Queue structure */
struct msgbuf {
  long mtype;
  char mtext[100];
};

int detach(int shmid, void *shmaddr);
int generaterandomnumber(int max);
void incrementclock(unsigned int *sec, unsigned int *nano, int amount);
void updatewaitclock(unsigned int *clocksec, unsigned int *clocknano, unsigned int *waitsec, unsigned int *waitnano);
void preparemessage(struct msgbuf buf, int type, char str[100+1], int *len);
void sendmessage(struct msgbuf buf, int msgid, int len);

#define MSG_WAIT_LIMIT 150000000
#define PERMS 0644

int main (int argc, char **argv)
{
  printf("::Begin:: Child Process --%d\n", getpid());
  int index = atoi(argv[2]); // Index/"PID" of process

  unsigned int *clocknano;                  // Shared memory segment for clock nanoseconds
  int clocknanoid = atoi(argv[1]); // ID for shared memory clock nanoseconds segment
  unsigned int *clocksec;                   // Shared memory segment for clock seconds
  int clocksecid = atoi(argv[0]);  // ID for shared memory clock seconds segment

  unsigned int waitsec;  // Time in seconds when process should make next request
  unsigned int waitnano; // Time in nanoseconds when process should make next request

  int len;           // Holds length of mtext to use in msgsnd invocation
  struct msgbuf buf; // Struct for message queue
  int msgid;         // ID for message queue
  key_t msgkey;      // Key for message queue

  int shouldterminate = 0; // Flag for process to end

  /* * MESSAGE QUEUE * */
  // Retrieve key established in oss.c
  if ((msgkey = ftok("keys.txt", 'C')) == -1)
  {
    perror("user_proc.c - ftok");
    exit(1);
  }

  // Access message queue using retrived key
  if ((msgid = msgget(msgkey, PERMS)) == -1)
  {
    perror("user_proc.c - msgget");
    exit(1);
  }


  /* * SHARED MEMORY CLOCK * */
  if ((clocksec = (int *) shmat(clocksecid, NULL, 0)) == (void *) -1)
  {
    perror("Failed to attach to memory segment.");
    return 1;
  }

  if ((clocknano = (int *) shmat(clocknanoid, NULL, 0)) == (void *) -1)
  {
    perror("Failed to attach to memory segment.");
    return 1;
  }


  char strindex[100+1] = {'\0'}; // Create string from shared memory clock seconds id
  sprintf(strindex, "%d", index); 

  // Initialize the wait clock
  updatewaitclock(clocksec, clocknano, &waitsec, &waitnano);

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  /* * * * *                     MAIN LOOP                       * * * * */
  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  while (shouldterminate == 0) {
   
    /* * * SEND MESSAGE: send a request/message to oss * * */

    if (*clocksec > waitsec || (*clocksec == waitsec && *clocknano > waitnano)) {
      preparemessage(buf, 99, strindex, &len);
      sendmessage(buf, msgid, len);
      updatewaitclock(clocksec, clocknano, &waitsec, &waitnano);
    }


    /* * * RECEIVE MESSAGE: receive approval/denial of resource request from oss * * */

    if(msgrcv(msgid, &buf, sizeof(buf.mtext), index, 0) == -1) {
      if (errno != 42) {
        perror("user.c - msgrcv");
        exit(1);
      }
    } else {
      printf(" - - -> P-%d received msg!\n", getpid());
      printf("   -msg: %s\n", buf.mtext);
    }
    
    if (*clocksec > 3) shouldterminate = 1;
  }

  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  /* * * * *                     END MAIN LOOP                   * * * * */
  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */




  /* * CLEAN UP * */
  // Detach from all shared memory segments
  detach(clocksecid, clocksec);
  detach(clocknanoid, clocknano);

  printf("::END:: Child Process --%d\n", getpid());

  return 0;
}

int detach(int shmid, void *shmaddr)
{
  int error = 0;

  if (shmdt(shmaddr) == -1)
    error = errno;
  if (!error)
  {
    //printf("CHILD: Successfully detached from the shared memory segment - id: %d\n", shmid);
    return 0;
  }
  errno = error;
  perror("Error: ");
  return -1;
}

int generaterandomnumber(int max)
{
  return (random() % max) + 1;
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

void updatewaitclock(unsigned int *clocksec, unsigned int *clocknano, unsigned int *waitsec, unsigned int *waitnano)
{
  *waitsec = *clocksec;
  *waitnano = *clocknano;
  incrementclock(waitsec, waitnano, generaterandomnumber(MSG_WAIT_LIMIT));
}

void preparemessage(struct msgbuf buf, int type, char str[100+1], int *len)
{
  buf.mtype = type;
  strcpy(buf.mtext, str);
  *len = strlen(buf.mtext);
}

void sendmessage(struct msgbuf buf, int msgid, int len)
{
  if (msgsnd(msgid, &buf, len + 1, 0) == -1) {
    perror("user_proc msgsnd: ");
  } else {
    printf("*** Child sent msg ***\n");
  }
}
