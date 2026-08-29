#include "kitchen.h"
#include "errorcodes.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_resources_from(const char* file_path, Kitchen *kitchen){
    FILE *resources_file = fopen(file_path, "r");

    if(resources_file == NULL){
        fprintf(stderr, "Couldn't open file %s\n", file_path);
        return FILE_NOT_FOUND;
    }

    char line[64];

    while(fgets(line, sizeof(line), resources_file) != NULL){
        // replace newline with null byte
        line[strcspn(line, "\n")] = '\0';

        char* seek = line;

        char* resource_name = strsep(&seek, ",");
        if (resource_name == NULL || *resource_name == '\0') {
            fprintf(stderr, "Malformed resource line\n");
            fclose(resources_file);
            return INVALID_RESOURCE;
        }
        if (!strcmp(resource_name, "resource")) continue;

        char* quantity_str = strsep(&seek, ",");
        char* clean_time_str = strsep(&seek, ",");
        if (quantity_str == NULL || clean_time_str == NULL) {
            fprintf(stderr, "Malformed resource line for '%s'\n", resource_name);
            fclose(resources_file);
            return INVALID_RESOURCE;
        }
        int quantity = atoi(quantity_str);
        int clean_time = atoi(clean_time_str);
        if (quantity <= 0 || clean_time < 0) {
            fprintf(stderr, "Invalid quantity/clean_time for '%s'\n", resource_name);
            fclose(resources_file);
            return INVALID_RESOURCE;
        }
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

        // Added here
        kitchen->resourceCount++;
        kitchen->resources = realloc(kitchen->resources, sizeof(Resource)*kitchen->resourceCount);
        kitchen->resources[kitchen->resourceCount-1] = resource;
    }

    if (kitchen->resourceCount == 0) {
        fprintf(stderr, "Resources file '%s' defines no resources\n", file_path);
        fclose(resources_file);
        return INVALID_RESOURCE;
    }

    fclose(resources_file);
    return ALL_OK;
}
