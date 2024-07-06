#include <Arduino.h>
#include <M5StickCPlus2.h>
#include <SPIFFS.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorMP3.h>
#include <BLEDevice.h>
#include <Adafruit_NeoPixel.h>

// 調整の余地のあるもの
const float AUDIO_VOLUME = 1.0;             // 効果音の音量　0.0~1.0
const float ACCEL_THRESHOLD = 2;            // 加速度のしきい値 通常モード
const float ACCEL_THRESHOLD_ULTIMATE = 1.5; // 加速度のしきい値 必殺技モード
// const float ACCEL_THRESHOLD = 3.5;          // 加速度のしきい値 通常モード
// const float ACCEL_THRESHOLD_ULTIMATE = 2.5; // 加速度のしきい値 必殺技モード

// LEDの出力ピンの定義
const int DIN_PIN = 32; // D1

// タスクハンドルの定義 LED発光が実行中か否か
TaskHandle_t myTaskHandle = NULL; // タスクハンドルの初期化

// モード管理関連の定義
const int SLEEP_MODE = 0;
const int FIGHT_MODE = 1;
const int ULTIMATE_MODE = 2;
const int MODES[] = {SLEEP_MODE, FIGHT_MODE, ULTIMATE_MODE};
int mode;
int modeOld;

// Audio関連の定義
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSPIFFS *file = nullptr;
AudioOutputI2S *out = nullptr;

// I2S接続ピンの定義
const int I2S_DOUT = 25;
const int I2S_BCLK = 26;
const int I2S_LRC = 0;

// センサー関連の定義
float ax, ay, az;
const float gravity = 0.5;

// 音声再生関連の変数
unsigned long playStartTime = 0;
const unsigned long PLAY_DURATION_THRESHOLD = 500; // 0.5秒

// 動き検出関連の変数
unsigned long lastMovementTime = 0;
const unsigned long MOVEMENT_COOLDOWN = 100; // 動きの検出間隔（ミリ秒）

/* サーバー:erviceのUUIDを指定 */
static BLEUUID serviceUUID("d352f8dc-570a-476f-b028-abb69100d5b6");
/* サーバー:CharacteristicのUUIDを指定 */
static BLEUUID charUUID("0ca975af-ebe9-4505-a795-c0740072abbe");

// BLE通信の判定に関する変数
static boolean doConnect = false;
static boolean doScan = false;
static boolean led = false;
static boolean connected = false;
static boolean oldDeviceConnected = false;

static BLERemoteCharacteristic *pRemoteCharacteristic; // 接続したサーバーのCharacteristic
static BLEAdvertisedDevice *myDevice;                  // 見つかったサーバー

const String check_addr = "70:04:1d:d4:d1:d1"; // サーバ側のMACアドレス

// LED
const int LED_COUNT = 32; // LEDの数
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);

// 色の定義
uint32_t none = pixels.Color(0, 0, 0);    // 消灯
uint32_t wait = pixels.Color(34, 38, 42); // 待機状態

uint32_t red = pixels.Color(51, 0, 0);
uint32_t orange = pixels.Color(51, 25, 0);
uint32_t yellow = pixels.Color(51, 51, 0);
uint32_t green = pixels.Color(0, 51, 0);
uint32_t cyan = pixels.Color(0, 51, 51);
uint32_t blue = pixels.Color(0, 0, 51);
uint32_t indigo = pixels.Color(15, 0, 26);
uint32_t violet = pixels.Color(28, 0, 51);
uint32_t pink = pixels.Color(51, 38, 41);
uint32_t brown = pixels.Color(33, 8, 8);
uint32_t gray = pixels.Color(25, 25, 25);
uint32_t gold = pixels.Color(51, 43, 0);
uint32_t silver = pixels.Color(38, 38, 38);
uint32_t beige = pixels.Color(49, 49, 44);
uint32_t lime = pixels.Color(0, 51, 0);

