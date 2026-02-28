#include "BootActivity.h"

#include <GfxRenderer.h>

#include "fontIds.h"
#include "images/CrossLarge.h"

#include "CrossPointSettings.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  
  renderer.clearScreen();


  if(SETTINGS.sleepScreen ==CrossPointSettings::SLEEP_SCREEN_MODE::DARK &&
     SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT &&
    SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM &&
    SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::COVER &&
    SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::BLANK &&
     SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM){ 
     renderer.drawImage(CrossLarge, (pageWidth - 128) / 2, (pageHeight - 128) / 2, 128, 128);
     renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, "CrossPoint", true, EpdFontFamily::BOLD);
     renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, "BOOTING");
     renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
     renderer.displayBuffer();
  }

}
