#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define SANTA_SLEEP 1
#define GNOME_SLEEP 2

typedef unsigned int limit_t;
typedef unsigned int count_t;

/*
 *  GLOBALS
 */
//  input
size_t n, gnomeCount;
count_t deliveryCount;
limit_t *gnomesLimit, *decorationsLimit, stashLimit;
//  variables
count_t stash = 0, leftToHang;
limit_t *gnomesOnLevel;
int *currentLevel;
//  IDs 
pthread_t santaClausID, *gnomeID;
//  mutexes
pthread_mutex_t stashMutex, leftToHangMutex, *gnomesOnLevelMutex,
    *decorationsLimitMutex;
//  conds
pthread_cond_t deliveryCond, fullStashCond;


/*
 *  ENVIRONMENT
 */
void input();
void init();
void join();
void delete();
/*
 *  SANTA CLAUS
 */
void* santaClaus(void*);
void deliverDecorations();
bool notAllDecorations();
void finishGnomes();
/*
 *  GNOME
 */
void* gnome(void*);
void takeDecoration();
bool goUp(size_t);
bool goDown(size_t);
void hangDecoration(size_t);
void goForNext(size_t);


int main()
{
    input();
    init();
    join();
    printf("Koniec\n");
    delete();
}


void input()
{
    scanf("%zu %zu %u %u", &n, &gnomeCount, &deliveryCount, &stashLimit);
    
    if ((gnomesLimit = (limit_t*)malloc(n * sizeof(limit_t))) == NULL) {
        perror("Dynamiczna alokacja k");
        exit(1);
    }
    if ((decorationsLimit = (limit_t*)malloc(n * sizeof(limit_t))) == NULL) {
        perror("Dynamiczn alokacja p");
        exit(1);
    }
    
    for (size_t i = 0; i < n; ++i)
        scanf("%u", gnomesLimit + i);
    for (size_t i = 0; i < n; ++i)
        scanf("%u", decorationsLimit + i);

    //  verify
    for (size_t i = 1; i < n; ++i)
        if (gnomesLimit[i - 1] <= gnomesLimit[i]) {
            perror("Niepoprawna wartosc ki");
            exit(1);
        }
    if (n == 0 || gnomeCount == 0 || deliveryCount == 0) {
        perror("Zerowa wartosc");
        exit(1);
    }
}


void init()
{
    //  variables
    leftToHang = 0;
    for (size_t i = 0; i < n; ++i)
        leftToHang += decorationsLimit[i];

    gnomesOnLevel = (limit_t*)malloc(n * sizeof(limit_t));
    for (size_t i = 0; i < n; ++i)
        gnomesOnLevel[i] = 0;

    currentLevel = (int*)malloc(gnomeCount * sizeof(int));
    for (size_t i = 0; i < gnomeCount; ++i)
        currentLevel[i] = -1;

    //  mutexes
    if (pthread_mutex_init(&stashMutex, NULL) != 0) {
        perror("Utworzenie mutexu ozdob na poziomie 0");
        exit(1);
    }
    if (pthread_mutex_init(&leftToHangMutex, NULL) != 0) {
        perror("Utworzenie mutexu pozostalych do powieszenia ozdob");
        exit(1);
    }
    if ((gnomesOnLevelMutex = (pthread_mutex_t*)malloc(n * 
    sizeof(pthread_mutex_t))) == NULL) {
        perror("Alokacja tablicy mutexow skrzatow na poziomie");
        exit(1);
    }
    if ((decorationsLimitMutex = (pthread_mutex_t*)malloc(n *
    sizeof(pthread_mutex_t))) == NULL) {
        perror("Alokacja tablicy mutexow ozdob do rozwieszenia");
        exit(1);
    }
    for (size_t i = 0; i < n; ++i) {
        if (pthread_mutex_init(gnomesOnLevelMutex + i, NULL) != 0) {
            perror("Utworzenie mutexu skrzatow na poziomie");
            exit(1);
        }
        if (pthread_mutex_init(decorationsLimitMutex + i, NULL) != 0) {
            perror("Utworzenie mutexu ozdob na poziomie");
            exit(1);
        }
    }
    
    //conds
    if (pthread_cond_init(&deliveryCond, NULL) != 0) {
        perror("Utworzenie zmiennej warunkowej dostawy");
        exit(1);
    }
    if (pthread_cond_init(&fullStashCond, NULL) != 0) {
        perror("Utworzenie zmiennej warunkowej pelnego poziomu 0");
        exit(1);
    }

    //  threads
    if (pthread_create(&santaClausID, NULL, santaClaus, NULL) != 0) {
        perror("Utworzenie watku mikolaja");
        exit(1);
    }
    gnomeID = (pthread_t*)malloc(gnomeCount * sizeof(pthread_t));
    for (size_t i = 0; i < gnomeCount; ++i)
        if (pthread_create(&gnomeID[i], NULL, gnome, (void*)i) != 0) {
            perror("Utworzenie watku skrzata");
            exit(1);
        }
}


void join()
{
    for (size_t i = 0; i < gnomeCount; ++i)
        pthread_join(gnomeID[i], NULL);
    pthread_join(santaClausID, NULL);
}


