#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

/*
 * SIGUSR1 est utilisé pour afficher l'heure / le temps écoulé / le temps restant selon le mode
 * SIGUSR2 est utilisé pour déclencher une sonnerie
 */

 /*
  * dans la CLI, le temps est donné au format [jours]j[heures]:[minutes]:[secondes].[dixiemes]
  * ou simplement [secondes].[dixiemes]
  * (à cause du fonctionnement du comptage, les valeurs hors plage sont automatiquement converties et ajoutées à l'unité du dessus par le fils)
  */

// global variables
long t = 0; // en dixièmes de seconde
long update_period = 1;

char valeur_initiale[1024], action[1024], pas[1024], valeur_finale[1024];

pid_t ppid, pid;

int selected_mode;

int status;

FILE* animation_info;
FILE** animation_frames;



void display_time()
{
    printf(
        "%ld:%02ld'%02ld.%ld\"\n", 
        (t/36000),
        (t/600)%60,
        (t/10)%60,
        t%10
    );
}

void display_tick(int sig)
{
    if (t % update_period == 0) {
        display_time();
    }
}

void ring(int sig)
{
    printf("LEEEEEEET'S GO !");
    kill(pid, SIGKILL);
    exit(0);
}

void tick(int sig)
{
    t++;
    kill(ppid, SIGUSR1);
}

void tack(int sig)
{
    t--;
    kill(ppid, SIGUSR1);
}

enum {
    ALARM_MODE,
    CHRONO_MODE,
    TIMER_MODE,
    TIME_MODE,
    UI_MODE
};
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
    strcpy(action, argv[1]);
    selected_mode = pick_from_cli_flag(action);

    ppid = getpid();
    pid = fork();

    if (pid != 0) { // le père
        switch(selected_mode) {
            case TIMER_MODE:
                strcpy(pas, "100000"/*ms*/);
                strcpy(valeur_finale, "0");
                strcpy(valeur_initiale, argv[2]);
                signal(SIGUSR1, display_tick);
                signal(SIGUSR2, ring);
                wait(&status);
                break;
        }
    }
    else { // le fils
        switch(selected_mode) {
            case TIMER_MODE:
                signal(SIGALRM, tack);
                struct itimerval  val  = { {0, 100000}, {0, 100000} },
                                  val2 = { {0, 100000}, {0, 100000} };
                setitimer(ITIMER_REAL, &val, &val2);
                break;
        }
        while (1) {
            pause();
        }
    }

    return 0;
}