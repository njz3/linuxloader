#ifndef ADM_BRIDGE_H
#define ADM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Namco N2 games built with NVIDIA's arcade driver inside them (Maximum Tune 3): their "adm" display calls answered
// with the loader's window, their OpenGL entry points sent to the loader's. admBridge.c
void admBridgeInstall(void);

#ifdef __cplusplus
}
#endif

#endif
