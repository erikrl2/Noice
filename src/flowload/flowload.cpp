#include "flowload.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace {
  double deg2rad(double d) {
    return d * M_PI / 180.0;
  }
  Eigen::Vector3d toV3(const std::vector<tinyobj::real_t>& a, int i) {
    return Eigen::Vector3d((double)a[3 * i + 0], (double)a[3 * i + 1], (double)a[3 * i + 2]);
  }
  Eigen::Vector2d toV2(const std::vector<tinyobj::real_t>& a, int i) {
    return Eigen::Vector2d((double)a[2 * i + 0], (double)a[2 * i + 1]);
  }
  Eigen::Vector3d safeNormalize(const Eigen::Vector3d& v, double eps = 1e-12) {
    double n = v.norm();
    return (n < eps) ? Eigen::Vector3d(0, 0, 0) : v / n;
  }
  Eigen::Vector3d projectToTangent(const Eigen::Vector3d& v, const Eigen::Vector3d& n) {
    return v - v.dot(n) * n;
  }
  double clampd(double x, double a, double b) {
    return std::max(a, std::min(b, x));
  }

  struct EdgeKey {
    int a, b;
    bool operator==(const EdgeKey& o) const noexcept { return a == o.a && b == o.b; }
  };
  struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& k) const noexcept {
      uint64_t a = (uint32_t)k.a;
      uint64_t b = (uint32_t)k.b;
      uint64_t x = (a << 32) ^ b;
      x ^= (x >> 33);
      x *= 0xff51afd7ed558ccdULL;
      x ^= (x >> 33);
      x *= 0xc4ceb9fe1a85ec53ULL;
      x ^= (x >> 33);
      return (size_t)x;
    }
  };

  struct SplitKey {
    int v, vt, sg;
    bool operator==(const SplitKey& o) const noexcept { return v == o.v && vt == o.vt && sg == o.sg; }
  };
  struct SplitKeyHash {
    std::size_t operator()(const SplitKey& k) const noexcept {
      uint64_t a = (uint32_t)k.v;
      uint64_t b = (uint32_t)(k.vt + 1);
      uint64_t c = (uint32_t)k.sg;
      uint64_t x = (a * 1315423911ULL) ^ (b * 2654435761ULL) ^ (c * 97531ULL);
      x ^= (x >> 33);
      x *= 0xff51afd7ed558ccdULL;
      x ^= (x >> 33);
      x *= 0xc4ceb9fe1a85ec53ULL;
      x ^= (x >> 33);
      return (size_t)x;
    }
  };
}; // namespace

