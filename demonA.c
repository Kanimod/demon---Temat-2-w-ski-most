#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

static int N; // liczba aut (watkow)
static int AutonaMoście = 0;
typedef enum { pusty = 0, zAdoB = 1, zBdoA = 2 } dir_t; // typ kierunku na moscie dla static dir_t RuchMostu = DIR_NONE;

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
    int on_bridge = (NaMościeAuto != -1) ? 1 : 0;
int suma = MiastoA + MiastoB + KolejkaA + KolejkaB + on_bridge;
if (suma != N) {
    fprintf(stderr, "BLAD: suma=%d, N=%d\n", suma, N);
}
    fflush(stdout);
}
typedef enum { stateKolejkaA = 0, stateKolejkaB = 1, stateMiastoA = 2, stateMiastoB = 3 } state_t;
typedef struct { // argumenty watku auta
    int id;
    state_t state;
} car_arg_t;

static void bridgesteering(){
    pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie sprawdzic stan symulacji
    if(NaMościeAuto == -1 && RuchMostu == pusty) { // sprawdzamy czy most jest wolny
            if(KolejkaA > 0 && KolejkaB == 0) {
                RuchMostu = zAdoB;
                sem_post(&semA); // odblokowujemy auto z kolejki A
            }
            else if(KolejkaB > 0 && KolejkaA == 0) {
                RuchMostu = zBdoA;
                sem_post(&semB); // odblokowujemy auto z kolejki B
            }
            else if(KolejkaA > 0 && KolejkaB > 0) { // jesli sa auta w obu kolejkach to przepuszczamy auto z kolejki ktora ma wiecej aut
                if(ZmiennikKierunku == zAdoB) {
                    RuchMostu = zAdoB;
                    sem_post(&semA); // odblokowujemy auto z kolejki A
                } else {
                    RuchMostu = zBdoA;
                    sem_post(&semB); // odblokowujemy auto z kolejki B
                }
            }
    }
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    return;
}

static void* inQueueA (void *arg) // watek auta podczas jazdy
{
    car_arg_t *c = (car_arg_t*)arg;
    int id = c->id;

    sem_wait(&semA); // czeka na pozwolenie z kolejki A
    sem_wait(&Most_sem); // czeka na pozwolenie z mostu
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
        print_state();
        ZmiennikKierunku = zBdoA; // zmieniamy kierunek dla kolejnego auta jesli sa auta w obu kolejkach
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    sem_post(&Most_sem); // odblokowujemy most
    bridgesteering();
    c->state = stateMiastoB; // auto jest teraz w miescie B
    return 0;
}

static void* inQueueB (void *arg) // watek auta podczas jazdy
{
    car_arg_t *c = (car_arg_t*)arg;
    int id = c->id;

    sem_wait(&semB); // czeka na pozwolenie z kolejki B
    sem_wait(&Most_sem); // czeka na pozwolenie z mostu
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
        print_state();
        ZmiennikKierunku = zAdoB; // zmieniamy kierunek dla kolejnego auta jesli sa auta w obu kolejkach
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    sem_post(&Most_sem); // odblokowujemy most
    bridgesteering();
    c->state = stateMiastoA; // auto jest teraz w miescie A
    return 0;
}

static void* inCityA(void *arg) // watek auta podczas jazdy
{
    car_arg_t *c = (car_arg_t*)arg;

    usleep(rnd_us(50*1000, 200*1000)); // auto jest w miescie A przez losowy czas od 50ms do 200ms
    pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
        MiastoA--;
        KolejkaA++;
        print_state();
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    bridgesteering();
    c->state = stateKolejkaA; // auto chce teraz jechac z A do B
    return 0;
}

static void* inCityB(void *arg) // watek auta podczas jazdy
{
    car_arg_t *c = (car_arg_t*)arg;

    usleep(rnd_us(50*1000, 200*1000)); // auto jest w miescie B przez losowy czas od 50ms do 200ms
    pthread_mutex_lock(&mtx); // blokujemy mutex by bezpiecznie zmienic stan symulacji
        MiastoB--;
        KolejkaB++;
        print_state();
    pthread_mutex_unlock(&mtx); // odblokowujemy mutex
    bridgesteering();
    c->state = stateKolejkaB; // auto chce teraz jechac z B do A
    return 0;
}

static void* car_thread_init(void *arg){
    car_arg_t *c= (car_arg_t*)arg;

    while (1){
        switch (c->state)
        {
        case stateKolejkaA:
            inQueueA(arg);
            break;
        case stateKolejkaB:
            inQueueB(arg);
            break;
        case stateMiastoA:
            inCityA(arg);
            break;
        case stateMiastoB:
            inCityB(arg);
            break;
        
        default:
            break;
        }
    }
    return NULL;
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

    sem_init(&Most_sem, 0, 1);//most jest od razu wolny do przejazdu wiec ma 1 pozwolenie
    sem_init(&semA, 0, 0);
    sem_init(&semB, 0, 0);


    pthread_t *t = calloc((size_t)N, sizeof(pthread_t)); // tablica watkow aut
    car_arg_t *args = calloc((size_t)N, sizeof(car_arg_t)); //tablica argumentow dla watkow aut

    for (int i = 0; i < N; i++) {
        args[i].id = i + 1;
        if (rand() % 2) {
            MiastoA++;
            args[i].state = stateMiastoA;
        } else {
            MiastoB++;
            args[i].state = stateMiastoB;
        }
    }

    print_state();

    for (int i = 0; i < N; i++) { // tworzenie watkow aut i przypisywanie im id
        args[i].id = i + 1;
        pthread_create(&t[i], NULL, car_thread_init, &args[i]);
    }

    for (int i = 0; i < N; i++) {
    pthread_join(t[i], NULL);
}

    return 0;
}
