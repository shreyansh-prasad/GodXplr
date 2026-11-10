#include <iostream>
#include <vector>
#include <string>

// GodXplr Liquid Glass Engine
class LiquidGlassRenderer {
public:
    LiquidGlassRenderer(int width, int height)
        : width_(width), height_(height) {}

    void renderBackdrop() {
        std::cout << "Rendering frosted obsidian glass\n";
    }

private:
    int width_;
    int height_;
};

int main() {
    LiquidGlassRenderer renderer(1420, 880);
    renderer.renderBackdrop();
    return 0;
}
