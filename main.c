#include <locale.h>
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

int ppid, pid;
int pipeh[2];

int initial_value, final_value;

int isReady = 0; // bool

int selected_mode;

int status;

FILE* animation_info;
FILE** animation_frames;

void time_to_ASCII_art(const long time)
{
    static const char ASCII_9[6][30]=                {" █████╗ ","██╔══██╗","╚██████║"," ╚═══██║"," █████╔╝"," ╚════╝ "};
    static const char ASCII_8[6][30]=                {" █████╗ ","██╔══██╗","╚█████╔╝","██╔══██╗","╚█████╔╝"," ╚════╝ "};
    static const char ASCII_7[6][30]=                {"███████╗","╚════██║","   ██╔╝ ","  ██╔╝  ","  ██║   ","  ╚═╝   "};
    static const char ASCII_6[6][30]=                {" ██████╗ ","██╔════╝ ","███████╗ ","██╔═══██╗","╚██████╔╝"," ╚═════╝ "};
    static const char ASCII_5[6][30]=                {"███████╗","██╔════╝","███████╗","╚════██║","███████║","╚══════╝"};
    static const char ASCII_4[6][30]=                {"██╗  ██╗","██║  ██║","███████║","╚════██║","     ██║","     ╚═╝"};
    static const char ASCII_3[6][30]=                {"██████╗ ","╚════██╗"," █████╔╝"," ╚═══██╗","██████╔╝","╚═════╝ "};
    static const char ASCII_2[6][30]=                {"██████╗ ","╚════██╗"," █████╔╝","██╔═══╝ ","███████╗","╚══════╝"};
    static const char ASCII_1[6][30]=                {" ██╗","███║","╚██║"," ██║"," ██║"," ╚═╝"};
    static const char ASCII_0[6][30]=                {" ██████╗ ","██╔═████╗","██║██╔██║","████╔╝██║","╚██████╔╝"," ╚═════╝ "};
    static const char ASCII_DOT[6][30]=              {"   ","   ","   ","   ","██╗","╚═╝"};
    static const char ASCII_COLON[6][30]=            {"   ","██╗","╚═╝","██╗","╚═╝","   "};
    static const char ASCII_SINGLE_QUOTE[6][30]=     {"██╗","██║","╚═╝","   ","   ","   "};
    static const char ASCII_DOUBLE_QUOTE[6][30]=     {"██╗██╗","██║██║","╚═╝╚═╝","      ","      ","      "};

    #define ASCII_9_width 8
    #define ASCII_8_width 8
    #define ASCII_7_width 8
    #define ASCII_6_width 9
    #define ASCII_5_width 8
    #define ASCII_4_width 8
    #define ASCII_3_width 8
    #define ASCII_2_width 8
    #define ASCII_1_width 4
    #define ASCII_0_width 9
    #define ASCII_DOT_width 3
    #define ASCII_COLON_width 3
    #define ASCII_SINGLE_QUOTE_width 3
    #define ASCII_DOUBLE_QUOTE_width 6

    char str_time[100];
    sprintf(
        str_time,
        "%ld:%02ld'%02ld.%01ld\"",
        time % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
        time % (60 * 60 * 10) / (60 * 10), // minutes
        time % (60 * 10) / 10, // seconds
        time % 10 // tenths
    );

    wclear(main_window);

    const char (* ASCII_art_value)[6][30];
    int ASCII_art_width;
    int offset = 0;
    for (int i=0; str_time[i] != '\0'; i++) {
        switch (str_time[i])
        {
        case '0': 
            ASCII_art_value = &ASCII_0;
            ASCII_art_width = ASCII_0_width;
            break;

        case '1': 
            ASCII_art_value = &ASCII_1;
            ASCII_art_width = ASCII_1_width;
            break;

        case '2': 
            ASCII_art_value = &ASCII_2;
            ASCII_art_width = ASCII_2_width;
            break;

        case '3': 
            ASCII_art_value = &ASCII_3;
            ASCII_art_width = ASCII_3_width;
            break;

        case '4': 
            ASCII_art_value = &ASCII_4;
            ASCII_art_width = ASCII_4_width;
            break;

        case '5': 
            ASCII_art_value = &ASCII_5;
            ASCII_art_width = ASCII_5_width;
            break;

        case '6': 
            ASCII_art_value = &ASCII_6;
            ASCII_art_width = ASCII_6_width;
            break;

        case '7': 
            ASCII_art_value = &ASCII_7;
            ASCII_art_width = ASCII_7_width;
            break;

        case '8': 
            ASCII_art_value = &ASCII_8;
            ASCII_art_width = ASCII_8_width;
            break;

        case '9': 
            ASCII_art_value = &ASCII_9;
            ASCII_art_width = ASCII_9_width;
            break;

        case '.': 
            ASCII_art_value = &ASCII_DOT;
            ASCII_art_width = ASCII_DOT_width;
            break;

        case ':': 
            ASCII_art_value = &ASCII_COLON;
            ASCII_art_width = ASCII_COLON_width;
            break;

        case '\'': 
            ASCII_art_value = &ASCII_SINGLE_QUOTE;
            ASCII_art_width = ASCII_SINGLE_QUOTE_width;
            break;

        case '"': 
            ASCII_art_value = &ASCII_DOUBLE_QUOTE;
            ASCII_art_width = ASCII_DOUBLE_QUOTE_width;
            break;
        }

        for (int j=0; j<6; j++) {
            mvwprintw(main_window, j, offset, (*ASCII_art_value)[j]);
        }

        offset += ASCII_art_width;
    }
    wrefresh(main_window);
}


