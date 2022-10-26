#include <locale.h>
#include <limits.h>
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

int display_offset = 0;

int ppid, pid,
    apid=-1, tpid=-1, cpid=-1;
int pipeh[2], apipeh[2], tpipeh[2], cpipeh[2];

long initial_value, final_value, alarm_time = -1;

int isReady = 0; // bool

int selected_mode;

int status;
bool isCounting = true;

FILE* animation_info;
FILE** animation_frames;

long chrono_avg, chrono_count;
long timer_avg, timer_count;

char comm[300], str[200];


enum {
    ALARM_MODE,
    CHRONO_MODE,
    TIMER_MODE,
    TIME_MODE,
    UI_MODE,
    STATS_MODE
};

void tack(int sig);
void tick(int sig);
void ring(int sig);
void neco_dance(int sig);
void display_tick(int sig);
void display_time();
long read_time(char input[]);
long time_to_wait_alarm(long input_time);
void to_count_or_not_to_count(int sig);
void time_to_ASCII_art(const long time);
void be_lazy(int sig);
void be_quiet(int sig);
void ready(int sig);
int pick_from_cli_flag(char *flag);
void init_ncurses(void);

void get_chrono_stats(void);
void set_chrono_stats(void);

void get_timer_stats(void);
void set_timer_stats(void);

void add_alarm_stats(long new_alarm_time);



