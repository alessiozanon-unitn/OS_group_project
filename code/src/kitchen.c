#include "kitchen.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_resources_from(const char* file_path, Kitchen *kitchen){
    FILE *resources_file = fopen(file_path, "r");

    if(resources_file == NULL){
        fprintf(stderr, "Couldn't open file %s\n", file_path);
        return -1;
    }

    char line[64];

    while(fgets(line, sizeof(line), resources_file) != NULL){
        // replace newline with null byte
        line[strcspn(line, "\n")] = '\0';

        char* seek = line;

        char* resource_name = strsep(&seek, ",");

        if (!strcmp(resource_name, "resource"))
            continue;

        int quantity = atoi(strsep(&seek, ","));
        int clean_time = atoi(strsep(&seek, ","));

        // Temporary resource to then add to the kitchen resource list
        Resource resource;
        resource.name = strdup(resource_name);
        resource.clean_time = clean_time;
        resource.dirtyDishesCount = 0;
        sem_init(&resource.clean, 0, quantity);
        sem_init(&resource.dirty, 0, 0);
        sem_init(&resource.dirtyCountersMutex, 0, 1);

        resource.dirtyResourceCounters = malloc(sizeof(int)*quantity);
        for(int i=0; i<quantity; i++){
          resource.dirtyResourceCounters[i] = 0;
        }

        printf("resourceCount: %d\n", kitchen->resourceCount);
        // Added here
        kitchen->resourceCount++;
        kitchen->resources = realloc(kitchen->resources, sizeof(Resource)*kitchen->resourceCount);
        kitchen->resources[kitchen->resourceCount-1] = resource;
    }

    fclose(resources_file);

    return 0;
}
