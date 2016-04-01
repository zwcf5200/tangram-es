#pragma once

#include "glm/vec2.hpp"

namespace Tangram {

namespace Primitives {

/* Setup the debug resolution size */
void setResolution(float width, float height);

/* Sets the current primitive color */
void setColor(unsigned int color);

/* Draws a line from origin to destination for the screen resolution resolution */
void drawLine(const glm::vec2& origin, const glm::vec2& destination);

/* Draws a rect from origin to destination for the screen resolution resolution */
void drawRect(const glm::vec2& origin, const glm::vec2& destination);

/* Draws a polyon of containing n points in screen space for the screen resolution resolution */
void drawPoly(const glm::vec2* polygon, size_t n);

}

}
