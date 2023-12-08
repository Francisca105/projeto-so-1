#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 3

// Função executada pelas threads
void *funcaoDaThread(void *arg) {
    int thread_id = *((int *)arg);    
    for (size_t i = 0; i < 100; i++)
    {
        printf("Thread %d\n", thread_id);
    }
    
    printf("Esta é a execução da Thread %d\n", thread_id);
    // printf("Esta é a execução da Thread %d\n", thread_id);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Criar três threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;  // IDs de thread começam em 1
        printf("Criando a Thread %d\n", i + 1);

        if (pthread_create(&threads[i], NULL, funcaoDaThread, (void *)&thread_ids[i]) != 0) {
            fprintf(stderr, "Erro ao criar a Thread %d\n", i + 1);
            exit(EXIT_FAILURE);
        }
    }

    // Esperar pela conclusão de cada thread
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("Esperando pela Thread %d\n", i + 1);
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Erro ao esperar pela Thread %d\n", i + 1);
            exit(EXIT_FAILURE);
        }
    }

    printf("Todas as threads foram concluídas com sucesso\n");

    return 0;
}