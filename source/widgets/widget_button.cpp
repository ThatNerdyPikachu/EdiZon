#include "widgets/widget_button.hpp"

WidgetButton::WidgetButton(Interpreter *interpreter, std::string funcName) :
 Widget(interpreter, false), m_funcName(funcName) {
  m_widgetDataType = STRING;

  m_displayText = "";
}

WidgetButton::~WidgetButton() {

}

void WidgetButton::draw(Gui *gui, u16 x, u16 y) {
  std::string displayText = m_displayText;

  std::replace(displayText.begin(), displayText.end(), '\n', ' ');

  if(displayText.length() >= 25) {
    displayText = displayText.substr(0, 24);
    displayText += "...";
  }

    gui->drawTextAligned(font20, x + WIDGET_WIDTH - 140, y + (WIDGET_HEIGHT / 2.0F), currTheme.separatorColor, displayText.c_str(), ALIGNED_RIGHT);
}

void WidgetButton::onInput(u32 kdown) {
  if (kdown & KEY_A)
    m_displayText = m_interpreter->callFunction(m_funcName);
}

void WidgetButton::onTouch(touchPosition &touch) {
    m_displayText = m_interpreter->callFunction(m_funcName);
}
