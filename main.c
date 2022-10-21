#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

// global variables
long t = 0; // en dixièmes de seconde
long update_period = 1;

pid_t ppid, pid;

int selected_mode;

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

void tick(int sig)
{
    kill(ppid, SIGUSR1);
}

enum {
    ALARM_MODE,
    CHRONO_MODE,
    TIMER_MODE,
    TIME_MODE,
    UI_MODE
}
int pick_from_cli_flag(char *flag)
{
    switch(flag[0]) {
        case 'a': return ALARM_MODE;
        case 'c': return CHRONO_MODE;
        case 't': return TIMER_MODE;
        case 'h': return TIME_MODE;
        default: return UI_MODE;
    }
}


int main(int argc, char** argv)
{
    ppid = getpid();
    pid = fork();

    // processus fils
    if (pid == 0) {
        signal(SIGALRM, tick);
        struct itimerval  val  = { {0, 100000}, {0, 100000} },
                          val2 = { {0, 100000}, {0, 100000} };
        setitimer(ITIMER_REAL, &val, &val2);
        while (1) {
            pause();
        }
    }

    switch(selected_mode = pick_from_cli_flag(argv[1])) {
        case TIMER_MODE:
            
            return 0;
    }

    return 0;
}

/*
        if (pid) {
            signal(SIGUSR1, next);
            wait(&status);
        }
        else {
            signal(SIGALRM, tick);
            struct itimerval  val = { {0, 100000}, {0, 100000} },
                            val2 = { {0, 100000}, {0, 100000} };
            setitimer(ITIMER_REAL, &val, &val2);
            while (1) {
                pause();
            }
        }
*/