int main(int argc, char** argv)
{
    if (argc >= 4) {
        display_offset = atoi(argv[3]);
    }

    int ch;
    signal(READY, ready);

    if (pipe(pipeh) == -1) {
        fprintf(stderr,"Pipe failed");
        exit(1);
    }

    ppid = getpid();
    pid = fork();

    if (pid == 0) { // le fils
        kill(ppid, READY);
        // waiting for pipe to be filled
        while(!isReady) {
            pause();
        }
        // reading pipe
        read(pipeh[READ], &selected_mode, sizeof(selected_mode));
        read(pipeh[READ], &initial_value, sizeof(initial_value));
        read(pipeh[READ], &final_value, sizeof(final_value));

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

            case CHRONO_MODE:
                signal(COUNT, to_count_or_not_to_count);
                signal(SIGALRM, tick);
                setitimer(ITIMER_REAL, &val, &val2);
                break;

            case TIME_MODE:
                signal(SIGALRM, tick);
                setitimer(ITIMER_REAL, &val, &val2);
                break;

        }
        while (1) {
            pause();
        }
    }
    else { // le père
        get_chrono_stats();
        get_timer_stats();

        if (argc > 1) {
            selected_mode = pick_from_cli_flag(argv[1]);
        }
        else {
            selected_mode = TIME_MODE;
        }

        switch(selected_mode) {
            case TIME_MODE:
                init_ncurses();

                signal(COUNT, display_tick);
                signal(RING, ring);

                while(!isReady) {
                    pause();
                }

                //nb de dixiemes depuis le debut de la journée
                time_t tt = time(0);
                struct tm *lt = localtime(&tt);
                t = (lt->tm_hour * 60 * 60 * 10) + (lt->tm_min * 60 * 10) + (lt->tm_sec * 10);

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(selected_mode)); // self-explanatory
                initial_value = t;
                write(pipeh[WRITE], &initial_value, sizeof(initial_value)); // initial_value
                final_value = -1;
                write(pipeh[WRITE], &final_value, sizeof(final_value)); // final_value
                kill(pid, READY);

                while((ch = getch()) != 'q'){
                    switch(ch){
                        case 'a':
                            move(display_offset + 6, 0);
                            clrtoeol();
                            signal(COUNT, be_quiet);
                            curs_set(1);
                            echo();
                            printw(" set alarm to ring at ");
                            scanw("%s", str);
                            alarm_time = read_time(str);
                            curs_set(0);
                            noecho();
                            signal(COUNT, display_tick);

                            move(display_offset + 6, 0);
                            clrtoeol();
                            printw("alarm :  %ld:%02ld'%02ld.%01ld\"",
                                alarm_time % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
                                alarm_time % (60 * 60 * 10) / (60 * 10), // minutes
                                alarm_time % (60 * 10) / 10, // seconds
                                alarm_time % 10 // tenths
                            );

                            comm[0] = '\0';
                            strcat(comm, "xterm -e ./clock a ");
                            strcat(comm, str);
                            strcat(comm, " &");
                            system(comm);
                            break;

                        case 'z':
                        case 't':
                            move(display_offset + 6, 0);
                            clrtoeol();
                            signal(COUNT, be_quiet);
                            curs_set(1);
                            echo();
                            printw(" countdown from ");
                            scanw("%s", str);
                            alarm_time = read_time(str);
                            curs_set(0);
                            noecho();
                            signal(COUNT, display_tick);

                            move(display_offset + 6, 0);
                            clrtoeol();

                            comm[0] = '\0';
                            strcat(comm, "xterm -e ./clock t ");
                            strcat(comm, str);
                            strcat(comm, " &");
                            system(comm);
                            break;

                        case 'c':
                        case 'e':
                            system("xterm -e ./clock c");
                            break;

                        case 'r':
                        case 's':
                            srand(time(NULL));
                            char fun_fact[500];
                            switch(rand() % 5) {
                                case 0: strcpy(fun_fact, "Il y a en moyenne 403 naissances au toutes les 24h."); break;
                                case 1: strcpy(fun_fact, "Un certain clip musical a été visionné 1 305 576 236 sur Youtube."); break;
                                case 2: strcpy(fun_fact, "La lumière du soleil met 8 minutes à arriver à la terre."); break;
                                case 3: strcpy(fun_fact, "En moyenne, on cligne des yeux 4 200 000 fois par an."); break;
                                case 4: strcpy(fun_fact, "Il y a 2 500 000 rivets sur la Tour Eiffel."); break;

                            }

                            mvprintw(8, 0,
                                "=======================================================================\n"
                                "= Statistiques                                                        \n"
                                "=======================================================================\n"
                                "=                                                                     \n"
                                "= Vous avez utilisé le chronomètre %ld fois ! Ça fait beaucoup là non ?\n"
                                "= Valeur chronométrée moyenne : %ldh%02ldm%02ld.%ld\n"
                                "= \n"
                                "= Vous avez utilisé le timer %ld fois !\n"
                                "= Durée moyenne : %ldh%02ldm%02ld.%ld\n"
                                "= \n"
                                "= %s\n"
                                "=====================================================================\n",
                                
                                chrono_count,
                                chrono_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
                                chrono_avg % (60 * 60 * 10) / (60 * 10), // minutes
                                chrono_avg % (60 * 10) / 10, // seconds
                                chrono_avg % 10, // tenths
                                
                                timer_count,
                                timer_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
                                timer_avg % (60 * 60 * 10) / (60 * 10), // minutes
                                timer_avg % (60 * 10) / 10, // seconds
                                timer_avg % 10, // tenths

                                fun_fact
                            );
                            break;
                    }
                }

                endwin();
                break;

            case STATS_MODE:
                srand(time(NULL));
                char fun_fact[500];
                switch(rand() % 5) {
                    case 0: strcpy(fun_fact, "Il y a en moyenne 403 naissances au toutes les 24h."); break;
                    case 1: strcpy(fun_fact, "Un certain clip musical a été visionné 1 305 576 236 sur Youtube."); break;
                    case 2: strcpy(fun_fact, "La lumière du soleil met 8 minutes à arriver à la terre."); break;
                    case 3: strcpy(fun_fact, "En moyenne, on cligne des yeux 4 200 000 fois par an."); break;
                    case 4: strcpy(fun_fact, "Il y a 2 500 000 rivets sur la Tour Eiffel."); break;

                }

                printf(
                    "=======================================================================\n"
                    "= Statistiques                                                        \n"
                    "=======================================================================\n"
                    "=                                                                     \n"
                    "= Vous avez utilisé le chronomètre %ld fois ! Ça fait beaucoup là non ?\n"
                    "= Valeur chronométrée moyenne : %ldh%02ldm%02ld.%ld\n"
                    "= \n"
                    "= Vous avez utilisé le timer %ld fois !\n"
                    "= Durée moyenne : %ldh%02ldm%02ld.%ld\n"
                    "= \n"
                    "= %s\n"
                    "=====================================================================\n",
                    
                    chrono_count,
                    chrono_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
                    chrono_avg % (60 * 60 * 10) / (60 * 10), // minutes
                    chrono_avg % (60 * 10) / 10, // seconds
                    chrono_avg % 10, // tenths
                    
                    timer_count,
                    timer_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
                    timer_avg % (60 * 60 * 10) / (60 * 10), // minutes
                    timer_avg % (60 * 10) / 10, // seconds
                    timer_avg % 10, // tenths

                    fun_fact
                );
                break;

            case TIMER_MODE:
                init_ncurses();

                signal(COUNT, display_tick);
                signal(RING, ring);

                while(!isReady) {
                    pause();
                }

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(selected_mode)); // self-explanatory
                initial_value = read_time(argv[2]);
                write(pipeh[WRITE], &initial_value, sizeof(initial_value)); // initial_value
                final_value = 0;
                write(pipeh[WRITE], &final_value, sizeof(final_value)); // final_value
                kill(pid, READY);

                timer_avg = ((timer_avg * timer_count) + initial_value) / (timer_count + 1);
                timer_count++;
                set_timer_stats();

                wait(&status);

                refresh();
                break;

            case ALARM_MODE:
                signal(COUNT, be_lazy);
                signal(RING, ring);

                while(!isReady) {
                    pause();
                }

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(selected_mode)); // self-explanatory
                initial_value = time_to_wait_alarm(read_time(argv[2]));
                write(pipeh[WRITE], &initial_value, sizeof(initial_value)); // initial_value
                final_value = 0;
                write(pipeh[WRITE], &final_value, sizeof(final_value)); // final_value
                kill(pid, READY);

                printf(
                    "alarm set !\nit will ring after %ld:%02ld'%02.1lf\" !\n", 
                    (initial_value / (10 * 60 * 60)), 
                    (initial_value / (10 * 60)) % 60, 
                    (initial_value % 600) / 10.0
                );

                wait(&status);
                break;

            case CHRONO_MODE:
                init_ncurses();
                
                signal(COUNT, display_tick);
                signal(RING, ring); //au cas ou le compteur dépasse la valeur max

                while(!isReady) {
                    pause();
                }

                // sending intialization data
                write(pipeh[WRITE], &selected_mode, sizeof(selected_mode)); // self-explanatory
                initial_value = 0;
                write(pipeh[WRITE], &initial_value, sizeof(initial_value)); // initial_value
                final_value = 60000;    //chrono compte jusqu'a 60 000 max
                write(pipeh[WRITE], &final_value, sizeof(final_value)); // final_value
                kill(pid, READY);
                
                mvprintw(6 + display_offset, 0, "[W]");
                mvprintw(7 + display_offset, 0, "[X]");
                mvprintw(8 + display_offset, 0, "[C]");

                while((ch = getch()) != 'q'){
                    switch(ch){
                        case ' ':
                            kill(pid, COUNT);                            
                            break;

                        case 'w':
                            mvprintw(6 + display_offset, 0, 
                                        "[W]  %ld:%02ld'%02ld.%ld\"\n", 
                                        (t/36000),
                                        (t/600)%60,
                                        (t/10)%60,
                                        t%10
                                        );
                                    
                            refresh();

                            break;

                        case 'x':
                            mvprintw(7 + display_offset, 0, 
                                        "[X]  %ld:%02ld'%02ld.%ld\"\n", 
                                        (t/36000),
                                        (t/600)%60,
                                        (t/10)%60,
                                        t%10
                                        );
                            refresh();  

                            break;

                        case 'c':
                            mvprintw(8 + display_offset, 0, 
                                        "[C]  %ld:%02ld'%02ld.%ld\"\n", 
                                        (t/36000),
                                        (t/600)%60,
                                        (t/10)%60,
                                        t%10
                                        );
                                        
                            refresh();

                            break;    
                    }
                }

                chrono_avg = ((chrono_avg * chrono_count) + t) / (chrono_count + 1);
                chrono_count++;
                set_chrono_stats();

                endwin();

                break;
        }
    }

    return 0;
}

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

    move(0 + display_offset, 0); clrtoeol();
    move(1 + display_offset, 0); clrtoeol();
    move(2 + display_offset, 0); clrtoeol();
    move(3 + display_offset, 0); clrtoeol();
    move(4 + display_offset, 0); clrtoeol();
    move(5 + display_offset, 0); clrtoeol();

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
            mvprintw(j + display_offset, offset, (*ASCII_art_value)[j]);
        }

        offset += ASCII_art_width;
    }
    refresh();
}


