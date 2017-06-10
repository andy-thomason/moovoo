////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2017
//
// gilgamesh: distance field class
//
// This field represents the minimum distance from a set of points.
// It is used in bioinformatics to represent surfaces of molecules and in
// computer graphics for ray and cone tracing of scenes.
// 

#ifndef GILGAMESH_DISTANCE_FIELD_INCLUDED
#define GILGAMESH_DISTANCE_FIELD_INCLUDED

#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

// Inspired by:
// George J. Grevera The "dead reckoning" signed distance transform.
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.102.7988&rep=rep1&type=pdf
//

namespace gilgamesh {

  class distance_field {
  public:
    distance_field() {
    }

    /// Construct a distance field from a set of points.
    /// Returns the distance for each 3D grid point and the index of the closest point
    /// (ie. the Voronoi region).
    distance_field(int xdim, int ydim, int zdim, float grid_spacing, glm::vec3 min, const std::vector<glm::vec3> &points, const std::vector<float> &radii) {
      int rmask = radii.size() == 1 ? 0 : -1;

      auto distance = [&points, &radii, rmask](int pindex, glm::vec3 pos) {
        return glm::length(points[pindex] - pos) - radii[pindex & rmask];
      };

      int size = xdim * ydim * zdim;
      pindices_ = std::vector<int>(size, -1);
      distances_ = std::vector<float>(size);

      // Set one grid point for each point in the list.
      // These points act as seeds for the sweep.
      for (int pindex = 0; pindex != points.size(); ++pindex) {
        glm::vec3 xyz = (points[pindex] - min) * (1.0f/ grid_spacing);
        int x = int(std::floor(xyz.x + 0.5f));
        int y = int(std::floor(xyz.y + 0.5f));
        int z = int(std::floor(xyz.z + 0.5f));
        //printf("%f %f %f %d %d %d\n", xyz.x, xyz.y, xyz.z, x, y, z);
        glm::vec3 pos = min + glm::vec3(x, y, z) * grid_spacing;
        if (x >= 0 && x < xdim && y >= 0 && y < ydim && z >= 0 && z < zdim) {
          int index = ((z * ydim) + y) * xdim + x; 
          float new_d = distance(pindex, pos);
          if (pindices_[index] == -1 || new_d < distances_[index]) {
            pindices_[index] = pindex;
            distances_[index] = new_d;
          }
        }
      }

      // sweep the field up and down.
      sweep(xdim, ydim, zdim, grid_spacing, min, int(points.size()), distance);
    }

    /// Closest distance to points
    std::vector<float> &distances() { return distances_; }

    /// Index of the closest point 
    std::vector<int> &pindices() { return pindices_; }
    
  private:
    // generalised bidirectional sweep for points, spheres or other primitives.
    template<class DistanceFn>
    void sweep(int xdim, int ydim, int zdim, float grid_spacing, glm::vec3 min, int num_objects, DistanceFn &distance) {
      // Offsets for up to 13 adjacent locations.
      // Note that when scanning down, these offsets are negated.
      std::array<int, 13> offsets;
      std::array<float, 13> diag;
      int max_k = 0;
      for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
          for (int x = -1; x <= 1; ++x) {
            if (z < 0 || z == 0 && y < 0 || z == 0 && y == 0 && x < 0) {
              diag[max_k] = glm::length(glm::vec3(x, y, z));
              offsets[max_k] = ((z * ydim) + y) * xdim + x;
              //printf("%2d %2d %2d %2d %5d %f\n", max_k, x, y, z, offsets[max_k], diag[max_k]);
              max_k++;
            }
          }
        }
      }

      // sweep up in xyz in pass 0 and down in pass 1
      glm::vec3 max = min + glm::vec3(xdim-1, ydim-1, zdim-1) * grid_spacing;
      int size = xdim * ydim * zdim;
      for (int pass = 0; pass != 2; ++pass) {
        int index = pass == 0 ? 0 : size-1;
        int mul = pass == 0 ? 1 : -1;
        glm::vec3 minmax = pass == 0 ? min : max;
        for (int z = 0; z != zdim; ++z) {
          for (int y = 0; y != ydim; ++y) {
            for (int x = 0; x != xdim; ++x, index += mul) {
              float dist = distances_[index];
              int pindex = pindices_[index];
              int min_k = (z == 0 ? 9 : 0) + (y == 0 ? 3 : 0) + (x == 0);
              glm::vec3 pos = minmax + glm::vec3(x, y, z) * (grid_spacing * mul);

              // My feeling here is that we can skip the diag[] term
              // and just minimise the euclidean distance.
              for (int k = min_k; k != max_k; ++k) {
                int new_index = index + offsets[k] * mul;
                float new_d = distances_[new_index] + diag[k];
                if (pindex == -1 || new_d < dist) {
                  //printf("p=%d (%f %f %f) k=%d ni=%d %f %f %d\n", pass, pos.x, pos.y, pos.z, k, new_index, distance, new_d, new_d < distance);
                  pindex = pindices_[new_index];
                  dist = distance(pindex, pos);
                }
              }
              distances_[index] = dist;
              pindices_[index] = pindex;
            }
          }
        }
      }
      /*for (int y = 0; y != ydim; ++y) {
        for (int x = 0; x != xdim; ++x) {
          int z = 25;
          int index = idx(x, y, z);
          printf("%c", (pindices_[index] % 64 + '!'));
        }
        printf("\n");
      }*/
    }

    // Closest distance to points
    std::vector<float> distances_;

    // Index of the closest point 
    std::vector<int> pindices_;
  };
}

#endif
