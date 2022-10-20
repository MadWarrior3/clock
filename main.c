#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// global variables
long t = 0; // en dixièmes de seconde
long update_period = 10;

int status;

FILE* animation_info;
FILE** animation_frames;

void update_display()
{
    printf(
        "%ld:%02ld'%02ld.%ld\"\n", 
        (t/36000),
        (t/600)%60,
        (t/10)%60,
        t%10
    );
}

void next(int sig)
{
    t++;
    if (t % update_period == 0) {
        update_display();
    }
}

int main(int argc, char** argv)
{
    pid_t ppid = getpid(),
          pid = fork();

    if (pid) {
        signal(SIGUSR1, next);
        wait(&status);
    }
    else {
        while (1) {
            usleep(100000);
            kill(ppid, SIGUSR1);
        }
    }

    return 0;
}