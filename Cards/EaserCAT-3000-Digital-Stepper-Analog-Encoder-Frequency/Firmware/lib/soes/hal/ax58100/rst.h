#ifndef __RST_H__
#define __RST_H__

#define ESC_Pin_RSTN               PB12

#ifdef __cplusplus
extern "C" {
#endif

void rst_setup(void);
void rst_low(void);
void rst_high(void);

void rst_check_start(void);
uint8_t is_esc_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __RST_H__ */
