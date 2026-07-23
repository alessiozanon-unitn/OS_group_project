#include "menu.h"
#include "kitchen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Menu menu;

int load_menu_from(const char *file_path, Kitchen *kitchen){
  FILE *menu_file = fopen(file_path, "r");
  
  if(menu_file == NULL){
    fprintf(stderr, "Couldn't open file %s\n", file_path);
    return -1;
  }

  int dishCount = 0;
  menu.dishes = NULL;

  char line[256];

  while(fgets(line, sizeof(line), menu_file) != NULL){
    // replace newline with null byte
    line[strcspn(line, "\n")] = '\0';

    // Pointer to go through the line
    char *seek = line;

    // Takes first entry and skips the first line
    char* name = strsep(&seek, ",");
    if (!strcmp("name", name))
      continue;

    // Stuff to build a dish
    int price = atoi(strsep(&seek, ","));
    int time = atoi(strsep(&seek, ","));
    int requiredSize = 0;
    int* requiredCount = NULL;
    int* requiredTypes = NULL;
    char *str;

    //Iterates over requirements
    while((str = strsep(&seek, ";")) != NULL){
      requiredCount = realloc(requiredCount, sizeof(int)*(requiredSize + 1));
      requiredTypes = realloc(requiredTypes, sizeof(int)*(requiredSize + 1));

      //Check if the requirement has > 1 item
      if(strchr(str, ':') != NULL){
        //Takes name of the resource
        char* str2 = strsep(&str, ":");

        // Find resource index within kitchen
        for(int i=0;i<kitchen->resourceCount;i++){
          if (!strcmp(str2, kitchen->resources[i].name)){
            //Sets requiredTypes with the index within kitchen's resources
            requiredTypes[requiredSize] = i;
            break;
          }
        }
        //Takes number of resources needed
        str2 = strsep(&str, ":");

        //Sets it
        requiredCount[requiredSize] = atoi(str2);


      }else{
        // Find resource index within kitchen
        for(int i=0;i<kitchen->resourceCount;i++){
          if (!strcmp(str, kitchen->resources[i].name)){
            //Sets requiredTypes with the index within kitchen's resources
            requiredTypes[requiredSize] = i;
            break;
          }
        }

        requiredCount[requiredSize] = 1;
      }
      requiredSize++;
    }

    Dish dish;

    dish.name = strdup(name);
    dish.price = price;
    dish.time = time;
    dish.requiredCount = requiredCount;
    dish.requiredTypes = requiredTypes;
    dish.requiredSize = requiredSize;

    menu.dishes = realloc(menu.dishes, sizeof(Dish)*(dishCount + 1));
    menu.dishes[dishCount] = dish;
    menu.dishCount = dishCount+1;

    dishCount++;
  }


  fclose(menu_file);
  return 0;
}
