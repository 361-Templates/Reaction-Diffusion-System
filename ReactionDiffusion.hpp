#ifndef REACTION_DIFFUSION_HPP
#define REACTION_DIFFUSION_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "emp/web/Canvas.hpp"

class ReactionDiffusion {
 private:
  int width;
  int height;
  double canvas_height;
  double canvas_width;

  std::vector<double> U; // U feeds into the system
  std::vector<double> V; // V consumes U to replicate and decays into inert product

  // Gray-Scott parameters
  double diffusion_u = 0.16;
  double diffusion_v = 0.08;
  double feed_rate = 0.035; // rate that U feeds into system
  double kill_rate = 0.060; // rate that V decays to nothing

  // Given the x, y coordinates on the grid, gives the index in the vector
  int Index(int x, int y) const {
    return y * width + x;
  }

 public:
  ReactionDiffusion(int grid_w = 60, int grid_h = 60, double canvas_h = 600.0, double canvas_w = 600.0)
      : width(grid_w), height(grid_h), U(grid_w * grid_h, 1.0), V(grid_w * grid_h, 0.0),
      canvas_height(canvas_h), canvas_width(canvas_w) {
    Reset();
  }

  // --- USEFUL HELPER METHODS ---

  /// Samples chemical V concentration directly at continuous canvas coordinates (x, y)
  double GetVAtCanvas(double canvas_x, double canvas_y) const {
    int gx = std::clamp(static_cast<int>(canvas_x / canvas_width * width), 0, width - 1);
    int gy = std::clamp(static_cast<int>(canvas_y / canvas_height * height), 0, height - 1);
    return V[Index(gx, gy)];
  }

  /// Projects a point at `distance` along `angle` from (canvas_x, canvas_y) and samples chemical V
  double SampleOffset(double canvas_x, double canvas_y, double angle, double distance) const {
    double probe_x = canvas_x + distance * std::cos(angle);
    double probe_y = canvas_y + distance * std::sin(angle);
    return GetVAtCanvas(probe_x, probe_y);
  }

  /// Deposits (or consumes if amount is negative) chemical V at canvas coordinates (x, y)
  void DepositAtCanvas(double canvas_x, double canvas_y, double amount = 0.2) {
    int gx = std::clamp(static_cast<int>(canvas_x / canvas_width * width), 0, width - 1);
    int gy = std::clamp(static_cast<int>(canvas_y / canvas_height * height), 0, height - 1);
    int idx = Index(gx, gy);
    V[idx] = std::clamp(V[idx] + amount, 0.0, 1.0);
  }

  void Reset() {
    std::fill(U.begin(), U.end(), 1.0);
    std::fill(V.begin(), V.end(), 0.0);

    // Seed morphogen source(s) via dictionary pairs such as {0,0}
    int seed_coords[][2] = {{0,0}};
    for (auto& coord : seed_coords) {
      int cx = coord[0];
      int cy = coord[1];
      for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
          int gx = cx + dx;
          int gy = cy + dy;
          if (gx >= 0 && gx < width && gy >= 0 && gy < height) {
            int idx = Index(gx, gy);
            U[idx] = 0.50;
            V[idx] = 0.25;
          }
        }
      }
    }
  }

  // Step Gray-Scott PDE forward in time
  void Update(int steps = 1) {
    for (int step = 0; step < steps; ++step) {
      std::vector<double> next_U = U;
      std::vector<double> next_V = V;

      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          int idx = Index(x, y);
          double u = U[idx];
          double v = V[idx];

          // 5-point discrete 2D Laplacian
          double lap_u = U[Index(x - 1, y)] + U[Index(x + 1, y)] + 
                         U[Index(x, y - 1)] + U[Index(x, y + 1)] - 4.0 * u;
          double lap_v = V[Index(x - 1, y)] + V[Index(x + 1, y)] + 
                         V[Index(x, y - 1)] + V[Index(x, y + 1)] - 4.0 * v;

          double uvv = u * v * v;
          next_U[idx] = std::clamp(u + (diffusion_u * lap_u - uvv + feed_rate * (1.0 - u)), 0.0, 1.0);
          next_V[idx] = std::clamp(v + (diffusion_v * lap_v + uvv - (feed_rate + kill_rate) * v), 0.0, 1.0);
        }
      }
      U = next_U;
      V = next_V;
    }
  }

  double GetV(double canvas_x, double canvas_y) const {
    return GetVAtCanvas(canvas_x, canvas_y);
  }

  // Calculate local concentration gradient ∇V = (∂V/∂x, ∂V/∂y)
  void GetGradient(double canvas_x, double canvas_y,  
                   double& grad_x, double& grad_y) const {
    int gx = std::clamp(static_cast<int>(canvas_x / canvas_width * width), 1, width - 2);
    int gy = std::clamp(static_cast<int>(canvas_y / canvas_height * height), 1, height - 2);

    grad_x = V[Index(gx + 1, gy)] - V[Index(gx - 1, gy)];
    grad_y = V[Index(gx, gy + 1)] - V[Index(gx, gy - 1)];
  }

  // Draw background field on canvas
  void Draw(emp::web::Canvas& canvas) const {
    double cell_w = canvas_width / width;
    double cell_h = canvas_height / height;

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        double val = V[Index(x, y)];
        if (val > 0.05) {
          int intensity = static_cast<int>(std::min(255.0, val * 550.0));
          std::string color = "rgb(" + std::to_string(intensity / 4) + "," + 
                              std::to_string(intensity / 2) + "," + 
                              std::to_string(intensity) + ")";
          canvas.Rect(x * cell_w, y * cell_h, cell_w, cell_h, color);
        }
      }
    }
  }

  int GetWidth() const { return width; }
  int GetHeight() const { return height; }
};

#endif // REACTION_DIFFUSION_HPP