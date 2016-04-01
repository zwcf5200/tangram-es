#pragma once

#include "style.h"

#include <mutex>
#include <tuple>

namespace Tangram {

class PolygonStyle : public Style {

protected:

    virtual void constructVertexLayout() override;
    virtual void constructShaderProgram() override;

    virtual std::unique_ptr<StyleBuilder> createBuilder() const override;

public:

    PolygonStyle(std::string name, Blending blendMode = Blending::none, GLenum drawMode = GL_TRIANGLES);

    virtual ~PolygonStyle() {}

};

}
