#pragma once
#include <string>

void DrawPopupWindow(unsigned int uX, unsigned int uY, unsigned int uWidth, unsigned int uHeight);  // idb

class GraphicsImage;

extern GraphicsImage *parchment;
extern GraphicsImage *messagebox_corner_ul;       // 5076AC
extern GraphicsImage *messagebox_corner_ll;       // 5076B4
extern GraphicsImage *messagebox_corner_ur;       // 5076A8
extern GraphicsImage *messagebox_corner_lr;       // 5076B0
extern GraphicsImage *messagebox_border_top;     // 507698
extern GraphicsImage *messagebox_border_bottom;  // 5076A4
extern GraphicsImage *messagebox_border_left;    // 50769C
extern GraphicsImage *messagebox_border_right;   // 5076A0

extern bool holdingMouseRightButton;
extern bool rightClickItemActionPerformed;
extern bool identifyOrRepairReactionPlayed;
