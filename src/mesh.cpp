#include "mesh.h"
#include "shader.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include <pmp/algorithms/normals.h>
#include <pmp/io/io.h>

using namespace std;
using namespace Eigen;
using namespace pmp;

Mesh::~Mesh() {
  if (_ready) {
    glDeleteBuffers(7, _vbo);
    glDeleteVertexArrays(1, &_vao);
  }
}

void Mesh::load(const string &filename) {
  cout << "Loading: " << filename << endl;

  pmp::read(*this, filename);
  pmp::face_normals(*this);
  pmp::vertex_normals(*this);

  // vertex properties
  auto vpoints = get_vertex_property<Point>("v:point");
  assert(vpoints);
  auto vpositions = add_vertex_property<Vector4f>("v:position");
  for (auto v : vertices()) {
    Vector4f pos;
    pos << static_cast<Eigen::Vector3f>(vpoints[v]), 1.f;
    vpositions[v] = pos;
  }

  auto vnormals = get_vertex_property<Normal>("v:normal");
  assert(vnormals);

  auto texcoords = get_vertex_property<TexCoord>("v:texcoord");
  if (!texcoords) {
    add_vertex_property<TexCoord>("v:texcoord", TexCoord(0, 0));
  }

  auto colors = get_vertex_property<Color>("v:color");
  if (!colors) {
    add_vertex_property<Color>("v:color", Color(1, 1, 1));
  }

  // face iterator
  SurfaceMesh::FaceIterator fit, fend = faces_end();
  // vertex circulator
  SurfaceMesh::VertexAroundFaceCirculator fvit, fvend;
  pmp::Vertex v0, v1, v2;
  for (fit = faces_begin(); fit != fend; ++fit) {
    fvit = fvend = vertices(*fit);
    v0 = *fvit;
    ++fvit;
    v2 = *fvit;

    do {
      v1 = v2;
      ++fvit;
      v2 = *fvit;
      _indices.push_back(v0.idx());
      _indices.push_back(v1.idx());
      _indices.push_back(v2.idx());
    } while (++fvit != fvend);
  }

  updateNormals();
  updateBoundingBox();
}

void Mesh::createGrid(int m, int n) {
  clear();
  _indices.clear();
  _indices.reserve(2 * 3 * m * n);
  reserve(m * n, 3 * m * n, 2 * m * n);

  float dx = 1.f / float(m - 1);
  float dy = 1.f / float(n - 1);

  Eigen::Array<pmp::Vertex, Dynamic, Dynamic> ids(m, n);
  for (int i = 0; i < m; ++i)
    for (int j = 0; j < n; ++j)
      ids(i, j) = add_vertex(Point(float(i) * dx, float(j) * dy, 0.));

  auto vpoints = get_vertex_property<Point>("v:point");
  auto vpositions = add_vertex_property<Vector4f>("v:position");
  for (auto v : vertices()) {
    Vector4f pos;
    pos << static_cast<Eigen::Vector3f>(vpoints[v]), 1.f;
    vpositions[v] = pos;
  }
  add_vertex_property<pmp::Color>("v:color");
  add_vertex_property<pmp::Normal>("v:normal");
  add_vertex_property<pmp::TexCoord>("v:texcoord");

  colors().fill(1);
  texcoords() = positions().topRows(2);

  for (int i = 0; i < m - 1; ++i) {
    for (int j = 0; j < n - 1; ++j) {
      pmp::Vertex v0, v1, v2, v3;
      v0 = ids(i + 0, j + 0);
      v1 = ids(i + 1, j + 0);
      v2 = ids(i + 1, j + 1);
      v3 = ids(i + 0, j + 1);
      add_triangle(v0, v1, v2);
      add_triangle(v0, v2, v3);

      _indices.push_back(v0.idx());
      _indices.push_back(v1.idx());
      _indices.push_back(v2.idx());

      _indices.push_back(v0.idx());
      _indices.push_back(v2.idx());
      _indices.push_back(v3.idx());
    }
  }
  updateNormals();
  updateBoundingBox();
}

void Mesh::createSphere(float radius, int nU, int nV) {
  int nbVertices = ((nU + 1) * (nV + 1));
  std::vector<pmp::Vertex> ids(nbVertices);
  Eigen::Matrix3Xf normals(3, nbVertices);
  Eigen::Matrix2Xf texcoords(2, nbVertices);

  for (int v = 0; v <= nV; ++v) {
    for (int u = 0; u <= nU; ++u) {
      int idx = u + (nU + 1) * v;
      float theta = u / float(nU) * M_PI;
      float phi = v / float(nV) * M_PI * 2;

      normals.col(idx) << sin(theta) * cos(phi), sin(theta) * sin(phi),
          cos(theta);
      if (normals.col(idx).squaredNorm() > 0.f)
        normals.col(idx).normalize();

      Vector3f position = normals.col(idx) * radius;

      texcoords.col(idx) << v / float(nV), u / float(nU);

      ids[idx] = add_vertex(position);
      _bbox.extend(position);
    }
  }

  auto vpoints = get_vertex_property<Point>("v:point");
  auto vpositions = add_vertex_property<Vector4f>("v:position");
  for (auto v : vertices()) {
    Vector4f pos;
    pos << static_cast<Eigen::Vector3f>(vpoints[v]), 1.f;
    vpositions[v] = pos;
  }
  add_vertex_property<pmp::Color>("v:color", Color(1, 1, 1));
  add_vertex_property<pmp::Normal>("v:normal");
  add_vertex_property<pmp::TexCoord>("v:texcoord");
  this->normals() = normals;
  this->texcoords() = texcoords;

  _indices.reserve(6 * nU * nV);
  for (int v = 0; v < nV; ++v) {
    for (int u = 0; u < nU; ++u) {
      int vindex = u + (nU + 1) * v;

      pmp::Vertex v0, v1, v2, v3;
      v0 = ids[vindex];
      v1 = ids[vindex + 1];
      v2 = ids[vindex + 1 + (nU + 1)];
      v3 = ids[vindex + (nU + 1)];

      add_triangle(v0, v1, v2);
      add_triangle(v0, v2, v3);

      _indices.push_back(v0.idx());
      _indices.push_back(v1.idx());
      _indices.push_back(v2.idx());

      _indices.push_back(v0.idx());
      _indices.push_back(v2.idx());
      _indices.push_back(v3.idx());
    }
  }
  pmp::face_normals(*this);
}