uint32_t lightPurple = pixels.Color(16, 10, 32);
uint32_t white = pixels.Color(85, 85, 85);
uint32_t veryLightCyan = pixels.Color(76, 80, 85);
uint32_t lightCyan = pixels.Color(68, 76, 85);
uint32_t lighterBlue = pixels.Color(59, 72, 85);
uint32_t lightBlue = pixels.Color(51, 68, 85);
uint32_t paleSkyBlue = pixels.Color(42, 63, 85);
uint32_t skyBlue = pixels.Color(34, 59, 85);
uint32_t mediumSkyBlue = pixels.Color(25, 55, 85);
uint32_t mediumBlue = pixels.Color(17, 51, 85);
uint32_t cobaltBlue = pixels.Color(8, 46, 85);
uint32_t royalBlue = pixels.Color(0, 42, 85);
uint32_t oceanBlue = pixels.Color(0, 38, 85);
uint32_t strongBlue = pixels.Color(0, 34, 85);
uint32_t bluee = pixels.Color(0, 30, 85);
uint32_t azureBlue = pixels.Color(0, 25, 85);
uint32_t navyBlue = pixels.Color(0, 21, 85);
uint32_t darkBlue = pixels.Color(0, 17, 85);
uint32_t veryDarkBlue = pixels.Color(0, 12, 85);
uint32_t deepBlue = pixels.Color(0, 8, 85);
uint32_t trueBlue = pixels.Color(0, 0, 85);

int LEDs[LED_COUNT]; // LED設定を個々に割り当てるための配列
// 白から青までの色の配列
uint32_t colors[] = {white, veryLightCyan, lightCyan, lighterBlue, lightBlue, paleSkyBlue, skyBlue,
                     mediumSkyBlue, mediumBlue, cobaltBlue, royalBlue, oceanBlue, strongBlue, bluee, azureBlue,
                     navyBlue, darkBlue, veryDarkBlue, deepBlue, trueBlue};

// 典型的な色の配列
uint32_t rainbow[] = {
    red,
    orange,
    yellow,
    green,
    cyan,
    blue,
    indigo,
    violet,
    pink,
    brown,
    gray,
    gold,
    silver,
    beige,
    lime};

u_int32_t rainbow_index[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
// 白っぽい色の配列
uint32_t blues[] = {white, veryLightCyan, lightCyan, lighterBlue, lightBlue, paleSkyBlue, skyBlue,
                    mediumSkyBlue, mediumBlue};
// 各配列の要素数取得
int color_index[] = {19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
u_int32_t colors_index[] = {0, 1, 2, 3, 4, 5};
int blue_index[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

// notify時のコールバック
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                           uint8_t *pData, size_t length, bool isNotify)
{
    // 接続しようとしているBLEデバイスのアドレスをシリアルモニタに表示
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());

    // データの長さをシリアルモニタに出力
    Serial.print(" of data length ");
    Serial.println(length);

    // 受信したデータを文字列に変換
    String receivedData = String((char *)pData, length);

    // データの内容をシリアルモニタに出力
    Serial.print("data: ");
    Serial.println(receivedData);

    // 受け取った値を整数に変換
    int value = receivedData.toInt();

    Serial.println("Value: " + String(value));

    switch (value)
    {
    case 0:
        Serial.println("Value is 0.");
        mode = SLEEP_MODE;
        break;
    case 1:
        Serial.println("Value is 1.");
        mode = FIGHT_MODE;
        break;
    case 2:
        Serial.println("Value is 2.");
        mode = ULTIMATE_MODE;
        break;
    default:
        Serial.println("Value is ?????.");
        mode = SLEEP_MODE;
    }
}

// クライアントコールバック
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        connected = true;
        Serial.println("Connected");
        // 起動時
        for (int i = LED_COUNT - 1; i >= 0; i--)
        {
            pixels.clear();
            for (int j = 0; j < 20; j++)
            {
                color_index[j] = i + 19 - j;
                pixels.setPixelColor(color_index[j], colors[j]);
            }
            pixels.show();
            delay(20);
        }
        Serial.println("ここから消灯");

        // XXX: これなんやろ？

        for (int i = LED_COUNT - 1; i >= 0; i--)
        {
            pixels.clear();
            pixels.setPixelColor(i, none);
            pixels.show();
        }
        Serial.println("光終了");
    }
    void onDisconnect(BLEClient *pclient)
    {
        Serial.println("Disconnected from server");

        // TODO: LEDを光らせる
        // コネクトにタスクの存在確認してあったら、タスクを削除する処理書いて、ここでalertLED()を引数にしてタスク実行すればいいかも？
        // 07/06 LED切れた時

        if (myTaskHandle != NULL || eTaskGetState(myTaskHandle) != eDeleted)
        {
            vTaskDelete(myTaskHandle);

        }
        for (int i = 0; i < LED_COUNT; i++)
        {
            pixels.clear();
            for (int j = 0; j < 20; j++)
            {
                color_index[j] = i + 19 - j;
                pixels.setPixelColor(color_index[j], colors[j]);
            }
            pixels.show();
            delay(20);
        }
        Serial.println("Disconnected from server");

        // LED消灯
        for (int i = LED_COUNT - 1; i >= 0; i--)
        {
            pixels.clear();
            pixels.setPixelColor(i, none);
            pixels.show();
        }
        connected = false;
        doScan = true;
        Serial.println("Disconnected from server");
    }
};