void delete()
{
    free(gnomesLimit);
    free(decorationsLimit);
    free(gnomeID);
    free(gnomesOnLevel);
    free(currentLevel);
    free(gnomesOnLevelMutex);
    free(decorationsLimitMutex);
}


void* santaClaus(void *arg)
{
    while (notAllDecorations()) {
        printf("Mikolaj przyjezdza\n");
        deliverDecorations();
        sleep(SANTA_SLEEP);
        printf("Mikolaj odjezdza\n", leftToHang);
    }
    finishGnomes();
}


void deliverDecorations()
{
    count_t currentDelivery = deliveryCount, delivered;
    while (currentDelivery > 0) {
        pthread_mutex_lock(&stashMutex);
        if (stash == stashLimit) {
            printf("Mikolaj czeka\n");
            pthread_cond_wait(&fullStashCond, &stashMutex);
        }
        delivered = stash + currentDelivery > stashLimit ?
            stashLimit - stash : currentDelivery;
        printf("Mikolaj daje %u z %u ozdob\n",
            delivered, currentDelivery);
        stash += delivered;
        currentDelivery -= delivered;
        pthread_mutex_unlock(&stashMutex);
        pthread_cond_broadcast(&deliveryCond);
    }
}


bool notAllDecorations()
{
    bool value;
    pthread_mutex_lock(&leftToHangMutex);
    value = leftToHang != 0;
    pthread_mutex_unlock(&leftToHangMutex);
    return value;
}


void finishGnomes()
{
    for (size_t i = 0; i < gnomeCount; ++i)
        pthread_cancel(gnomeID[i]);
    pthread_cond_broadcast(&deliveryCond);
}


void* gnome(void *arg)
{
    size_t id = (size_t)arg;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while (1) {
        takeDecoration();
        printf("Skrzat %zu bierze ozdobe\n", id);
        hangDecoration(id);
        printf("Skrzat %zu wiesza na poziomie %d\n", 
            id, currentLevel[id]);
        goForNext(id);
        printf("Skrzat %zu czeka na kolejna\n", id);
    }
}


void takeDecoration()
{
    pthread_mutex_lock(&stashMutex);
    while (stash == 0) 
        pthread_cond_wait(&deliveryCond, &stashMutex);
    --stash;
    pthread_mutex_unlock(&stashMutex);
    pthread_cond_signal(&fullStashCond);
}


bool goUp(size_t id)
{
    const size_t current = currentLevel[id], 
        next = currentLevel[id] + 1;
    if (next == n)
        return false;

    pthread_mutex_lock(gnomesOnLevelMutex + next);
    if (next != 0)
        pthread_mutex_lock(gnomesOnLevelMutex + current);

    if (gnomesOnLevel[next] == gnomesLimit[next]) {
        pthread_mutex_unlock(gnomesOnLevelMutex + next);
        if (next != 0)
            pthread_mutex_unlock(gnomesOnLevelMutex + current);
        return false;
    }

    ++currentLevel[id];
    ++gnomesOnLevel[next];
    if (next != 0) { 
        --gnomesOnLevel[current];
        pthread_mutex_unlock(gnomesOnLevelMutex + current);
    }
    pthread_mutex_unlock(gnomesOnLevelMutex + next);
    return true;
}


bool goDown(size_t id)
{
    if (currentLevel[id] == -1)
        return false;

    const size_t current = currentLevel[id], 
        next = current - 1;
    pthread_mutex_lock(gnomesOnLevelMutex + current);
    if (current == 0) {
        --gnomesOnLevel[0];
        currentLevel[id] = -1;
        pthread_mutex_unlock(gnomesOnLevelMutex);
        return true;
    }
    pthread_mutex_lock(gnomesOnLevelMutex + next);
    if (gnomesOnLevel[next] == gnomesLimit[next]) {
        pthread_mutex_unlock(gnomesOnLevelMutex + next);
        pthread_mutex_unlock(gnomesOnLevelMutex + current);
        return false;
    }
    --currentLevel[id];
    --gnomesOnLevel[current];
    ++gnomesOnLevel[next];
    pthread_mutex_unlock(gnomesOnLevelMutex + next);
    pthread_mutex_unlock(gnomesOnLevelMutex + current);
    return true;
}


void hangDecoration(size_t id)
{
    while(1) {
        if (goUp(id) && currentLevel[id] != -1) {
            pthread_mutex_lock(decorationsLimitMutex + 
                currentLevel[id]);
            if (decorationsLimit[currentLevel[id]]) {
                --decorationsLimit[currentLevel[id]];
                pthread_mutex_unlock(decorationsLimitMutex +
                    currentLevel[id]);
                pthread_mutex_lock(&leftToHangMutex);
                --leftToHang;
                pthread_mutex_unlock(&leftToHangMutex);
                sleep(GNOME_SLEEP);
                return;
            }
            pthread_mutex_unlock(decorationsLimitMutex +
                currentLevel[id]);
        } else goDown(id);
    }
}


void goForNext(size_t id)
{
    while(currentLevel[id] != -1)
        goDown(id);
}