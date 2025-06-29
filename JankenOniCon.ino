// じゃんけん鬼パラメータ変更装置用スケッチ

#include <EncoderTool.h>
#include <Keyboard.h>
#include <MsTimer2.h>

#include "ToJankenOniData.h"

// 各ピン設定、プログラム中では高速化のためレジスタを直接操作する
#define ENC_1A_PIN 4
#define ENC_1B_PIN 5
#define ENC_2A_PIN 6
#define ENC_2B_PIN 7
#define ENC_3A_PIN 8
#define ENC_3B_PIN 9

#define SW_ENC1_PIN A2 // PF5
#define SW_ENC2_PIN A1 // PF6
#define SW_ENC3_PIN A0 // PF7

#define SW_MODE1_PIN 10 // PB6, Jump
#define SW_MODE2_PIN 11 // PB7, Move
#define SW_MODE3_PIN 12 // PD6, Camera
#define SW_MODE4_PIN 13 // PC7, System

#define SW_1_PIN A3 // PF4
#define SW_2_PIN A4 // PF1
#define SW_3_PIN A5 // PF0

#define SIG_SAMD_BUSY_PIN 15  //PB1
#define SIG_SAMD_DATA1_PIN 16 // PB2
#define SIG_SAMD_DATA2_PIN 17 // PB0

#define TIMER_INTERVAL 5 // 5[ms]ごとにスイッチを読む
// TIMER_INTERVALが5[ms]なら1200回で1秒、72000回で1分
#define TIMER_RESET_COUNT 72000 // 72000*5[ms]=60[s]ごとにカウンターをリセット

#define ENCODER_COUNT_MIN -5
#define ENCODER_COUNT_MAX 5
int8_t count1Old = 0;
uint8_t count1flag = 0;

// Systemがmode4、Jumpがmode1に対応
enum class OperationMode
{
  System, Camera, Move, Jump
};

EncoderTool::PolledEncoder enc1_;
EncoderTool::PolledEncoder enc2_;
EncoderTool::PolledEncoder enc3_;

void TimerFlash();

uint32_t timerCount_ = 0;

bool serialOutQueue_;
bool switchReadQueue_;
bool keyboardOutQueue_;

char infoMessage_[100];

enum class OperationMode mode_;

uint8_t swMode_;
uint16_t swParam_;
uint8_t encStatus_;

// D15（=PB1）、D16（=PB2）、D17（=PB0）の3ビットでSAMDマイコンにmodeを伝達
// D15がHのときにSAMDはD16・17をリード、D15がLのときは変更中と判断してSAMDはリードしない
void PinOutForSAMD(OperationMode mode)
{
  PORTB &= ~0x2;

  switch(mode)
  {
    // 上位ビットPB0、下位ビットPB2
    case OperationMode::System: // 00
      PORTB &= ~0x5;
      break;
    case OperationMode::Camera: // 01
      PORTB &= ~0x4;
      PORTB |= 0x1;
      break;
    case OperationMode::Move:   // 10
      PORTB |= 0x4;
      PORTB &= ~0x1;
      break;
    case OperationMode::Jump:   // 11
      PORTB |= 0x5;
      break;
  }

  PORTB |= 0x2;
}

void setup() {
  // put your setup code here, to run once:
  MsTimer2::set(TIMER_INTERVAL, TimerFlash);

  // 時計回りでカウンターが増加
  enc1_.begin(ENC_1B_PIN, ENC_1A_PIN);
  enc2_.begin(ENC_2B_PIN, ENC_2A_PIN);
  enc3_.begin(ENC_3B_PIN, ENC_3A_PIN);

  enc1_.setLimits(ENCODER_COUNT_MIN, ENCODER_COUNT_MAX);
  enc2_.setLimits(-5,5, true);
  enc3_.setLimits(0,10,true);

  pinMode(SIG_SAMD_BUSY_PIN, OUTPUT);
  pinMode(SIG_SAMD_DATA1_PIN, OUTPUT);
  pinMode(SIG_SAMD_DATA2_PIN, OUTPUT);

  serialOutQueue_ = false;
  switchReadQueue_ = false;
  keyboardOutQueue_ = false;
  infoMessage_[0] = '\0';

  swMode_ = 0x0; // 上位4ビットは前フレームの値、正論理
  swParam_ = 0xFFFF; // 上位8ビットは前フレームの値
  encStatus_ = 0x0; // 1エンコーダにつき2ビットで状態を表現、00:不変、01:up、10:down

  Serial.begin(115200);
  Keyboard.begin();

  mode_ = OperationMode::System;
  PinOutForSAMD(mode_);

  MsTimer2::start();
}

void loop() {
  // put your main code here, to run repeatedly:
  enc1_.tick();
  enc2_.tick();
  enc3_.tick();

  if(switchReadQueue_)
  {
    switchReadQueue_ = false;
    DoSwitchReadProcess();
  }

  // ロータリーエンコーダの処理はキューではなくloop内で直接実行
  if (enc1_.valueChanged())
  {
    int value = enc1_.getValue();
    if(count1Old > value)
    {
      //Serial.println("-");
      count1flag = 0x2;
    }
    else
    {
      //Serial.println("+");
      count1flag = 0x1;
    }
    if(value == ENCODER_COUNT_MIN || value == ENCODER_COUNT_MAX)
    {
      enc1_.setValue(0);
      count1Old = 0;
    }
    else
    {
      count1Old = value;
    }
    serialOutQueue_ = true;
  }

  if (enc1_.valueChanged() || enc2_.valueChanged() || enc3_.valueChanged())
  {
    serialOutQueue_ = true;
    keyboardOutQueue_ = true;
    DoEncoderReadProcess();
  }

  if(keyboardOutQueue_)
  {
    keyboardOutQueue_ = false;
    DoKeyboardOutProcess();
  }

  if(serialOutQueue_)
  {
    serialOutQueue_ = false;
    DoSerialOutProcess();
  }
}

