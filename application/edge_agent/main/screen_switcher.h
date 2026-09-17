#ifndef _SCREEN_SWITCHER_H_
#define _SCREEN_SWITCHER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 按键循环切屏：BOOT 键(GPIO14) 按一次切下一屏
 * 循环顺序：工卡 → 天气日历 → 待办 → 工卡
 */
void screen_switcher_start(int start_index);

#ifdef __cplusplus
}
#endif

#endif