long time_to_wait_alarm(long input_time){
    //nb de dixiemes depuis le debut de la journée
    time_t tt = time(0);
    struct tm *lt = localtime(&tt);
    t = (lt->tm_hour * 60 * 60 * 10) + (lt->tm_min * 60 * 10) + (lt->tm_sec * 10);

    long final_time = input_time - t;

    if(final_time <0){
        final_time += 864000; //rajoute 24h si l'heure voulue est "avant" l'heure qu'il est
    }

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
            strcpy(temp, "");
        }
        if(input[fin] == 'h'){
            strncpy(temp, input+debut, fin);
            heures = atol(temp);
            debut = fin+1;
            strcpy(temp, "");
        }
        if (input[fin] == 'm'){
            strncpy(temp, input+debut, fin);
            min = atol(temp);
            debut = fin+1;
            strcpy(temp, "");
        }
        if (input[fin] == 's'){
            strncpy(temp, input+debut, fin);
            sec = atof(temp);
            debut = fin+1;
            strcpy(temp, "");
        }
    }

    dixiemes = (sec*10) + (min*60*10) + (heures*60*60*10) + (jours*24*60*60*10);
    return dixiemes;
}

void display_time()
{
    // mvwprintw(main_window, 0, 0, 
    //     "%ld:%02ld'%02ld.%ld\"\n", 
    //     (t/36000),
    //     (t/600)%60,
    //     (t/10)%60,
    //     t%10
    // );
    time_to_ASCII_art(t);
    // wrefresh(main_window);
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

    setlocale(LC_ALL, "");
    initscr();
    curs_set(0);
    noecho();
    timeout(-1);
    main_window = newwin(LINES, COLS, 0, 0);
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
                signal(COUNT, display_tick);
                signal(RING, ring);

                while(!isReady) {
                    pause();
                }

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(int)); // self-explanatory
                initial_value = time_to_wait_alarm(read_time(argv[2]));
                write(pipeh[WRITE], &initial_value, sizeof(int)); // initial_value
                final_value = 0;
                write(pipeh[WRITE], &final_value, sizeof(int)); // final_value
                kill(pid, READY);

                wait(&status);

                printf("babai");

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
        
        struct itimerval  val  = { {0, 100000}, {0, 100000} },
                          val2 = { {0, 100000}, {0, 100000} };
        
        switch(selected_mode) {
            case TIMER_MODE:
                signal(SIGALRM, tack);
                setitimer(ITIMER_REAL, &val, &val2);
                break;

            case ALARM_MODE:
                signal(SIGALRM, tack);
                setitimer(ITIMER_REAL, &val, &val2);
                break;

        }
        while (1) {
            pause();
        }
    }

    //  Test fonction conversion
    // long dix_sec = read_time(argv[1]);
    // printf("%ld\n", dix_sec);

    printf("%ld", time_to_wait_alarm(read_time(argv[1])));


    return 0;
}