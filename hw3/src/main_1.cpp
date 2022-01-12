#include <stdio.h>
#include <string.h>
#include "MyAI.h"

int main(){
  char read[1024], write[1024], output[2048], *token;
  const char *data[50];
  char response[100];
  MyAI myai;
  myai.reset_board(data, response);
  // for(auto i: {1,2,3,4,5}){
    myai.genmove(data, response);
  // }
  return 0;
}