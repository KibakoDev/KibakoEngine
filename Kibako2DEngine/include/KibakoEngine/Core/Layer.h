// Base interface implemented by application layers
#pragma once

#include <string>
#include <utility>

namespace KibakoEngine
{
    class Application;
    class SpriteBatch2D;

    // Minimal interface used by the application loop to drive layers.
    class Layer
    {
    public:
        explicit Layer(std::string name) : m_name(std::move(name)) {}
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        // Variable step (once per frame)
        virtual void OnUpdate(float /*dt*/) {}

        // Fixed step 
        virtual void OnFixedUpdate(float /*fixedDt*/) {}

        // Render step
        virtual void OnRender(SpriteBatch2D& /*batch*/) {}

        const std::string& Name() const { return m_name; }

    protected:
        Application* m_appPtr = nullptr;

    private:
        std::string m_name;
    };
} // namespace KibakoEngine