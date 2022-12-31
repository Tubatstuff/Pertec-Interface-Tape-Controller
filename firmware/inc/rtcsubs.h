//  RTC routine definition.

#ifndef RTC_H_DEFINED

void InitializeRTC( void);
void ConfigRTC( void);
void ShowRTCTime( void);
void ShowRTCDate(void);
void SetRTCTime(void);
uint32_t GetRTCDOSTime( void);
#define RTC_H_DEFINED 1
#endif