void to_count_or_not_to_count(int sig)
{
    isCounting = !isCounting;
}

long time_to_wait_alarm(long input_time)
{
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
    // mvprintw(0, 0, 
    //     "%ld:%02ld'%02ld.%ld\"\n", 
    //     (t/36000),
    //     (t/600)%60,
    //     (t/10)%60,
    //     t%10
    // );
    time_to_ASCII_art(t);
    // refresh();
}

void display_tick(int sig)
{
    read(pipeh[READ], &t, sizeof(t));
    if (t % update_period == 0) {
        display_time();
    }
}

void be_lazy(int sig)
{
    // nada
}

void be_quiet(int sig)
{
    t++;
}

void neco_dance(int sig)
{
    refresh();
}

void ring(int sig)
{
    printf("ding !");

    if (selected_mode == ALARM_MODE) {
        init_ncurses();
    }    
        
    system("xdg-open ./rr.mp4");

    // quit
    endwin();
    kill(pid, SIGKILL);
    exit(0);
}

void tick(int sig)
{
    if(!isCounting){
        return;
    }

    t++;
    write(pipeh[WRITE], &t, sizeof(t));
    kill(ppid, COUNT);

    if (final_value >= 0) {
        if (t >= final_value) {
            kill(ppid, RING);
        }
    }
}

