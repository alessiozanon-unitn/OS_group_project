#include "menu.h"
#include "errorcodes.h"
#include "kitchen.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Menu menu;

int load_menu_from(const char *file_path, Kitchen *kitchen){
  FILE *menu_file = fopen(file_path, "r");

  if(menu_file == NULL){
    fprintf(stderr, "Couldn't open file %s\n", file_path);
    return FILE_NOT_FOUND;
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
    if (name == NULL || *name == '\0') {
      fprintf(stderr, "Malformed menu line\n");
      fclose(menu_file);
      return INVALID_DISH;
    }
    if (!strcmp("name", name)) continue;

    char* price_str = strsep(&seek, ",");
    char* time_str = strsep(&seek, ",");
    if (price_str == NULL || time_str == NULL) {
      fprintf(stderr, "Malformed menu line for '%s'\n", name);
      fclose(menu_file);
      return INVALID_DISH;
    }
    int price = atoi(price_str);
    int time = atoi(time_str);    int requiredSize = 0;
    int* requiredCount = NULL;
    int* requiredTypes = NULL;
    char *str;

    //Iterates over requirements
    while((str = strsep(&seek, ";")) != NULL){
      requiredCount = realloc(requiredCount, sizeof(int)*(requiredSize + 1));
      requiredTypes = realloc(requiredTypes, sizeof(int)*(requiredSize + 1));

      //Check if the requirement has > 1 item
      if(strchr(str, ':') != NULL){
        char* str2 = strsep(&str, ":");
        char* count_str = strsep(&str, ":");
        if (str2 == NULL || count_str == NULL) {
          fprintf(stderr, "Malformed requirement for dish '%s'\n", name);
          free(requiredCount); free(requiredTypes);
          fclose(menu_file);
          return INVALID_DISH;
        }
        // Find resource index within kitchen
        bool found = false;
        for(int i=0;i<kitchen->resourceCount;i++){
          if (!strcmp(str2, kitchen->resources[i].name)){
            //Sets requiredTypes with the index within kitchen's resources
            requiredTypes[requiredSize] = i;
            found = true;
            break;
          }
        }
        if (!found) {
          fprintf(stderr, "Dish '%s' references unknown resource '%s'\n", name, str2);
          free(requiredCount); free(requiredTypes);
          fclose(menu_file);
          return INVALID_RESOURCE;
        }
        //Sets it
        requiredCount[requiredSize] = atoi(count_str);

      }else{
        // Find resource index within kitchen
        bool found = false;
        for(int i=0;i<kitchen->resourceCount;i++){
          if (!strcmp(str, kitchen->resources[i].name)){
            //Sets requiredTypes with the index within kitchen's resources
            requiredTypes[requiredSize] = i;
            found = true;
            break;
          }
        }
        if (!found) {
          fprintf(stderr, "Dish '%s' references unknown resource '%s'\n", name, str);
          free(requiredCount); free(requiredTypes);
          fclose(menu_file);
          return INVALID_RESOURCE;
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

  if (dishCount == 0) {
    fprintf(stderr, "Menu file '%s' defines no dishes\n", file_path);
    fclose(menu_file);
    return INVALID_DISH;
  }

  fclose(menu_file);
  return ALL_OK;
}
