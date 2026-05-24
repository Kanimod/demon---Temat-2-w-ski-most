#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int N; // Liczba samochodów podawana przy uruchomieniu
int inA = 0; // Liczba samochodów znajdujących się po stronie A
int inB = 0; // Liczba samochodów znajdujących się po stronie B
int waitingA = 0; // Liczba samochodów oczekujących na przejazd od strony A
int waitingB = 0; // Liczba samochodów oczekujących na przejazd od strony B
int bridge = 0; // Stan mostu: 0 - wolny, 1 - zajęty

pthread_mutex_t mutex; // Zabezpiecza współdzielone dane przed jednoczesnym dostępem wielu wątków

// Zmienna warunkowa
pthread_cond_t cond;

// Funkcja wyświetla aktualny stan
void print_state(int id, const char* direction)
{
    printf("A-%d %d>>> [>> %d %s >>] <<<%d %d-B\n", inA, waitingA, id, direction, waitingB, inB);
    fflush(stdout);
}

// Wejście na most od strony A
// Próba wejścia samochodu na most z miasta A do miasta B
void enterBridgeA(int id)
{
    // Blokada sekcji krytycznej
    pthread_mutex_lock(&mutex);

    // Samochód ustawia się w kolejce
    waitingA++;

    // Jeżeli most zajęty, wątek przechodzi w stan oczekiwania
    while(bridge == 1)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    // Samochód opuszcza kolejkę
    waitingA--;

    // Zajęcie mostu
    bridge = 1;

    // Samochód opuszcza stronę A
    inA--;

    // Wyświetlenie zmian
    print_state(id,"A->B");

    // Zwolnienie sekcji krytycznej
    pthread_mutex_unlock(&mutex);
}

// Wejscie na most od strony B
// Próba wejścia samochodu na most z miasta B do miasta A
void enterBridgeB(int id)
{
    pthread_mutex_lock(&mutex);

    // Dodanie do kolejki
    waitingB++;

    // Jeżeli most zajęty, wątek przechodzi w stan oczekiwania
    while(bridge == 1)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    // Samochód opuszcza kolejkę
    waitingB--;

    // Zajęcie mostu
    bridge = 1;

    // Opuszczenie miasta B
    inB--;

    // Wypisanie zmian
    print_state(id,"B->A");

    // Zwolnienie sekcji krytycznej
    pthread_mutex_unlock(&mutex);
}


// Trzeba dodać wątek samochodu
/*
Funkcja ma robić:
   - przejazd A->B
   - zwolnienie mostu
   - przebudzenie oczekujących
   - przejazd B->A
*/

int main(int argc, char* argv[])
{
    /*
       Trzeba dodać:
       - pobranie N
       - inicjalizacja mutex
       - inicjalizacja cond

       pthread_mutex_init()
       pthread_cond_init()

       - tworzenie wątków
       - pthread_join()
       - usuwanie mutex i cond
    */

    return 0;
}
