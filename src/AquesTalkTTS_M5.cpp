/*************************************************
  AquesTalkTTS - AquesTalk ESP32のラッパークラス
  各種M5Stack用（M5Core/M5Stick/M5Atom/...） 
  漢字仮名混じりテキストからの音声合成も可能
  
  特徴：
  ・M5Unifiedライブラリ使用。音声出力はこれに依存。
  ・非同期型で、別タスクで発声処理
  ・SDのアクセスは、SD.hの代わりに高速なSdFat.h(SdFatライブラリ)の指定も可能

  動作確認環境：
     M5Stack gray, M5Stack coreS3
     Arduino core for the ESP32 v2.0.17, M5Stack 2.1.2
     M5Unified 0.1.17
     SdFat v2.2.3
    
  * M5Unifiedライブラリ（＋関連ライブラリ）をあらかじめインストールしてください。
  * 音声記号列からの合成だけの場合は、辞書データやSD周りの設定は不要です
  * AquesTalk ESP32ライブラリはあらかじめzipファイルからインストールしてください
    https://www.a-quest.com/download.html
  * aqdic_open()/aqdic_close()/aqdic_read()関数は 辞書データ(aqdic_m.bin)にアクセスする関数で、
    libaquestalk.aから呼び出されます。
  * 漢字テキストからの合成の場合は、辞書データファイルをSDカード上に配置しておきます。
  * ワークバッファを実行時にヒープ上に確保します。create()は400B、createK()は20.4KBを使います。
  * 本ソースは改変自由です
**************************************************/

//#define USE_FATFS // FatFsを用いる場合に指定
#define USE_SDFAT // SdFatライブラリ(別途インストール）を用いる場合に指定

#if   defined(USE_FATFS)
  #include <FFat.h>
#elif defined(USE_SDFAT)
  #include <SdFat.h>
#else
  #include <SD.h>
#endif

#include <M5Unified.h>
#include <aquestalk.h>
#include "AquesTalkTTS.h"

// AquesTalk-ESP32 licencekey ナ行マ行がヌになる制限を解除
static constexpr char* LICENCE_KEY = "XXX-XXX-XXX"; 

static constexpr const char * FILE_DIC = "/aq_dic/aqdic_m.bin";  // SD上の辞書データの場所
static constexpr int LEN_FRAME = 32;
static constexpr int N_BUF_KOE = 1024;  // 音声記号列のバッファサイズ
static constexpr uint8_t m5spk_virtual_channel = 0;

static uint32_t *workbuf = nullptr; // work buffer for AquesTalk pico
static uint8_t *workbufK = nullptr; // work buffer for AqKanji2Roman
static TaskHandle_t task_handle = nullptr;
static volatile bool is_talking = false;
static int level = 0; // フレーム単位の音量(リップシンク用)
static uint8_t gVol = 255;  // 音量 初期値はmax

static void talk_task(void *arg);
static int calc_level(int16_t *wav, uint16_t len);
static void volume(int16_t *wav, uint16_t len, uint8_t gVol);

AquesTalkTTS  TTS;    // the only instance of AquesTalkTTS class

// 漢字仮名混じり文からの音声合成の場合の初期化 heap:21KB
int AquesTalkTTS::createK()
{
  if(!sd_begin()) return 501; // ERR:SDカード未検出
  
  if(!workbufK){
    workbufK = (uint8_t*)malloc(SIZE_AQK2R_MIN_WORK_BUF);
    if(workbufK==0) return 401; // no heap memory
  }
  int iret = CAqK2R_Create(workbufK, SIZE_AQK2R_MIN_WORK_BUF);
  if(iret){
      free(workbufK);
      workbufK=0;
      return iret;  // AqKanji2Roman Init error
  }
  return create();  //音声記号からの音声合成の初期化
}

