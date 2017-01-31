// GLEventWindow.cpp

#include <Engine/UpdatePipeline.h>
#include <Engine/SystemFrame.h>
#include <Engine/ProcessingFrame.h>
#include <Engine/Components/Gameplay/CInput.h>
#include "../include/GLWindow/GLEventWindow.h"

namespace sge
{
    GLFWwindow* create_sge_opengl_window(const char* title, int width, int height)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
        return glfwCreateWindow(width, height, title, nullptr, nullptr);
    }

    GLEventWindow::GLEventWindow()
        : _has_focus(true),
        _window(nullptr),
        _mouse_x(0.0),
        _mouse_y(0.0)
    {
    }

    GLEventWindow::GLEventWindow(GLFWwindow* window)
        : GLEventWindow()
    {
        set_window(window);
    }

    GLEventWindow::~GLEventWindow()
    {
        unset_window();
    }

    void GLEventWindow::set_window(GLFWwindow* window)
    {
        // Remove the currently active window (if any)
        unset_window();
        if (!window)
        {
            return;
        }

        // Set up the given window on this window
        _window = window;
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, &glfw_window_resize_callback);
        glfwSetWindowFocusCallback(window, &glfw_window_focus_callback);
        glfwSetKeyCallback(_window, &glfw_key_callback);
        glfwSetMouseButtonCallback(_window, &glfw_mouse_button_callback);
        glfwSetCursorPosCallback(window, &glfw_cursor_callback);
    }

    void GLEventWindow::unset_window()
    {
        if (!_window)
        {
            return;
        }

        glfwSetCursorPosCallback(_window, nullptr);
        glfwSetMouseButtonCallback(_window, nullptr);
        glfwSetKeyCallback(_window, nullptr);
        glfwSetWindowFocusCallback(_window, nullptr);
        glfwSetFramebufferSizeCallback(_window, nullptr);
        glfwSetWindowUserPointer(_window, nullptr);
        _window = nullptr;
    }

    void GLEventWindow::set_resize_callback(std::function<ResizeCallbackFn> resize_callback)
    {
        _resize_callback = std::move(resize_callback);
    }

    void GLEventWindow::register_pipeline(UpdatePipeline& pipeline)
    {
        pipeline.register_system_fn("gl_window_input", this, &GLEventWindow::process_input);
    }

    void GLEventWindow::set_bindings(InputBindings bindings)
    {
        _bindings = std::move(bindings);
    }

    void GLEventWindow::glfw_window_resize_callback(GLFWwindow* window, int width, int height)
    {
        auto* event_window = static_cast<GLEventWindow*>(glfwGetWindowUserPointer(window));

        if (event_window->_resize_callback)
        {
            event_window->_resize_callback(*event_window, width, height);
        }
    }

    void GLEventWindow::glfw_window_focus_callback(GLFWwindow* window, int has_focus)
    {
        auto* event_window = static_cast<GLEventWindow*>(glfwGetWindowUserPointer(window));
        event_window->_has_focus = (has_focus == GLFW_TRUE);
    }

    void GLEventWindow::glfw_key_callback(GLFWwindow* window, int key, int scancode, int state, int mods)
    {
        auto* event_window = static_cast<GLEventWindow*>(glfwGetWindowUserPointer(window));

        // Search for the key in the set of bindings
        auto iter = event_window->_bindings.key_bindings.find(key);
        if (iter == event_window->_bindings.key_bindings.end())
        {
            return;
        }

        // If the key has been pressed
        switch (state)
        {
        case GLFW_PRESS:
            event_window->_active_action_bindings.insert(iter->second.c_str());
            break;

        case GLFW_RELEASE:
            event_window->_active_action_bindings.erase(iter->second.c_str());
            break;
        }
    }

    void GLEventWindow::glfw_mouse_button_callback(GLFWwindow* window, int key, int state, int mods)
    {
        auto* event_window = static_cast<GLEventWindow*>(glfwGetWindowUserPointer(window));

        // Search for the mouse button in the set of bindings
        auto iter = event_window->_bindings.mouse_button_bindings.find(key);
        if (iter == event_window->_bindings.mouse_button_bindings.end())
        {
            return;
        }

        // If the button has been pressed
        switch (state)
        {
        case GLFW_PRESS:
            event_window->_active_action_bindings.insert(iter->second.c_str());
            break;

        case GLFW_RELEASE:
            event_window->_active_action_bindings.erase(iter->second.c_str());
            break;
        }
    }

    void GLEventWindow::glfw_cursor_callback(GLFWwindow* window, double x, double y)
    {
        auto event_window = static_cast<GLEventWindow*>(glfwGetWindowUserPointer(window));

        // Update mouse X and Y
        event_window->_mouse_x = x;
        event_window->_mouse_y = y;
    }

    void GLEventWindow::process_input(SystemFrame& frame, float /*time*/, float /*dt*/)
    {
        if (!_has_focus)
        {
            return;
        }

        frame.process_entities([this, &frame](
            ProcessingFrame& pframe,
            EntityId /*entity*/,
            const CInput& input)
        {
            // For each keyboard binding
            for (auto binding : this->_active_action_bindings)
            {
                input.add_action_event(CInput::FActionEvent{ binding });
            }

            // For each mouse x-axis binding
            for (const auto& binding : this->_bindings.mouse_x_bindings)
            {
                input.add_axis_event(CInput::FAxisEvent{ binding.c_str(), static_cast<float>(this->_mouse_x), 1.f });
            }

            // For each mouse y-axis binding
            for (const auto& binding : this->_bindings.mouse_y_bindings)
            {
                input.add_axis_event(CInput::FAxisEvent{ binding.c_str(), static_cast<float>(this->_mouse_y), 1.f });
            }
        });
    }
}
