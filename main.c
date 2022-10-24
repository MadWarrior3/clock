#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>

#define READY (SIGRTMIN+2)
#define COUNT SIGUSR1
#define RING  SIGUSR2

#define READ  0
#define WRITE 1

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

WINDOW *main_window;

pid_t ppid, pid;
int pipeh[2];

int initial_value, final_value;

int isReady = 0; // bool

int selected_mode;

int status;

FILE* animation_info;
FILE** animation_frames;


long time_to_wait_alarm(long input_time){
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    
    long final_time = input_time - ((current_time.tv_sec + current_time.tv_usec)/100000);
    return final_time;
}

long read_time(char input[])
{
    char temp[40];
    long jours = 0, heures = 0, min = 0, dixiemes = 0;
    float sec = 0;
    int fin = 0, debut = 0;

    for(input[fin]; input[fin] != '\0'; fin++){
        if(input[fin] == 'j'){
            strncpy(temp, input+debut, fin);
            jours = atol(temp);
            debut = fin+1;
            mvwprintw(main_window, 0, 0, "--temp jours : %s\n", temp);
            strcpy(temp, "");
        }
        if(input[fin] == 'h'){
            strncpy(temp, input+debut, fin);
            heures = atol(temp);
            debut = fin+1;
            mvwprintw(main_window, 0, 0, "--temp heures : %s\n", temp);
            strcpy(temp, "");
        }
        if (input[fin] == 'm'){
            strncpy(temp, input+debut, fin);
            min = atol(temp);
            debut = fin+1;
            mvwprintw(main_window, 0, 0, "--temp minutes : %s\n", temp);
            strcpy(temp, "");
        }
        if (input[fin] == 's'){
            strncpy(temp, input+debut, fin);
            sec = atof(temp);
            debut = fin+1;
            mvwprintw(main_window, 0, 0, "--temp sec : %s\n", temp);
            strcpy(temp, "");
        }
    }

    dixiemes = (sec*10) + (min*60*10) + (heures*60*60*10) + (jours*24*60*60*10);
    return dixiemes;
}

void display_time()
{
    mvwprintw(main_window, 0, 0, 
        "%ld:%02ld'%02ld.%ld\"\n", 
        (t/36000),
        (t/600)%60,
        (t/10)%60,
        t%10
    );
    wrefresh(main_window);
}

void display_tick(int sig)
{
    read(pipeh[READ], &t, sizeof(t));
    if (t % update_period == 0) {
        display_time();
    }
}

void ring(int sig)
{
    endwin();
    kill(pid, SIGKILL);
    printf("LEEEEEEET'S GO !\n");
    exit(0);
}

void tick(int sig)
{
    t++;
    write(pipeh[WRITE], &t, sizeof(t));
    kill(ppid, COUNT);

    if (t >= final_value) {
        kill(ppid, RING);
    }
}

void tack(int sig)
{
    t--;
    write(pipeh[WRITE], &t, sizeof(t));
    kill(ppid, COUNT);

    if (t <= final_value) {
        kill(ppid, RING);
    }
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

void ready(int sig)
{
    isReady = 1;
}


int main(int argc, char** argv)
{
    signal(READY, ready);

    initscr();
    main_window = newwin(LINES, COLS, 0, 0);
    mvwprintw(main_window, 0, 0, "chienne");
    wrefresh(main_window);

    if (pipe(pipeh) == -1) {
        fprintf(stderr,"Pipe failed");
        exit(1);
    }

    ppid = getpid();
    pid = fork();

    if (pid != 0) { // le père
        selected_mode = pick_from_cli_flag(argv[1]);

        switch(selected_mode) {
            case TIMER_MODE:
                signal(COUNT, display_tick);
                signal(RING, ring);

                while(!isReady) {
                    pause();
                }

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(int)); // self-explanatory
                initial_value = 10*atof(argv[2]);
                write(pipeh[WRITE], &initial_value, sizeof(int)); // initial_value
                final_value = 0;
                write(pipeh[WRITE], &final_value, sizeof(int)); // final_value
                kill(pid, READY);

                wait(&status);

                mvwprintw(main_window, 0, 0, "babai");
                wrefresh(main_window);

    wrefresh(main_window);
            break;

            case ALARM_MODE:
            

            break;
        }
    }
    else { // le fils
        kill(ppid, READY);
        // waiting for pipe to be filled
        while(!isReady) {
            pause();
        }
        // reading pipe
        read(pipeh[READ], &selected_mode, sizeof(int));
        read(pipeh[READ], &initial_value, sizeof(int));
        read(pipeh[READ], &final_value, sizeof(int));

        t = initial_value;
        
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