void tack(int sig)
{
    t--;
    write(pipeh[WRITE], &t, sizeof(t));
    kill(ppid, COUNT);

    if (final_value >= 0) {
        if (t <= final_value) {
            kill(ppid, RING);
        }
    }
}

int pick_from_cli_flag(char *flag)
{
    switch(flag[0]) {
        case 'a': return ALARM_MODE;
        case 'c': return CHRONO_MODE;
        case 't': return TIMER_MODE;
        case 'h': return TIME_MODE;
        case 's': return STATS_MODE;
        default: return TIME_MODE;
    }
}

void ready(int sig)
{
    isReady = 1;
}

void init_ncurses(void)
{
    setlocale(LC_ALL, "");
    initscr();
    curs_set(0);
    noecho();
    raw();
    timeout(-1);
    refresh();
    start_color();
}

void get_chrono_stats()
{
    FILE *f = fopen("chrono.clst", "r");
    char line[200] = "";

    // reading global average, format is "moy XXhXXmXX.Xs"
    fgets(line, 199, f);
    chrono_avg = read_time(line + 4);

    // reading count, format is "count XXX"
    fgets(line, 199, f);
    chrono_count = atol(line + 6);

    fclose(f);
}

void set_chrono_stats()
{
    FILE *f = fopen("chrono.clst", "w");

    fprintf(
        f, 
        "moy %ldh%02ldm%02ld.%ld\n"
        "count %ld\n",

        chrono_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
        chrono_avg % (60 * 60 * 10) / (60 * 10), // minutes
        chrono_avg % (60 * 10) / 10, // seconds
        chrono_avg % 10, // tenths

        chrono_count
    );

    fclose(f);
}


void get_timer_stats()
{
    FILE *f = fopen("timer.clst", "r");
    char line[200] = "";

    // reading global average, format is "moy XXhXXmXX.Xs"
    fgets(line, 199, f);
    timer_avg = read_time(line + 4);

    // reading count, format is "count XXX"
    fgets(line, 199, f);
    timer_count = atol(line + 6);

    fclose(f);
}


void set_timer_stats()
{
    FILE *f = fopen("timer.clst", "w");

    fprintf(
        f, 
        "moy %ldh%02ldm%02ld.%ld\n"
        "count %ld\n",

        timer_avg % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
        timer_avg % (60 * 60 * 10) / (60 * 10), // minutes
        timer_avg % (60 * 10) / 10, // seconds
        timer_avg % 10, // tenths

        timer_count
    );

    fclose(f);
}


void add_alarm_stats(long new_alarm_time)
{
    FILE *f = fopen("alarm.clst", "a");

    fprintf(
        f, 
        "%ldh%02ldm%02ld.%ld\n",
        new_alarm_time % (24 * 60 * 60 * 10) / (60 * 60 * 10), // hours
        new_alarm_time % (60 * 60 * 10) / (60 * 10), // minutes
        new_alarm_time % (60 * 10) / 10, // seconds
        new_alarm_time % 10 // tenths
    );

    fclose(f);
}