void TimerFlash()
{
  // TIMER_INTERVAL=5[ms]ごとに呼ばれる
  if(timerCount_ % TIMER_INTERVAL == 0)
  {
    switchReadQueue_ = true;
  }

  timerCount_++;
  if(timerCount_ == TIMER_RESET_COUNT)
  {
    timerCount_ = 0;
  }
}

// モードスイッチとパラメータ変更スイッチの処理
// ロータリーエンコーダの押下はこの関数で処理するが、回転は別関数
void DoSwitchReadProcess()
{
  // モード変更スイッチ取得
  // swMode = MSB-LSB: 0,0,0,0, mode4,mode3,mode2,mode1
  swMode_ <<= 4;
  swMode_ |= (PINC & 0x80) >> 4; // PC7 = mode4:System
  swMode_ |= (PIND & 0x40) >> 4; // PD6 = mode3:Camera
  swMode_ |= (PINB & 0xC0) >> 6; // PB7 = mode2:Move, PB6 = mode1:Jump

  // パラメータ変更スイッチ取得
  // swParam = MSB-LSB: Enc3sw,Enc2sw,Enc1sw,sw1, 0,0,sw2,sw3
  swParam_ <<= 8;
  swParam_ |= PINF & 0xF3; // PF7-4, 1-0

  // モード変更確認
  if(swMode_ >> 4 != (swMode_ & 0xF)) // 変化時のみ反応
  {
    // モードの代入、スイッチ同時押しで複数ビットが入力されていたらMSB優先
    // SAMDボード向けにmodeを出力
    if(swMode_ & 0x8 && (swMode_ >> 4 & 0x8) == 0)
    {
      mode_ = OperationMode::System;
    }
    else if(swMode_ & 0x4 && (swMode_ >> 4 & 0x4) == 0)
    {
      mode_ = OperationMode::Camera;
    }
    else if(swMode_ & 0x2 && (swMode_ >> 4 & 0x2) == 0)
    {
      mode_ = OperationMode::Move;
    }
    else if(swMode_ & 0x1 && (swMode_ >> 4 & 0x1) == 0)
    {
      mode_ = OperationMode::Jump;
    }

    PinOutForSAMD(mode_);

    serialOutQueue_ = true;

    // モード変更はUnityに通知する必要がない
  }

  // パラメータ変更確認
  if(swParam_ >> 8 != (swParam_ & 0xFF)) // 変化時のみ反応
  {
    serialOutQueue_ = true;
    keyboardOutQueue_ = true;
  }
}

void DoEncoderReadProcess()
{
}

void DoSerialOutProcess()
{
  sprintf(infoMessage_, "%d,%3d,%3d,%3d,%3d,%d", swMode_ & 0xF, swParam_ & 0xFF, enc1_.getValue(), enc2_.getValue(), enc3_.getValue(), mode_);
  Serial.println(infoMessage_);
}

void DoKeyboardOutProcess()
{
  char* charSet = nullptr;
  switch(mode_)
  {
    case OperationMode::System:
      charSet = sendchar[0];
      break;
    case OperationMode::Camera:
      charSet = sendchar[1];
      break;
    case OperationMode::Move:
      charSet = sendchar[2];
      break;
    case OperationMode::Jump:
      charSet = sendchar[3];
      break;
  }

  // パラメータ変更スイッチの処理
  if(swParam_ >> 8 != (swParam_ & 0xFF)) // 変化時のみ反応
  {
    // swParam = MSB-LSB: Enc3sw,Enc2sw,Enc1sw,sw1, 0,0,sw2,sw3
    for(uint8_t i = 0; i < 8; i++)
    {
      if(i <= 1 || i >= 4) // 0-1ビットと4ビット以上のときに処理
      {
        if((swParam_ >> 8 & 1 << i) == 1 << i && (swParam_ & 1 << i) == 0)
        {
          char c = 0;
          switch(i)
          {
            case 0: // sw3
              c = charSet[PARAM_SW3];
              break;
            case 1: // sw2
              c = charSet[PARAM_SW2];
              break;
            case 4: // sw1
              c = charSet[PARAM_SW1];
              break;
            case 5: // Enc1sw
              c = charSet[ENC1SW];
              break;
            case 6: // Enc2sw
              c = charSet[ENC2SW];
              break;
            case 7: // Enc3sw
              c = charSet[ENC3SW];
              break;
          }
          Keyboard.write(c);
        }
      }
    }
  }

  // ロータリーエンコーダの処理
  if(count1flag == 0x1)
  {
    char c = charSet[ENC1UP];
    Keyboard.write(c);
  }
  else if(count1flag == 0x2)
  {
    char c = charSet[ENC1DN];
    Keyboard.write(c);
  }
}
