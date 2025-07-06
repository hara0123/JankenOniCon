#ifndef __TO_JANKEN_ONI_DATA_H_
#define __TO_JANKEN_ONI_DATA_H_

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
const char sendChar[][KINDS_OF_CHAR] =
{
  {',','.','/','K','L',';','I','O','P','B','N','M'}, // Camera
  {',','.','/','K','L',';','I','O','P','B','N','M'}, // Move
  {',','.','/','K','L',';','I','O','P','B','N','M'}, // Dash
  {',','.','/','K','L',';','I','O','P','B','N','M'}, // Jump
};

#define MODIFIER_BIT_NONE 0
#define MODIFIER_BIT_S 1
#define MODIFIER_BIT_C 2
#define MODIFIER_BIT_A 4

// 各モードでShiftキー・Ctrlキー・Altキーを押すかどうか
// .：修飾キーなし、S：Shiftキー併用、C：Ctrlキー併用、A：Altキー併用
const uint8_t sendModifierKey[][KINDS_OF_CHAR] =
{
  {0,0,0,0,0,0,0,0,0,0,0,0}, // Camera
  {1,1,1,1,1,1,1,1,1,1,1,1}, // Move、Shiftキー併用
  {2,2,2,2,2,2,2,2,2,2,2,2}, // Dash、Ctrlキー併用
  {3,3,3,3,3,3,3,3,3,3,3,3}, // Jump、Shift・Ctrlキー併用
};


#endif