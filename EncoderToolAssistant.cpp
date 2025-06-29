#include "EncoderToolAssistant.h"

// カウンター増なら+1、カウンター減なら-1、変化なしなら0を返す
int8_t EncoderToolAssistant::GetStatus()
{
  int8_t encoderStatus;

  if (enc_.valueChanged())
  {
    int value = enc_.getValue();

    if(counterOld > value)
    {
      encoderStatus = -1; // カウンター減
    }
    else
    {
      encoderStatus = 1; // カウンター増
    }

    if(value == ENCODER_COUNT_MIN || value == ENCODER_COUNT_MAX)
    {
      enc_.setValue(0);
      counterOld = 0;
    }
    else
    {
      counterOld = value;
    }
  }
  else
  {
    encoderStatus = 0;
  }

  return encoderStatus;
}
