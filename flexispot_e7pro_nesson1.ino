#include <M5Unified.h>

// D1 (GPIO7) will be TX1 -> No.6(TX) of e2pro 
// D2 (GPIO2) will be RX1 -> No.5(RX) of e2pro
// D3 (GPI06) will be No.4(DisplayPin20) of e2pro
// see https://github.com/iMicknl/LoctekMotion_IoT/tree/main

char lastResult[6] = "";   // 直前に表示した値（"1.95" など）を保存

// Supported Commands
const byte cmd_wakeup[] = {0x9b, 0x06, 0x02, 0x00, 0x00, 0x6c, 0xa1, 0x9d};
const byte cmd_up[] = {0x9b, 0x06, 0x02, 0x01, 0x00, 0xfc, 0xa0, 0x9d};
const byte cmd_dwn[] = {0x9b, 0x06, 0x02, 0x02, 0x00, 0x0c, 0xa0, 0x9d};
const byte cmd_pre1[] = {0x9b, 0x06, 0x02, 0x04, 0x00, 0xac, 0xa3, 0x9d};
const byte cmd_pre2[] = {0x9b, 0x06, 0x02, 0x08, 0x00, 0xac, 0xa6, 0x9d};
const byte cmd_pre3[] = {0x9b, 0x06, 0x02, 0x10, 0x00, 0xac, 0xac, 0x9d}; // stand
const byte cmd_mem[] = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};
const byte cmd_pre4[] = {0x9b, 0x06, 0x02, 0x00, 0x01, 0xac, 0x60, 0x9d}; // sit
const byte cmd_alarmoff[] = {0x9b, 0x06, 0x02, 0x40, 0x00, 0xaC, 0x90, 0x9d};
const byte cmd_childlock[] = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};

// Status Protocol
// 0 byte 0x9b: Start Command
const uint8_t START_BYTE  = 0x9B;  // フレーム開始
// 1 byte 0xXX: Command Length
// 2 byte 0x12: Height Stream 
const uint8_t CMD_DISPLAY = 0x12;  // 7セグ表示, 以降は高さ表示のフォーマット
const uint8_t CMD_SLEEP = 0x13;  // スリープ
// 3 byte 0xXX: First Digit (8 Segment) / Reverse Bit
// 4 byte 0xXX: Second Digit (8 Segment) / Reverse Bit
// 5 byte 0xXX: Third Digit (8 Segment) / Reverse Bit
// 6 byte 0xXX: 16bit Modbus-CRC16 Checksum 1
// 7 byte 0xXX: 16bit Modbus-CRC16 Checksum 2
// 8 byte 0x9b: End of Command

enum RecvState {
  WAIT_START,
  WAIT_LEN,
  WAIT_PAYLOAD
};

RecvState state = WAIT_START;

uint8_t frameLen = 0;
const uint8_t MAX_FRAME_LEN = 32;  // 必要に応じて増やす
uint8_t frameBuf[MAX_FRAME_LEN];
uint8_t frameIndex = 0;

//////////////
// Commands //
//////////////
void turnon() {
  // Turn desk in operating mode by setting controller DisplayPin20 to HIGH
  Serial.println("Sending Turn on Command");
  digitalWrite(D3, HIGH);
  delay(1000);
  digitalWrite(D3, LOW);
}

void wake() {
  turnon();

  Serial.println("Sending Wakeup Command");
  Serial1.flush();
  Serial1.write(cmd_wakeup, sizeof(cmd_wakeup));
}

void memory() {
  turnon();

  Serial.println("Sending Memory Command");
  Serial1.flush();
  Serial1.write(cmd_mem, sizeof(cmd_mem));
}

void pre1() {
  turnon();

  Serial.println("Sending Preset 1 Command");
  Serial1.flush();
  Serial1.write(cmd_pre1, sizeof(cmd_pre1));
}

void pre2() {
  turnon();

  Serial.println("Sending Preset 2 Command");
  Serial1.flush();
  Serial1.write(cmd_pre2, sizeof(cmd_pre2));
}

void pre3() {
  turnon();

  Serial.println("Sending Preset 3 Command");
  Serial1.flush();
  Serial1.write(cmd_pre3, sizeof(cmd_pre3));
}

void pre4() {
  turnon();

  Serial.println("Sending Preset 4 Command");
  Serial1.flush();
  Serial1.write(cmd_pre4, sizeof(cmd_pre4));
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);  // 横画面モードに設定

  // テキストのプロパティ設定
  M5.Display.setTextDatum(MC_DATUM);              // テキスト配置の中央基準
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);  // 白文字、黒背景
  M5.Display.setTextSize(2); // 大きさ2倍

  // 画面をクリアしてテキストを描画
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.drawString("Flexispot Controller", M5.Display.width() / 2, M5.Display.height() / 2);

  // デバッグシリアル設定
  Serial.begin(115200);
  Serial.println("Serial Ready.");

  // e2pro向けシリアル
  // Initialize Serial1 on D1 and D2
  // Format: Serial1.begin(baudrate, config, rxPin, txPin);
  Serial1.begin(9600, SERIAL_8N1, D2, D1);
  Serial.println("Serial1 Ready.");

  // DisplayPin20 configured as an output
  pinMode(D3, OUTPUT);
  digitalWrite(D3, LOW);

}

