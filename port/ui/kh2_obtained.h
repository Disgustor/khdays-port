/* Days pickup text attached to gaug's one-shot OBTAINED animation. */
#ifndef KHDAYS_KH2_OBTAINED_H
#define KHDAYS_KH2_OBTAINED_H
#ifdef __cplusplus
extern "C" {
#endif
#include "elements.h"
#include "../gfx/sequence_image.h"
int khdays_ui_obtained_enabled(void);
void khdays_ui_obtained_request(int announcement);
void khdays_ui_obtained_frame(unsigned frame);
void khdays_ui_obtained_begin(void);
int khdays_ui_obtained_text(const KhdaysUiElement *anchor, const unsigned *rgba, int w, int h, unsigned version);
int khdays_ui_obtained_owns(void);
void khdays_ui_obtained_end(void);
void khdays_ui_obtained_commands(KhdaysSequenceImage *out);
#ifdef __cplusplus
}
#endif
#endif
