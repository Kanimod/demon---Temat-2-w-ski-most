#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

static int N; // liczba aut (watkow)
typedef enum { pusty = 0, zAdoB = 1, zBdoA = 2 } dir_t; // typ kierunku na moscie dla static dir_t RuchMostu = DIR_NONE;
static volatile sig_atomic_t stop_flag = 0; // flaga do zatrzymania programu po otrzymaniu SIGINT

static void on_sigint(int signo) { // handler dla SIGINT
    //void(signo)
    stop_flag = 1;
}

static int MiastoA = 0, MiastoB = 0;
static int KolejkaA  = 0, KolejkaB  = 0;
static int NaMościeAuto = -1;
static dir_t RuchMostu = pusty; // kierunek ruchu na moscie, pusty oznacza ze most jest wolny

static dir_t ZmiennikKierunku = zAdoB; // przelacza kierunek ruchu na moscie

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER; // mutex do ochrony wspolnych zmiennych 

static sem_t Most; // pilnuje by na moscie bylo max 1 auto
static sem_t KolejkaA; //kolejka samochodw co chca jechac z A do B
static sem_t KolejkaB; //kolejka samochodw co chca jechac z B do A

// Funkcja wyświetla aktualny stan symulacji. Zakładamy e mutex jest jua zablokowany!!!
/* Format: A-x y>>> [>>(<<) id >>(<<)] <<<z w-B
  gdzie:
   MiastoA - liczba aut po stronie A
   KolejkaA - liczba aut czekających po stronie A
   NaMościeAuto - numer samochodu przejeżdżającego
   KolejkaB - liczba aut oczekujących po stronie B
   MiastoB - liczba aut po stronie B
*/

void print_state(void)
{
    if(NaMościeAuto != -1) {
        if(RuchMostu == zAdoB) {
            printf("A-%d %d>>> [>> %d >>] <<<%d %d-B\n", MiastoA, KolejkaA, NaMościeAuto, KolejkaB, MiastoB);
        } else {
            printf("A-%d %d>>> [<< %d <<] <<<%d %d-B\n", MiastoA, KolejkaA, NaMościeAuto, KolejkaB, MiastoB);
        }
    }
    else {
        printf("A-%d %d>>> [ --- ] <<<%d %d-B\n", MiastoA, KolejkaA, KolejkaB, MiastoB);
    }
    fflush(stdout);
}


typedef struct { // argumenty watku auta
    int id;
} car_arg_t;

static void* car_thread(void *arg) // warek auta
{ 
    car_arg_t *c = (car_arg_t*)arg;
    int id = c->id;

}

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uzycie: %s N\n", argv[0]);
        return 1;
    }

    N=atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "N musi byc > 0\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sem_init(&Most, 0, 1);//most jest od razu wolny do przejazdu wiec ma 1 pozwolenie
    sem_init(&KolejkaA, 0, 0);
    sem_init(&KolejkaB, 0, 0);


    pthread_t *t = calloc((size_t)N, sizeof(pthread_t)); // tablica watkow aut
    car_arg_t *args = calloc((size_t)N, sizeof(car_arg_t)); //tablica argumentow dla watkow aut

    for (int i = 0; i < N; i++) { // tworzenie watkow aut i przypisywanie im id
        args[i].id = i + 1;
        pthread_create(&t[i], NULL, car_thread, &args[i]);
    }

    while (!stop_flag) { // sprawdzamy czy sigint nie zostal otrzymany
        usleep(100 * 1000);
    }

    for (int i = 0; i < N; i++) { // po otrzymaniu sigint odblokowujemy wszystkie watki aut z kolejek by mogly sie zakonczyc
        sem_post(&KolejkaA);
        sem_post(&KolejkaB);
    }

    sem_post(&Most); //odblokowujemy tez most jak cos na nim jest 

    for (int i = 0; i < N; i++) { // czekamy na zakonczenie wszystkich watkow aut
        pthread_join(t[i], NULL);
    }



    // zwalniamy pamiec i niszczymy semafory i mutexy
    free(t); 
    free(args);

    sem_destroy(&Most);
    sem_destroy(&KolejkaA);
    sem_destroy(&KolejkaB);
    pthread_mutex_destroy(&mtx);

    printf("Koniec.\n");
    return 0;
}
