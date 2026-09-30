#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "ReactionDiffusion.hpp"

#include "emp/config/ArgManager.hpp"
#include "emp/math/Random.hpp"
#include "emp/prefab/ConfigPanel.hpp"
#include "emp/web/Animate.hpp"
#include "emp/web/Button.hpp"
#include "emp/web/Div.hpp"
#include "emp/web/UrlParams.hpp"
#include "emp/web/web.hpp"

EMP_BUILD_CONFIG(MyConfigType,
    VALUE(ITERATIONS, int, 4, "How many iterations should run?")
)

emp::web::Document doc("animation");
emp::web::Document settings("settings");
emp::web::Document buttons("buttons");
MyConfigType config;

struct TurtleState {
  double x;
  double y;
  double angle;
  double line_width;
  int branch_depth;
};

class SystemAnimator : public emp::web::Animate {
 private:
  double canvas_width = 600.0;
  double canvas_height = 600.0;

  // Web UI Components
  emp::web::Canvas canvas{canvas_width, canvas_height, "canvas"};
  emp::web::Div info_panel{"info_panel"};

  // L-System Grammar & Parameters
  std::string axiom;
  std::string current_string;
  std::unordered_map<char, std::string> rules;

  int current_depth = 0;
  int max_depth = 4;
  double initial_step_length = 35.0;
  double step_length = 35.0;
  double step_decay = 0.68;
  double angle_rad = 0.0;

  // Animation timing
  int frame_counter = 0;
  int frames_per_expansion = 60;

  // Reaction-Diffusion Grid Instance
  ReactionDiffusion rd_grid{60, 60, canvas_height, canvas_width};
  double tropism_strength = 0.18;

  // Useful state variables
  double cur_x = canvas_width / 2.0;
  double cur_y = canvas_height - 30.0;
  double cur_angle = -3.14159265358979323846 / 2.0;
  double cur_width = 3.5;
  int branch_depth = 0;

 public:
  SystemAnimator() {
    doc << canvas;
    doc << GetToggleButton("Toggle");
    doc << GetStepButton("Step");
    doc << emp::web::Button([this]() { Reset(); }, "Reset", "reset_btn");
    doc << info_panel;

    auto specs = emp::ArgManager::make_builtin_specs(&config);
    emp::ArgManager am(emp::web::GetUrlParams(), specs);
    
    am.UseCallbacks();
    if (am.HasUnused()) std::exit(EXIT_FAILURE);

    SetupGrammar();

    emp::prefab::ConfigPanel config_panel(config);
    settings << config_panel;

    Reset();
  }

  void SetupGrammar() {
    axiom = "F";
    rules['F'] = "F[+F]F[-F][F]";

    angle_rad = 22.5 * (3.14159265358979323846 / 180.0);
    initial_step_length = 32.0;
    step_decay = 0.68;
    max_depth = config.ITERATIONS();
  }

  void ResetTurtleState() {
    cur_x = canvas_width / 2.0;
    cur_y = canvas_height - 30.0;
    cur_angle = -3.14159265358979323846 / 2.0;
    cur_width = 3.5;
    branch_depth = 0;
  }

  void Reset() {
    current_string = axiom;
    current_depth = 0;
    step_length = initial_step_length;
    frame_counter = 0;

    rd_grid.Reset();
    std::vector<TurtleState> state_stack;
    DrawSubstring(current_string, state_stack);
    UpdateUI();
  }

  void DrawSubstring(std::string expansion, std::vector<TurtleState>& state_stack){
    for (char sub_c : expansion) {
      if (sub_c == 'F') {
        double next_x = cur_x + step_length * std::cos(cur_angle);
        double next_y = cur_y + step_length * std::sin(cur_angle);

        std::string line_color = "lightgreen";
        double line_width = 1.0;

        canvas.Line(cur_x, cur_y, next_x, next_y, line_color, line_width);

        cur_x = next_x;
        cur_y = next_y;
      } else if (sub_c == '+') {
        cur_angle += angle_rad;
      } else if (sub_c == '-') {
        cur_angle -= angle_rad;
      } else if (sub_c == '[') {
        state_stack.push_back({cur_x, cur_y, cur_angle, cur_width, branch_depth});
        branch_depth++;
      } else if (sub_c == ']') {
        if (!state_stack.empty()) {
          TurtleState top = state_stack.back();
          state_stack.pop_back();
          cur_x = top.x;
          cur_y = top.y;
          cur_angle = top.angle;
          cur_width = top.line_width;
          branch_depth = top.branch_depth;
        }
      }
    }

  }

  void ExpandAndDraw() {
    canvas.Clear("#0b1d28");
    rd_grid.Draw(canvas);

    ResetTurtleState();
    std::vector<TurtleState> state_stack;
    std::string next_string = "";
    
    for (char c : current_string) {
      std::string expansion = "";
      // Determines how the current character will be expanded
      if (rules.count(c)) {
        expansion = rules.at(c);
      } else {
        expansion = std::string(1, c);
      }

      next_string += expansion;

      // Draws the newly expanded component
      DrawSubstring(expansion, state_stack);
    }

    // Save expanded string for the next growth depth
    current_string = next_string;
    step_length *= step_decay;
    current_depth++;

    UpdateUI();
  }

  // For non-expansion animation frames
  void RedrawFullSystem() {
    canvas.Clear("#0b1d28");
    rd_grid.Draw(canvas);

    ResetTurtleState();
    std::vector<TurtleState> state_stack;
    DrawSubstring(current_string, state_stack);
  }

  void UpdateUI() {
    info_panel.Clear();
    info_panel << "<p><b>Growth Depth:</b> " << current_depth << " / " << max_depth << "</p>";
    info_panel << "<p><b>Genotype Length (Characters):</b> " << current_string.length() << "</p>";
  }

  void DoFrame() override {
    rd_grid.Update(4);

    frame_counter++;
    if (frame_counter >= frames_per_expansion && current_depth < max_depth) {
      ExpandAndDraw();
      frame_counter = 0;
    } else {
      RedrawFullSystem(); // Re-render background & current plant state
    }
  }
};

SystemAnimator animator;

int main() {
  animator.Step();
}