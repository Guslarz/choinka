#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define SANTA_SLEEP 2
#define GNOME_SLEEP 1

typedef unsigned int limit_t;
typedef unsigned int count_t;

/*
 *  GLOBALS
 */
//  input
size_t n, gnomeCount;
count_t deliveryCount;
limit_t *gnomesLimit, *decorationsLimit;
//  variables
count_t stash = 0, leftToHang;
limit_t *gnomesOnLevel;
int *currentLevel;
//  IDs 
pthread_t santaClausID, *gnomeID;
//  mutexes
pthread_mutex_t stashMutex, leftToHangMutex, gnomesOnLevelMutex,
    decorationsLimitMutex;
//  conds
pthread_cond_t deliveryCond;


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
    delete();
}


void input()
{
    scanf("%zu %zu %u", &n, &gnomeCount, &deliveryCount);
    
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
    if (pthread_mutex_init(&gnomesOnLevelMutex, NULL) != 0) {
        perror("Utworzenie mutexu skrzatow na poziomie");
        exit(1);
    }
    if (pthread_mutex_init(&decorationsLimitMutex, NULL) != 0) {
        perror("Utworzenie mutexu ozdob na poziomie");
        exit(1);
    }
    
    //conds
    if (pthread_cond_init(&deliveryCond, NULL) != 0) {
        perror("Utworzenie zmiennej warunkowej dostawy");
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
}


void* santaClaus(void *arg)
{
    while (notAllDecorations()) {
        deliverDecorations();
        sleep(SANTA_SLEEP);
        printf("Brakuje %u ozdob\n", leftToHang);
    }
    finishGnomes();
}


void deliverDecorations()
{
    pthread_mutex_lock(&stashMutex);
    stash += deliveryCount;
    pthread_mutex_unlock(&stashMutex);
    pthread_cond_broadcast(&deliveryCond);
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
        printf("%zu bierze ozdobe\n", id);
        hangDecoration(id);
        printf("%zu wiesza na poziomie %d\n", id, currentLevel[id]);
        goForNext(id);
        printf("%zu czeka na kolejna\n", id);
    }
}


void takeDecoration()
{
    pthread_mutex_lock(&stashMutex);
    while (stash == 0) 
        pthread_cond_wait(&deliveryCond, &stashMutex);
    --stash;
    pthread_mutex_unlock(&stashMutex);
}


bool goUp(size_t id)
{
    const size_t current = currentLevel[id], 
        next = currentLevel[id] + 1;
    if (next == n)
        return false;

    pthread_mutex_lock(&gnomesOnLevelMutex);
    if (gnomesOnLevel[next] == gnomesLimit[next]) {
        pthread_mutex_unlock(&gnomesOnLevelMutex);
        return false;
    }
    if (next != 0) 
        --gnomesOnLevel[current];
    ++gnomesOnLevel[next];
    currentLevel[id] = next;
    pthread_mutex_unlock(&gnomesOnLevelMutex);
    return true;
}


bool goDown(size_t id)
{
    if (currentLevel[id] == -1)
        return false;

    const size_t current = currentLevel[id], 
        next = current - 1;
    pthread_mutex_lock(&gnomesOnLevelMutex);
    if (current == 0) {
        --gnomesOnLevel[current];
        currentLevel[id] = -1;
        pthread_mutex_unlock(&gnomesOnLevelMutex);
        return true;
    }
    if (gnomesOnLevel[next] == gnomesLimit[next]) {
        pthread_mutex_unlock(&gnomesOnLevelMutex);
        return false;
    }
    --gnomesOnLevel[current];
    ++gnomesOnLevel[next];
    currentLevel[id] = next;
    pthread_mutex_unlock(&gnomesOnLevelMutex);
    return true;
}


void hangDecoration(size_t id)
{
    while(1) {
        if (goUp(id)) {
            pthread_mutex_lock(&decorationsLimitMutex);
            if (decorationsLimit[currentLevel[id]]) {
                --decorationsLimit[currentLevel[id]];
                pthread_mutex_unlock(&decorationsLimitMutex);
                pthread_mutex_lock(&leftToHangMutex);
                --leftToHang;
                pthread_mutex_unlock(&leftToHangMutex);
                sleep(GNOME_SLEEP);
                return;
            }
            pthread_mutex_unlock(&decorationsLimitMutex);
        } else goDown(id);
    }
}


void goForNext(size_t id)
{
    while(currentLevel[id] != -1)
        goDown(id);
}