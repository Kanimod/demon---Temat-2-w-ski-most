#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

static int N; // liczba aut (watkow)
typedef enum { pusty = 0, zAdoB = 1, zBdoA = 2 } dir_t; // typ kierunku na moscie dla static dir_t RuchMostu = DIR_NONE;

static int MiastoA = 0, MiastoB = 0; // liczba aut znajdujących się w miastach
static int KolejkaA  = 0, KolejkaB  =  0; // liczba aut czekających przed mostem
static int NaMościeAuto = -1; // numer auta aktualnie jadącego przez most(-1 to pusty most)
static dir_t RuchMostu = pusty; // kierunek ruchu na moscie, pusty oznacza ze most jest wolny

static dir_t ZmiennikKierunku = zAdoB; // przelacza kierunek ruchu na moscie

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER; // mutex do ochrony wspolnych zmiennych 

static pthread_cond_t condA = PTHREAD_COND_INITIALIZER; // condition variable dla aut jadących z A do B
static pthread_cond_t condB = PTHREAD_COND_INITIALIZER; // // condition variable dla aut jadących z B do A

// losowanie czasu w mikrosekundach
static int rnd_us(int min_us, int max_us) {
    return min_us + rand() % (max_us - min_us + 1);
}

// wypisywanie aktualnego stanu symulacji
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
    int on_bridge = (NaMościeAuto != -1) ? 1 : 0; // sprawdzanie poprawności liczby aut
    
    int suma = MiastoA + MiastoB + KolejkaA + KolejkaB + on_bridge;
    if (suma != N) {
        fprintf(stderr, "BLAD: suma=%d, N=%d\n", suma, N); // suma aut zawsze powinna być równa N
}
    fflush(stdout);
}

typedef enum { stateKolejkaA = 0, stateKolejkaB = 1, stateMiastoA = 2, stateMiastoB = 3 } state_t; // możliwe stany auta
typedef struct { // argumenty watku auta
    int id;
    state_t state;
} car_arg_t;

// sprawdza czy auto z A może wjechać na most
static int can_drive_A()
{
    return (
        // most musi być pusty
        NaMościeAuto == -1 &&
        
        // kierunek musi być pusty lub zgodny
        (
            RuchMostu == pusty ||
            RuchMostu == zAdoB
        ) &&
        // sprawdzanie naprzemienności
        (
            KolejkaB == 0 ||
            ZmiennikKierunku == zAdoB
        )
    );
}

// sprawdza czy auto z B może wjechać na most
static int can_drive_B()
{
    return (
        NaMościeAuto == -1 &&
        (
            RuchMostu == pusty ||
            RuchMostu == zBdoA
        ) &&
        (
            KolejkaA == 0 ||
            ZmiennikKierunku == zBdoA
        )
    );
}

static void* inQueueA(void *arg) // funkcja wykonywana gdy auto stoi w kolejce A
{
    car_arg_t *c = (car_arg_t*)arg;

    pthread_mutex_lock(&mtx);

    while (!can_drive_A()) { // czekamy dopóki nie można wjechać na most
        pthread_cond_wait(&condA, &mtx);
    }

    KolejkaA--; // auto opuszcza kolejkę
    NaMościeAuto = c->id; // auto wjeżdża na most
    RuchMostu = zAdoB; // ustawienie kierunku ruchu

    print_state();

    pthread_mutex_unlock(&mtx);

    usleep(rnd_us(100*1000, 300*1000)); // symulacja przejazdu przez most

    pthread_mutex_lock(&mtx);

    NaMościeAuto = -1; // auto opuszcza most
    MiastoB++; // auto dociera do miasta B

    RuchMostu = pusty; // most jest pusty
    ZmiennikKierunku = zBdoA; // następne auto powinno jechać z B

    print_state();

    pthread_cond_broadcast(&condA); // budzimy wszystkie oczekujące auta
    pthread_cond_broadcast(&condB);

    pthread_mutex_unlock(&mtx);

    c->state = stateMiastoB; // zmiana stanu auta

    return NULL;
}

