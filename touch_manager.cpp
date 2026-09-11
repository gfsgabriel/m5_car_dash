#include "touch_manager.h"
#include "display_manager.h"
#include <M5Unified.h>

void inicializarTouch() {}

void atualizarTouch() {
  if (M5.Touch.getCount() == 0) {
    processarTouchHover(-1, -1);
    return;
  }

  auto detail = M5.Touch.getDetail(0);

  // Touch Ativo -> passa coordenadas pra camada de Hover
  if (detail.isPressed()) {
    processarTouchHover(detail.x, detail.y);
  }

  // Soltou o dedo -> dispara a Ação de Release
  if (detail.wasReleased()) {
    processarTouchRelease(detail.x, detail.y);
  }
}