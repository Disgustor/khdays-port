/* Place independent images beside their source shapes, preserving the composition's order. */
#ifndef KHDAYS_ONESCREEN_IMAGES_H
#define KHDAYS_ONESCREEN_IMAGES_H

#include "../../ui/composition.h"

typedef struct KhdaysFramePackage KhdaysFramePackage;

/* The resulting shapes stay valid until the next call; input is a fresh composition. */
void khdays_onescreen_images(const KhdaysFramePackage *package, const KhdaysSituation *situation,
                             KhdaysComposition *composition);

#endif
