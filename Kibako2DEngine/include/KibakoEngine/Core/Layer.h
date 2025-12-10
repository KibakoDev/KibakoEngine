// Base interface implemented by application layers
#pragma once

#include <string>

namespace KibakoEngine
{
    class Application;

    // Minimal interface used by the application loop to drive layers.
    class Layer
    {
    public:
        explicit Layer(std::string name) : m_name(std::move(name)) {}
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(float /*dt*/) {}

        // Rendering is delegated to the concrete layer.
        virtual void OnRender(class SpriteBatch2D& /*batch*/) {}

        const std::string& Name() const { return m_name; }

    protected:
        Application* m_appPtr = nullptr;

    private:
        std::string m_name;
    };
} // namespace KibakoEngine

