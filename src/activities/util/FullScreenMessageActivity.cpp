#include "FullScreenMessageActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"

void FullScreenMessageActivity::onEnter() {
  Activity::onEnter();

  const auto height = renderer.getLineHeight(NOTOSANS_12_FONT_ID);
  const auto top = (renderer.getScreenHeight() - height) / 2;

  renderer.clearScreen();
  renderer.drawCenteredText(NOTOSANS_12_FONT_ID, top, text.c_str(), true, style);
  renderer.displayBuffer(refreshMode);
}