void loop() {
  // ボタン処理
  M5.update();
  if (M5.BtnA.wasPressed()) {
    Serial.println("A Btn Pressed");
    wake();
  }
  if (M5.BtnA.wasReleased()) {
    Serial.println("A Btn Released");
  }
  if (M5.BtnB.wasPressed()) {
    Serial.println("B Btn Pressed");
    //memory();
    pre4();
  }
  if (M5.BtnB.wasReleased()) {
    Serial.println("B Btn Released");
  }

  while (Serial1.available() > 0) {
    uint8_t b = (uint8_t)Serial1.read();

    switch (state) {
      case WAIT_START:
        if (b == START_BYTE) {
          state = WAIT_LEN;
          // デバッグ用
          //Serial.println(F("START_BYTE 受信"));
        }
        break;

      case WAIT_LEN:
        frameLen = b;
        if (frameLen == 0 || frameLen > MAX_FRAME_LEN) {
          Serial.println(F("不正な長さ"));
          resetState();
        } else {
          frameIndex = 0;
          state = WAIT_PAYLOAD;
          // Serial.print(F("LEN=")); Serial.println(frameLen);
        }
        break;

      case WAIT_PAYLOAD:
        frameBuf[frameIndex++] = b;
        if (frameIndex >= frameLen) {
          // フレームを処理
          handleFrame();
          // 次のフレームに備えて状態を戻す
          resetState();
        }
        break;
    }
  }
}

// 8bit のビット反転関数（b7..b0 -> b0..b7）
uint8_t reverseBits(uint8_t x) {
  x = (x & 0xF0) >> 4 | (x & 0x0F) << 4;
  x = (x & 0xCC) >> 2 | (x & 0x33) << 2;
  x = (x & 0xAA) >> 1 | (x & 0x55) << 1;
  return x;
}

// 7セグ＋ドットのパターンを数字に変換
// raw : 受信した生の 8bit データ（例：0x86）
// dot : その桁にドットが点灯しているかどうかを返す
char segToDigit(uint8_t raw, bool &dot) {
  // まずビット列を反転
  uint8_t r = reverseBits(raw);

  // 反転後の LSB(ビット0) をドットとみなす
  dot = (r & 0x01) != 0;

  // ドットビットを落として、数字部分だけにする
  uint8_t pat = r & 0xFE;

  // 標準的な7セグのパターン（ビット反転後）との対応表
  // 0:0xFC, 1:0x60, 2:0xDA, 3:0xF2, 4:0x66,
  // 5:0xB6, 6:0xBE, 7:0xE0, 8:0xFE, 9:0xF6
//  Serial.print(pat);
//  Serial.print(" ");
  switch (pat) {
    case 0xFC: return '0';
    case 0x60: return '1';
    case 0xDA: return '2';
    case 0xF2: return '3';
    case 0x66: return '4';
    case 0xB6: return '5';
    case 0xBE: return '6';
    case 0xE0: return '7';
    case 0xFE: return '8';
    case 0xF6: return '9';
    case 0x00: return ' '; // 非表示化のデータ。
    case 0x02: return '-'; // ハイフン
    case 0x1C: return 'L'; // L
    case 0x3A: return 'o'; // o
    case 0x9C: return 'C'; // C
    default:   return '?';  // 不明なパターン
  }
}

// 受信した1フレームを処理
void handleFrame() {
  // 最低でも [CMD(=0x12), seg1, seg2, seg3] の4バイト必要
  if (frameLen < 4) {
    Serial.println(F("フレーム長が足りません"));
    return;
  }

  uint8_t cmd = frameBuf[0];

  if (cmd == CMD_DISPLAY) {
    // 2,3,4バイト目を7セグデータとして処理
    uint8_t segBytes[3] = {
      frameBuf[1],
      frameBuf[2],
      frameBuf[3]
    };

    char digits[3];
    bool dots[3];

    for (int i = 0; i < 3; i++) {
      digits[i] = segToDigit(segBytes[i], dots[i]);
    }

    // 出力用バッファ（最大 "9.99" で5文字＋終端）
    char result[6];
    int pos = 0;

    for (int i = 0; i < 3; i++) {
      result[pos++] = digits[i];
      if (dots[i]) {
        result[pos++] = '.';
      }
    }
    result[pos] = '\0';

    Serial.print(F("表示データ: "));
    Serial.println(result);  // 例: "1.95"

    // ★ 前回表示と同じなら何もしない（点滅防止）
    if (strcmp(result, lastResult) != 0) {
      // ★ 新しい値を保存
      strncpy(lastResult, result, sizeof(lastResult));
      lastResult[sizeof(lastResult) - 1] = '\0';

      // ★ 画面更新（fillScreen はやめる）
      //   背景色付きで drawString しているので、文字の周囲は上書きされます
      M5.Display.setTextDatum(MC_DATUM);
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextFont(7); // 48ピクセル7セグ風フォント

      if(strcmp(result, "   ") == 0) {
        M5.Display.fillScreen(TFT_BLACK);
        Serial.println("Clear Display");
      } else {
        M5.Display.drawString(
          lastResult,
          M5.Display.width() / 2,
          M5.Display.height() / 2
        );
      }
    }
  } else if (cmd == CMD_SLEEP) {
    // 必ずこのパケットが飛ぶわけではなさそう
//    M5.Display.fillScreen(TFT_BLACK);
      Serial.println("Clear Display");
  } else {
    // 今回は CMD_DISPLAY 以外は無視（必要なら他コマンドも追加）
    //Serial.print(F("未知のコマンド: 0x"));
    //Serial.println(cmd, HEX);
        // CMD_DISPLAY 以外のコマンド → frameBuf をダンプ表示
    Serial.print("Unknown CMD: 0x");
    Serial.println(cmd, HEX);

    Serial.print("Raw frame: ");
    for (int i = 0; i < frameLen; i++) {
        if (frameBuf[i] < 16) Serial.print('0'); // 1桁のとき 0 を付ける
        Serial.print(frameBuf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
  }
}

void resetState() {
  state = WAIT_START;
  frameLen = 0;
  frameIndex = 0;
}