// 音声記号からの音声合成のみを使う場合の初期化 heap:400B
int AquesTalkTTS::create()
{
  if(!workbuf){
    workbuf = (uint32_t*)malloc(AQ_SIZE_WORKBUF*sizeof(uint32_t));
    if(workbuf==0) return 401;  // no heap memory
  }
  int iret = CAqTkPicoF_Init(workbuf, LEN_FRAME, LICENCE_KEY);
  if(iret)    return iret;  // AquesTalk Init error
  
  xTaskCreateUniversal(talk_task, "talk_task", 4096, nullptr, 1, &task_handle, APP_CPU_NUM);
  return 0;
}

void AquesTalkTTS::release()
{
  stop();
  if(task_handle) vTaskDelete(task_handle);
  if(workbuf) free(workbuf);
  if(workbufK){
    CAqK2R_Release();
    free(workbufK);
  }
  workbuf = 0; workbufK = 0; task_handle = nullptr;
}

int AquesTalkTTS::playK(const char *kanji, int speed)
{
  if(workbufK==0) return 403; // not initialized ( use TTS.createK() before. )
  if(workbuf==0) return 402;  // not initialized use TTS.create()

  // 漢字テキストを音声記号列に変換
  char  koe[N_BUF_KOE];
  int iret = CAqK2R_Convert(kanji, koe, N_BUF_KOE);
  if(iret)  return iret+1000; // return error code

  return play(koe, speed);  //音声記号列からの音声合成 非同期
}

int AquesTalkTTS::play(const char *koe, int speed)
{
  stop();
  int iret = CAqTkPicoF_SetKoe((const uint8_t*)koe, 100, 0xFFu);
  if (iret) return iret;  // return error code

  is_talking = true;
  xTaskNotifyGive(task_handle);
  return 0;
}

void AquesTalkTTS::stop()
{
   if (is_talking) { is_talking = false; vTaskDelay(1); }
} 

bool AquesTalkTTS::isPlay()
{
  return is_talking;
}

void AquesTalkTTS::wait()
{
  while (is_talking) { vTaskDelay(1); }
}

int AquesTalkTTS::getLevel()
{
  return level;
}

void AquesTalkTTS::setVolume(uint8_t vol)
{
  gVol = vol;
}

//-----------------------------------------------------------------
// 逐次発声用タスク
void talk_task(void *arg)
{
  int16_t wav[3][LEN_FRAME];
  int tri_index = 0;
  for (;;)
  {
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY ); // wait notify
    while (is_talking)
    {
      uint16_t len;
      if (CAqTkPicoF_SyntheFrame(wav[tri_index], &len)) { is_talking = false; break; }
      level = calc_level(wav[tri_index], len); // calc output level for Avatar(lip-sync)
      volume(wav[tri_index], len, gVol);
      M5.Speaker.playRaw(wav[tri_index], len, 8000, false, 1, m5spk_virtual_channel, false);
      tri_index = tri_index < 2 ? tri_index + 1 : 0;
    }
  }
}
// calc output level for Avatar(lip-sync) 
int calc_level(int16_t *wav, uint16_t len)
{
  uint32_t sum = 0;
  if(len!=LEN_FRAME)  return 0;
  for(uint16_t i = 0; i < LEN_FRAME; i++) {
    sum += abs(wav[i]);
  }
  return (int)(sum / LEN_FRAME);  // 5bit right shift 
}

void volume(int16_t *wav, uint16_t len, uint8_t gVol)
{
  if(gVol==255) return;
  for(uint16_t i=0; i<len; i++){
    wav[i] = (int16_t)( wav[i] * gVol / 256 ); 
  }
}

/*****************************************************************************

  辞書データ(aqdic_m.bin)のアクセス関数
  
  ここで定義する関数は、AquesTalk ESP32ライブラリから呼び出されます。
  辞書データを読み込む機能を、使用するハードウェア構成に応じて実装します。

  漢字仮名混じり文からの音声合成を行う場合に記述が必須で、
  さもなければリンク時にエラーとなります。
  音声記号列からの音声合成だけを使用する場合はこれら関数の記述は不要です。
  
  辞書データの配置場所は以下などが考えられます。
  ・SDカード上のファイル  << 本実装
  ・SPIシリアルフラッシュメモリ
  ・メモリマップドされたシリアルフラッシュ
  ・マイコン内蔵フラッシュメモリ
  
  辞書データは大量かつランダムにアクセスされるので、
  この関数の処理量が音声合成のパフォーマンスに与える影響は大きいです。
  
******************************************************************************/

