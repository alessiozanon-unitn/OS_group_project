#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Menu *menu;

int load_menu_from(char *file_path){
  FILE *menu_file = fopen(file_path, "r");

  if(menu_file == NULL){
    fprintf(stderr, "Couldn't open file %s\n", file_path);
    return -1;
  }

  int dishCount = 0;
  menu = realloc(menu, (dishCount + 1));

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

    dishCount++;

    // Stuff to build a dish
    int price = atoi(strsep(&seek, ","));
    int time = atoi(strsep(&seek, ","));
    int requiredSize = 0;
    int* requiredCount = malloc(0);
    int* requiredTypes = malloc(0);
    char *str;

    //Iterates over requirements
    while((str = strsep(&seek, ";")) != NULL){
      requiredCount = realloc(requiredCount, sizeof(int)*(requiredSize + 1));
      requiredTypes = realloc(requiredTypes, sizeof(int)*(requiredSize + 1));

      //Check if the requirement has > 1 item
      if(strchr(str, ':') != NULL){
        char* str2 = strsep(&str, ":");
        //requiredTypes[requiredSize] = str2; Needs resource string -> index number conversion
      }else{
        requiredCount[requiredSize] = 1;
      }
      requiredSize++;
    }

    Dish *dish = malloc(sizeof(Dish));

    dish->name = name;
    dish->price = price;
    dish->time = time;
    dish->requiredCount = requiredCount;
    dish->requiredTypes = requiredTypes;
    dish->requiredSize = requiredSize;


    dishCount++;
  }

  return 0;
}