void Mesh::init() {
  glGenVertexArrays(1, &_vao);
  glGenBuffers(7, _vbo);
  updateVBO();
  _ready = true;
}

void Mesh::updateAll() {
  updateNormals();
  updateBoundingBox();
  updateVBO();
}

void Mesh::updateNormals() {
  pmp::face_normals(*this);
  pmp::vertex_normals(*this);
}

void Mesh::updateVBO() {
  glBindVertexArray(_vao);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vbo[VBO_IDX_FACE]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * _indices.size(),
               _indices.data(), GL_STATIC_DRAW);

  VertexProperty<Vector4f> vertices =
      get_vertex_property<Vector4f>("v:position");
  size_t n_vertices = vertices.vector().size();
  if (vertices) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_POSITION]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vector4f) * n_vertices,
                 vertices.vector()[0].data(), GL_STATIC_DRAW);
  }

  VertexProperty<Normal> vnormals = get_vertex_property<Normal>("v:normal");
  if (vnormals) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_NORMAL]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Normal) * n_vertices,
                 vnormals.vector()[0].data(), GL_STATIC_DRAW);
  }

  VertexProperty<TexCoord> texcoords =
      get_vertex_property<TexCoord>("v:texcoord");
  if (texcoords) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_TEXCOORD]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TexCoord) * n_vertices,
                 texcoords.vector()[0].data(), GL_STATIC_DRAW);
  }

  VertexProperty<Color> colors = get_vertex_property<Color>("v:color");
  if (colors) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_COLOR]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Color) * n_vertices,
                 colors.vector()[0].data(), GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
}

void Mesh::updateBoundingBox() {
  _bbox.setNull();
  VertexProperty<Point> vertices = get_vertex_property<Point>("v:point");
  for (const auto &p : vertices.vector())
    _bbox.extend(static_cast<Vector3f>(p));
}

void Mesh::draw(Shader &shader) {
  if (!_ready)
    init();

  glBindVertexArray(_vao);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vbo[VBO_IDX_FACE]);

  int vertex_loc = shader.getAttribLocation("vtx_position");
  auto vertices = get_vertex_property<Vector4f>("v:position");
  if (vertex_loc >= 0 && vertices) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_POSITION]);
    glVertexAttribPointer(vertex_loc, 4, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(vertex_loc);
  }

  int color_loc = shader.getAttribLocation("vtx_color");
  auto colors = get_vertex_property<Color>("v:color");
  if (color_loc >= 0 && colors) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_COLOR]);
    glVertexAttribPointer(color_loc, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(color_loc);
  }

  int normal_loc = shader.getAttribLocation("vtx_normal");
  auto vnormals = get_vertex_property<Normal>("v:normal");
  if (normal_loc >= 0 && vnormals) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_NORMAL]);
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(normal_loc);
  }

  int texCoord_loc = shader.getAttribLocation("vtx_texcoord");
  auto texcoords = get_vertex_property<TexCoord>("v:texcoord");
  if (texCoord_loc >= 0 && texcoords) {
    glBindBuffer(GL_ARRAY_BUFFER, _vbo[VBO_IDX_TEXCOORD]);
    glVertexAttribPointer(texCoord_loc, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(texCoord_loc);
  }

  glDrawElements(GL_TRIANGLES, _indices.size(), GL_UNSIGNED_INT, 0);

  if (vertex_loc >= 0)
    glDisableVertexAttribArray(vertex_loc);
  if (color_loc >= 0)
    glDisableVertexAttribArray(color_loc);
  if (normal_loc >= 0)
    glDisableVertexAttribArray(normal_loc);
  if (texCoord_loc >= 0)
    glDisableVertexAttribArray(texCoord_loc);

  glBindVertexArray(0);
}

// Add the quad to the mesh
// p0 ---- p1
// |     / |
// |  /    |
// p2 ---- p3

void Mesh::addQuad(const Vector4f &p0, const Vector4f &p1, const Vector4f &p2,
                   const Vector4f &p3) {

  const pmp::Vertex v0 = add_vertex(p0.head<3>());
  const pmp::Vertex v1 = add_vertex(p1.head<3>());
  const pmp::Vertex v2 = add_vertex(p2.head<3>());
  const pmp::Vertex v3 = add_vertex(p3.head<3>());

  auto vertices = get_vertex_property<Vector4f>("v:position");
  vertices[v0] = p0;
  vertices[v1] = p1;
  vertices[v2] = p2;
  vertices[v3] = p3;

  add_triangle(v0, v1, v2);
  add_triangle(v1, v3, v2);

  _indices.push_back(v0.idx());
  _indices.push_back(v1.idx());
  _indices.push_back(v2.idx());

  _indices.push_back(v1.idx());
  _indices.push_back(v3.idx());
  _indices.push_back(v2.idx());
}

Mesh *Mesh::computeShadowVolume(const Vector3f &lightPos, bool interpolated) {
  (void)interpolated;
  return nullptr;
}
