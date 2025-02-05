/**
 * \par Copyright (C), 2018, AQUEST
 * \class AquesTalkTTS
 * \brief   Text-to-Sppech library. uses AquesTalk-ESP32, I2S, intenal-DAC.
 * @file    AquesTalkTTS.h
 * @author  AQUEST
 * @version V2.4.0
 * @date    2024/10/12
 * @brief   Header for AquesTalkTTS.cpp module
 *
 * \par Description
 * This file is a TTS class for ESP32.
 *
 * \par Method List:
 *    
 *
        TTS.create();
        TTS.createK();
        TTS.release();
        TTS.play(const char *koe, int speed);
        TTS.playK(const char *koe, int speed);
        TTS.isPlay();
        TTS.stop();
        TTS.wait();
        TTS.getLavel();
        TTS.setVolume(uint8_t vol)
 *
 * \par History:
 * <pre>
 * `<Author>`         `<Time>`        `<Version>`        `<Descr>`
 * Nobuhide Yamazaki    2018/03/29        0.1.0          Creation.
 * Nobuhide Yamazaki    2018/08/08        0.2.0          add AqKanji2Roman-M.
 * Nobuhide Yamazaki    2024/10/01        2.3.0          add wait()
 * Nobuhide Yamazaki    2024/10/13        2.4.0          add setVolume()
 * </pre>
 *
 */

#ifndef _AQUESTALK_TTS_H_
#define _AQUESTALK_TTS_H_
#include <stdint.h>

class AquesTalkTTS {
	public:
		int createK();//heap:21KB
		int create();//heap:400B
		void release();

		int playK(const char *kanji, int speed=100);//kanji: kanji-text (UTF8)
		int play(const char *koe, int speed=100);//koe: onsei-kigouretu(roman)

		void stop();
		bool isPlay();
		void wait();
		int getLevel();
		void setVolume(uint8_t vol);

	protected:
		bool sd_begin();
};
extern AquesTalkTTS TTS;

#endif // !defined(_AQUESTALK_TTS_H_)