bool ComputeUvFlowfieldFromOBJ(
    const std::string& objPath,
    std::vector<float>& outVert,
    std::vector<unsigned int>& outInd,
    const FlowfieldSettings& settings
) {
  tinyobj::ObjReaderConfig cfg;
  cfg.triangulate = false;
  tinyobj::ObjReader reader;
  if (!reader.ParseFromFile(objPath, cfg)) {
    std::cerr << "Failed to read OBJ" << std::endl;
    return false;
  }
  const auto& attrib = reader.GetAttrib();
  const auto& shapes = reader.GetShapes();

  const int nV_in = (int)(attrib.vertices.size() / 3);
  const int nVT_in = (int)(attrib.texcoords.size() / 2);
  if (nV_in <= 0 || nVT_in <= 0) {
    std::cerr << "OBJ must have positions and texcoords" << std::endl;
    return false;
  }

  // --- Build input poly list
  std::vector<std::vector<tinyobj::index_t>> polys;
  for (const auto& sh : shapes) {
    size_t index_offset = 0;
    for (size_t f = 0; f < sh.mesh.num_face_vertices.size(); f++) {
      int fv = sh.mesh.num_face_vertices[f];
      if (fv < 3) {
        index_offset += fv;
        continue;
      }
      std::vector<tinyobj::index_t> poly;
      for (int k = 0; k < fv; k++) poly.push_back(sh.mesh.indices[index_offset + k]);
      polys.push_back(std::move(poly));
      index_offset += fv;
    }
  }

  // --- Internal triangulation view for edges/creases/normals/tangents
  struct TriCorner {
    tinyobj::index_t idx;
    int poly, corner;
  };
  struct Tri {
    TriCorner c0, c1, c2;
    int v0() const { return c0.idx.vertex_index; }
    int v1() const { return c1.idx.vertex_index; }
    int v2() const { return c2.idx.vertex_index; }
  };
  std::vector<Tri> tris;
  for (int p = 0; p < (int)polys.size(); ++p) {
    const auto& poly = polys[(size_t)p];
    int fv = (int)poly.size();
    for (int i = 1; i < fv - 1; ++i) {
      Tri t;
      t.c0 = {poly[0], p, 0};
      t.c1 = {poly[i], p, i};
      t.c2 = {poly[i + 1], p, i + 1};
      tris.push_back(t);
    }
  }

  // --- Triangle normals/areas
  std::vector<Eigen::Vector3d> triN(tris.size());
  std::vector<double> triA(tris.size());
  for (size_t ti = 0; ti < tris.size(); ++ti) {
    const Tri& t = tris[ti];
    if (t.v0() < 0 || t.v1() < 0 || t.v2() < 0 || t.v0() >= nV_in || t.v1() >= nV_in || t.v2() >= nV_in) {
      triN[ti] = Eigen::Vector3d(0, 0, 0);
      triA[ti] = 0.0;
      continue;
    }
    Eigen::Vector3d p0 = toV3(attrib.vertices, t.v0()), p1 = toV3(attrib.vertices, t.v1()),
                    p2 = toV3(attrib.vertices, t.v2());
    Eigen::Vector3d nraw = (p1 - p0).cross(p2 - p0);
    double dblA = nraw.norm();
    if (dblA < 1e-20) {
      triN[ti] = Eigen::Vector3d(0, 0, 0);
      triA[ti] = 0.0;
      continue;
    }
    triN[ti] = nraw / dblA;
    triA[ti] = 0.5 * dblA;
  }

  // --- Build triangle-edge adjacency; crease computation
  std::unordered_map<EdgeKey, std::pair<int, int>, EdgeKeyHash> edgeToTris;
  for (int ti = 0; ti < (int)tris.size(); ++ti) {
    const Tri& t = tris[(size_t)ti];
    auto addTE = [&](int a, int b) {
      EdgeKey ek{std::min(a, b), std::max(a, b)};
      auto& slot = edgeToTris[ek];
      if (slot.first == 0 && slot.second == 0)
        slot.first = ti;
      else if (slot.second == 0)
        slot.second = ti;
    };
    addTE(t.v0(), t.v1());
    addTE(t.v1(), t.v2());
    addTE(t.v2(), t.v0());
  }
  std::unordered_set<EdgeKey, EdgeKeyHash> creaseEdges;
  if (settings.creaseThresholdAngle > 0) {
    double thresh = deg2rad(settings.creaseThresholdAngle);
    for (const auto& kv : edgeToTris) {
      const EdgeKey& e = kv.first;
      int t0 = kv.second.first, t1 = kv.second.second;
      if (t0 < 0 || t1 < 0) continue;
      const Eigen::Vector3d &n0 = triN[(size_t)t0], n1 = triN[(size_t)t1];
      if (n0.norm() < 1e-12 || n1.norm() < 1e-12) continue;
      double ang = std::acos(clampd(n0.dot(n1), -1.0, 1.0));
      if (ang > thresh) creaseEdges.insert(e);
    }
  }

  // --- Smoothing groups per corner (see full version above; code omitted here for brevity but implemented as
  // described)
  struct SmoothingHandler {
    std::vector<std::unordered_map<int, int>> vTriToSG;
    void compute(int nV_in, const std::vector<Tri>& tris, const std::unordered_set<EdgeKey, EdgeKeyHash>& creaseEdges) {
      vTriToSG.resize((size_t)nV_in);
      std::vector<std::vector<int>> incident((size_t)nV_in);
      for (int ti = 0; ti < (int)tris.size(); ++ti) {
        const Tri& t = tris[(size_t)ti];
        if (t.v0() >= 0) incident[(size_t)t.v0()].push_back(ti);
        if (t.v1() >= 0) incident[(size_t)t.v1()].push_back(ti);
        if (t.v2() >= 0) incident[(size_t)t.v2()].push_back(ti);
      }
      auto connected = [&](int v, int ta, int tb) -> bool {
        const Tri &A = tris[(size_t)ta], &B = tris[(size_t)tb];
        int an1 = -1, an2 = -1, bn1 = -1, bn2 = -1;
        if (A.v0() == v) {
          an1 = A.v1();
          an2 = A.v2();
        } else if (A.v1() == v) {
          an1 = A.v0();
          an2 = A.v2();
        } else if (A.v2() == v) {
          an1 = A.v0();
          an2 = A.v1();
        } else
          return false;
        if (B.v0() == v) {
          bn1 = B.v1();
          bn2 = B.v2();
        } else if (B.v1() == v) {
          bn1 = B.v0();
          bn2 = B.v2();
        } else if (B.v2() == v) {
          bn1 = B.v0();
          bn2 = B.v1();
        } else
          return false;
        auto isNonCrease = [&](int x) {
          EdgeKey ek{std::min(v, x), std::max(v, x)};
          return creaseEdges.find(ek) == creaseEdges.end();
        };
        if (an1 == bn1 || an1 == bn2) return isNonCrease(an1);
        if (an2 == bn1 || an2 == bn2) return isNonCrease(an2);
        return false;
      };
      for (int v = 0; v < nV_in; ++v) {
        const auto& inc = incident[(size_t)v];
        if (inc.empty()) continue;
        auto& triToSg = vTriToSG[(size_t)v];
        int nextSg = 0;
        for (int tStart : inc) {
          if (triToSg.count(tStart)) continue;
          int sg = nextSg++;
          std::queue<int> q;
          q.push(tStart);
          triToSg[tStart] = sg;
          while (!q.empty()) {
            int ta = q.front();
            q.pop();
            for (int tb : inc) {
              if (triToSg.count(tb)) continue;
              if (connected(v, ta, tb)) {
                triToSg[tb] = sg;
                q.push(tb);
              }
            }
          }
        }
      }
    }
    int getSG(int v, int triIndex) const {
      if (v < 0 || v >= (int)vTriToSG.size()) return 0;
      const auto& m = vTriToSG[(size_t)v];
      auto it = m.find(triIndex);
      if (it == m.end()) return 0;
      return it->second;
    }
  } sh;
  if (settings.creaseThresholdAngle > 0) sh.compute(nV_in, tris, creaseEdges);

  // --- For each poly/corner: build out-vertex (SplitKey), and build faces for output; polyOutCorner for triangle
  // mapping
  std::unordered_map<SplitKey, int, SplitKeyHash> splitMap;
  std::vector<Eigen::Vector3d> outV;
  std::vector<std::vector<int>> outFaces(polys.size());
  std::vector<std::vector<int>> polyOutCorner(polys.size());

  auto getVT = [&](const tinyobj::index_t& idx) {
    int vt = idx.texcoord_index;
    if (vt < 0 || vt >= nVT_in) return -1;
    return vt;
  };
  auto getOrCreateOut = [&](int vin, int vt, int sg) {
    SplitKey key{vin, vt, sg};
    auto it = splitMap.find(key);
    if (it != splitMap.end()) return it->second;
    int idx = (int)outV.size();
    outV.push_back(toV3(attrib.vertices, vin));
    splitMap.emplace(key, idx);
    return idx;
  };

  // if split_by_crease, we need to know the smoothing group id for each poly corner (from an incident triangle)
  struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const noexcept {
      // Einfache Kombinierung der beiden ints zu einem size_t
      return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
  };
  std::unordered_map<std::pair<int, int>, int, PairHash> cornerToSG; // (polyid,corner)->sg
  if (settings.creaseThresholdAngle > 0) {
    for (size_t ti = 0; ti < tris.size(); ++ti) {
      const Tri& t = tris[ti];
      cornerToSG[{t.c0.poly, t.c0.corner}] = sh.getSG(t.v0(), (int)ti);
      cornerToSG[{t.c1.poly, t.c1.corner}] = sh.getSG(t.v1(), (int)ti);
      cornerToSG[{t.c2.poly, t.c2.corner}] = sh.getSG(t.v2(), (int)ti);
    }
  }

  for (int p = 0; p < (int)polys.size(); ++p) {
    const auto& poly = polys[(size_t)p];
    int fv = (int)poly.size();
    outFaces[(size_t)p].resize((size_t)fv);
    polyOutCorner[(size_t)p].resize((size_t)fv);
    for (int k = 0; k < fv; ++k) {
      const tinyobj::index_t& idx = poly[(size_t)k];
      int vin = idx.vertex_index;
      int vt = getVT(idx);
      int sg = 0;
      if (settings.creaseThresholdAngle > 0) {
        auto it = cornerToSG.find({p, k});
        sg = (it != cornerToSG.end()) ? it->second : 0;
      }
      int idxOut = getOrCreateOut(vin, vt, sg);
      outFaces[(size_t)p][(size_t)k] = idxOut;
      polyOutCorner[(size_t)p][(size_t)k] = idxOut;
    }
  }
  int nV_out = (int)outV.size();

  // --- Build adjacency (for fill)
  std::vector<std::unordered_set<int>> adj((size_t)nV_out);
  for (const auto& face : outFaces) {
    int fv = (int)face.size();
    for (int i = 0; i < fv; ++i) {
      int a = face[i], b = face[(i + 1) % fv];
      if (a != b) {
        adj[(size_t)a].insert(b);
        adj[(size_t)b].insert(a);
      }
    }
  }

  // --- Tangent/normal accumulation (as before: per-vertex normals and U/V-tangent, area-weighted) ---
  std::vector<Eigen::Vector3d> vNormal((size_t)nV_out, Eigen::Vector3d(0, 0, 0));
  std::vector<Eigen::Vector3d> vTangent((size_t)nV_out, Eigen::Vector3d(0, 0, 0));
  std::vector<double> vWeight((size_t)nV_out, 0.0);

  auto getUV = [&](int vt) -> Eigen::Vector2d {
    if (vt < 0 || vt >= nVT_in) return Eigen::Vector2d(0, 0);
    return toV2(attrib.texcoords, vt);
  };

  auto outIndexForTriCorner = [&](const Tri& t, int corner) -> int {
    const TriCorner* c = (corner == 0) ? &t.c0 : (corner == 1 ? &t.c1 : &t.c2);
    int p = c->poly, k = c->corner;
    if (p < 0 || p >= (int)polyOutCorner.size()) return -1;
    if (k < 0 || k >= (int)polyOutCorner[(size_t)p].size()) return -1;
    return polyOutCorner[(size_t)p][(size_t)k];
  };

  for (size_t ti = 0; ti < tris.size(); ++ti) {
    const Tri& t = tris[ti];
    double area = triA[ti];
    const Eigen::Vector3d& n = triN[ti];
    if (area <= 0.0 || n.norm() < 1e-12) continue;
    int ov0 = outIndexForTriCorner(t, 0), ov1 = outIndexForTriCorner(t, 1), ov2 = outIndexForTriCorner(t, 2);
    if (ov0 < 0 || ov1 < 0 || ov2 < 0) continue;
    vNormal[(size_t)ov0] += area * n;
    vNormal[(size_t)ov1] += area * n;
    vNormal[(size_t)ov2] += area * n;

    int vt0 = t.c0.idx.texcoord_index, vt1 = t.c1.idx.texcoord_index, vt2 = t.c2.idx.texcoord_index;
    if (vt0 < 0 || vt1 < 0 || vt2 < 0 || vt0 >= nVT_in || vt1 >= nVT_in || vt2 >= nVT_in) continue;
    int v0 = t.v0(), v1 = t.v1(), v2 = t.v2();
    if (v0 < 0 || v1 < 0 || v2 < 0) continue;
    Eigen::Vector3d p0 = toV3(attrib.vertices, v0), p1 = toV3(attrib.vertices, v1), p2 = toV3(attrib.vertices, v2);
    Eigen::Vector2d w0 = getUV(vt0), w1 = getUV(vt1), w2 = getUV(vt2);
    Eigen::Vector3d e1 = p1 - p0, e2 = p2 - p0;
    Eigen::Vector2d d1 = w1 - w0, d2 = w2 - w0;
    double denom = d1.x() * d2.y() - d2.x() * d1.y();
    if (std::abs(denom) < 1e-20) continue;
    double r = 1.0 / denom;
    Eigen::Vector3d dPdu = (e1 * d2.y() - e2 * d1.y()) * r, dPdv = (e2 * d1.x() - e1 * d2.x()) * r;
    Eigen::Vector3d tdir = (settings.axis == 'U') ? dPdu : dPdv;
    tdir = projectToTangent(tdir, n);
    tdir = safeNormalize(tdir);
    if (tdir.norm() < 1e-12) continue;

    auto accumT = [&](int ov) {
      Eigen::Vector3d cur = vTangent[(size_t)ov];
      if (cur.norm() > 1e-12 && cur.dot(tdir) < 0)
        vTangent[(size_t)ov] += area * (-tdir);
      else
        vTangent[(size_t)ov] += area * tdir;
      vWeight[(size_t)ov] += area;
    };
    accumT(ov0);
    accumT(ov1);
    accumT(ov2);
  }

  // Normalize normals/per-vertex tangent plane
  std::vector<Eigen::Vector3d> outFlow((size_t)nV_out, Eigen::Vector3d(0, 0, 0));
  for (int v = 0; v < nV_out; ++v) {
    Eigen::Vector3d t = (vWeight[(size_t)v] > 0.0)
        ? (vTangent[(size_t)v] / vWeight[(size_t)v])
        : Eigen::Vector3d(0, 0, 0);
    Eigen::Vector3d n = safeNormalize(vNormal[(size_t)v]);
    if (n.norm() < 1e-12) n = Eigen::Vector3d(0, 1, 0);
    t = projectToTangent(t, n);
    t = safeNormalize(t);
    outFlow[(size_t)v] = t;
  }
  // Fill for zero vectors
  const int fillerIterations = 5;
  for (int pass = 0; pass < fillerIterations; ++pass) {
    int changed = 0;
    for (int v = 0; v < nV_out; ++v) {
      if (outFlow[(size_t)v].norm() > 1e-6) continue;
      Eigen::Vector3d n = safeNormalize(vNormal[(size_t)v]);
      if (n.norm() < 1e-12) n = Eigen::Vector3d(0, 1, 0);
      Eigen::Vector3d acc(0, 0, 0);
      int cnt = 0;
      for (int nb : adj[(size_t)v]) {
        Eigen::Vector3d d = outFlow[(size_t)nb];
        if (d.norm() < 1e-6) continue;
        if (acc.norm() > 1e-12 && acc.dot(d) < 0) d *= -1.0;
        acc += d;
        cnt++;
      }
      if (cnt > 0) {
        Eigen::Vector3d t = safeNormalize(projectToTangent(acc, n));
        if (t.norm() > 0) {
          outFlow[(size_t)v] = t;
          changed++;
        }
      }
    }
    if (changed == 0) break;
  }

  // --- Output: triangulated mesh, layout: pos.xyz, flow.xyz, indices (GL_TRIANGLES)
  outVert.reserve((size_t)nV_out * 6);
  for (int i = 0; i < nV_out; ++i) {
    const auto &p = outV[(size_t)i], &t = outFlow[(size_t)i];
    outVert.push_back((float)p.x());
    outVert.push_back((float)p.y());
    outVert.push_back((float)p.z());
    outVert.push_back((float)t.x());
    outVert.push_back((float)t.y());
    outVert.push_back((float)t.z());
  }
  // Output triangles
  outInd.reserve((size_t)tris.size() * 3);
  for (int p = 0; p < (int)polys.size(); ++p) {
    const auto& face = polyOutCorner[(size_t)p];
    int fv = (int)face.size();
    if (fv < 3) continue;
    for (int i = 1; i < fv - 1; ++i) {
      outInd.push_back((unsigned int)face[0]);
      outInd.push_back((unsigned int)face[i]);
      outInd.push_back((unsigned int)face[i + 1]);
    }
  }
  return true;
}