//-----------------------------------------------------------------
// FileSystemの起動
//    エラーのときはtrueを返す
#if defined(USE_FATFS)
static File fp;
bool AquesTalkTTS::sd_begin()
{
  return FFat.begin();
}
#elif defined(USE_SDFAT)
static SdFs sd;
static FsFile fp;
//static SPIClass SPI_EXT;
bool AquesTalkTTS::sd_begin()
{
  return sd.begin(SdSpiConfig(GPIO_NUM_4, SHARED_SPI, SD_SCK_MHZ(24)));// M5Stack/M5StackCore2/M5StackCoreS3
  // M5StickC、ATOMなど外付けのSD(TF)モジュールを使う場合は以下のように実装に応じて指定する
  //SPI_EXT.begin( 0, 36, 26, -1); // M5StickC/M5StickCPlus (SCK, MISO, MOSI, CS)
  //SPI_EXT.begin(23, 33, 19, -1); // ATOM LITE+Atmic-SPK (SCK, MISO, MOSI, CS)
  //SPI_EXT.begin( 7,  8,  6, -1); // ATOMS3 LITE+Atmic-SPK (SCK, MISO, MOSI, CS)
  //return sd.begin(SdSpiConfig(-1, SPI_EXT, SD_SCK_MHZ(24), &SPI_EXT); // CS:NC
}
#else
static File fp;
//static SPIClass SPI_EXT;
bool AquesTalkTTS::sd_begin()
{
  return SD.begin(GPIO_NUM_4, SPI, 24000000);  // M5Stack/M5StackCore2/M5StackCoreS3
  // M5StickC、ATOMなど外付けのSD(TF)モジュールを使う場合は以下のように実装に応じて指定する
  //SPIClass SPI_EXT;
  //SPI_EXT.begin( 0, 36, 26, -1); // M5StickC/M5StickCPlus (SCK, MISO, MOSI, CS)
  //SPI_EXT.begin(23, 33, 19, -1); // ATOM LITE+Atmic-SPK (SCK, MISO, MOSI, CS)
  //SPI_EXT.begin( 7,  8,  6, -1); // ATOMS3 LITE+Atmic-SPK (SCK, MISO, MOSI, CS)
  //return SD.begin(-1, SPI_EXT, 24000000); // CS:NC
}
#endif

// 仮想的な辞書データの先頭アドレス （NULL以外なら任意で良い。但し4byteアライメント）
static uint16_t ADDR_ORG = 0x10001000U;

//-----------------------------------------------------------------
// 辞書データアクセスの初期化
// CAqK2R_Create()内から一度だけ呼び出される
// 戻り値
//    仮想的な辞書データの先頭アドレスを返す（0以外。4byteアライメント)。
//    エラーのときは0を返す
extern "C" size_t aqdic_open()
{
#if  defined(USE_FATFS)
  fp = FFat.open(FILE_DIC);
#elif defined(USE_SDFAT)
  fp = sd.open(FILE_DIC);
#else
  fp = SD.open(FILE_DIC);
#endif
  if(!fp) return 0;  // err
  return ADDR_ORG; // ok
}

//-----------------------------------------------------------------
// 辞書データアクセスの終了
// CAqK2R_Release()内から一度だけ呼び出される
extern "C" void aqdic_close()
{
  if(fp) fp.close();
}

//-----------------------------------------------------------------
// 辞書データの読み込み
// pos: 先頭アドレス[byte]
// size: 読み込むサイズ[byte]
// buf:  読み込むデータ配列 uint8_t(size)
// 戻り値： 読みこんだバイト数
//CAqK2R_Convert()/CAqK2R_ConvertW()から複数回呼び出される
extern "C" size_t aqdic_read(size_t pos, size_t size, void *buf)
{
  fp.seek(pos-ADDR_ORG);
  return fp.read((uint8_t*)buf,size);
}
