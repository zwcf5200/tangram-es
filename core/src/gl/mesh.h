#pragma once

#include "gl.h"
#include "vertexLayout.h"
#include "vao.h"
#include "util/types.h"
#include "platform.h"
#include "style/style.h"

#include <string>
#include <vector>
#include <memory>
#include <cstring> // for memcpy
#include <cassert>

#define MAX_INDEX_VALUE 65535 // Maximum value of GLushort

namespace Tangram {

/*
 * Mesh - Drawable collection of geometry contained in a vertex buffer and
 * (optionally) an index buffer
 */
struct MeshBase {
public:
    /*
     * Creates a Mesh for vertex data arranged in the structure described by
     * vertexLayout to be drawn using the OpenGL primitive type drawMode
     */
    MeshBase(std::shared_ptr<VertexLayout> vertexlayout, GLenum drawMode = GL_TRIANGLES,
             GLenum hint = GL_STATIC_DRAW);

    MeshBase();

    /*
     * Set Vertex Layout for the mesh object
     */
    void setVertexLayout(std::shared_ptr<VertexLayout> vertexLayout);

    /*
     * Set Draw mode for the mesh object
     */
    void setDrawMode(GLenum drawMode = GL_TRIANGLES);

    /*
     * Destructs this Mesh and releases all associated OpenGL resources
     */
    ~MeshBase();

    /*
     * Copies all added vertices and indices into OpenGL buffer objects; After
     * geometry is uploaded, no more vertices or indices can be added
     */
    virtual void upload();

    /*
     * Sub data upload of the mesh, returns true if this results in a buffer binding
     */
    void subDataUpload(GLbyte* data = nullptr);

    /*
     * Renders the geometry in this mesh using the ShaderProgram shader; if
     * geometry has not already been uploaded it will be uploaded at this point
     */
    void draw(ShaderProgram& shader);

    size_t bufferSize() const;

protected:

    int m_generation; // Generation in which this mesh's GL handles were created

    // Used in draw for legth and offsets: sumIndices, sumVertices
    // needs to be set by compile()
    std::vector<std::pair<uint32_t, uint32_t>> m_vertexOffsets;

    std::shared_ptr<VertexLayout> m_vertexLayout;

    size_t m_nVertices;
    GLuint m_glVertexBuffer;

    std::unique_ptr<Vao> m_vaos;

    // Compiled vertices for upload
    GLbyte* m_glVertexData = nullptr;

    size_t m_nIndices;
    GLuint m_glIndexBuffer;
    // Compiled  indices for upload
    GLushort* m_glIndexData = nullptr;

    GLenum m_drawMode;
    GLenum m_hint;

    bool m_isUploaded;
    bool m_isCompiled;
    bool m_dirty;
    bool m_keepMemoryData;

    GLsizei m_dirtySize;
    GLintptr m_dirtyOffset;

    bool checkValidity();

    size_t compileIndices(const std::vector<std::pair<uint32_t, uint32_t>>& offsets,
                          const std::vector<uint16_t>& indices, size_t offset);

    void setDirty(GLintptr byteOffset, GLsizei byteSize);
};

template<class T>
struct MeshData {

    MeshData() {}
    MeshData(std::vector<uint16_t>&& indices, std::vector<T>&& vertices)
        : indices(std::move(indices)),
          vertices(std::move(vertices)) {
        offsets.emplace_back(this->indices.size(), this->vertices.size());
    }

    std::vector<uint16_t> indices;
    std::vector<T> vertices;
    std::vector<std::pair<uint32_t, uint32_t>> offsets;

    void clear() {
        offsets.clear();
        indices.clear();
        vertices.clear();
    }
};

template<class T>
class Mesh : public StyledMesh, protected MeshBase {
public:

    Mesh(std::shared_ptr<VertexLayout> vertexLayout, GLenum drawMode,
         GLenum hint = GL_STATIC_DRAW)
        : MeshBase(vertexLayout, drawMode, hint) {};

    virtual ~Mesh() {}

    size_t bufferSize() const override {
        return MeshBase::bufferSize();
    }

