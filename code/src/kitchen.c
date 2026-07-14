#include "kitchen.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int load_resources_from(char* file_path, Kitchen *kitchen){
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

        if (strcmp(resource_name, "resource"))
            continue;

        int quantity = atoi(strsep(&seek, ","));
        int clean_time = atoi(strsep(&seek, ","));

        Resource resource;
        resource.name = strdup(resource_name);
        resource.clean_time = clean_time;




    }

    // After the cycle clean, and dirtyResourceCounters must be initialized since then it
    // resourceCount will be known



    return 0;
}
