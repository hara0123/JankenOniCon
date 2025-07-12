// じゃんけん鬼パラメータ変更装置用スケッチ

#include <EncoderTool.h>
#include <Keyboard.h>
#include <MsTimer2.h>

#include "ToJankenOniData.h"
#include "EncoderToolAssistant.h"

#define SERIAL_OUT_ENABLE 0 // 0 or 1、0にするとシリアル出力を無効にする（beginも実行しない）

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

#define TIMER_INTERVAL 5 // **[ms]ごとにスイッチを読む
// TIMER_INTERVALが5[ms]なら1200回で1秒、72000回で1分、あ、コメント違うかも、後で直す
#define TIMER_RESET_COUNT 72000

uint8_t count1flag = 0;

// Systemがmode4、Jumpがmode1に対応
enum class OperationMode
{
  Camera, Move, Dash, Jump
};

EncoderTool::PolledEncoder enc1_;
EncoderTool::PolledEncoder enc2_;
EncoderTool::PolledEncoder enc3_;

EncoderToolAssistant encAssistant[3] = {enc1_, enc2_, enc3_};

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
    case OperationMode::Camera: // 00
      PORTB &= ~0x5;
      break;
    case OperationMode::Move: // 01
      PORTB &= ~0x4;
      PORTB |= 0x1;
      break;
    case OperationMode::Dash:   // 10
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

  if(SERIAL_OUT_ENABLE)
  {
    Serial.begin(115200);
    delay(100); // 安定するまでちょっと待ちたい
  }

  mode_ = OperationMode::Camera;
  PinOutForSAMD(mode_);

  Keyboard.begin();

  MsTimer2::start();
}

void loop() {
  // put your main code here, to run repeatedly:
  if(switchReadQueue_)
  {
    switchReadQueue_ = false;
    DoSwitchReadProcess();
  }

  // ロータリーエンコーダの処理はキューではなくloop内で直接実行
  DoEncoderReadProcess();

  // キーボード出力処理は入力処理の後ろで呼び出す
  if(keyboardOutQueue_)
  {
    keyboardOutQueue_ = false;
    DoKeyboardOutProcess();
  }

  // シリアルモニターでの確認が不要であればコメントアウト
  if(SERIAL_OUT_ENABLE && serialOutQueue_)
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
      mode_ = OperationMode::Camera;
    }
    else if(swMode_ & 0x4 && (swMode_ >> 4 & 0x4) == 0)
    {
      mode_ = OperationMode::Move;
    }
    else if(swMode_ & 0x2 && (swMode_ >> 4 & 0x2) == 0)
    {
      mode_ = OperationMode::Dash;
    }
    else if(swMode_ & 0x1 && (swMode_ >> 4 & 0x1) == 0)
    {
      mode_ = OperationMode::Jump;
    }

    PinOutForSAMD(mode_);

    serialOutQueue_ = true;

    // モード変更はキーボード出力する必要がない
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
  enc1_.tick();
  enc2_.tick();
  enc3_.tick();

  // encStatus_ = MSB-LSB: 0,0,Enc3b1,Enc3b0,Enc2b1,Enc2b0,Enc1b1,Enc1b0
  // 1エンコーダにつき2ビットで状態を表現、00:不変、01:up、10:down

  for(int i = 0; i < 3; i++)
  {
    int8_t status = encAssistant[i].GetStatus();
    encStatus_ &= ~(0x3 << 2 * i);
    if(status == -1)
    {
      encStatus_ |= 0x2 << 2 * i;
      keyboardOutQueue_ = true; // 上書き
      serialOutQueue_ = true;
    }
    else if(status == 1)
    {
      encStatus_ |= 0x1 << 2 * i;
      keyboardOutQueue_ = true; // 上書き
      serialOutQueue_ = true;
    }
  }
}

void DoSerialOutProcess()
{
  sprintf(infoMessage_, "%d,%3d,%3d,%3d,%3d,%d", swMode_ & 0xF, swParam_ & 0xFF, enc1_.getValue(), enc2_.getValue(), enc3_.getValue(), mode_);
  Serial.println(infoMessage_);
}