// 接続
bool connectToServer()
{
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient *pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    /* Connect to the remote BLE Server */
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    /* Obtain a reference to the service we are after in the remote BLE server */
    BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our service");

    /* Obtain a reference to the characteristic in the service of the remote BLE server */
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our characteristic");

    /* Read the value of the characteristic */
    /* Initial value is 'Hello, World!' */
    if (pRemoteCharacteristic->canRead())
    {
        std::string value = pRemoteCharacteristic->readValue();
        Serial.print("The characteristic value was: ");
        Serial.println(value.c_str());
    }
    if (pRemoteCharacteristic->canNotify())
    {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    return true;
}

// BLEアドバイス検索
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /* Called for each advertising BLE server. */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        String find_addr = advertisedDevice.toString().c_str();

        Serial.println(advertisedDevice.toString().c_str());

        if (find_addr.indexOf(check_addr) >= 0)
        {
            Serial.println("Mac Found");
            /* 見付かった */
            Serial.println("found");

            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);

            doConnect = true;
            delay(1000);

            doScan = false;
        }
    }
};

// 関数ポインタの型定義
typedef void (*TaskFunction)();

// 関数プロトタイプ宣言
void stopAction();
void playAction(const char *filename, const char *message, TaskFunction func);
void fightSetup();
void defaultFightMode();
void ultimateMode();
void bleLedInit();
void bleLoopTask(void *pvParameters);
void alertLED();
void horizontalCutLED();
void heightCutLED();
void ultimateChargeLED();
void ultimateReleaseLED();

void setup()
{
    M5.begin();
    Serial.begin(115200);
    M5.Lcd.setRotation(0);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextFont(2);
    M5.Lcd.println("Initializing...");

    // BLEの初期化
    BLEDevice::init("ESP32-BLE-Client");
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    Serial.println("Setup done");
    delay(3000);

    BLEDevice::getScan()->start(0);

    // モードをセットアップ
    mode = SLEEP_MODE;
    modeOld = mode;

    // LED初期化
    bleLedInit();
    // xTaskCreatePinnedToCore(bleLoopTask, "bleLoopTask", 8092, NULL, 10, NULL, 0);
}

// メインループ
void loop()
{
    M5.update();

    if (doConnect == true)
    {
        Serial.println("コネクトサーバーの前");
        if (connectToServer())
        {
            Serial.println("We are now connected to the BLE Server.");
        }
        else
        {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
        }
        doConnect = false;
    }
    if (connected)
    {
        // 状態を取得し、どの処理を行うか判定する
        // TODO : モードの取得
        // ここ多分いらない、notificationで値が変更されるであろう
        if (M5.BtnA.isPressed())
        {
            Serial1.println("FIGHT_MODE");
            mode = FIGHT_MODE;
        }
        if (M5.BtnB.isPressed())
        {
            Serial1.println("ULTIMATE_MODE");
            mode = ULTIMATE_MODE;
        }
        if (M5.BtnC.isPressed())
        {
            Serial1.println("SLEEP_MODE");
            mode = SLEEP_MODE;
        }
    }
    else if (doScan)
    {
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }

    // モードを実行
    switch (mode)
    {
    case SLEEP_MODE:
        if (modeOld != SLEEP_MODE)
        {
            // TODO: スリープモードのセットアップ
            Serial.println("Sleep Mode\n");
            modeOld = SLEEP_MODE;
            if (myTaskHandle != NULL || eTaskGetState(myTaskHandle) != eDeleted)
            {
                vTaskDelete(myTaskHandle);
            }
            // LED消灯
            for (int i = LED_COUNT - 1; i >= 0; i--)
            {
                pixels.clear();
                pixels.setPixelColor(i, none);
                pixels.show();
            }
        }
        // TODO: スリープモードの実行
        break;
    case FIGHT_MODE:
        if (modeOld == SLEEP_MODE)
        {
            Serial.println("Fight Mode\n");
            fightSetup();
            modeOld = FIGHT_MODE;
        }
        defaultFightMode();
        break;
    case ULTIMATE_MODE:
        if (modeOld == SLEEP_MODE)
        {
            Serial.println("Ultimate Mode\n");
            fightSetup();
            modeOld = ULTIMATE_MODE;
        }
        ultimateMode();
        break;
    }
}
void bleLedInit()
{
    // XXX: ピンを確かめる
    pinMode(46, OUTPUT);
    digitalWrite(46, HIGH);
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    pixels.begin();

    Serial.println("Starting Arduino BLE Client application...");
    BLEDevice::init("ESP32-BLE-Client");

    // LED配列を初期化
    for (int i = 0; i < LED_COUNT; i++)
    {
        LEDs[i] = i + 1;
    }

    /* Retrieve a Scanner and set the callback we want to use to be informed when we
       have detected a new device.  Specify that we want active scanning and start the
       scan to run for 5 seconds. */
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    BLEDevice::getScan()->start(0);
}

