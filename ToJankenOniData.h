#ifndef __TO_JANKEN_ONI_DATA_H_
#define __TO_JANKEN_ONI_DATA_H_

#define MAX_MODE_NUM 4
#define KINDS_OF_CHAR 12

// ここから
#define ENC1UP 0
#define ENC1DN 1
#define ENC1SW 2
#define ENC2UP 3
#define ENC2DN 4
#define ENC2SW 5
#define ENC3UP 6
#define ENC3DN 7
#define ENC3SW 8
#define PARAM_SW1 9
#define PARAM_SW2 10
#define PARAM_SW3 11
// ここまでは変更しないこと

// モード1・2・3・4
const char sendchar[][KINDS_OF_CHAR] =
{
  {'a','b','c','d','e','f','g','h','i','j','k','l'}, // system
  {'m','n','o','p','q','r','s','t','u','v','w','x'}, // camera
  {'y','z','A','B','C','D','E','F','G','H','I','J'}, // move
  {'K','L','M','N','O','P','Q','R','S','T','U','V'}, // jump
};

#endif