void DoKeyboardOutProcess()
{
  // 文字セットの選択
  char* charSet = nullptr;
  uint8_t* modifierKeySet = nullptr;
  switch(mode_)
  {
    case OperationMode::Camera:
      charSet = sendChar[0];
      modifierKeySet = sendModifierKey[0];
      break;
    case OperationMode::Move:
      charSet = sendChar[1];
      modifierKeySet = sendModifierKey[1];
      break;
    case OperationMode::Dash:
      charSet = sendChar[2];
      modifierKeySet = sendModifierKey[2];
      break;
    case OperationMode::Jump:
      charSet = sendChar[3];
      modifierKeySet = sendModifierKey[3];
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
          uint8_t modifier = 0;
          switch(i)
          {
            case 0: // sw3
              c = charSet[PARAM_SW3];
              modifier = modifierKeySet[PARAM_SW3];
              break;
            case 1: // sw2
              c = charSet[PARAM_SW2];
              modifier = modifierKeySet[PARAM_SW2];
              break;
            case 4: // sw1
              c = charSet[PARAM_SW1];
              modifier = modifierKeySet[PARAM_SW1];
              break;
            case 5: // Enc1sw
              c = charSet[ENC1SW];
              modifier = modifierKeySet[ENC1SW];
              break;
            case 6: // Enc2sw
              c = charSet[ENC2SW];
              modifier = modifierKeySet[ENC2SW];
              break;
            case 7: // Enc3sw
              c = charSet[ENC3SW];
              modifier = modifierKeySet[ENC3SW];
              break;
          }
          if(modifier & MODIFIER_BIT_S){
            Keyboard.press(KEY_LEFT_SHIFT);
          }
          if(modifier & MODIFIER_BIT_C){
            Keyboard.press(KEY_LEFT_CTRL);
          }
          if(modifier & MODIFIER_BIT_A){
            Keyboard.press(KEY_LEFT_ALT);
          }
          Keyboard.write(c);
          delay(10);
          if(modifier & MODIFIER_BIT_S || modifier & MODIFIER_BIT_C || modifier & MODIFIER_BIT_A){
            Keyboard.releaseAll();
          }
        }
      }
    }
  }

  // ロータリーエンコーダの処理
  // encStatus_ = MSB-LSB: 0,0,Enc3b1,Enc3b0,Enc2b1,Enc2b0,Enc1b1,Enc1b0
  // 1エンコーダにつき2ビットで状態を表現、00:不変、01:up、10:down
  for(uint8_t i = 0; i < 3; i++)
  {
    uint8_t status = encStatus_ >> 2 * i & 0x3;
    // エンコーダー文字のピックアップ
    char cUp, cDown;
    uint8_t modifierUp, modifierDown;
    switch(i)
    {
      case 0:
        cUp = charSet[ENC1UP];
        cDown = charSet[ENC1DN];
        modifierUp = modifierKeySet[ENC1UP];
        modifierDown = modifierKeySet[ENC1DN];
        break;
      case 1:
        cUp = charSet[ENC2UP];
        cDown = charSet[ENC2DN];
        modifierUp = modifierKeySet[ENC2UP];
        modifierDown = modifierKeySet[ENC2DN];
        break;
      case 2:
        cUp = charSet[ENC3UP];
        cDown = charSet[ENC3DN];
        modifierUp = modifierKeySet[ENC3UP];
        modifierDown = modifierKeySet[ENC3DN];
        break;
    }

    if(status == 0x1)
    {
      if(modifierUp & MODIFIER_BIT_S){
        Keyboard.press(KEY_LEFT_SHIFT);
      }
      if(modifierUp & MODIFIER_BIT_C){
        Keyboard.press(KEY_LEFT_CTRL);
      }
      if(modifierUp & MODIFIER_BIT_A){
        Keyboard.press(KEY_LEFT_ALT);
      }
      Keyboard.write(cUp);
      delay(10);
      if(modifierUp & MODIFIER_BIT_S || modifierUp & MODIFIER_BIT_C || modifierUp & MODIFIER_BIT_A){
        Keyboard.releaseAll();
      }
    }
    else if(status == 0x2)
    {
      if(modifierDown & MODIFIER_BIT_S){
        Keyboard.press(KEY_LEFT_SHIFT);
      }
      if(modifierDown & MODIFIER_BIT_C){
        Keyboard.press(KEY_LEFT_CTRL);
      }
      if(modifierDown & MODIFIER_BIT_A){
        Keyboard.press(KEY_LEFT_ALT);
      }
      Keyboard.write(cDown);
      delay(10);
      if(modifierDown & MODIFIER_BIT_S || modifierDown & MODIFIER_BIT_C || modifierDown & MODIFIER_BIT_A){
        Keyboard.releaseAll();
      }
    }
  }
}