void bleLoopTask(void *pvParameters)
{
    void (*func)() = (void (*)())pvParameters;
    while (1)
    {
        func();
        // TODO: ループの実行いつまでするかの監視
        // 多分タスクが終わり命令出てるか把握する感覚、わからん
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void alertLED()
{
    // 起動中
    for (int j = 0; j < LED_COUNT; j++)
    {
        pixels.setPixelColor(j, wait);
        pixels.show();
        delay(5);
    }
    delay(5000);
}

void activeLED()
{
    while (1)
    {
        for (int i = LED_COUNT - 1; 0 <= i; i--)
        {
            pixels.clear();
            pixels.setPixelColor(i, lightBlue);
            pixels.show();
            delay(50);
            pixels.setPixelColor(i, none);
            pixels.show();
        }
        delay(500);
    }
}
// 横に切る
void horizontalCutLED()
{
    // TODO: 横に切る光り方
    // ゆうくんが帰り際言ってたやつ
    // ランダムな場所から12個、ランダムに選ばれた色（一種類）でLEDを点灯
    // アナログ入力ピンから読み取ったノイズを使って乱数シードを設定
    int led_random = esp_random() % 26;
    int light_random = esp_random() % 15;

    for (int j = led_random; 0 <= j; j--)
    {
        pixels.clear();
        for (int i = 0; i < 15; i++)
        {
            rainbow_index[i] = j + 14 - i;
            pixels.setPixelColor(rainbow_index[i], rainbow[light_random]);
        }
        pixels.setPixelColor(j, none);
        pixels.show();
        delay(20);
    }
}

// 縦に切る
void heightCutLED()
{
    // TODO: 縦に切る光り方
    // 縦降りの処理
    while (true)
    {
        for (int i = LED_COUNT - 1; i >= 0; i--)
        {
            pixels.clear();
            for (int j = 0; j < 6; j++)
            {
                colors_index[j] = i + 5 - j;
                pixels.setPixelColor(colors_index[j], colors[j]);
            }
            pixels.show();
            delay(20);
        }
    }
}

// 必殺技のチャージ
void ultimateChargeLED()
{
    // 必殺待機
    for (int i = 0; i < 10; i++)
    {
        for (int i = 0; i < LED_COUNT; i++)
        {
            pixels.clear();
            for (int j = 0; j < 9; j++)
            {
                blue_index[j] = i + 8 - j;
                pixels.setPixelColor(blue_index[j], blues[j]);
            }
            pixels.show();
            delay(10);
        }
    }
}

// 必殺技の解放
void ultimateReleaseLED()
{
    // ランダムに点滅

    while (1)
    {
        pixels.clear();
        for (int j = 0; j < 20; j++)
        {
            if (j % 2 == 0)
            {
                pixels.setPixelColor((esp_random() % LED_COUNT), lightPurple);
            }
            else
            {
                pixels.setPixelColor((esp_random() % LED_COUNT), lighterBlue);
            }
        }
        pixels.show();
        delay(20);
    }
}

// アクション実行
// 引数に再生するファイルの名前とメッセージ、実行する関数LEDパターンの関数を渡す

void playAction(const char *filename, const char *message, TaskFunction func)
{
    // タスクがあれば停止
    if (myTaskHandle != NULL || eTaskGetState(myTaskHandle) != eDeleted)
    {
        vTaskDelete(myTaskHandle);

    }
    // MP3があれば停止
    if (mp3)
    {
        mp3->stop();
        delete mp3;
        delete file;
        mp3 = nullptr;
        file = nullptr;
    }
    delay(10);

    // MP3を開始
    file = new AudioFileSourceSPIFFS(filename);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(file, out);

    // タスクを開始
    xTaskCreate(bleLoopTask, "bleLoopTask", 2048, (void *)func, 1, &myTaskHandle);

    M5.Lcd.println(message);
    playStartTime = millis();
}

// MP3を停止し、ファイル・オブジェクトを破棄
void stopAction()
{
    // MP3があれば停止
    if (mp3)
    {
        mp3->stop();
        delete mp3;
        delete file;
        mp3 = nullptr;
        file = nullptr;
    }
    // タスクがあれば停止
    if (myTaskHandle != NULL || eTaskGetState(myTaskHandle) != eDeleted)
    {
        vTaskDelete(myTaskHandle);
        // LED消灯
        for (int i = LED_COUNT - 1; i >= 0; i--)
        {
            pixels.clear();
            pixels.setPixelColor(i, none);
            pixels.show();
            // delay(1);
        }
        xTaskCreate(bleLoopTask, "bleLoopTask", 2048, (void *)activeLED, 1, &myTaskHandle);
    }
}

// 戦闘モードのセットアップ
void fightSetup()
{
    if (!SPIFFS.begin())
    {
        M5.Lcd.println("SPIFFS Mount Failed");
        return;
    }

    out = new AudioOutputI2S();
    out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    out->SetGain(AUDIO_VOLUME);

    M5.Lcd.setCursor(0, 0);
    M5.Lcd.clear();
    M5.Lcd.println("Ready!");
    xTaskCreate(bleLoopTask, "bleLoopTask", 2048, (void *)activeLED, 1, &myTaskHandle);

    // TODO: 戦闘モードに入った時に音と光を鳴らす処理を書く
    // playAction(音声ファイル, メッセージ, LEDパターンの関数);?
}

// 標準の戦闘モード中は縦と横の動きを監視するよ
void defaultFightMode()
{
    M5.Imu.getAccelData(&ax, &ay, &az);

    // 重力を考慮して加速度を計算
    ax -= gravity;

    unsigned long currentTime = millis();
    const char *audioFile = nullptr;
    const char *message = nullptr;
    TaskFunction func = nullptr;

    // 前回の動き検出から一定時間経過している場合のみ新しい動きを検出
    if (currentTime - lastMovementTime > MOVEMENT_COOLDOWN)
    {
        if (ACCEL_THRESHOLD < abs(az) && abs(ax) < abs(az))
        {
            audioFile = "/horizontal.mp3";
            message = "Horizontal";
            // 実行する関数
            func = horizontalCutLED;
            lastMovementTime = currentTime;
        }
        else if (ACCEL_THRESHOLD < abs(ax) && abs(az) < abs(ax))
        {
            audioFile = "/height.mp3";
            message = "Height";
            // 実行する関数
            func = heightCutLED;
            lastMovementTime = currentTime;
        }
    }

    if (mp3 && mp3->isRunning())
    {
        if (!mp3->loop())
        {
            stopAction();
        }
        else if (audioFile && currentTime - playStartTime > PLAY_DURATION_THRESHOLD)
        {
            playAction(audioFile, message, func);
        }
    }
    else if (audioFile)
    {
        playAction(audioFile, message, func);
    }
}

// 必殺技の処理
void ultimateMode()
{
    const char *audioFile = nullptr;
    const char *message = nullptr;
    TaskFunction func = nullptr;

    // チャージの動きまで待機（ayが-2.5より小さくなるまで）
    do
    {
        M5.Imu.getAccelData(&ax, &ay, &az);
        delay(10);
    } while (ay <= ACCEL_THRESHOLD_ULTIMATE);

    // チャージ音の再生
    audioFile = "/ultimateCharge.mp3";
    message = "Ultimate Charge";
    // 実行する関数
    func = ultimateChargeLED;

    playAction(audioFile, message, func);

    // 終わるまで再生
    while (mp3 && mp3->isRunning())
    {
        if (!mp3->loop())
        {
            stopAction();
            break;
        }
    }
    // XXX:現状音が終わったらLEDも消えると思う

    // リリースの動きまで待機（ayが2.5より大きくなるまで）
    do
    {
        M5.Imu.getAccelData(&ax, &ay, &az);
        delay(10);
    } while (ay >= -ACCEL_THRESHOLD_ULTIMATE);

    // リリース音の再生
    audioFile = "/ultimateRelease.mp3";
    message = "Ultimate Release";

    // 実行する関数
    func = ultimateReleaseLED;

    playAction(audioFile, message, func);

    // 再生
    while (mp3 && mp3->isRunning())
    {
        if (!mp3->loop())
        {
            stopAction();
            break;
        }
    }
    // XXX:現状音が終わったらLEDも消えると思う

    // 終了
    // 値を設定し通知を送信
    pRemoteCharacteristic->writeValue("3");
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    mode = FIGHT_MODE;
}