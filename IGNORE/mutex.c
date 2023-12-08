#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 3

pthread_mutex_t mutex; // Declaração do mutex

// Função executada pelas threads
void *funcaoDaThread(void *arg) {
    int thread_id = *((int *)arg);    
    pthread_mutex_lock(&mutex); // Bloqueia o acesso ao recurso compartilhado
    for (size_t i = 0; i < 100; i++)
    {
        printf("Thread %d\n", thread_id);
    }
    
    printf("Esta é a execução da Thread %d\n", thread_id);
    pthread_mutex_unlock(&mutex); // Libera o acesso ao recurso compartilhado
    // printf("Esta é a execução da Thread %d\n", thread_id);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    pthread_mutex_init(&mutex, NULL); // Inicializa o mutex

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

    pthread_mutex_destroy(&mutex); // Destroi o mutex

    printf("Todas as threads foram concluídas com sucesso\n");

    return 0;
}