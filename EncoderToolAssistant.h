// ロータリーエンコーダのカウンタ値を直接使うのではなく、値の増減の取得をするときに役立つクラス

#ifndef __ENCODER_TOOL_ASSISTANT_H_
#define __ENCODER_TOOL_ASSISTANT_H_

// 実際の値は利用しないのでいくつでも良いが、int8_tの範囲にすること
#define ENCODER_COUNT_MIN -100
#define ENCODER_COUNT_MAX 100

#include <EncoderTool.h>

class EncoderToolAssistant
{
  EncoderTool::PolledEncoder& enc_;

  int8_t counterOld;
  int8_t valueMax;
  int8_t valueMin;

public:
	EncoderToolAssistant(EncoderTool::PolledEncoder& enc) : enc_(enc)
  {
    counterOld = 0;
    valueMax = ENCODER_COUNT_MAX;
    valueMin = ENCODER_COUNT_MIN;

    enc_.setLimits(valueMin, valueMax);
  }

  int8_t GetStatus();
};

#endif
