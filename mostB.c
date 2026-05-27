#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// Typ określający kierunek jazdy samochodu
typedef enum
{
    zAdoB,      // samochód jedzie z miasta A do B
    zBdoA,      // samochód jedzie z miasta B do A
    pusty       // most wolny
} dir_t;

// Struktura przekazywana do każdego wątku samochodu
typedef struct
{
    int id;      // numer samochodu
    dir_t kierunek;      // aktualny kierunek jazdy
} car_arg_t;

// liczba samochodów przekazana jako parametr programu
static int N;

// liczba samochodów aktualnie znajdujących się w mieście A i B
static int MiastoA=0;
static int MiastoB=0;

// liczba samochodów oczekujących na most
static int KolejkaA=0;
static int KolejkaB=0;

// numer samochodu znajdującego się na moście
static int NaMoscie=-1;      // -1 oznacza brak samochodu

// informacja czy most jest zajęty
static int MostZajety=0;

// aktualny kierunek ruchu na moście
static dir_t RuchMostu=pusty;

// mutex
static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;

// zmienne warunkowe dla kolejek po stronie A i B
static pthread_cond_t condA=PTHREAD_COND_INITIALIZER;
static pthread_cond_t condB=PTHREAD_COND_INITIALIZER;


// wyświetla aktualny stan symulacji
void print_state()
{
    // jeśli na moście jest samochód
    if(NaMoscie!=-1)
    {
        // ruch z A do B
        if(RuchMostu==zAdoB)
        {
            printf("A-%d %d >>> [>> %d >>] <<< %d %d-B\n", MiastoA, KolejkaA, NaMoscie, KolejkaB, MiastoB);
        }
        // ruch z B do A
        else
        {
            printf("A-%d %d >>> [<< %d <<] <<< %d %d-B\n", MiastoA, KolejkaA, NaMoscie, KolejkaB, MiastoB);
        }
    }
    // most pusty
    else
    {
        printf("A-%d %d >>> [ --- ] <<< %d %d-B\n", MiastoA, KolejkaA, KolejkaB, MiastoB);
    }
  
    fflush(stdout);
}

// funkcja wykonywana przez każdy wątek samochodu
void* car_thread(void *arg)
{
    car_arg_t *car=(car_arg_t*)arg;

    int id=car->id;

    // aktualny kierunek jazdy
    dir_t kierunek=car->kierunek;

    // samochód jeździ dopóki program nie zostanie zatrzymany
    while(1)
    {
        pthread_mutex_lock(&mtx);

        // samochód ustawia się do odpowiedniej kolejki
        if(kierunek==zAdoB)
            KolejkaA++;
        else
            KolejkaB++;

        print_state();

        // Czekanie aż most będzie wolny
        while(MostZajety)
        {
            // samochód A->B czeka
            if(kierunek==zAdoB)
                pthread_cond_wait(&condA, &mtx);
            // samochód B->A czeka
            else
                pthread_cond_wait(&condB, &mtx);
        }

        // zajęcie mostu
        MostZajety=1;
        // ustawienie kierunku ruchu mostu
        RuchMostu=kierunek;
        // samochód opuszcza miasto i kolejkę
        if(kierunek==zAdoB)
        {
            KolejkaA--;
            MiastoA--;
        }
        else
        {
            KolejkaB--;
            MiastoB--;
        }

        // zapisujemy samochód na moście
        NaMoscie=id;

        print_state();

        pthread_mutex_unlock(&mtx);

        // przejazd przez most
        usleep(1000000);

        pthread_mutex_lock(&mtx);

        // samochód zjechał z mostu
        NaMoscie=-1;

        // most staje się wolny
        MostZajety=0;
        RuchMostu=pusty;

        // samochód trafia do przeciwnego miasta
        if(kierunek==zAdoB)
            MiastoB++;
        else
            MiastoA++;

        print_state();

        // preferuje przeciwną stronę, jeśli ktoś czeka
        if(kierunek==zAdoB)
        {
            if(KolejkaB>0)
                pthread_cond_signal(&condB);
            else
                pthread_cond_signal(&condA);
        }
        else
        {
            if(KolejkaA>0)
                pthread_cond_signal(&condA);
            else
                pthread_cond_signal(&condB);
        }

        pthread_mutex_unlock(&mtx);

        // po dojechaniu samochód zawraca
        if(kierunek==zAdoB)
            kierunek=zBdoA;
        else
            kierunek=zAdoB;
    }

    return NULL;
}

int main(int argc,char *argv[])
{

    return 0;
}
