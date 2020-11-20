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

int detach(int shmid, void *shmaddr);
int generaterandomnumber(int max);

#define PERMS 0644

/* Message Queue structure */
struct msgbuf {
  long mtype;
  char mtext[100];
};

int main (int argc, char **argv)
{
  printf("::Begin:: Child Process --%d\n", getpid());
  int index = atoi(argv[2]); // Index/"PID" of process

  int *clocknano;                  // Shared memory segment for clock nanoseconds
  int clocknanoid = atoi(argv[1]); // ID for shared memory clock nanoseconds segment
  int *clocksec;                   // Shared memory segment for clock seconds
  int clocksecid = atoi(argv[0]);  // ID for shared memory clock seconds segment

  int len;           // Holds length of mtext to use in msgsnd invocation
  struct msgbuf buf; // Struct for message queue
  int msgid;         // ID for message queue
  key_t msgkey;      // Key for message queue

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



  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  /* * * * *                     MAIN LOOP                       * * * * */
  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  int i;
  for (i = 0; i < 1; i++) {
    char str[100+1] = {'\0'}; // Create string from shared memory clock seconds id
    sprintf(str, "%d", index); 

    /* * * SEND MESSAGE: 'request a resource' from oss * * */ 
    buf.mtype = 99;
    strcpy(buf.mtext, str);
    len = strlen(buf.mtext);

    if (msgsnd(msgid, &buf, len+1, 0) == -1) {
      perror("user_proc_msgsnd:");
      exit(1);
    }

    /* * * RECEIVE MESSAGE: receive approval/denial of resource request from oss * * */

    if(msgrcv(msgid, &buf, sizeof(buf.mtext), index, 0) == -1) {
      perror("user.c - msgrcv");
      exit(1);
    }

    printf("P-%d received msg!\n", getpid());
    printf("   -msg: %s\n", buf.mtext);

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
