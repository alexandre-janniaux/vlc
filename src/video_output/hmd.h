#ifndef HMD_H
#define HMD_H

int vout_openHMD(vout_thread_t *p_vout);
int vout_stopHMD(vout_thread_t *p_vout);

void HMDEvent(vout_hmd_t *p_hmd, int query, va_list args);

#endif // HMD_H
