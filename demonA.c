#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

static int N; // liczba aut (watkow)
typedef enum { pusty = 0, zAdoB = 1, zBdoA = 2 } dir_t; // typ kierunku na moscie dla static dir_t RuchMostu = DIR_NONE;
static volatile sig_atomic_t stop_flag = 0; // flaga do zatrzymania programu po otrzymaniu SIGINT

static void on_sigint(int signo) { // handler dla SIGINT
    stop_flag = 1;
}

static int MiastoA = 0, MiastoB = 0;
static int KolejkaA  = 0, KolejkaB  =  0;
static int NaMościeAuto = -1;
static dir_t RuchMostu = pusty; // kierunek ruchu na moscie, pusty oznacza ze most jest wolny

static dir_t ZmiennikKierunku = zAdoB; // przelacza kierunek ruchu na moscie

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER; // mutex do ochrony wspolnych zmiennych 

static sem_t Most_sem; // pilnuje by na moscie bylo max 1 auto
static sem_t semA; //kolejka samochodw co chca jechac z A do B
static sem_t semB; //kolejka samochodw co chca jechac z B do A

static int rnd_us(int min_us, int max_us) {
    return min_us + rand() % (max_us - min_us + 1);
}

void print_state(void)
{
    if(NaMościeAuto != -1) {
        if(RuchMostu == zAdoB) {
            printf("A-%d %d >>> [>> %d >>] <<< %d %d-B\n", MiastoA, KolejkaA, NaMościeAuto, KolejkaB, MiastoB);
        } else {
            printf("A-%d %d >>> [<< %d <<] <<< %d %d-B\n", MiastoA, KolejkaA, NaMościeAuto, KolejkaB, MiastoB);
        }
    }
    else {
        printf("A-%d %d >>> [ --- ] <<< %d %d-B\n", MiastoA, KolejkaA, KolejkaB, MiastoB);
    }
    fflush(stdout);
}

typedef enum { IN_A, IN_B, WAIT_A, WAIT_B } car_state_t;
typedef struct { // argumenty watku auta
    int id;
    car_state_t state;
} car_arg_t;


static void bridge_steering(){
    pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie sprawdzic stan symulacji
    if(NaMościeAuto == -1) { // sprawdzamy czy most jest wolny
            if(KolejkaA > 0 && KolejkaB == 0) {
                RuchMostu = zAdoB;
                sem_post(&semA); // odblokowujemy auto z kolejki A
            } else if(KolejkaB > 0 && KolejkaA == 0) {
                RuchMostu = zBdoA;
                sem_post(&semB); // odblokowujemy auto z kolejki B
            } else if(KolejkaA > 0 && KolejkaB > 0) {
                RuchMostu = ZmiennikKierunku;
                if(ZmiennikKierunku == zAdoB) {
                    sem_post(&semA); // odblokowujemy auto z kolejki A
                } else {
                    sem_post(&semB); // odblokowujemy auto z kolejki B
                }
                ZmiennikKierunku = (ZmiennikKierunku == zAdoB) ? zBdoA : zAdoB; // zmieniamy kierunek dla nastepnego razu
            }
    }
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    return;
}