    void draw(ShaderProgram& shader) override {
        MeshBase::draw(shader);
    }

    void compile(const std::vector<MeshData<T>>& meshes);

    void compile(const MeshData<T>& mesh);

    /*
     * Update nVerts vertices in the mesh with the new T value newVertexValue
     * starting after byteOffset in the mesh vertex data memory
     */
    void updateVertices(Range vertexRange, const T& newVertexValue);

    /*
     * Update nVerts vertices in the mesh with the new attribute A
     * newAttributeValue starting after byteOffset in the mesh vertex data
     * memory
     */
    template<class A>
    void updateAttribute(Range vertexRange, const A& newAttributeValue,
                         size_t attribOffset = 0);
};


template<class T>
void Mesh<T>::compile(const std::vector<MeshData<T>>& meshes) {

    m_nVertices = 0;
    m_nIndices = 0;

    for (auto& m : meshes) {
        m_nVertices += m.vertices.size();
        m_nIndices += m.indices.size();
    }

    int stride = m_vertexLayout->getStride();
    m_glVertexData = new GLbyte[m_nVertices * stride];

    size_t offset = 0;
    for (auto& m : meshes) {
        size_t nBytes = m.vertices.size() * stride;
        std::memcpy(m_glVertexData + offset,
                    (const GLbyte*)m.vertices.data(),
                    nBytes);

        offset += nBytes;
    }

    assert(offset == m_nVertices * stride);

    if (m_nIndices > 0) {
        m_glIndexData = new GLushort[m_nIndices];

        size_t offset = 0;
        for (auto& m : meshes) {
            offset = compileIndices(m.offsets, m.indices, offset);
        }
        assert(offset == m_nIndices);
    }

    m_isCompiled = true;
}

template<class T>
void Mesh<T>::compile(const MeshData<T>& mesh) {

    m_nVertices = mesh.vertices.size();
    m_nIndices = mesh.indices.size();

    int stride = m_vertexLayout->getStride();
    m_glVertexData = new GLbyte[m_nVertices * stride];

    std::memcpy(m_glVertexData,
                (const GLbyte*)mesh.vertices.data(),
                m_nVertices * stride);

    if (m_nIndices > 0) {
        m_glIndexData = new GLushort[m_nIndices];
        compileIndices(mesh.offsets, mesh.indices, 0);
    }

    m_isCompiled = true;
}

template<class T>
template<class A>
void Mesh<T>::updateAttribute(Range vertexRange, const A& newAttributeValue,
                              size_t attribOffset) {

    if (m_glVertexData == nullptr) {
        assert(false);
        return;
    }

    const size_t aSize = sizeof(A);
    const size_t tSize = sizeof(T);
    static_assert(aSize <= tSize, "Invalid attribute size");

    if (vertexRange.start < 0 || vertexRange.length < 1) {
        return;
    }
    if (size_t(vertexRange.start + vertexRange.length) > m_nVertices) {
        //LOGW("Invalid range");
        return;
    }
    if (attribOffset >= tSize) {
        //LOGW("Invalid attribute offset");
        return;
    }

    size_t start = vertexRange.start * tSize + attribOffset;
    size_t end = start + vertexRange.length * tSize;

    // update the vertices attributes
    for (size_t offset = start; offset < end; offset += tSize) {
        std::memcpy(m_glVertexData + offset, &newAttributeValue, aSize);
    }

    // set all modified vertices dirty
    setDirty(start, (vertexRange.length - 1) * tSize + aSize);
}

template<class T>
void Mesh<T>::updateVertices(Range vertexRange, const T& newVertexValue) {
    if (m_glVertexData == nullptr) {
        assert(false);
        return;
    }

    size_t tSize = sizeof(T);

    if (vertexRange.start + vertexRange.length > int(m_nVertices)) {
        return;
    }


    size_t start = vertexRange.start * tSize;
    size_t end = start + vertexRange.length * tSize;

    // update the vertices
    for (size_t offset = start; offset < end; offset += tSize) {
        std::memcpy(m_glVertexData + offset, &newVertexValue, tSize);
    }

    setDirty(start, end - start);
}

}