// funkcja wykonywana gdy auto stoi w kolejce B
static void* inQueueB(void *arg)
{
    car_arg_t *c = (car_arg_t*)arg;

    pthread_mutex_lock(&mtx);

    while (!can_drive_B()) { // czekamy aż będzie można przejechać
        pthread_cond_wait(&condB, &mtx);
    }

    KolejkaB--; // auto opuszcza kolejkę
    NaMościeAuto = c->id; // auto wjeżdża na most
    RuchMostu = zBdoA; // ustawienie kierunku

    print_state();

    pthread_mutex_unlock(&mtx);

    usleep(rnd_us(100*1000, 300*1000)); // symulacja przejazdu

    pthread_mutex_lock(&mtx);

    NaMościeAuto = -1; // auto zjeżdża z mostu
    MiastoA++; // auto dojeżdża do miasta A

    RuchMostu = pusty; // most pusty
    ZmiennikKierunku = zAdoB; // następne auto powinno jechać z A

    print_state();

    pthread_cond_broadcast(&condA); // budzimy oczekujące auta
    pthread_cond_broadcast(&condB);

    pthread_mutex_unlock(&mtx);

    c->state = stateMiastoA; // zmiana stanu auta

    return NULL;
}

static void* inCityA(void *arg) // funkcja wykonywana gdy auto znajduje się w mieście A
{
    car_arg_t *c = (car_arg_t*)arg;

    usleep(rnd_us(50*1000, 200*1000)); // symulacja jazdy po mieście

    pthread_mutex_lock(&mtx);

    MiastoA--; // auto opuszcza miasto A
    KolejkaA++; // auto ustawia się w kolejce

    print_state();

    pthread_cond_broadcast(&condA); // budzimy auta czekające po stronie A

    pthread_mutex_unlock(&mtx);

    c->state = stateKolejkaA; // zmiana stanu auta

    return NULL;
}

static void* inCityB(void *arg) // funkcja wykonywana gdy auto znajduje się w mieście B
{
    car_arg_t *c = (car_arg_t*)arg;

    usleep(rnd_us(50*1000, 200*1000)); // symulacja jazdy po mieście

    pthread_mutex_lock(&mtx);

    MiastoB--; // auto opuszcza miasto B
    KolejkaB++; // auto ustawia się w kolejce

    print_state();

    pthread_cond_broadcast(&condB); // budzimy auta po stronie B

    pthread_mutex_unlock(&mtx);

    c->state = stateKolejkaB; // zmiana stanu auta

    return NULL;
}

static void* car_thread_init(void *arg) // główna funkcja wątku auta
{
    car_arg_t *c = (car_arg_t*)arg;

    while (1) { // auta jeżdżą bez końca

        switch (c->state) {

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

    if (argc != 2) { // sprawdzanie argumentów
        fprintf(stderr, "Uzycie: %s N\n", argv[0]);
        return 1;
    }

    N = atoi(argv[1]);

    if (N <= 0) { // liczba aut musi być dodatnia
        fprintf(stderr, "N musi byc > 0\n");
        return 1;
    }

    pthread_t *t = // tablica wątków
        calloc((size_t)N, sizeof(pthread_t));

    car_arg_t *args = // tablica argumentów dla aut
        calloc((size_t)N, sizeof(car_arg_t));

    for (int i = 0; i < N; i++) { // losowe rozmieszczenie aut w miastach

        args[i].id = i + 1;

        if (rand() % 2) {
            MiastoA++;
            args[i].state = stateMiastoA;
        } else {
            MiastoB++;
            args[i].state = stateMiastoB;
        }
    }

    print_state(); // wypisanie stanu początkowego

    for (int i = 0; i < N; i++) { // tworzenie wątków aut
        pthread_create(&t[i], NULL, car_thread_init, &args[i]);
    }

    for (int i = 0; i < N; i++) { // oczekiwanie na zakończenie wątków(w praktyce nigdy się nie zakończą)
        pthread_join(t[i], NULL);
    }

    return 0;
}
