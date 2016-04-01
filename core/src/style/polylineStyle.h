#pragma once

#include "style.h"

namespace Tangram {

class PolylineStyle : public Style {

protected:

    virtual void constructVertexLayout() override;
    virtual void constructShaderProgram() override;

    virtual std::unique_ptr<StyleBuilder> createBuilder() const override;

public:

    PolylineStyle(std::string name, Blending blendMode = Blending::none, GLenum drawMode = GL_TRIANGLES);

    virtual ~PolylineStyle() {}

};

}