static void* car_thread_driving (void *arg) // watek auta podczas jazdy
{
    car_arg_t *c = (car_arg_t*)arg;
    int id = c->id;
    car_state_t state = c->state; 

    while (!stop_flag) { // sprawdzamy czy sigint nie zostal otrzymany
        if(state == WAIT_A) {
            sem_wait(&semA); // czeka na pozwolenie z kolejki A
            if (stop_flag) break;
            sem_wait(&Most_sem); // czeka na pozwolenie z mostu
            if (stop_flag) { sem_post(&Most_sem); break; }

            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            KolejkaA--;
            NaMościeAuto = id;
            RuchMostu = zAdoB;
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex

            usleep(rnd_us(100*1000, 300*1000)); // auto jest na moscie przez losowy czas od 100ms do 300ms

            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            NaMościeAuto = -1;
            RuchMostu = pusty; // most jest teraz wolny
            MiastoB++;
            state = IN_B; // auto jest teraz w miescie B
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex

            sem_post(&Most_sem); // odblokowujemy most
            bridge_steering(); // sprawdzamy czy mozemy wpuścic auto z kolejki A

        } else if(state == WAIT_B) {
            sem_wait(&semB); // czeka na pozwolenie z kolejki B
            if (stop_flag) break;
            sem_wait(&Most_sem); // czeka na pozwolenie z mostu
            if (stop_flag) { sem_post(&Most_sem); break; }

            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            KolejkaB--;
            NaMościeAuto = id;
            RuchMostu = zBdoA;
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex

            usleep(rnd_us(100*1000, 300*1000)); // auto jest na moscie przez losowy czas od 100ms do 300ms

            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            NaMościeAuto = -1;
            RuchMostu = pusty; // most jest teraz wolny
            MiastoA++;
            state = IN_A; // auto jest teraz w miescie A
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex

            sem_post(&Most_sem); // odblokowujemy most
            bridge_steering(); // sprawdzamy czy mozemy wpuścic auto z kolejki A
        }
        else if(state == IN_A) {
            usleep(rnd_us(50*1000, 200*1000)); // auto jest w miescie A przez losowy czas od 50ms do 200ms
            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            MiastoA--;
            KolejkaA++;
            state = WAIT_A; // auto chce teraz jechac z A do B
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex
            bridge_steering(); // sprawdzamy czy mozemy wpuścic auto z kolejki A
        } else if(state == IN_B) {
            usleep(rnd_us(50*1000, 200*1000)); // auto jest w miescie B przez losowy czas od 50ms do 200ms
            pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
            MiastoB--;
            KolejkaB++;
            state = WAIT_B; // auto chce teraz jechac z B do A
            print_state();
            pthread_mutex_unlock(&mtx); // odblokowujemy mutex
            bridge_steering(); // sprawdzamy czy mozemy wpuścic auto z kolejki B
        }
    }
    return NULL;
}

static void* car_thread_init(void *arg) // warek auta
{ 
    car_arg_t *c = (car_arg_t*)arg;

    // losujemy kierunek jazdy auta
    dir_t kierunek = (rand() % 2) ? zAdoB : zBdoA;
    pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
    if (kierunek == zAdoB) {
        if((rand() % 2)){
            KolejkaA++;
            c->state = WAIT_A;
        } else {
            MiastoA++;
            c->state = IN_A;
        }
    } else {
        if((rand() % 2)){
            KolejkaB++;
            c->state = WAIT_B;
        } else {
            MiastoB++;
            c->state = IN_B;
        }
    }
    print_state();
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex

    bridge_steering();
    return car_thread_driving(arg);
}


int main(int argc, char const *argv[])
{
    srand(time(NULL));

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

    sem_init(&Most_sem, 0, 1);//most jest od razu wolny do przejazdu wiec ma 1 pozwolenie
    sem_init(&semA, 0, 0);
    sem_init(&semB, 0, 0);


    pthread_t *t = calloc((size_t)N, sizeof(pthread_t)); // tablica watkow aut
    car_arg_t *args = calloc((size_t)N, sizeof(car_arg_t)); //tablica argumentow dla watkow aut


    for (int i = 0; i < N; i++) { // tworzenie watkow aut i przypisywanie im id
        args[i].id = i + 1;
        pthread_create(&t[i], NULL, car_thread_init, &args[i]);
    }

    while (!stop_flag) { // sprawdzamy czy sigint nie zostal otrzymany
        usleep(100 * 1000);
    }

    for (int i = 0; i < N; i++) { // po otrzymaniu sigint odblokowujemy wszystkie watki aut z kolejek by mogly sie zakonczyc
        sem_post(&semA);
        sem_post(&semB);
    }

    for (int i = 0; i < N; i++){
    sem_post(&Most_sem);
    }

    for (int i = 0; i < N; i++) { // czekamy na zakonczenie wszystkich watkow aut
        pthread_join(t[i], NULL);
    }



    // zwalniamy pamiec i niszczymy semafory i mutexy
    free(t); 
    free(args);

    sem_destroy(&Most_sem);
    sem_destroy(&semA);
    sem_destroy(&semB);
    pthread_mutex_destroy(&mtx);

    printf("Koniec.\n");
    return 0;
}
